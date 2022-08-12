#ifndef __GPIO_H__
#define __GPIO_H__

#include <em_device.h>
#include "cmu.h"
#include "systick.h"
#include "utils.h"
#include "nvic.h"

// LED MACROS
#define LED_HIGH()          PERI_REG_BIT_SET(&(GPIO->P[4].DOUT)) = BIT(10)
#define LED_LOW()           PERI_REG_BIT_CLEAR(&(GPIO->P[4].DOUT)) = BIT(10)
#define LED_TOGGLE()        GPIO->P[4].DOUTTGL = BIT(10);
#define LED_STATUS()        !!(GPIO->P[4].DOUT & BIT(10))

void gpio_init();

#endif  // __GPIO_H__
