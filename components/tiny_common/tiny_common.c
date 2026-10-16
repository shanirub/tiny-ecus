#include "tiny_common.h"

#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_idf_version.h"
#include "esp_log.h"

static const char *TAG = "tiny_common";

static const char *chip_name(esp_chip_model_t model)
{
    switch (model) {
    case CHIP_ESP32:   return "ESP32";
    case CHIP_ESP32C3: return "ESP32-C3";
    default:           return "unknown";
    }
}

void tiny_common_print_banner(const char *role)
{
    const esp_app_desc_t *app = esp_app_get_description();
    esp_chip_info_t chip;
    esp_chip_info(&chip);

    ESP_LOGI(TAG, "tiny-ecus %s", role);
    ESP_LOGI(TAG, "app %s %s (built %s %s)", app->project_name, app->version, app->date, app->time);
    ESP_LOGI(TAG, "chip %s rev v%d.%d, %d core(s)", chip_name(chip.model),
             chip.revision / 100, chip.revision % 100, chip.cores);
    ESP_LOGI(TAG, "ESP-IDF %s", esp_get_idf_version());
}
