const GPIO = require.main.require("./lib/gpio");
const { I2C, I2CDevice } = require.main.require("./lib/i2c");

class MCP3221 extends I2CDevice
{
    scale_factor;
    reference;

    constructor(bus, addr, bus_enable_gpio)
    {
        if(bus instanceof I2CDevice)
            super(bus.bus, bus.addr, bus.bus_enable_gpio);
        else
            super(bus, 0x48 | (addr & 0x07), bus_enable_gpio);

        this.scale_factor = 1;
    }

    set_scale_factor(scale_factor)
    {
        this.scale_factor = scale_factor;
    }
    get_scale_factor()
    {
        return this.scale_factor;
    }

    set_reference(reference)
    {
        this.reference = reference;
    }
    get_reference()
    {
        return this.reference;
    }

    async get_voltage(samples = 1)
    {
        if(isNaN(samples) || samples < 1)
            throw new Error("Invalid sample count");

        let result = await super.read(2 * samples);
        let accum = 0;

        for(let i = 0; i < samples; i++)
        {
            let code = result.readUInt16BE(i * 2) & 0x0FFF;
            let voltage = this.reference * code / 4096; // mV

            accum += voltage;
        }

        accum /= samples;

        return accum * this.scale_factor;
    }
}

module.exports = MCP3221;