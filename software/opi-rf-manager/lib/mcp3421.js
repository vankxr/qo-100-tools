const GPIO = require.main.require("./lib/gpio");

class MCP3421
{
    bus;
    bus_enable_gpio;
    addr;

    constructor(bus, addr, bus_enable_gpio)
    {
        this.bus = bus;
        this.bus_enable_gpio = bus_enable_gpio;

        this.addr = 0x68 | (addr & 0x07);
    }

    async probe()
    {
        const release = await this.bus.mutex.acquire();

        try
        {
            if(this.bus_enable_gpio)
                await this.bus_enable_gpio.set_value(GPIO.HIGH);

            if((await this.bus.scan(this.addr)).indexOf(this.addr) === -1)
                throw new Error("Could not find MCP3421 at address 0x" + this.addr.toString(16));
        }
        finally
        {
            try
            {
                if(this.bus_enable_gpio)
                    await this.bus_enable_gpio.set_value(GPIO.LOW);
            }
            finally
            {
                release();
            }
        }

        return true;
    }

    async read_burst()
    {
        let buf = Buffer.alloc(4, 0);

        let result;
        const release = await this.bus.mutex.acquire();

        try
        {
            if(this.bus_enable_gpio)
                await this.bus_enable_gpio.set_value(GPIO.HIGH);

            result = await this.bus.i2cRead(this.addr, 4, buf);
        }
        finally
        {
            try
            {
                if(this.bus_enable_gpio)
                    await this.bus_enable_gpio.set_value(GPIO.LOW);
            }
            finally
            {
                release();
            }
        }

        if(!result)
            throw new Error("I2C Read failed");

        if(result.bytesRead < 4)
            throw new Error("I2C Read failed, expected " + 4 + " bytes, got " + result.bytesRead);

        return result.buffer;
    }
    async read_config()
    {
        let buf = await this.read_burst();

        return buf.readUInt8(3);
    }
    async write_config(config)
    {
        let buf = Buffer.alloc(1, 0);

        buf.writeUInt8(config, 0);

        const release = await this.bus.mutex.acquire();

        try
        {
            if(this.bus_enable_gpio)
                await this.bus_enable_gpio.set_value(GPIO.HIGH);

            await this.bus.i2cWrite(this.addr, 1, buf);
        }
        finally
        {
            try
            {
                if(this.bus_enable_gpio)
                    await this.bus_enable_gpio.set_value(GPIO.LOW);
            }
            finally
            {
                release();
            }
        }
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

        return accum;
    }
}

module.exports = MCP3421;