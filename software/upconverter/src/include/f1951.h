#ifndef __F1951_H__
#define __F1951_H__

#include <em_device.h>
#include <stdlib.h>
#include "systick.h"
#include "atomic.h"
#include "gpio.h"
#include "usart.h"

extern float F1951_ATTENUATION[2];

#define F1951_IF_ATT_ID 0
#define F1951_RF_ATT_ID 1

uint8_t f1951_init(uint8_t ubID);

uint8_t f1951_set_attenuation(uint8_t ubID, float fAttenuation);

#endif // __F1951_H__