#ifndef __ADC_H__
#define __ADC_H__

#include <em_device.h>
#include "cmu.h"

#define ADC_VBIAS1_CHAN         ADC_SINGLECTRL_POSSEL_APORT4XCH13
#define ADC_VBIAS2_CHAN         ADC_SINGLECTRL_POSSEL_APORT1XCH2
#define ADC_VBIAS3_CHAN         ADC_SINGLECTRL_POSSEL_APORT1XCH30
#define ADC_VBIAS4_CHAN         ADC_SINGLECTRL_POSSEL_APORT0XCH6

void adc_init();

float adc_get_avdd();
float adc_get_dvdd();
float adc_get_iovdd();
float adc_get_corevdd();
float adc_get_vbias1();
float adc_get_vbias2();
float adc_get_vbias3();
float adc_get_vbias4();

float adc_get_temperature();

#endif  // __ADC_H__
