#pragma once
#include "nvs_flash.h"

int get_int(const char *key, int default_value);
void set_int(const char *key, int value);