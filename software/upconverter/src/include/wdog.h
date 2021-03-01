#ifndef __WDOG_H__
#define __WDOG_H__

#include <em_device.h>
#include "cmu.h"
#include "nvic.h"

typedef void (* wdog_warning_isr_t)();

void wdog_init(uint32_t ulTimeoutConfig);
void wdog_set_warning_isr(wdog_warning_isr_t pfISR);
float wdog_get_timeout_period();
float wdog_get_warning_period();
void wdog_feed();

#endif  // __WDOG_H__