#include "xdm1041.h"
#include <string.h>
#include "nvs_flash.h"
#include "esp_log.h"

struct SettingDescriptor owon_functions[] = {
    {.code=FUNC_UNKOWN, .rxtx_values={.rx_value="UNKNOWN", .tx_value="UNKNOWN"}},
    {.code=VOLT_DC, .rxtx_values={.rx_value="VOLT", .tx_value="VOLT:DC"}, .rate_applicable=true, .range_storage_key="RANGEVDC", .range_rx_getter=get_vdc_range, .range_tx_getter=get_tx_vdc_range, .auto_storage_key="AUTOVDC"},
    {.code=VOLT_AC, .rxtx_values={.rx_value="VOLT AC", .tx_value="VOLT:AC"}, .rate_applicable=true, .range_storage_key="RANGEVAC", .range_rx_getter=get_vac_range, .range_tx_getter=get_tx_vac_range, .auto_storage_key="AUTOVAC"},
    {.code=CURR_DC, .rxtx_values={.rx_value="CURR", .tx_value="CURR:DC"}, .rate_applicable=true, .range_storage_key="RANGEADC", .range_rx_getter=get_curr_range, .range_tx_getter=get_tx_curr_range, .auto_storage_key="AUTOADC"},
    {.code=CURR_AC, .rxtx_values={.rx_value="CURR AC", .tx_value="CURR:AC"}, .rate_applicable=true, .range_storage_key="RANGEAAC", .range_rx_getter=get_curr_range, .range_tx_getter=get_tx_curr_range, .auto_storage_key="AUTOAAC"},
    {.code=FREQ, .rxtx_values={.rx_value="FREQ", .tx_value="FREQ"}},
    {.code=PER, .rxtx_values={.rx_value="PER", .tx_value="PER"}},
    {.code=CAP, .rxtx_values={.rx_value="CAP", .tx_value="CAP"}, .range_storage_key="RANGECAP", .range_rx_getter=get_cap_range, .range_tx_getter=get_tx_cap_range, .auto_storage_key="AUTOCAP"},
    {.code=CONT, .rxtx_values={.rx_value="CONT", .tx_value="CONT"}},
    {.code=DIOD, .rxtx_values={.rx_value="DIOD", .tx_value="DIOD"}},
    {.code=FRES, .rxtx_values={.rx_value="FRES", .tx_value="FRES"}},
    {.code=RES, .rxtx_values={.rx_value="RES", .tx_value="RES"}, .rate_applicable=true, .range_storage_key="RANGERES", .range_rx_getter=&get_res_range, .range_tx_getter=get_tx_res_range, .auto_storage_key="AUTORES"},
    {.code=TEMP, .rxtx_values={.rx_value="TEMP", .tx_value="TEMP"}, .range_storage_key="RANGETEMP", .range_rx_getter=get_temp_range, .range_tx_getter=get_tx_temp_range}
};

struct SettingDescriptor rates[] = {
    {.code=RATE_UNKNOWN, .rxtx_values={.rx_value="", .tx_value=""}},
    {.code=RATE_FAST, .rxtx_values={.rx_value="F", .tx_value="F"}},
    {.code=RATE_SLOW, .rxtx_values={.rx_value="S", .tx_value="S"}},
    {.code=RATE_MED, .rxtx_values={.rx_value="M", .tx_value="M"}}
};

// Adding an unknow value because the multi-meter starts ranges at 1
struct RxTxValues vdc_ranges[] = {
    {.rx_value="UNKNOWN", .tx_value=""},
    {.rx_value="50 mV", .tx_value="50E-3"},
    {.rx_value="500 mV", .tx_value="500E-3"},
    {.rx_value="5 V", .tx_value="5"},
    {.rx_value="50 V", .tx_value="50"},
    {.rx_value="500 V", .tx_value="500"},
    {.rx_value="1000 V", .tx_value="1000"}
};

struct RxTxValues vac_ranges[] = {
    {.rx_value="UNKNOWN", .tx_value=""},
    {.rx_value="500 mV", .tx_value="500E-3"},
    {.rx_value="5 V", .tx_value="5"},
    {.rx_value="50 V", .tx_value="50"},
    {.rx_value="500 V", .tx_value="500"},    
    {.rx_value="750 V", .tx_value="750"}
};

