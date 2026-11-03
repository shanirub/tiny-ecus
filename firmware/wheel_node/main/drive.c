#include "drive.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "encoder.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pins.h"
#include "telemetry.h"
#include "tiny_pi.h"
#include "tiny_speed.h"

#define PERIOD_MS        10   // 100 Hz control period
#define DRIVE_PRIORITY   6    // highest of our tasks
#define DRIVE_STACK      3072
// 48 counts per motor turn x 1:90 gearbox. C.3 measured 4386 by hand; the
// nominal value is kept (decided 2026-10-29).
#define COUNTS_PER_TURN  4320

// LEDC: 20 kHz (above hearing), 10-bit duty. Duty 1 << 10 is constantly high.
#define PWM_FREQ_HZ      20000
#define PWM_RES          LEDC_TIMER_10_BIT
#define PWM_FULL         (1u << 10)
#define PWM_MODE         LEDC_LOW_SPEED_MODE  // the only mode on the C3
#define PWM_TIMER        LEDC_TIMER_0
#define PWM_CH_IN1       LEDC_CHANNEL_0
#define PWM_CH_IN2       LEDC_CHANNEL_1

// Protections (Phase 1 plan D.2). Starting values, to be revisited with the
// D.4 measurements.
static const tiny_motor_cfg_t s_cfg = {
    .cap = 800,          // 80 % duty at most
    .slew = 50,          // 5 % per period: 0 -> 100 % takes at least 200 ms
    .stall_duty = 300,   // stall check from 30 % duty up
    .stall_counts = 1,   // <= 1 count per 10 ms (about 1.4 wheel rpm) = not turning
    .stall_steps = 30,   // for 300 ms
};

// Speed controller (Phase 1 E). The feedforward is the D.4.2 line
// (rpm = 1.37 per % - 4.2): 10 / 1.37 = 7.3 permille per rpm, plus 4.2 rpm of
// friction. kp and ki come from a simulation of that line with a 60 ms time
// constant; the bench check (2026-11-01) kept them unchanged. SET changes all
// three live.
static tiny_pi_cfg_t s_pi_cfg = {
    .kp = 6.0f,
    .ki = 20.0f,
    .ff = 7.3f,
    .ff_offset = 4.2f,
    .dt = PERIOD_MS / 1000.0f,
    .out_max = 800.0f,  // the duty cap, s_cfg.cap
};

// Shared between the drive task and the console; only touched inside s_lock.
// s_lock masks interrupts, the encoder's too, so nothing slow runs inside it:
// the float math works on copies (measured 2026-11-02: with tiny_pi_step()
// inside, the locked section reached 50 µs, against encoder edges as close as
// 14.4 µs apart at full speed).
static tiny_motor_t s_motor;
static tiny_motor_decay_t s_decay = TINY_MOTOR_DECAY_SLOW;
static tiny_pi_t s_pi;
static bool s_speed_run;   // the run in progress is a SPEED run
static float s_setpoint;   // its wanted speed, rpm
static uint32_t s_cmd_seq; // bumped by every command that changes the run
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

static void set_pins(uint32_t in1, uint32_t in2)
{
    ESP_ERROR_CHECK(ledc_set_duty(PWM_MODE, PWM_CH_IN1, in1));
    ESP_ERROR_CHECK(ledc_update_duty(PWM_MODE, PWM_CH_IN1));
    ESP_ERROR_CHECK(ledc_set_duty(PWM_MODE, PWM_CH_IN2, in2));
    ESP_ERROR_CHECK(ledc_update_duty(PWM_MODE, PWM_CH_IN2));
}

// Only this task writes the driver pins. Running: driver awake, PWM in the
// selected decay mode. Otherwise: both inputs low (coast) and the driver asleep.
// STBY is the outer switch: inputs before waking, sleep before the inputs.
// A new LEDC duty starts at the next PWM period (up to 50 µs) and the two
// channels may switch in different periods, so the inputs pass through
// mixed states; with the driver asleep those never reach the motor.
static void apply(tiny_motor_mode_t mode, int32_t duty, tiny_motor_decay_t decay)
{
    if (mode == TINY_MOTOR_RUN) {
        uint32_t in1, in2;
        tiny_motor_pins(duty, decay, PWM_FULL, &in1, &in2);
        set_pins(in1, in2);
        gpio_set_level(MOTOR_STBY_GPIO, 1);  // the driver ignores inputs for up to 1 ms after waking
    } else {
        gpio_set_level(MOTOR_STBY_GPIO, 0);
        set_pins(0, 0);
    }
}

