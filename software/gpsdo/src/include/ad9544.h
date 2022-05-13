#ifndef __AD9544_H__
#define __AD9544_H__

#include <em_device.h>
#include <stdlib.h>
#include "systick.h"
#include "atomic.h"
#include "gpio.h"
#include "i2c.h"

#define AD9544_I2C_ADDR 0x48

// Registers
#define AD9544_REG_ 0x00

uint8_t ad9544_init();
void ad9544_isr();

#endif // __AD9544_H__