struct RxTxValues curr_ranges[] = {
    {.rx_value="UNKNOWN", .tx_value="0"},
    {.rx_value="500 uA", .tx_value="500E-6"},
    {.rx_value="5 mA", .tx_value="5E-3"},
    {.rx_value="50 mA", .tx_value="50E-3"},
    {.rx_value="500 mA", .tx_value="500E-3"},
    {.rx_value="5 A", .tx_value="5"},
    {.rx_value="10 A", .tx_value="10"}
};

struct RxTxValues res_ranges[] = {
    {.rx_value="UNKNOWN", .tx_value="0"},
    {.rx_value="500 Ω", .tx_value="500"},
    {.rx_value="5 KΩ", .tx_value="5E3"},
    {.rx_value="50 KΩ", .tx_value="50E3"},
    {.rx_value="500 KΩ", .tx_value="500E3"},
    {.rx_value="5 MΩ", .tx_value="5E6"},
    {.rx_value="50 MΩ", .tx_value="50E6"} 
};

struct RxTxValues cap_ranges[] = {
    {.rx_value="UNKNOWN", .tx_value="0"},
    {.rx_value="50 nF", .tx_value="50E-9"},
    {.rx_value="500 nF", .tx_value="500E-9"},
    {.rx_value="5uF", .tx_value="5E-6"},
    {.rx_value="50uF", .tx_value="50E-6"},
    {.rx_value="500uF", .tx_value="500E-6"},
    {.rx_value="5 mF", .tx_value="5E-3"},     
    {.rx_value="50 mF", .tx_value="50E-3"} 
};

struct RxTxValues temp_ranges[] = {
    {.rx_value="UNKNOWN", .tx_value="0"},
    {.rx_value="Pt100", .tx_value="PT100"},
    {.rx_value="KITS90", .tx_value="KITS90"}
};

int get_int_setting(const char *key, int default_value) {
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle);
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

void set_int_setting(const char *key, int value) {
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle);
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

CommandCode get_command_code(const char *str) {
    if (strcmp(str, RATE) == 0) return CMD_RATE;
    if (strcmp(str, FUNC) == 0)  return CMD_FUNC;
    if (strcmp(str, RANGE) == 0) return CMD_RANGE;
    if (strcmp(str, AUTO) == 0) return CMD_AUTO;
    return CMD_UNKNOWN;
}

struct SettingDescriptor get_rate(char* rate_value) {
    for (int i = 0; i < ARRAY_SIZE(rates); i++) {
        if (strcmp(rate_value, rates[i].rxtx_values.rx_value) == 0) return rates[i];
    }
    return rates[0];
}

int get_range(char* range_string, struct RxTxValues ranges[], int item_count) {
    for (int i = 0; i < item_count; i++) {
        if (strcmp(ranges[i].rx_value, range_string) == 0) return i;
    }
    return 0;
}

int get_vdc_range(char* range_string) {
    return get_range(range_string, vdc_ranges, ARRAY_SIZE(vdc_ranges));
}

char* get_tx_vdc_range(int current_range) {
    return vdc_ranges[current_range].tx_value;
}

int get_vac_range(char* range_string) {
    return get_range(range_string, vac_ranges, ARRAY_SIZE(vac_ranges));
}

char* get_tx_vac_range(int current_range) {
    return vac_ranges[current_range].tx_value;
}

int get_curr_range(char* range_string) {
    return get_range(range_string, curr_ranges, ARRAY_SIZE(curr_ranges));
}

char* get_tx_curr_range(int current_range) {
    return curr_ranges[current_range].tx_value;
}

int get_res_range(char* range_string) {
    return get_range(range_string, res_ranges, ARRAY_SIZE(res_ranges));
}

char* get_tx_res_range(int current_range) {
    return res_ranges[current_range].tx_value;
}

int get_cap_range(char* range_string) {
    return get_range(range_string, cap_ranges, ARRAY_SIZE(cap_ranges));
}

char* get_tx_cap_range(int current_range) {
    return cap_ranges[current_range].tx_value;
}

int get_temp_range(char* range_string) {
    return get_range(range_string, temp_ranges, ARRAY_SIZE(temp_ranges));
}

char* get_tx_temp_range(int current_range) {
    return temp_ranges[current_range].tx_value;
}


int get_function_count() {
    return ARRAY_SIZE(owon_functions);
}

struct SettingDescriptor *get_functions() {
    return owon_functions;
}

struct SettingDescriptor get_function(char* function_string) {
    for (int i = 0; i < get_function_count(); i++) {
        if (strcmp(owon_functions[i].rxtx_values.rx_value, function_string) == 0) return owon_functions[i];
    }
    return owon_functions[FUNC_UNKOWN];
}

