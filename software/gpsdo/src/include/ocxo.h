#ifndef __OCXO_H__
#define __OCXO_H__

#include <em_device.h>
#include <stdlib.h>
#include "atomic.h"
#include "gpio.h"
#include "usart.h"


uint8_t ocxo_init();
void gps_timepulse_isr();

void ocxo_set_vcont(float fVCont);

#endif // __OCXO_H__