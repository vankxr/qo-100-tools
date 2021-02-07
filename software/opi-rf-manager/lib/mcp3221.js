const GPIO = require.main.require("./lib/gpio");
const { I2C, I2CDevice} = require.main.require("./lib/i2c");

class MCP3221 extends I2CDevice
{
    constructor(bus, addr, bus_enable_gpio)
    {
        if(bus instanceof I2CDevice)
            super(bus.bus, bus.addr, bus.bus_enable_gpio);
        else
            super(bus, 0x48 | (addr & 0x07), bus_enable_gpio);
    }

    async get_voltage(samples = 1)
    {
        let accum = 0;

        for(let i = 0; i < samples; i++)
        {
            let result = await super.read(2);
            let voltage = 2500 * result.readUInt16BE(0) / 4096; // mV

            accum += voltage;
        }

        accum /= samples;

        return accum;
    }
}

module.exports = MCP3221;