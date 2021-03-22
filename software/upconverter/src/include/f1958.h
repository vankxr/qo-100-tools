#ifndef __F1958_H__
#define __F1958_H__

#include <em_device.h>
#include <stdlib.h>
#include "systick.h"
#include "atomic.h"
#include "gpio.h"
#include "usart.h"

extern float F1958_ATTENUATION[3];

#define F1958_IF_ATT_ID     0
#define F1958_RF1_ATT_ID    1
#define F1958_RF2_ATT_ID    2

uint8_t f1958_init(uint8_t ubID);

uint8_t f1958_set_attenuation(uint8_t ubID, float fAttenuation);

#endif // __F1958_H__