static void drive_task(void *arg)
{
    (void)arg;
    int32_t last_pos;
    uint32_t invalid;
    encoder_read(&last_pos, &invalid);

    // What the motor output during the period now running, for its telemetry row.
    telem_mode_t out_mode = TELEM_MODE_IDLE;
    int32_t out_setpoint = 0;
    int32_t out_duty = 0;

    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(PERIOD_MS));
        uint32_t t_ms = (uint32_t)(esp_timer_get_time() / 1000);

        int32_t pos;
        encoder_read(&pos, &invalid);
        // Wrapping difference, like the decoder's position. After ENC ZERO it
        // jumps once; that only reads as "moving" for one period.
        int32_t counts = (int32_t)((uint32_t)pos - (uint32_t)last_pos);
        last_pos = pos;

        int32_t rpm_milli = tiny_speed_rpm_milli(counts, PERIOD_MS, COUNTS_PER_TURN);

        // In a SPEED run the controller sets the target and tiny_motor still
        // applies the cap, slew, stall trip and timeout. Once the run's time is
        // up the controller stops, so the ramp down to 0 isn't fought.
        // Copy its state under the lock, step it outside, write it back only if
        // no command (SPEED, DUTY, STOP) came in meanwhile; if one did, this
        // period's result is dropped and the next period uses the new state.
        portENTER_CRITICAL(&s_lock);
        bool controlling = s_speed_run && s_motor.mode == TINY_MOTOR_RUN && s_motor.steps_left > 0;
        tiny_pi_t pi = s_pi;
        tiny_pi_cfg_t pi_cfg = s_pi_cfg;
        float setpoint_rpm = s_setpoint;
        uint32_t seq = s_cmd_seq;
        portEXIT_CRITICAL(&s_lock);

        int32_t target = 0;
        int32_t setpoint = 0;
        if (controlling) {
            target = tiny_pi_step(&pi, &pi_cfg, setpoint_rpm, rpm_milli / 1000.0f);
            setpoint = (int32_t)(setpoint_rpm * 1000.0f);
        }

        portENTER_CRITICAL(&s_lock);
        if (controlling && seq == s_cmd_seq) {
            s_pi = pi;
            tiny_motor_set_target(&s_motor, &s_cfg, target);
        }
        int32_t duty = tiny_motor_step(&s_motor, &s_cfg, counts);
        tiny_motor_mode_t mode = s_motor.mode;
        if (mode != TINY_MOTOR_RUN) {
            s_speed_run = false;
        }
        telem_mode_t tmode = mode == TINY_MOTOR_FAULT ? TELEM_MODE_STALL
                           : mode == TINY_MOTOR_IDLE  ? TELEM_MODE_IDLE
                           : s_speed_run              ? TELEM_MODE_SPEED
                                                      : TELEM_MODE_DUTY;
        tiny_motor_decay_t decay = s_decay;
        portEXIT_CRITICAL(&s_lock);

        apply(mode, duty, decay);

        // The row for the period that just ended pairs its counts with the
        // duty that produced them. Pushed after apply() so it never delays the
        // output; telemetry_push() doesn't block.
        telemetry_sample_t s = {
            .t_ms = t_ms,
            .mode = out_mode,
            .setpoint_milli = out_setpoint,
            .duty_permille = out_duty,
            .counts = counts,
            .rpm_milli = rpm_milli,
        };
        telemetry_push(&s);
        out_mode = tmode;
        out_setpoint = setpoint;
        out_duty = duty;
    }
}

