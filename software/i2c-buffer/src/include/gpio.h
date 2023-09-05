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

// I2C BUFFER MACROS
#define BUF1_ENABLE()       PERI_REG_BIT_SET(&(GPIO->P[0].DOUT)) = BIT(0)
#define BUF1_DISABLE()      PERI_REG_BIT_CLEAR(&(GPIO->P[0].DOUT)) = BIT(0)
#define BUF2_ENABLE()       PERI_REG_BIT_SET(&(GPIO->P[1].DOUT)) = BIT(11)
#define BUF2_DISABLE()      PERI_REG_BIT_CLEAR(&(GPIO->P[1].DOUT)) = BIT(11)
#define BUF3_ENABLE()       PERI_REG_BIT_SET(&(GPIO->P[3].DOUT)) = BIT(4)
#define BUF3_DISABLE()      PERI_REG_BIT_CLEAR(&(GPIO->P[3].DOUT)) = BIT(4)
#define BUF4_ENABLE()       PERI_REG_BIT_SET(&(GPIO->P[2].DOUT)) = BIT(13)
#define BUF4_DISABLE()      PERI_REG_BIT_CLEAR(&(GPIO->P[2].DOUT)) = BIT(13)

// BIAS MACROS
#define BIAS1_ENABLE()      PERI_REG_BIT_SET(&(GPIO->P[4].DOUT)) = BIT(12)
#define BIAS1_DISABLE()     PERI_REG_BIT_CLEAR(&(GPIO->P[4].DOUT)) = BIT(12)
#define BIAS2_ENABLE()      PERI_REG_BIT_SET(&(GPIO->P[0].DOUT)) = BIT(1)
#define BIAS2_DISABLE()     PERI_REG_BIT_CLEAR(&(GPIO->P[0].DOUT)) = BIT(1)
#define BIAS3_ENABLE()      PERI_REG_BIT_SET(&(GPIO->P[1].DOUT)) = BIT(13)
#define BIAS3_DISABLE()     PERI_REG_BIT_CLEAR(&(GPIO->P[1].DOUT)) = BIT(13)
#define BIAS4_ENABLE()      PERI_REG_BIT_SET(&(GPIO->P[3].DOUT)) = BIT(5)
#define BIAS4_DISABLE()     PERI_REG_BIT_CLEAR(&(GPIO->P[3].DOUT)) = BIT(5)

// SCL SENSE MACROS
#define SCL1_SENSE()        !!(GPIO->P[1].DIN & BIT(8))
#define SCL2_SENSE()        !!(GPIO->P[1].DIN & BIT(7))
#define SCL3_SENSE()        !!(GPIO->P[2].DIN & BIT(15))
#define SCL4_SENSE()        !!(GPIO->P[2].DIN & BIT(14))

void gpio_init();

#endif  // __GPIO_H__
