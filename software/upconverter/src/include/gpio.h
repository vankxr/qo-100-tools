#ifndef __GPIO_H__
#define __GPIO_H__

#include <em_device.h>
#include "cmu.h"
#include "systick.h"
#include "utils.h"
#include "nvic.h"

// LED MACROS
#define LED_HIGH()          PERI_REG_BIT_SET(&(GPIO->P[1].DOUT)) = BIT(7)
#define LED_LOW()           PERI_REG_BIT_CLEAR(&(GPIO->P[1].DOUT)) = BIT(7)
#define LED_TOGGLE()        GPIO->P[1].DOUTTGL = BIT(7);

// PLL MACROS
#define PLL_ENABLE()        PERI_REG_BIT_SET(&(GPIO->P[2].DOUT)) = BIT(15)
#define PLL_DISABLE()       PERI_REG_BIT_CLEAR(&(GPIO->P[2].DOUT)) = BIT(15)
#define PLL_LATCH()         PERI_REG_BIT_SET(&(GPIO->P[0].DOUT)) = BIT(0)
#define PLL_UNLATCH()       PERI_REG_BIT_CLEAR(&(GPIO->P[0].DOUT)) = BIT(0)
#define PLL_UNMUTE()        PERI_REG_BIT_SET(&(GPIO->P[4].DOUT)) = BIT(13)
#define PLL_MUTE()          PERI_REG_BIT_CLEAR(&(GPIO->P[4].DOUT)) = BIT(13)
#define PLL_LOCKED()        !!(GPIO->P[4].DIN & BIT(11))

// MIXER MACROS
#define MIXER_ENABLE()      PERI_REG_BIT_SET(&(GPIO->P[2].DOUT)) = BIT(14)
#define MIXER_DISABLE()     PERI_REG_BIT_CLEAR(&(GPIO->P[2].DOUT)) = BIT(14)

// PA MACROS
#define PA_STG1_ENABLE()    PERI_REG_BIT_CLEAR(&(GPIO->P[0].DOUT)) = BIT(2)
#define PA_STG1_DISABLE()   PERI_REG_BIT_SET(&(GPIO->P[0].DOUT)) = BIT(2)
#define PA_STG2_ENABLE()    PERI_REG_BIT_SET(&(GPIO->P[2].DOUT)) = BIT(0)
#define PA_STG2_DISABLE()   PERI_REG_BIT_CLEAR(&(GPIO->P[2].DOUT)) = BIT(0)

// ATT MACROS
#define ATT_IF_SELECT()     PERI_REG_BIT_CLEAR(&(GPIO->P[2].DOUT)) = BIT(13)
#define ATT_IF_UNSELECT()   PERI_REG_BIT_SET(&(GPIO->P[2].DOUT)) = BIT(13)
#define ATT_RF_SELECT()     PERI_REG_BIT_CLEAR(&(GPIO->P[0].DOUT)) = BIT(1)
#define ATT_RF_UNSELECT()   PERI_REG_BIT_SET(&(GPIO->P[0].DOUT)) = BIT(1)

void gpio_init();

#endif  // __GPIO_H__
