#ifndef __GPIO_H__
#define __GPIO_H__

#include <em_device.h>
#include "cmu.h"
#include "systick.h"
#include "utils.h"
#include "nvic.h"

// LED MACROS
#define LED_HIGH()          PERI_REG_BIT_SET(&(GPIO->P[4].DOUT)) = BIT(11)
#define LED_LOW()           PERI_REG_BIT_CLEAR(&(GPIO->P[4].DOUT)) = BIT(11)
#define LED_TOGGLE()        GPIO->P[4].DOUTTGL = BIT(11);
#define LED_STATUS()        !!(GPIO->P[4].DOUT & BIT(11))

// RELAY MACROS
#define RELAY0_ON()         PERI_REG_BIT_SET(&(GPIO->P[1].DOUT)) = BIT(8)
#define RELAY0_OFF()        PERI_REG_BIT_CLEAR(&(GPIO->P[1].DOUT)) = BIT(8)
#define RELAY0_TOGGLE()     GPIO->P[1].DOUTTGL = BIT(8);
#define RELAY1_ON()         PERI_REG_BIT_SET(&(GPIO->P[1].DOUT)) = BIT(7)
#define RELAY1_OFF()        PERI_REG_BIT_CLEAR(&(GPIO->P[1].DOUT)) = BIT(7)
#define RELAY1_TOGGLE()     GPIO->P[1].DOUTTGL = BIT(7);
#define RELAY2_ON()         PERI_REG_BIT_SET(&(GPIO->P[2].DOUT)) = BIT(1)
#define RELAY2_OFF()        PERI_REG_BIT_CLEAR(&(GPIO->P[2].DOUT)) = BIT(1)
#define RELAY2_TOGGLE()     GPIO->P[2].DOUTTGL = BIT(1);
#define RELAY3_ON()         PERI_REG_BIT_SET(&(GPIO->P[0].DOUT)) = BIT(2)
#define RELAY3_OFF()        PERI_REG_BIT_CLEAR(&(GPIO->P[0].DOUT)) = BIT(2)
#define RELAY3_TOGGLE()     GPIO->P[0].DOUTTGL = BIT(2);
#define RELAY4_ON()         PERI_REG_BIT_SET(&(GPIO->P[0].DOUT)) = BIT(1)
#define RELAY4_OFF()        PERI_REG_BIT_CLEAR(&(GPIO->P[0].DOUT)) = BIT(1)
#define RELAY4_TOGGLE()     GPIO->P[0].DOUTTGL = BIT(1);
#define RELAY5_ON()         PERI_REG_BIT_SET(&(GPIO->P[0].DOUT)) = BIT(0)
#define RELAY5_OFF()        PERI_REG_BIT_CLEAR(&(GPIO->P[0].DOUT)) = BIT(0)
#define RELAY5_TOGGLE()     GPIO->P[0].DOUTTGL = BIT(0);
#define RELAY6_ON()         PERI_REG_BIT_SET(&(GPIO->P[2].DOUT)) = BIT(14)
#define RELAY6_OFF()        PERI_REG_BIT_CLEAR(&(GPIO->P[2].DOUT)) = BIT(14)
#define RELAY6_TOGGLE()     GPIO->P[2].DOUTTGL = BIT(14);
#define RELAY7_ON()         PERI_REG_BIT_SET(&(GPIO->P[2].DOUT)) = BIT(13)
#define RELAY7_OFF()        PERI_REG_BIT_CLEAR(&(GPIO->P[2].DOUT)) = BIT(13)
#define RELAY7_TOGGLE()     GPIO->P[2].DOUTTGL = BIT(13);
#define RELAY8_ON()         PERI_REG_BIT_SET(&(GPIO->P[3].DOUT)) = BIT(7)
#define RELAY8_OFF()        PERI_REG_BIT_CLEAR(&(GPIO->P[3].DOUT)) = BIT(7)
#define RELAY8_TOGGLE()     GPIO->P[3].DOUTTGL = BIT(7);
#define RELAY9_ON()         PERI_REG_BIT_SET(&(GPIO->P[3].DOUT)) = BIT(6)
#define RELAY9_OFF()        PERI_REG_BIT_CLEAR(&(GPIO->P[3].DOUT)) = BIT(6)
#define RELAY9_TOGGLE()     GPIO->P[3].DOUTTGL = BIT(6);
#define RELAY10_ON()        PERI_REG_BIT_SET(&(GPIO->P[3].DOUT)) = BIT(5)
#define RELAY10_OFF()       PERI_REG_BIT_CLEAR(&(GPIO->P[3].DOUT)) = BIT(5)
#define RELAY10_TOGGLE()    GPIO->P[3].DOUTTGL = BIT(5);
#define RELAY11_ON()        PERI_REG_BIT_SET(&(GPIO->P[3].DOUT)) = BIT(4)
#define RELAY11_OFF()       PERI_REG_BIT_CLEAR(&(GPIO->P[3].DOUT)) = BIT(4)
#define RELAY11_TOGGLE()    GPIO->P[3].DOUTTGL = BIT(4);

void gpio_init();

#endif  // __GPIO_H__
