#ifndef __GPIO_H__
#define __GPIO_H__

#include <em_device.h>
#include "cmu.h"
#include "systick.h"
#include "utils.h"
#include "nvic.h"

// LED MACROS
#define LED_HIGH()          PERI_REG_BIT_SET(&(GPIO->P[1].DOUT)) = BIT(11)
#define LED_LOW()           PERI_REG_BIT_CLEAR(&(GPIO->P[1].DOUT)) = BIT(11)
#define LED_TOGGLE()        GPIO->P[1].DOUTTGL = BIT(11);
#define LED_STATUS()        !!(GPIO->P[1].DOUT & BIT(11))

// PLL MACROS
#define PLL_ENABLE()        PERI_REG_BIT_SET(&(GPIO->P[2].DOUT)) = BIT(14)
#define PLL_DISABLE()       PERI_REG_BIT_CLEAR(&(GPIO->P[2].DOUT)) = BIT(14)
#define PLL_LATCH()         PERI_REG_BIT_SET(&(GPIO->P[2].DOUT)) = BIT(15)
#define PLL_UNLATCH()       PERI_REG_BIT_CLEAR(&(GPIO->P[2].DOUT)) = BIT(15)
#define PLL_UNMUTE()        PERI_REG_BIT_SET(&(GPIO->P[4].DOUT)) = BIT(13)
#define PLL_MUTE()          PERI_REG_BIT_CLEAR(&(GPIO->P[4].DOUT)) = BIT(13)
#define PLL_LOCKED()        !!(GPIO->P[4].DIN & BIT(11))

// MIXER MACROS
#define MIXER_ENABLE()      PERI_REG_BIT_SET(&(GPIO->P[3].DOUT)) = BIT(7)
#define MIXER_DISABLE()     PERI_REG_BIT_CLEAR(&(GPIO->P[3].DOUT)) = BIT(7)

// PA MACROS
#define PA_STG1_2_ENABLE()  PERI_REG_BIT_SET(&(GPIO->P[1].DOUT)) = BIT(7)
#define PA_STG1_2_DISABLE() PERI_REG_BIT_CLEAR(&(GPIO->P[1].DOUT)) = BIT(7)
#define PA_STG3_ENABLE()    PERI_REG_BIT_SET(&(GPIO->P[1].DOUT)) = BIT(8)
#define PA_STG3_DISABLE()   PERI_REG_BIT_CLEAR(&(GPIO->P[1].DOUT)) = BIT(8)

// ATT MACROS
#define ATT_IF_UNLATCH()    PERI_REG_BIT_CLEAR(&(GPIO->P[3].DOUT)) = BIT(6)
#define ATT_IF_LATCH()      PERI_REG_BIT_SET(&(GPIO->P[3].DOUT)) = BIT(6)
#define ATT_RF1_UNLATCH()   PERI_REG_BIT_CLEAR(&(GPIO->P[0].DOUT)) = BIT(1)
#define ATT_RF1_LATCH()     PERI_REG_BIT_SET(&(GPIO->P[0].DOUT)) = BIT(1)
#define ATT_RF2_UNLATCH()   PERI_REG_BIT_CLEAR(&(GPIO->P[0].DOUT)) = BIT(2)
#define ATT_RF2_LATCH()     PERI_REG_BIT_SET(&(GPIO->P[0].DOUT)) = BIT(2)

void gpio_init();

#endif  // __GPIO_H__
