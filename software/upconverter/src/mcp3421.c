#include "mcp3421.h"

static uint8_t pubMCP3421Buffer[4];

static void mcp3421_shift(uint8_t ubCount)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        i2c1_read(MCP3421_I2C_ADDR, pubMCP3421Buffer, ubCount, I2C_STOP);
    }
}

uint8_t mcp3421_init()
{
    delay_ms(2);

    if(!i2c1_write(MCP3421_I2C_ADDR, 0, 0, I2C_STOP))
        return 0;

    mcp3421_shift(4); // Shift POR register status

    return 1;
}
void mcp3421_sleep()
{
    uint8_t ubConfig = mcp3421_read_config();

    mcp3421_write_config(ubConfig & ~(MCP3421_BUSY | MCP3421_CONTINUOUS));
}
void mcp3421_wakeup()
{
    if(!(pubMCP3421Buffer[3] & MCP3421_CONTINUOUS))
        return;

    uint8_t ubConfig = mcp3421_read_config();

    mcp3421_write_config(ubConfig | MCP3421_CONTINUOUS);
}
void mcp3421_write_config(uint8_t ubConfig)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        i2c1_write_byte(MCP3421_I2C_ADDR, ubConfig, I2C_STOP);
    }
}
uint8_t mcp3421_read_config()
{
    mcp3421_shift(4);

    return pubMCP3421Buffer[3];
}
double mcp3421_read_adc(uint8_t ubGain)
{
    uint8_t ubConfig = mcp3421_read_config();
    uint8_t ubResolution = ((ubConfig & 0x0C) >> 1) + 12;

    mcp3421_write_config((ubConfig & ~0x03) | (ubGain & 0x03) | MCP3421_BUSY);

    if(ubResolution > 16)
    {
        do
        {
            mcp3421_shift(4);
        }
        while((ubConfig = pubMCP3421Buffer[3]) & MCP3421_BUSY);

        int32_t lResult = 0;

        lResult |= ((int32_t)pubMCP3421Buffer[0]) << 16;
        lResult |= ((int32_t)pubMCP3421Buffer[1]) << 8;
        lResult |= (int32_t)pubMCP3421Buffer[2];

        if(pubMCP3421Buffer[0] & 0x80)
            lResult |= 0xFF000000; // Propagate the last bit for signed operations

        return (double)lResult * 2048.f / (1UL << (ubResolution - 1 + (ubGain & 0x03)));
    }
    else
    {
        do
        {
            mcp3421_shift(3);
        }
        while((ubConfig = pubMCP3421Buffer[2]) & MCP3421_BUSY);

        int16_t sResult = 0;

        sResult |= ((int16_t)pubMCP3421Buffer[0]) << 8;
        sResult |= (int16_t)pubMCP3421Buffer[1];

        return (double)sResult * 2048.f / (1UL << (ubResolution - 1 + (ubGain & 0x03)));
    }
}
