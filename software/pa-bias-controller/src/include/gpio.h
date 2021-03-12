#ifndef __GPIO_H__
#define __GPIO_H__

#include <em_device.h>
#include "cmu.h"
#include "systick.h"
#include "utils.h"
#include "nvic.h"

// LED MACROS
#define LED_HIGH()          PERI_REG_BIT_SET(&(GPIO->P[2].DOUT)) = BIT(15)
#define LED_LOW()           PERI_REG_BIT_CLEAR(&(GPIO->P[2].DOUT)) = BIT(15)
#define LED_TOGGLE()        GPIO->P[2].DOUTTGL = BIT(15);
#define LED_STATUS()        !!(GPIO->P[2].DOUT & BIT(15))

// TEC MACROS
#define TEC1_ENABLE()       PERI_REG_BIT_SET(&(GPIO->P[2].DOUT)) = BIT(1)
#define TEC1_DISABLE()      PERI_REG_BIT_CLEAR(&(GPIO->P[2].DOUT)) = BIT(1)
#define TEC2_ENABLE()       PERI_REG_BIT_SET(&(GPIO->P[1].DOUT)) = BIT(8)
#define TEC2_DISABLE()      PERI_REG_BIT_CLEAR(&(GPIO->P[1].DOUT)) = BIT(8)
#define TEC3_ENABLE()       PERI_REG_BIT_SET(&(GPIO->P[2].DOUT)) = BIT(0)
#define TEC3_DISABLE()      PERI_REG_BIT_CLEAR(&(GPIO->P[2].DOUT)) = BIT(0)
#define TEC4_ENABLE()       PERI_REG_BIT_SET(&(GPIO->P[1].DOUT)) = BIT(7)
#define TEC4_DISABLE()      PERI_REG_BIT_CLEAR(&(GPIO->P[1].DOUT)) = BIT(7)

// AFE MACROS
#define AFE_SELECT()        PERI_REG_BIT_CLEAR(&(GPIO->P[4].DOUT)) = BIT(13)
#define AFE_UNSELECT()      PERI_REG_BIT_SET(&(GPIO->P[4].DOUT)) = BIT(13)

// PA MACROS
#define PA1_ENABLE()        PERI_REG_BIT_SET(&(GPIO->P[1].DOUT)) = BIT(14)
#define PA1_DISABLE()       PERI_REG_BIT_CLEAR(&(GPIO->P[1].DOUT)) = BIT(14)
#define PA2_ENABLE()        PERI_REG_BIT_SET(&(GPIO->P[1].DOUT)) = BIT(13)
#define PA2_DISABLE()       PERI_REG_BIT_CLEAR(&(GPIO->P[1].DOUT)) = BIT(13)

void gpio_init();

#endif  // __GPIO_H__
