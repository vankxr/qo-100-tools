#ifndef __GPIO_H__
#define __GPIO_H__

#include <em_device.h>
#include "cmu.h"
#include "systick.h"
#include "utils.h"
#include "nvic.h"

// LED MACROS
#define LED_HIGH()          PERI_REG_BIT_SET(&(GPIO->P[2].DOUT)) = BIT(10)
#define LED_LOW()           PERI_REG_BIT_CLEAR(&(GPIO->P[2].DOUT)) = BIT(10)
#define LED_TOGGLE()        GPIO->P[2].DOUTTGL = BIT(10);

// PLL MACROS
#define PLL_ENABLE()        PERI_REG_BIT_SET(&(GPIO->P[3].DOUT)) = BIT(9)
#define PLL_DISABLE()       PERI_REG_BIT_CLEAR(&(GPIO->P[3].DOUT)) = BIT(9)
#define PLL_LATCH()         PERI_REG_BIT_SET(&(GPIO->P[3].DOUT)) = BIT(10)
#define PLL_UNLATCH()       PERI_REG_BIT_CLEAR(&(GPIO->P[3].DOUT)) = BIT(10)
#define PLL_UNMUTE()        PERI_REG_BIT_SET(&(GPIO->P[3].DOUT)) = BIT(14)
#define PLL_MUTE()          PERI_REG_BIT_CLEAR(&(GPIO->P[3].DOUT)) = BIT(14)
#define PLL_LOCKED()        PERI_REG_BIT(&(GPIO->P[3].DIN), 15)

// MIXER MACROS
#define MIXER_ENABLE()      PERI_REG_BIT_SET(&(GPIO->P[5].DOUT)) = BIT(3)
#define MIXER_DISABLE()     PERI_REG_BIT_CLEAR(&(GPIO->P[5].DOUT)) = BIT(3)

// PA MACROS
#define PA_5V0_ENABLE()     PERI_REG_BIT_CLEAR(&(GPIO->P[0].DOUT)) = BIT(0)
#define PA_5V0_DISABLE()    PERI_REG_BIT_SET(&(GPIO->P[0].DOUT)) = BIT(0)
#define PA_12V0_ENABLE()    PERI_REG_BIT_SET(&(GPIO->P[0].DOUT)) = BIT(1)
#define PA_12V0_DISABLE()   PERI_REG_BIT_CLEAR(&(GPIO->P[0].DOUT)) = BIT(1)

// ATT MACROS
#define ATT_SELECT()        PERI_REG_BIT_CLEAR(&(GPIO->P[1].DOUT)) = BIT(15)
#define ATT_UNSELECT()      PERI_REG_BIT_SET(&(GPIO->P[1].DOUT)) = BIT(15)

void gpio_init();

#endif  // __GPIO_H__
