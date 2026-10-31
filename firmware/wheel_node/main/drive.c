#include "drive.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "encoder.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pins.h"

#define PERIOD_MS        10   // 100 Hz control period
#define DRIVE_PRIORITY   6    // highest of our tasks
#define DRIVE_STACK      3072

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

// Shared between the drive task and the console; only touched inside s_lock.
static tiny_motor_t s_motor;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

static void set_pins(uint32_t in1, uint32_t in2)
{
    ESP_ERROR_CHECK(ledc_set_duty(PWM_MODE, PWM_CH_IN1, in1));
    ESP_ERROR_CHECK(ledc_update_duty(PWM_MODE, PWM_CH_IN1));
    ESP_ERROR_CHECK(ledc_set_duty(PWM_MODE, PWM_CH_IN2, in2));
    ESP_ERROR_CHECK(ledc_update_duty(PWM_MODE, PWM_CH_IN2));
}

// Only this task writes the driver pins. Running: driver awake, slow-decay PWM.
// Otherwise: both inputs low (coast) and the driver asleep.
// STBY is the outer switch: inputs before waking, sleep before the inputs.
// A new LEDC duty starts at the next PWM period (up to 50 µs) and the two
// channels may switch in different periods, so the inputs pass through
// mixed states; with the driver asleep those never reach the motor.
static void apply(tiny_motor_mode_t mode, int32_t duty)
{
    if (mode == TINY_MOTOR_RUN) {
        uint32_t in1, in2;
        tiny_motor_pins(duty, PWM_FULL, &in1, &in2);
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

    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(PERIOD_MS));

        int32_t pos;
        encoder_read(&pos, &invalid);
        // Wrapping difference, like the decoder's position. After ENC ZERO it
        // jumps once; that only reads as "moving" for one period.
        int32_t counts = (int32_t)((uint32_t)pos - (uint32_t)last_pos);
        last_pos = pos;

        portENTER_CRITICAL(&s_lock);
        int32_t duty = tiny_motor_step(&s_motor, &s_cfg, counts);
        tiny_motor_mode_t mode = s_motor.mode;
        portEXIT_CRITICAL(&s_lock);

        apply(mode, duty);
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
    *run_pct = s_motor.target / 10;
    portEXIT_CRITICAL(&s_lock);
    return ok;
}

void drive_stop(void)
{
    portENTER_CRITICAL(&s_lock);
    tiny_motor_stop(&s_motor);
    portEXIT_CRITICAL(&s_lock);
}

void drive_status(tiny_motor_mode_t *mode, int32_t *duty_pct)
{
    portENTER_CRITICAL(&s_lock);
    *mode = s_motor.mode;
    *duty_pct = s_motor.duty / 10;
    portEXIT_CRITICAL(&s_lock);
}
