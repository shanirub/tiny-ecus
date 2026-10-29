#include "encoder.h"

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

// Both levels from one register read, so A and B are sampled at the same
// instant. gpio_get_level() would be two reads and isn't guaranteed to be in
// IRAM.
static inline IRAM_ATTR uint8_t read_state(void)
{
    uint32_t in = REG_READ(GPIO_IN_REG);
    return tiny_qdec_state((in >> ENC_A_GPIO) & 1u, (in >> ENC_B_GPIO) & 1u);
}

// Runs on every edge of A or B. If both pins changed before it ran, the GPIO
// driver calls it once per pin: the first call sees both changed (counted as
// invalid), the second sees no change and adds 0.
static void IRAM_ATTR encoder_isr(void *arg)
{
    (void)arg;
    portENTER_CRITICAL_ISR(&s_lock);
    tiny_qdec_update(&s_qdec, read_state());
    portEXIT_CRITICAL_ISR(&s_lock);
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
