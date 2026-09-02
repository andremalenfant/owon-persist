#include "nvs_flash.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "storage.h"
#include "xdm1041.h"

#define OWON_TX GPIO_NUM_0
#define OWON_RX GPIO_NUM_1
#define TAG "OWON-PERSIST"
#define STORAGE_NAMESPACE TAG

char PATTERN_SOFT_START[] = {0x00,0x01,0x00};

char write_buffer[100];
char read_buffer[100];

FunctionCode current_function;
RateCode current_rate;

struct SettingDescriptor owon_functions[] = {
    {.code=FUNC_UNKOWN, .rxtx_values={.rx_value="UNKNOWN", .tx_value="UNKNOWN"}},
    {.code=VOLT_DC, .rxtx_values={.rx_value="VOLT", .tx_value="VOLT:DC"}, .range_storage_key="RANGEVDC", .range_rx_getter=get_vdc_range, .range_tx_getter=get_tx_vdc_range, .auto_storage_key="AUTOVDC"},
    {.code=VOLT_AC, .rxtx_values={.rx_value="VOLT AC", .tx_value="VOLT:AC"}, .range_storage_key="RANGEVAC", .range_rx_getter=get_vac_range, .range_tx_getter=get_tx_vac_range, .auto_storage_key="AUTOVAC"},
    {.code=CURR_DC, .rxtx_values={.rx_value="CURR", .tx_value="CURR:DC"}, .range_storage_key="RANGEADC", .range_rx_getter=get_curr_range, .range_tx_getter=get_tx_curr_range, .auto_storage_key="AUTOADC"},
    {.code=CURR_AC, .rxtx_values={.rx_value="CURR AC", .tx_value="CURR:AC"}, .range_storage_key="RANGEAAC", .range_rx_getter=get_curr_range, .range_tx_getter=get_tx_curr_range, .auto_storage_key="AUTOAAC"},
    {.code=FREQ, .rxtx_values={.rx_value="FREQ", .tx_value="FREQ"}},
    {.code=PER, .rxtx_values={.rx_value="PER", .tx_value="PER"}},
    {.code=CAP, .rxtx_values={.rx_value="CAP", .tx_value="CAP"}, .range_storage_key="RANGECAP", .range_rx_getter=get_cap_range, .range_tx_getter=get_tx_cap_range, .auto_storage_key="AUTOCAP"},
    {.code=CONT, .rxtx_values={.rx_value="CONT", .tx_value="CONT"}},
    {.code=DIOD, .rxtx_values={.rx_value="DIOD", .tx_value="DIOD"}},
    {.code=FRES, .rxtx_values={.rx_value="FRES", .tx_value="FRES"}},
    {.code=RES, .rxtx_values={.rx_value="RES", .tx_value="RES"}, .range_storage_key="RANGERES", .range_rx_getter=&get_res_range, .range_tx_getter=get_tx_res_range, .auto_storage_key="AUTORES"},
    {.code=TEMP, .rxtx_values={.rx_value="TEMP", .tx_value="TEMP"}}
};

