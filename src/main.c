#include "nvs_flash.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "xdm1041.h"
#include <errno.h>

#define OWON_TX GPIO_NUM_0
#define OWON_RX GPIO_NUM_1

char PATTERN_SOFT_START[] = {0x00,0x01,0x00};

char write_buffer[100];
char read_buffer[100];

FunctionCode current_function;
RateCode current_rate;

bool read_from_owon() {    
    int i = 0;
    int retry_count = 0;
    memset(read_buffer, 0, sizeof(read_buffer));
    while (i < sizeof(read_buffer) - 1) {
        uint8_t c;
        int rx_bytes = uart_read_bytes(UART_NUM_1, &c, 1, pdMS_TO_TICKS(200));
        if (rx_bytes <= 0) {
            retry_count++;
            if (retry_count >= 10) {
                ESP_LOGI(TAG, "no response in %d tries", retry_count);
                return false;
            } else {
                continue;
            }
        }
        if (i == 0 && (c == 66 || c == 64)) {
            ESP_LOGI(TAG, "error response detected");
            return false;
        }
        if (c == '\"' || c == '\r') {
            //ESP_LOGI(TAG, "skip double quotes and carriage return");
            continue;
        }
        if (c == '\n') {
            read_buffer[i] = '\0'; // Null-terminate read_buffer safely and skip line feed
            return strlen(read_buffer) > 0;
        }        
        read_buffer[i++] = (char)c;
        if (i >= 3) {
            if (memcmp(&read_buffer[i-3], &PATTERN_SOFT_START, 3) == 0) {
                ESP_LOGI(TAG, "Soft Start Detected");
                esp_restart();
            }
        }                
    }
    return false;
}

bool decode_and_store_value(char *command) {
    switch (get_command_code(command)) {
        case CMD_FUNC:
            struct SettingDescriptor function = get_function(read_buffer);
            if (function.code != FUNC_UNKOWN && current_function != function.code) {
                current_function = function.code;
                set_int_setting(command, (int)current_function);
            }
            return function.code != FUNC_UNKOWN;
        case CMD_RATE:
            struct SettingDescriptor rate = get_rate(read_buffer);
            if (rate.code != RATE_UNKNOWN && current_rate != rate.code) {
                current_rate = rate.code;
                set_int_setting(command, (int)current_rate);
            }
            return rate.code != RATE_UNKNOWN;
        case CMD_AUTO:
            struct SettingDescriptor *current_range_function = &get_functions()[current_function];
            if (current_range_function->auto_storage_key != NULL) {
                char *endptr;
                long new_value = strtol(read_buffer, &endptr, 10);
                if (*endptr == '\0' && current_range_function->auto_value != new_value) {
                    current_range_function->auto_value = new_value;
                    set_int_setting(current_range_function->auto_storage_key, current_range_function->auto_value);
                }
                return *endptr == '\0';
            }
        case CMD_RANGE:
            current_range_function = &get_functions()[current_function];
            if (current_range_function->range_storage_key != NULL) {
                int new_value = current_range_function->range_rx_getter(read_buffer);
                if (new_value != 0 && current_range_function->range_value != new_value) {
                    current_range_function->range_value = new_value;
                    set_int_setting(current_range_function->range_storage_key, current_range_function->range_value);
                }
                return new_value != 0;
            }
    }
    return false;
}

void query_owon(char *command) {
    memset(write_buffer, 0, sizeof(write_buffer));
    sprintf(write_buffer, "%s?\n", command);
    ESP_LOGI(TAG, "Requesting %s", command);
    uart_write_bytes(UART_NUM_1, write_buffer, strlen(write_buffer));
    if (read_from_owon()) {
        if (decode_and_store_value(command)) {
            ESP_LOGI(TAG, "%s = %s", command, read_buffer);
        } else {
            ESP_LOGI(TAG, "ignoring %s = %s", command, read_buffer);
        }
    }   
    vTaskDelay(pdMS_TO_TICKS(1000));
}

void write_to_owon(char *command) {
    ESP_LOGI(TAG, "Writing %s", command);
    uart_write_bytes(UART_NUM_1, write_buffer, sizeof(write_buffer)); 
    vTaskDelay(pdMS_TO_TICKS(200));
}


void read_stored_settings() {
    current_function = get_int_setting(FUNC, (int)FUNC_UNKOWN);
    current_rate = get_int_setting(RATE, (int)RATE_FAST);
    for (int i = 0; i < get_function_count(); i++) {
        if (get_functions()[i].auto_storage_key != NULL) {
            get_functions()[i].auto_value = get_int_setting(get_functions()[i].auto_storage_key, 0);        
        }
        if (get_functions()[i].range_storage_key != NULL) {
            get_functions()[i].range_value = get_int_setting(get_functions()[i].range_storage_key, 0);        
        }
    }
}

void write_stored_settings() {
    memset(write_buffer, 0, sizeof(write_buffer));
    struct SettingDescriptor *current_function_desc = &get_functions()[current_function];
    if (current_function_desc->range_storage_key != NULL && current_function_desc->auto_value == 0) {
        char *range = current_function_desc->range_tx_getter(current_function_desc->range_value);
        sprintf(write_buffer, "CONF:%s %s\n", current_function_desc->rxtx_values.tx_value, range);       
    } else {
        sprintf(write_buffer, "CONF:%s\n", current_function_desc->rxtx_values.tx_value);        
    }
    write_to_owon(write_buffer);
    sprintf(write_buffer, "%s %s\n", RATE, rates[current_rate].rxtx_values.tx_value);
    write_to_owon(write_buffer);
}

void poll_task(void *pvParameters) {
    while(true) {
        query_owon(FUNC);
        struct SettingDescriptor *current_function_desc = &get_functions()[current_function];
        if (current_function_desc->rate_applicable) {
            query_owon(RATE);
        }
        if (current_function_desc->auto_storage_key != NULL) {
            query_owon(AUTO);
        }
        if (current_function_desc->range_storage_key != NULL) {
            query_owon(RANGE);
        }    
    }
}

void init_serial() {
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, OWON_TX, OWON_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, 1024, 1024, 0, NULL, 0));
}

void app_main() {
    nvs_flash_init();    
    vTaskDelay(pdMS_TO_TICKS(5000));
    read_stored_settings();
    init_serial();
    vTaskDelay(pdMS_TO_TICKS(200));
    write_stored_settings();
    xTaskCreate(poll_task, "poll_task", 2096, NULL, tskIDLE_PRIORITY + 1, NULL);
}