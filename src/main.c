#include "nvs_flash.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "storage.h"

#define KEYPAD_TX GPIO_NUM_0
#define KEYPAD_RX GPIO_NUM_1
#define TAG "OWON-PERSIST"
#define STORAGE_NAMESPACE TAG

#define QUERY_RATE  "RATE"
#define QUERY_FUNC  "FUNC1"
#define QUERY_RANGE "RANGE"
#define QUERY_AUTO  "AUTO"

char PATTERN_SOFT_START[] = {0x00,0x01,0x00};

TaskHandle_t write_task_handle = NULL;

char write_buffer[100];
char read_buffer[100];
char response_buffer[100];

char current_function[100] = "";
char current_auto = '0';
char current_v_range[100] = "";
char current_a_range[100] = "";
char current_rate = 'F';

void init_serial() {  
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));

    // Assign TX = GPIO 4, RX = GPIO 5 (Change as needed)
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, KEYPAD_TX, KEYPAD_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // Install driver with RX/TX buffers of 1024 bytes and no queue
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, 1024, 1024, 0, NULL, 0));
}

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
        if (c == '\"') {
            //ESP_LOGI(TAG, "skip double quotes");
            continue;
        }
        read_buffer[i++] = (char)c;
        if (i >= 3) {
            char three[3];
            memcpy(three, &read_buffer[i-3],3);
            //ESP_LOGI(TAG, "three=%X %X %X", three[0], three[1], three[2]);
            if (memcmp(&read_buffer[i-3], &PATTERN_SOFT_START, 3) == 0) {
                ESP_LOGI(TAG, "Soft Start Detected");
                esp_restart();
            }
        }                
        if (c == '\n') {
            read_buffer[i > 0 ? i-1 : i] = '\0'; //null-terminate without the \n
            memset(response_buffer, 0, sizeof(response_buffer));
            strncpy(response_buffer, read_buffer, strlen(read_buffer) - 1);
            response_buffer[sizeof(response_buffer) - 1] = '\0';
            return true;
        }
    }
    return false;
}

void sanitize_and_store_value(char *command) {
    bool save = false;
    if (strcmp(command, QUERY_FUNC) == 0) {
        //ESP_LOGI(TAG, "RESP=%s|", response_buffer);
        if (strcmp(response_buffer, "VOLT") == 0 || strcmp(response_buffer, "CURR") == 0) {
            char *pos = strstr(response_buffer, " ");
            if (pos != NULL) {
                *pos = ':';
            } else {
                strcat(response_buffer, ":DC");
            }
        }
        if (strcmp(current_function, response_buffer) != 0) {
            strcpy(current_function, response_buffer);
            ESP_LOGI(TAG,"Storing %s, value: %s, length:%d", command, current_function, strlen(response_buffer) + 1);
            store_value(STORAGE_NAMESPACE, command, current_function, strlen(current_function) + 1);
        }        
    } else if (strcmp(command, QUERY_RANGE) == 0) {
        if (strcmp(current_function, "VOLT:DC") == 0 || strcmp(current_function, "VOLT:AC") == 0) {
            if (strcmp(current_v_range, response_buffer) != 0) {
                strcpy(current_v_range, response_buffer);
                ESP_LOGI(TAG,"Storing %s, value: %s, length:%d current_function:%s", QUERY_RANGE"V", current_v_range, strlen(current_v_range) + 1, current_function);
                store_value(STORAGE_NAMESPACE, QUERY_RANGE"V", current_v_range, strlen(current_v_range) + 1);
            }
        } else if (strcmp(current_function, "CURR:DC") == 0 || strcmp(current_function, "CURR:AC") == 0) {
            if (strcmp(current_a_range, response_buffer) != 0) {
                strcpy(current_a_range, response_buffer);
                ESP_LOGI(TAG,"Storing %s, value: %s, length:%d", QUERY_RANGE"A", current_a_range, strlen(current_a_range) + 1);
                store_value(STORAGE_NAMESPACE, QUERY_RANGE"A", current_a_range, strlen(current_a_range) + 1);            
            }
        }
    } else if (strcmp(command, QUERY_AUTO) == 0) {
        if (current_auto != response_buffer[0]) {
            current_auto = response_buffer[0];
            ESP_LOGI(TAG,"Storing %s, value: %c, length:%d", QUERY_AUTO, current_auto, sizeof(current_auto));
            store_value(STORAGE_NAMESPACE, QUERY_AUTO, &current_auto, sizeof(current_auto));            
        }
        
    } else if (strcmp(command, QUERY_RATE) == 0) {
        if (response_buffer[0] != current_rate) {
            current_rate = response_buffer[0];
            ESP_LOGI(TAG,"Storing %s, value: %c, length:%d", QUERY_RATE, current_rate, sizeof(current_rate));
            store_value(STORAGE_NAMESPACE, QUERY_RATE, &current_rate, sizeof(current_rate));
        }
    }
}

