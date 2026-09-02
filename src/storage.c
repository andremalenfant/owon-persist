#include "storage.h"
#include "esp_log.h"

#define TAG "OWON-PERSIST"

int get_int(const char *key, int default_value) {
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(TAG, NVS_READWRITE, &handle);
    if (ret == ESP_OK) {
        int value;
        ret = nvs_get_i32(handle, key, (uint32_t*)&value);        
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Read value for %s = %d", key, value);
            return value;
        }
    }
    ESP_LOGI(TAG, "Error reading %s, ret = %s", key, esp_err_to_name(ret));
    nvs_close(handle);
    return default_value;
}

void set_int(const char *key, int value) {
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(TAG, NVS_READWRITE, &handle);
    if (ret == ESP_OK) {
        ret = nvs_set_i32(handle, key, (uint32_t)value);        
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "stored value for %s = %d", key, value);
            return;
        }
    }
    ESP_LOGI(TAG, "error saving %s, ret = %s", key, esp_err_to_name(ret));
    nvs_close(handle);
}
