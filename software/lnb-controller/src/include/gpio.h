#ifndef __GPIO_H__
#define __GPIO_H__

#include <em_device.h>
#include "cmu.h"
#include "systick.h"
#include "utils.h"
#include "nvic.h"

// LED MACROS
#define LED_HIGH()          PERI_REG_BIT_SET(&(GPIO->P[2].DOUT)) = BIT(1)
#define LED_LOW()           PERI_REG_BIT_CLEAR(&(GPIO->P[2].DOUT)) = BIT(1)
#define LED_TOGGLE()        GPIO->P[2].DOUTTGL = BIT(1);
#define LED_STATUS()        !!(GPIO->P[2].DOUT & BIT(1))

// VBIAS MACROS
#define VBIAS1_ENABLE()     PERI_REG_BIT_SET(&(GPIO->P[4].DOUT)) = BIT(12)
#define VBIAS1_DISABLE()    PERI_REG_BIT_CLEAR(&(GPIO->P[4].DOUT)) = BIT(12)
#define VBIAS1_STATUS()     !!(GPIO->P[4].DOUT & BIT(12))
#define VBIAS1_POWER_GOOD() !!(GPIO->P[4].DIN & BIT(10))
#define VBIAS2_ENABLE()     PERI_REG_BIT_SET(&(GPIO->P[4].DOUT)) = BIT(13)
#define VBIAS2_DISABLE()    PERI_REG_BIT_CLEAR(&(GPIO->P[4].DOUT)) = BIT(13)
#define VBIAS2_STATUS()     !!(GPIO->P[4].DOUT & BIT(13))
#define VBIAS2_POWER_GOOD() !!(GPIO->P[4].DIN & BIT(11))

// REFERENE SYNTHESIZER MACROS
#define REF_SYNTH_OUT_EN()  PERI_REG_BIT_CLEAR(&(GPIO->P[2].DOUT)) = BIT(14)
#define REF_SYNTH_OUT_DIS() PERI_REG_BIT_SET(&(GPIO->P[2].DOUT)) = BIT(14)

void gpio_init();

#endif  // __GPIO_H__
