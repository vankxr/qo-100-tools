const GPIO = require.main.require("./lib/gpio");

class MCP3221
{
    bus;
    bus_enable_gpio;
    addr;

    constructor(bus, addr, bus_enable_gpio)
    {
        this.bus = bus;
        this.bus_enable_gpio = bus_enable_gpio;

        this.addr = 0x48 | (addr & 0x07);
    }

    async probe()
    {
        const release = await this.bus.mutex.acquire();

        try
        {
            if(this.bus_enable_gpio)
                await this.bus_enable_gpio.set_value(GPIO.HIGH);

            if((await this.bus.scan(this.addr)).indexOf(this.addr) === -1)
                throw new Error("Could not find MCP3221 at address 0x" + this.addr.toString(16));
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

    async get_voltage(samples = 1)
    {
        let accum = 0;
        let buf = Buffer.alloc(2, 0);

        const release = await this.bus.mutex.acquire();

        try
        {
            if(this.bus_enable_gpio)
                await this.bus_enable_gpio.set_value(GPIO.HIGH);

            for(let i = 0; i < samples; i++)
            {
                let result = await this.bus.i2cRead(this.addr, 2, buf);

                if(!result)
                    throw new Error("I2C Read failed");

                if(result.bytesRead < 2)
                    throw new Error("I2C Read failed, expected " + 2 + " bytes, got " + result.bytesRead);

                let voltage = 2500 * result.buffer.readUInt16BE(0) / 4096; // mV

                accum += voltage;
            }
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

        accum /= samples;

        return accum;
    }
}

module.exports = MCP3221;