char query_owon(char *command) {
    memset(write_buffer, 0, sizeof(write_buffer));
    sprintf(write_buffer, "%s?\n", command);
    ESP_LOGI(TAG, "Requesting %s", command);
    uart_write_bytes(UART_NUM_1, &write_buffer, sizeof(write_buffer));
    if (read_from_owon(false)) {
        sanitize_and_store_value(command);
        ESP_LOGI(TAG, "%s = %s", command, response_buffer);
    }   
    vTaskDelay(pdMS_TO_TICKS(1000));
}

void write_to_owon(char *command) {
    ESP_LOGI(TAG, "Writing %s", command);
    uart_write_bytes(UART_NUM_1, &write_buffer, sizeof(write_buffer)); 
    vTaskDelay(pdMS_TO_TICKS(200));
    //read_from_owon(true);
}

void write_saved_settings() {
    memset(write_buffer, 0, sizeof(write_buffer));
    sprintf(write_buffer, "%s %c\n", QUERY_RATE, current_rate);
    write_to_owon(write_buffer);
    ESP_LOGI(TAG, "current auto in write=%c", current_auto);
    sprintf(write_buffer, "%s %c\n", QUERY_AUTO, current_auto);
    write_to_owon(write_buffer);    
    if (current_auto == '1') {
        sprintf(write_buffer, "CONF:%s\n", current_function);
    } else {
        if (strcmp(current_function, "VOLT:DC") == 0 || strcmp(current_function, "VOLT:AC") == 0) {
            sprintf(write_buffer, "CONF:%s %s\n", current_function, current_v_range);
        } else if (strcmp(current_function, "CURR:DC") == 0 || strcmp(current_function, "CURR:AC") == 0) {
            sprintf(write_buffer, "CONF:%s %s\n", current_function, current_a_range);
        }
    }
    write_to_owon(write_buffer);
}

void read_stored_values() {
    esp_err_t err = get_stored_value(STORAGE_NAMESPACE, QUERY_FUNC, current_function, 99);
    if (err == ESP_OK) {
        current_function[sizeof(current_function) - 1] = '\0'; 
        ESP_LOGI(TAG, "Current function = %s", current_function);
    } else {
        ESP_LOGI(TAG,"Retreiving " QUERY_FUNC " NVS return: %s\n", esp_err_to_name(err));        
    }

    err = get_stored_value(STORAGE_NAMESPACE, QUERY_AUTO, &current_auto, sizeof(current_auto));
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Current auto = %c", current_auto);
    } else {
        ESP_LOGI(TAG,"Retreiving " QUERY_AUTO " NVS return: %s\n", esp_err_to_name(err));        
    }

    err = get_stored_value(STORAGE_NAMESPACE, QUERY_RATE, &current_rate, sizeof(current_auto));
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Current rate = %c", current_rate);
    } else {
        ESP_LOGI(TAG,"Retreiving " QUERY_RATE " NVS return: %s\n", esp_err_to_name(err));        
    }     

    err = get_stored_value(STORAGE_NAMESPACE, QUERY_RANGE"V", current_v_range, 99);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Current v range = %s", current_v_range);
    } else {
        ESP_LOGI(TAG,"Retreiving " QUERY_RANGE"V" " NVS return: %s\n", esp_err_to_name(err));        
    }    

    err = get_stored_value(STORAGE_NAMESPACE, QUERY_RANGE"A", current_a_range, 99);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Current a range = %s", current_a_range);
    } else {
        ESP_LOGI(TAG,"Retreiving " QUERY_RANGE"A" " NVS return: %s\n", esp_err_to_name(err));        
    }           
}

void write_task( void * pvParameters ) {
    while(true) {
        query_owon(QUERY_RATE);
        query_owon(QUERY_AUTO);
        query_owon(QUERY_FUNC);
        query_owon(QUERY_RANGE);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main() {
    nvs_flash_init();    
    vTaskDelay(pdMS_TO_TICKS(5000));
    read_stored_values();
    init_serial();
    vTaskDelay(pdMS_TO_TICKS(2000));
    write_saved_settings();
    xTaskCreate(write_task, "write_task", 2048, NULL, tskIDLE_PRIORITY + 1, &write_task_handle);
}