#include "mcp4728.h"

uint8_t mcp4728_init()
{
    delay_ms(2);

    if(!i2c1_write(MCP4728_I2C_ADDR, 0, 0, I2C_STOP))
        return 0;

    return 1;
}

void mcp4728_busy_wait()
{
    uint8_t ubStatus;

    do
    {
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
            ubStatus = i2c1_read_byte(MCP4728_I2C_ADDR, I2C_STOP);
        }
    } while(!(ubStatus & 0x80));
}

void mcp4728_fast_write(uint16_t *pusData)
{
    uint8_t pubData[8];

    for(uint8_t i = 0; i < 4; i++)
    {
        pusData[i] &= 0x3FFF;

        pubData[i * 2 + 0] = MCP4728_CMD_WRITE_FAST | (pusData[i] >> 8);
        pubData[i * 2 + 1] = pusData[i] >> 0;
    }

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        i2c1_write(MCP4728_I2C_ADDR, pubData, sizeof(pubData), I2C_STOP);
    }
}

void mcp4728_channel_write(uint8_t ubChannel, uint16_t usData)
{
    if(ubChannel > 3)
        return;

    uint8_t pubData[3];

    pubData[0] = MCP4728_CMD_WRITE | (ubChannel << 1);
    pubData[1] = usData >> 8;
    pubData[2] = usData >> 0;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        i2c1_write(MCP4728_I2C_ADDR, pubData, sizeof(pubData), I2C_STOP);
    }
}
void mcp4728_channel_nvm_write(uint8_t ubChannel, uint16_t usData)
{
    if(ubChannel > 3)
        return;

    uint8_t pubData[3];

    pubData[0] = MCP4728_CMD_WRITE_SINGLE_EEPROM | (ubChannel << 1);
    pubData[1] = usData >> 8;
    pubData[2] = usData >> 0;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        i2c1_write(MCP4728_I2C_ADDR, pubData, sizeof(pubData), I2C_STOP);
    }

    mcp4728_busy_wait();
}
uint16_t mcp4728_channel_read(uint8_t ubChannel)
{
    if(ubChannel > 3)
        return 0;

    uint8_t pubData[24];

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        i2c1_read(MCP4728_I2C_ADDR, pubData, sizeof(pubData), I2C_STOP);
    }

    return ((uint16_t)pubData[ubChannel * 6 + 1] << 8) | (uint16_t)pubData[ubChannel * 6 + 2];
}
uint16_t mcp4728_channel_nvm_read(uint8_t ubChannel)
{
    if(ubChannel > 3)
        return 0;

    uint8_t pubData[24];

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        i2c1_read(MCP4728_I2C_ADDR, pubData, sizeof(pubData), I2C_STOP);
    }

    return ((uint16_t)pubData[ubChannel * 6 + 4] << 8) | (uint16_t)pubData[ubChannel * 6 + 5];
}
void mcp4728_channel_nvm_copy(uint8_t ubChannel)
{
    if(ubChannel > 3)
        return;

    mcp4728_channel_nvm_write(ubChannel, mcp4728_channel_read(ubChannel));
}