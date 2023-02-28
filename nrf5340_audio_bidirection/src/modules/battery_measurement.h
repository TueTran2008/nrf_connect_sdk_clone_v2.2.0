#ifndef _BATTERY_MEASUREMENT_
#define _BATTERY_MEASUREMENT_
#include <stdint.h>
int battery_measurement_init();

int battery_raw_data_get(uint16_t *bat_vol);

#endif