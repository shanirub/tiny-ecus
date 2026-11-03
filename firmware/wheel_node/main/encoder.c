#include "encoder.h"

#include <stdatomic.h>

#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "pins.h"
#include "soc/gpio_reg.h"
#include "soc/soc.h"
#include "tiny_qdec.h"

// Shared between the ISR and tasks; only touched inside s_lock.
static tiny_qdec_t s_qdec;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static atomic_bool s_probe;

// Both levels from one register read, so A and B are sampled at the same
// instant. gpio_get_level() would be two reads and isn't guaranteed to be in
// IRAM.
//
// B goes in the decoder's first channel and A in the second. That reverses the
// count direction, so positive duty counts up and gives positive rpm, the same
// sign as the duty (Phase 1 E; before that, positive duty counted down). Data
// and logs from before the change have the opposite sign.
static inline IRAM_ATTR uint8_t read_state(void)
{
    uint32_t in = REG_READ(GPIO_IN_REG);
    return tiny_qdec_state((in >> ENC_B_GPIO) & 1u, (in >> ENC_A_GPIO) & 1u);
}

// Runs on every edge of A or B. How one edge reaches it, e.g. A rising:
//   1. The GPIO peripheral sets A's bit in the interrupt status register.
//   2. That raises the GPIO interrupt, which all pins share, to the CPU.
//   3. The CPU pauses the running task, saves its registers and enters
//      ESP-IDF's interrupt entry code, which calls the GPIO ISR service (the
//      dispatcher installed in encoder_start()).
//   4. The dispatcher reads the status register, and for each set bit clears
//      it and calls that pin's handler: here encoder_isr.
//   5. encoder_isr reads both pins and updates the decoder.
//   6. The dispatcher handles any other set bit (B's, if B also changed),
//      then returns, and the CPU restores and continues the task.
// The status bit is cleared before the handler runs, so an edge during it
// raises a new interrupt. Edges arriving faster than this whole path merge
// into one status bit and are lost: two merged steps count as invalid, three
// or four are silently miscounted (as -1 or 0). Phase 1 D.4 checks that the
// invalid count stays 0 at full speed.
//
// If both pins changed before it ran, it is called once per pin: the first
// call sees both changed (counted as invalid), the second sees no change and
// adds 0.
//
// The probe (PROBE ON) is high from here to the end of this handler only. The
// time from an encoder edge to the probe's rising edge shows steps 1-4; the
// time after it (the dispatcher's return and the task restore) isn't visible.
// Plain register writes: one store each, in IRAM like the rest.
static void IRAM_ATTR encoder_isr(void *arg)
{
    (void)arg;
    bool probe = atomic_load_explicit(&s_probe, memory_order_relaxed);
    if (probe) {
        REG_WRITE(GPIO_OUT_W1TS_REG, 1u << ISR_PROBE_GPIO);
    }
    portENTER_CRITICAL_ISR(&s_lock);
    tiny_qdec_update(&s_qdec, read_state());
    portEXIT_CRITICAL_ISR(&s_lock);
    if (probe) {
        REG_WRITE(GPIO_OUT_W1TC_REG, 1u << ISR_PROBE_GPIO);
    }
}

void encoder_start(void)
{
    const gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << ENC_A_GPIO) | (1ULL << ENC_B_GPIO),
        .mode = GPIO_MODE_INPUT,
        // The encoder drives both lines push-pull (Phase 0 F.4), so the pull-up
        // doesn't change its signal. It keeps an unplugged input from floating
        // and firing a stream of interrupts on noise.
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));

    tiny_qdec_init(&s_qdec, read_state());

    // IRAM flag: the ISR keeps running while the flash is busy. It and all it
    // calls (tiny_qdec, see its linker.lf) are therefore in RAM.
    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_IRAM));
    ESP_ERROR_CHECK(gpio_isr_handler_add(ENC_A_GPIO, encoder_isr, NULL));
    ESP_ERROR_CHECK(gpio_isr_handler_add(ENC_B_GPIO, encoder_isr, NULL));
}

void encoder_read(int32_t *position, uint32_t *invalid)
{
    portENTER_CRITICAL(&s_lock);
    *position = s_qdec.position;
    *invalid = s_qdec.invalid;
    portEXIT_CRITICAL(&s_lock);
}

void encoder_zero(void)
{
    portENTER_CRITICAL(&s_lock);
    tiny_qdec_zero(&s_qdec);
    portEXIT_CRITICAL(&s_lock);
}

// Called from the console task only. The pin is an output before the handler
// can drive it, and the handler has stopped driving it before it goes back to
// an input; a handler already running may still clear the pin once, harmless.
void encoder_set_probe(bool on)
{
    if (on == atomic_load(&s_probe)) {
        return;
    }
    if (on) {
        gpio_set_level(ISR_PROBE_GPIO, 0);
        const gpio_config_t cfg = {
            .pin_bit_mask = 1ULL << ISR_PROBE_GPIO,
            .mode = GPIO_MODE_OUTPUT,
        };
        ESP_ERROR_CHECK(gpio_config(&cfg));
        atomic_store(&s_probe, true);
    } else {
        atomic_store(&s_probe, false);
        // Off: neither input nor output and no pulls, like the pin after reset.
        // gpio_reset_pin() also releases the output reservation that
        // gpio_config() took (re-configuring a reserved pin logs a conflict),
        // but turns the pull-up on, so that goes off again.
        ESP_ERROR_CHECK(gpio_reset_pin(ISR_PROBE_GPIO));
        ESP_ERROR_CHECK(gpio_pullup_dis(ISR_PROBE_GPIO));
    }
}

bool encoder_probe(void)
{
    return atomic_load(&s_probe);
}
