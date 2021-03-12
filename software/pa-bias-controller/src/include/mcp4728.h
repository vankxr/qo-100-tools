#ifndef __MCP4728_H__
#define __MCP4728_H__

#include <em_device.h>
#include "i2c.h"
#include "atomic.h"
#include "systick.h"

#define MCP4728_I2C_ADDR 0x60 // Base address 0x60

// Commands
#define MCP4728_CMD_WRITE_FAST              0x00
#define MCP4728_CMD_WRITE                   0x40
#define MCP4728_CMD_WRITE_SEQ_EEPROM        0x50
#define MCP4728_CMD_WRITE_SINGLE_EEPROM     0x58
#define MCP4728_CMD_WRITE_I2C_ADDR          0x60
#define MCP4728_CMD_WRITE_VREF              0x80
#define MCP4728_CMD_WRITE_POWER_DOWN        0xA0
#define MCP4728_CMD_WRITE_GAIN              0xC0

#define MCP4728_CHAN_VREF_EXTERNAL  0x0000
#define MCP4728_CHAN_VREF_INTERNAL  0x8000
#define MCP4728_CHAN_PD_NORMAL      0x0000
#define MCP4728_CHAN_PD_1K          0x2000
#define MCP4728_CHAN_PD_100K        0x4000
#define MCP4728_CHAN_PD_500K        0x6000
#define MCP4728_CHAN_GAIN_X1        0x0000
#define MCP4728_CHAN_GAIN_X2        0x1000

// Constants
#define MCP4728_INTERNAL_REF 2048.f
#define MCP4728_EXTERNAL_REF 3300.f


uint8_t mcp4728_init();

void mcp4728_busy_wait();

void mcp4728_fast_write(uint16_t *pusData);

void mcp4728_channel_write(uint8_t ubChannel, uint16_t usData);
void mcp4728_channel_nvm_write(uint8_t ubChannel, uint16_t usData);
uint16_t mcp4728_channel_read(uint8_t ubChannel);
uint16_t mcp4728_channel_nvm_read(uint8_t ubChannel);
void mcp4728_channel_nvm_copy(uint8_t ubChannel);

#endif  // __MCP4728_H__
