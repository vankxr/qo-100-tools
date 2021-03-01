#ifndef __MCP3421_H__
#define __MCP3421_H__

#include <em_device.h>
#include "i2c.h"
#include "atomic.h"
#include "systick.h"

#define MCP3421_I2C_ADDR 0x68 // Base address 0x68

#define MCP3421_BUSY                0x80
#define MCP3421_CONTINUOUS          0x10
#define MCP3421_ONE_SHOT            0x00
#define MCP3421_RESOLUTION_12BIT    0x00
#define MCP3421_RESOLUTION_14BIT    0x04
#define MCP3421_RESOLUTION_16BIT    0x08
#define MCP3421_RESOLUTION_18BIT    0x0C
#define MCP3421_PGA_X1              0x00
#define MCP3421_PGA_X2              0x01
#define MCP3421_PGA_X4              0x02
#define MCP3421_PGA_X8              0x03


uint8_t mcp3421_init();
void mcp3421_sleep();
void mcp3421_wakeup();

void mcp3421_write_config(uint8_t ubConfig);
uint8_t mcp3421_read_config();
double mcp3421_read_adc(uint8_t ubGain);

#endif  // __MCP3421_H__
