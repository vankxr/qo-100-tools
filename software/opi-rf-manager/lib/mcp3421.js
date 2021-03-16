const GPIO = require.main.require("./lib/gpio");
const { I2C, I2CDevice } = require.main.require("./lib/i2c");

class MCP3421 extends I2CDevice
{
    scale_factor;

    constructor(bus, addr, bus_enable_gpio)
    {
        if(bus instanceof I2CDevice)
            super(bus.bus, bus.addr, bus.bus_enable_gpio);
        else
            super(bus, 0x68 | (addr & 0x07), bus_enable_gpio);

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

    async read_burst()
    {
        return super.read(4);
    }
    async read_config()
    {
        let buf = await this.read_burst();

        return buf.readUInt8(3);
    }
    async write_config(config)
    {
        await super.write(config);
    }

    async config(resolution, continuous)
    {
        let config = (await this.read_config()) & 0x63;

        switch(resolution)
        {
            case 12:
                config |= 0x00;
            break;
            case 14:
                config |= 0x04;
            break;
            case 16:
                config |= 0x08;
            break;
            case 18:
                config |= 0x0C;
            break;
        }

        if(continuous)
            config |= 0x10;

        await this.write_config(config);
    }
    async get_voltage(gain, samples = 1)
    {
        if(isNaN(samples) || samples < 1)
            throw new Error("Invalid sample count");

        let config = await this.read_config();
        let resolution = ((config & 0x0C) >> 1) + 12;

        config &= 0x7C;

        if(typeof(gain) === "number")
        {
            switch(gain)
            {
                case 1:
                    config |= 0x00;
                break;
                case 2:
                    config |= 0x01;
                break;
                case 4:
                    config |= 0x02;
                break;
                case 8:
                    config |= 0x03;
                break;
            }
        }

        this.write_config(config);

        let accum = 0;

        for(let i = 0; i < samples; i++)
        {
            if(!(config & 0x10))
                await this.write_config(config | 0x80);

            let buf;

            do
                buf = await this.read_burst();
            while(buf.readUInt8(3) & 0x80);

            let voltage;

            if(resolution > 16)
            {
                let result = Buffer.alloc(4, (buf.readUInt8(0) & 0x80) ? 0xFF : 0x00);

                result.writeUInt8(buf.readUInt8(0), 1);
                result.writeUInt8(buf.readUInt8(1), 2);
                result.writeUInt8(buf.readUInt8(2), 3);

                voltage = result.readInt32BE(0) * 2048 / (1 << (resolution - 1 + (config & 0x03)));
            }
            else
            {
                voltage = buf.readInt16BE(0) * 2048 / (1 << (resolution - 1 + (config & 0x03)));
            }

            accum += voltage;
        }

        accum /= samples;

        return accum * this.scale_factor;
    }
}

module.exports = MCP3421;