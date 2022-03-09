#ifndef __GPIO_H__
#define __GPIO_H__

#include <em_device.h>
#include "cmu.h"
#include "systick.h"
#include "utils.h"
#include "nvic.h"

// LED MACROS
#define LED_HIGH()              PERI_REG_BIT_SET(&(GPIO->P[1].DOUT)) = BIT(13)
#define LED_LOW()               PERI_REG_BIT_CLEAR(&(GPIO->P[1].DOUT)) = BIT(13)
#define LED_TOGGLE()            GPIO->P[1].DOUTTGL = BIT(13);
#define LED_STATUS()            !!(GPIO->P[1].DOUT & BIT(13))

// POWER MACROS
#define PWR_3V3_PGOOD()         PERI_REG_BIT(&(GPIO->P[0].DIN), 6)
#define PWR_1V8_PLL_ENABLE()    PERI_REG_BIT_SET(&(GPIO->P[0].DOUT)) = BIT(6)
#define PWR_1V8_PLL_DISABLE()   PERI_REG_BIT_CLEAR(&(GPIO->P[0].DOUT)) = BIT(6)
#define PWR_OCXO_ENABLE()       PERI_REG_BIT_SET(&(GPIO->P[4].DOUT)) = BIT(11)
#define PWR_OCXO_DISABLE()      PERI_REG_BIT_CLEAR(&(GPIO->P[4].DOUT)) = BIT(11)

void gpio_init();

#endif  // __GPIO_H__
