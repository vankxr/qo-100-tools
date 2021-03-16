const { I2C, I2CDevice } = require.main.require("./lib/i2c");

class LTC4151 extends I2CDevice
{
    current_shunt_value;
    adin_scale_factor;

    constructor(bus, addr, bus_enable_gpio)
    {
        if(bus instanceof I2CDevice)
            super(bus.bus, bus.addr, bus.bus_enable_gpio);
        else
            super(bus, 0x67 + ((addr < 0 || addr > 8) ? 0 : addr), bus_enable_gpio);

        this.current_shunt_value = 1;
        this.adin_scale_factor = 1;
    }

    set_current_shunt_value(shunt_value)
    {
        this.current_shunt_value = shunt_value;
    }
    get_current_shunt_value()
    {
        return this.current_shunt_value;
    }
    set_adin_scale_factor(scale_factor)
    {
        this.adin_scale_factor = scale_factor;
    }
    get_adin_scale_factor()
    {
        return this.adin_scale_factor;
    }

    async write(reg, data)
    {
        if(typeof(reg) !== "number" || isNaN(reg) || reg < 0 || reg > 6)
            throw new Error("Invalid register");

        if(typeof(data) === "number")
            data = [data];

        if(Array.isArray(data))
            data = Buffer.from(data);

        if(!(data instanceof Buffer))
            throw new Error("Invalid data");

        let buf = Buffer.alloc(data.length + 1, 0);

        buf.writeUInt8(reg, 0);
        data.copy(buf, 1, 0);

        await super.write(buf);
    }
    async read(reg, count = 1)
    {
        if(typeof(reg) !== "number" || isNaN(reg) || reg < 0 || reg > 6)
            throw new Error("Invalid register");

        if(typeof(count) !== "number" || isNaN(count) || count < 1)
            throw new Error("Invalid count");

        let buf = await super.read(6);

        return buf.slice(reg, reg + count);
    }

    async config()
    {
        await this.write(0x06, 0x0C);
    }

    async get_current(samples = 1)
    {
        if(isNaN(samples) || samples < 1)
            throw new Error("Invalid sample count");

        let accum = 0;

        for(let i = 0; i < samples; i++)
        {
            let buf = await this.read(0x00, 2);
            let code = buf.readUInt16BE(0) >> 4;
            let voltage = 81.92 * code / 4096; // mV

            accum += voltage;
        }

        accum /= samples;

        return accum / this.current_shunt_value;
    }
    async get_vin_voltage(samples = 1)
    {
        if(isNaN(samples) || samples < 1)
            throw new Error("Invalid sample count");

        let accum = 0;

        for(let i = 0; i < samples; i++)
        {
            let buf = await this.read(0x02, 2);
            let code = buf.readUInt16BE(0) >> 4;
            let voltage = 102400 * code / 4096; // mV

            accum += voltage;
        }

        accum /= samples;

        return accum;
    }
    async get_adin_voltage(samples = 1)
    {
        if(isNaN(samples) || samples < 1)
            throw new Error("Invalid sample count");

        let accum = 0;

        for(let i = 0; i < samples; i++)
        {
            let buf = await this.read(0x04, 2);
            let code = buf.readUInt16BE(0) >> 4;
            let voltage = 2048 * code / 4096; // mV

            accum += voltage;
        }

        accum /= samples;

        return accum * this.adin_scale_factor;
    }
}

module.exports = LTC4151;