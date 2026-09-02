#pragma once

#define TAG "OWON-PERSIST"

#define RATE  "RATE"
#define FUNC  "FUNC1"
#define RANGE "RANGE"
#define AUTO  "AUTO"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

struct RxTxValues {
    char* rx_value;
    char* tx_value;
};

struct SettingDescriptor {
    int code;
    struct RxTxValues rxtx_values;
    char *range_storage_key;
    int (*range_rx_getter)(char*);
    int range_value;
    char *auto_storage_key;
    int auto_value;
    char* (*range_tx_getter)(int);
};

typedef enum {
    CMD_UNKNOWN,
    CMD_RATE,
    CMD_FUNC,
    CMD_RANGE,
    CMD_AUTO
} CommandCode;

CommandCode get_command_code(const char *str) {
    if (strcmp(str, RATE) == 0) return CMD_RATE;
    if (strcmp(str, FUNC) == 0)  return CMD_FUNC;
    if (strcmp(str, RANGE) == 0) return CMD_RANGE;
    if (strcmp(str, AUTO) == 0) return CMD_AUTO;
    return CMD_UNKNOWN;
}

typedef enum {
    RATE_FAST,
    RATE_MED,
    RATE_SLOW
} RateCode;

struct SettingDescriptor rates[] = {
    {.code=RATE_FAST, .rxtx_values={.rx_value="F", .tx_value="F"}},
    {.code=RATE_SLOW, .rxtx_values={.rx_value="S", .tx_value="S"}},
    {.code=RATE_MED, .rxtx_values={.rx_value="M", .tx_value="M"}}
};

struct SettingDescriptor get_rate(char* rate_value) {
    for (int i = 0; i < ARRAY_SIZE(rates); i++) {
        if (strcmp(rate_value, rates[i].rxtx_values.rx_value) == 0) return rates[i];
    }
    return rates[0];
}

typedef enum {
    FUNC_UNKOWN,
    VOLT_DC,
    VOLT_AC,
    CURR_DC,
    CURR_AC,
    FREQ,
    PER,
    CAP,
    CONT,
    DIOD,
    FRES,
    RES,
    TEMP,
} FunctionCode;

struct RxTxValues vdc_ranges[] = {
    {.rx_value="UNKNOWN", .tx_value="0"},
    {.rx_value="50 mV", .tx_value="50E-3"},
    {.rx_value="500 mV", .tx_value="500E-3"},
    {.rx_value="5 V", .tx_value="5"},
    {.rx_value="50 V", .tx_value="50"},
    {.rx_value="500 V", .tx_value="500"},
    {.rx_value="1000 V", .tx_value="1000"}
};

struct RxTxValues vac_ranges[] = {
    {.rx_value="UNKNOWN", .tx_value="0"},
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

struct SettingDescriptor get_function(struct SettingDescriptor owon_functions[], int total_elements, char* function_string) {
    for (int i = 0; i < total_elements; i++) {
        if (strcmp(owon_functions[i].rxtx_values.rx_value, function_string) == 0) return owon_functions[i];
    }
    return owon_functions[FUNC_UNKOWN];
}