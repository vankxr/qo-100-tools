#ifndef __ADC_H__
#define __ADC_H__

#include <em_device.h>
#include "cmu.h"

#define ADC_VBAT_DIV            4.f         // Voltage divider ratio

#define ADC_VBAT_CHAN           ADC_SINGLECTRL_POSSEL_APORT4XCH7

#define LOW_BAT_VOLTAGE         3550.f
#define LOW_BAT_VOLTAGE_HYST    100.f

void adc_init();

float adc_get_avdd();
float adc_get_dvdd();
float adc_get_iovdd();
float adc_get_corevdd();
float adc_get_vbat();

float adc_get_temperature();

#endif  // __ADC_H__
