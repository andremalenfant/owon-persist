#pragma once
#include "nvs_flash.h"

#define STORAGE_NAMESPACE TAG

esp_err_t get_stored_value(const char *namespace_name, const char *key, void * value, size_t max_buffer_size);
esp_err_t store_value(const char *namespace_name, const char *key, void *value, size_t value_size);