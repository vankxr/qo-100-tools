#ifndef __ADC_H__
#define __ADC_H__

#include <em_device.h>
#include "cmu.h"

#define ADC_VIN_DIV             11.f // Voltage divider ratio
#define ADC_5V0_DIV             2.333333f // Voltage divider ratio

#define ADC_VIN_CHAN            ADC_SINGLECTRL_POSSEL_APORT1XCH30
#define ADC_5V0_CHAN            ADC_SINGLECTRL_POSSEL_APORT2XCH29

#define ADC_VPDET_CHAN          ADC_SINGLECTRL_POSSEL_APORT1XCH0

void adc_init();

float adc_get_avdd();
float adc_get_dvdd();
float adc_get_iovdd();
float adc_get_corevdd();
float adc_get_vin();
float adc_get_5v0();
float adc_get_vpdet();

float adc_get_temperature();

#endif  // __ADC_H__
