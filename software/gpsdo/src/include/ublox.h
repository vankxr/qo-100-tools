#ifndef __UBLOX_H__
#define __UBLOX_H__

#include <em_device.h>
#include <stdlib.h>
#include <string.h>
#include "systick.h"
#include "atomic.h"
#include "gpio.h"
#include "i2c.h"

#define UBLOX_I2C_ADDR 0x42

// Registers
#define UBLOX_REG_ 0x00

uint8_t ublox_init();
void ublox_isr();

void ublox_poll();

void ublox_config_i2c_port();
void ublox_config_uart_port();

#endif // __UBLOX_H__