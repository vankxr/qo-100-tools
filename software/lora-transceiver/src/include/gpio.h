#ifndef __GPIO_H__
#define __GPIO_H__

#include <em_device.h>
#include "cmu.h"
#include "systick.h"
#include "utils.h"
#include "nvic.h"

// LED MACROS
#define LED_HIGH()          PERI_REG_BIT_SET(&(GPIO->P[1].DOUT)) = BIT(8)
#define LED_LOW()           PERI_REG_BIT_CLEAR(&(GPIO->P[1].DOUT)) = BIT(8)
#define LED_TOGGLE()        GPIO->P[1].DOUTTGL = BIT(8);
#define LED_STATUS()        !!(GPIO->P[1].DOUT & BIT(8))

void gpio_init();

#endif  // __GPIO_H__
