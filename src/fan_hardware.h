#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void fan_hardware_init(void);
void fan_hardware_set_level(int level, bool dc_mode);
bool fan_hardware_poll_button(int *requested_level);