bool read_from_owon() {    
    int i = 0;
    int retry_count = 0;
    memset(read_buffer, 0, sizeof(read_buffer));
    while (i < sizeof(read_buffer) - 1) {
        uint8_t c;
        int rx_bytes = uart_read_bytes(UART_NUM_1, &c, 1, pdMS_TO_TICKS(200));
        //ESP_LOGI(TAG, "rx_bytes:%d, byte:%X", rx_bytes, c);
        if (rx_bytes <= 0) {
            ESP_LOGI(TAG, "rx_bytes:%d", rx_bytes);
            retry_count++;
            if (retry_count >= 5) {
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
            size_t input_len = strlen(read_buffer);
            return strlen(read_buffer) > 0;
        }        
        read_buffer[i++] = (char)c;
        if (i >= 3) {
            char three[3];
            memcpy(three, &read_buffer[i-3],3);
            if (memcmp(&read_buffer[i-3], &PATTERN_SOFT_START, 3) == 0) {
                ESP_LOGI(TAG, "Soft Start Detected");
                esp_restart();
            }
        }                
    }
    return false;
}

void decode_and_store_value(char *command) {
    switch (get_command_code(command)) {
        case CMD_FUNC:
            struct SettingDescriptor function = get_function(owon_functions, ARRAY_SIZE(owon_functions), read_buffer);
            if (current_function != function.code) {
                current_function = function.code;
                set_int(command, (int)current_function);
            }
            break;
        case CMD_RATE:
            struct SettingDescriptor rate = get_rate(read_buffer);
            if (current_rate != rate.code) {
                current_rate = rate.code;
                set_int(command, (int)current_rate);
            }
            break;
        case CMD_AUTO:
            struct SettingDescriptor *current_range_function = &owon_functions[current_function];
            int new_value = atoi(read_buffer);
            if (current_range_function->auto_storage_key != NULL) {
                if (current_range_function->auto_value != new_value) {
                    current_range_function->auto_value = new_value;
                    set_int(current_range_function->range_storage_key, current_range_function->auto_value);
                }
            }
            break;
        case CMD_RANGE:
            current_range_function = &owon_functions[current_function];
            if (current_range_function->range_storage_key != NULL) {
                int new_value = current_range_function->range_rx_getter(read_buffer);
                if (current_range_function->range_value != new_value) {
                    current_range_function->range_value = new_value;
                    set_int(current_range_function->range_storage_key, current_range_function->range_value);
                }
            }
            break;
    }
}

void query_owon(char *command) {
    memset(write_buffer, 0, sizeof(write_buffer));
    sprintf(write_buffer, "%s?\n", command);
    ESP_LOGI(TAG, "Requesting %s", command);
    uart_write_bytes(UART_NUM_1, write_buffer, strlen(write_buffer));
    if (read_from_owon()) {
        decode_and_store_value(command);
        ESP_LOGI(TAG, "%s = %s", command, read_buffer);
    }   
    vTaskDelay(pdMS_TO_TICKS(1000));
}

void write_to_owon(char *command) {
    ESP_LOGI(TAG, "Writing %s", command);
    uart_write_bytes(UART_NUM_1, write_buffer, sizeof(write_buffer)); 
    vTaskDelay(pdMS_TO_TICKS(200));
}


void read_stored_values() {
    current_function = get_int(FUNC, (int)FUNC_UNKOWN);
    current_rate = get_int(RATE, (int)RATE_FAST);
    for (int i = 0; i < ARRAY_SIZE(owon_functions); i++) {
        if (owon_functions[i].auto_storage_key != NULL) {
            owon_functions[i].auto_value = get_int(owon_functions[i].auto_storage_key, 0);        
        }
        if (owon_functions[i].range_storage_key != NULL) {
            owon_functions[i].range_value = get_int(owon_functions[i].range_storage_key, 0);        
        }
    }
}

void write_stored_settings() {
    memset(write_buffer, 0, sizeof(write_buffer));
    if (owon_functions[current_function].auto_value) {
        sprintf(write_buffer, "CONF:%s\n", owon_functions[current_function].rxtx_values.tx_value);
    } else {
        char* range = "";
        if (owon_functions[current_function].range_storage_key != NULL) {
            range = owon_functions[current_function].range_tx_getter(owon_functions[current_function].range_value);
        }
        sprintf(write_buffer, "CONF:%s %s\n", owon_functions[current_function].rxtx_values.tx_value, range);
    }
    write_to_owon(write_buffer);
    sprintf(write_buffer, "%s %s\n", RATE, rates[current_rate].rxtx_values.tx_value);
    write_to_owon(write_buffer);\
}

void poll_task(void *pvParameters) {
    while(true) {
        query_owon(FUNC);
        query_owon(RATE);
        query_owon(AUTO);
        query_owon(RANGE);
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
    read_stored_values();
    init_serial();
    vTaskDelay(pdMS_TO_TICKS(200));
    write_stored_settings();
    xTaskCreate(poll_task, "poll_task", 2096, NULL, tskIDLE_PRIORITY + 1, NULL);
}