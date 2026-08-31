#include "storage.h"
#include "esp_log.h"

#define TAG "OWON-PERSIST"

esp_err_t get_stored_value(const char *namespace_name, const char *key, void * value, size_t max_buffer_size) {
    nvs_handle_t my_handle;
    esp_err_t err;

    err = nvs_open(namespace_name, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    size_t required_size = 0;  // value will default to 0, if not set yet in NVS
    err = nvs_get_blob(my_handle, key, NULL, &required_size);

    if (err != ESP_OK) {
        nvs_close(my_handle); // FIX: Close handle before exit
        return err;
    }    
    ESP_LOGI(TAG, "max_buffer_size = %d, required size = %d", max_buffer_size, required_size);
    if (required_size > max_buffer_size) {
        nvs_close(my_handle); // FIX: Close handle before exit
        return ESP_ERR_NVS_INVALID_LENGTH; 
    }

    if (required_size > 0) {
        err = nvs_get_blob(my_handle, key, value, &required_size);
        if (err != ESP_OK) {
            return err;
        }
    } else {
        return ESP_ERR_NVS_NOT_FOUND;
    }
    nvs_close(my_handle);
    return ESP_OK;
}

esp_err_t store_value(const char *namespace_name, const char *key, void *value, size_t value_size) {
    nvs_handle_t my_handle;
    esp_err_t err;

    err = nvs_open(namespace_name, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    size_t required_size = 0;  // value will default to 0, if not set yet in NVS
    /*err = nvs_get_blob(my_handle, key, NULL, &required_size);    
    
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;*/

    required_size += value_size;
    err = nvs_set_blob(my_handle, key, value, required_size);

    if (err != ESP_OK) return err;

    // Commit
    err = nvs_commit(my_handle);
    if (err != ESP_OK) return err;

    // Close
    nvs_close(my_handle);

    return ESP_OK;
}