void drive_start(void)
{
    // STBY low before the pin becomes an output, so it never drives high.
    // Until here the driver's own pull-downs hold all three pins low.
    gpio_set_level(MOTOR_STBY_GPIO, 0);
    const gpio_config_t stby = {
        .pin_bit_mask = 1ULL << MOTOR_STBY_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&stby));

    const ledc_timer_config_t timer = {
        .speed_mode = PWM_MODE,
        .duty_resolution = PWM_RES,
        .timer_num = PWM_TIMER,
        .freq_hz = PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));
    const int gpios[2] = {MOTOR_IN1_GPIO, MOTOR_IN2_GPIO};
    const ledc_channel_t channels[2] = {PWM_CH_IN1, PWM_CH_IN2};
    for (int i = 0; i < 2; i++) {
        const ledc_channel_config_t ch = {
            .gpio_num = gpios[i],
            .speed_mode = PWM_MODE,
            .channel = channels[i],
            .timer_sel = PWM_TIMER,
            .duty = 0,  // low: coast
        };
        ESP_ERROR_CHECK(ledc_channel_config(&ch));
    }

    tiny_motor_init(&s_motor);
    BaseType_t ok = xTaskCreate(drive_task, "drive", DRIVE_STACK, NULL, DRIVE_PRIORITY, NULL);
    ESP_ERROR_CHECK(ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}

bool drive_run(int32_t duty_pct, uint32_t seconds, int32_t *run_pct)
{
    portENTER_CRITICAL(&s_lock);
    bool ok = tiny_motor_run(&s_motor, &s_cfg, duty_pct * 10, seconds * (1000 / PERIOD_MS));
    if (ok) {
        s_speed_run = false;
        s_cmd_seq++;
    }
    *run_pct = s_motor.target / 10;
    portEXIT_CRITICAL(&s_lock);
    return ok;
}

bool drive_speed(int32_t rpm, uint32_t seconds)
{
    // The feedforward in floats, outside the lock, from a copy of the gains.
    portENTER_CRITICAL(&s_lock);
    tiny_pi_cfg_t pi_cfg = s_pi_cfg;
    portEXIT_CRITICAL(&s_lock);
    float setpoint = (float)rpm;
    int32_t start = (int32_t)tiny_pi_feedforward(&pi_cfg, setpoint);

    portENTER_CRITICAL(&s_lock);
    // A new speed run starts with a clear integral; a new setpoint during one
    // keeps it, so the duty doesn't jump.
    bool fresh = !(s_speed_run && s_motor.mode == TINY_MOTOR_RUN);
    bool ok = tiny_motor_run(&s_motor, &s_cfg, fresh ? start : s_motor.target,
                             seconds * (1000 / PERIOD_MS));
    if (ok) {
        if (fresh) {
            tiny_pi_reset(&s_pi);
        }
        s_speed_run = true;
        s_setpoint = setpoint;
        s_cmd_seq++;
    }
    portEXIT_CRITICAL(&s_lock);
    return ok;
}

void drive_stop(void)
{
    portENTER_CRITICAL(&s_lock);
    tiny_motor_stop(&s_motor);
    s_speed_run = false;
    s_cmd_seq++;
    portEXIT_CRITICAL(&s_lock);
}

void drive_set_gain(drive_gain_t gain, float value)
{
    portENTER_CRITICAL(&s_lock);
    switch (gain) {
    case DRIVE_GAIN_KP: s_pi_cfg.kp = value; break;
    case DRIVE_GAIN_KI: s_pi_cfg.ki = value; break;
    case DRIVE_GAIN_FF: s_pi_cfg.ff = value; break;
    }
    portEXIT_CRITICAL(&s_lock);
}

void drive_gains(float *kp, float *ki, float *ff)
{
    portENTER_CRITICAL(&s_lock);
    *kp = s_pi_cfg.kp;
    *ki = s_pi_cfg.ki;
    *ff = s_pi_cfg.ff;
    portEXIT_CRITICAL(&s_lock);
}

bool drive_set_decay(tiny_motor_decay_t decay)
{
    portENTER_CRITICAL(&s_lock);
    bool ok = s_motor.mode != TINY_MOTOR_RUN;
    if (ok) {
        s_decay = decay;
    }
    portEXIT_CRITICAL(&s_lock);
    return ok;
}

void drive_status(tiny_motor_mode_t *mode, int32_t *duty_pct, tiny_motor_decay_t *decay)
{
    portENTER_CRITICAL(&s_lock);
    *mode = s_motor.mode;
    *duty_pct = s_motor.duty / 10;
    *decay = s_decay;
    portEXIT_CRITICAL(&s_lock);
}
