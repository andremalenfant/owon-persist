#pragma once

#include <stdbool.h>

#define TAG "OWON-PERSIST"
#define STORAGE_NAMESPACE TAG

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
    bool rate_applicable;
};

typedef enum {
    CMD_UNKNOWN,
    CMD_RATE,
    CMD_FUNC,
    CMD_RANGE,
    CMD_AUTO
} CommandCode;

typedef enum {
    RATE_UNKNOWN,
    RATE_FAST,
    RATE_MED,
    RATE_SLOW
} RateCode;

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

//declared in xdm1041.c to share it with main
extern struct SettingDescriptor owon_functions[];
extern struct SettingDescriptor rates[];
extern struct RxTxValues vdc_ranges[];
extern struct RxTxValues vac_ranges[];
extern struct RxTxValues curr_ranges[];
extern struct RxTxValues res_ranges[];
extern struct RxTxValues cap_ranges[];

int get_int_setting(const char *key, int default_value);
void set_int_setting(const char *key, int value);

int get_function_count();
struct SettingDescriptor *get_functions();
CommandCode get_command_code(const char *str);
struct SettingDescriptor get_rate(char* rate_value);
int get_vdc_range(char* range_string);
char* get_tx_vdc_range(int current_range);
int get_vac_range(char* range_string);
char* get_tx_vac_range(int current_range);
int get_curr_range(char* range_string);
char* get_tx_curr_range(int current_range);
int get_res_range(char* range_string);
char* get_tx_res_range(int current_range);
int get_cap_range(char* range_string);
char* get_tx_cap_range(int current_range);
struct SettingDescriptor get_function(char* function_string);