const GPIO = require.main.require("./lib/gpio");

class RelayController
{
    bus;
    bus_enable_gpio;
    addr;

    constructor(bus, bus_enable_gpio)
    {
        this.bus = bus;
        this.bus_enable_gpio = bus_enable_gpio;

        this.addr = 0x33;
    }

    async probe()
    {
        const release = await this.bus.mutex.acquire();

        try
        {
            if(this.bus_enable_gpio)
                await this.bus_enable_gpio.set_value(GPIO.HIGH);

            if((await this.bus.scan(this.addr)).indexOf(this.addr) === -1)
                throw new Error("Could not find RelayController at address 0x" + this.addr.toString(16));
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

    async write_burst(reg, data)
    {
        let buf = Buffer.alloc(data.length + 1, 0);

        buf.writeUInt8(reg, 0);

        for(let i = 0; i < data.length; i++)
            buf.writeUInt8(data[i], i + 1);

        const release = await this.bus.mutex.acquire();

        try
        {
            if(this.bus_enable_gpio)
                await this.bus_enable_gpio.set_value(GPIO.HIGH);

            await this.bus.i2cWrite(this.addr, buf.length, buf);
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
    async write_register(reg, data)
    {
        await this.write_burst(reg, [data]);
    }
    async read_burst(reg, count)
    {
        let buf = Buffer.alloc(1, 0);

        buf.writeUInt8(reg, 0);

        let result;
        const release = await this.bus.mutex.acquire();

        try
        {
            if(this.bus_enable_gpio)
                await this.bus_enable_gpio.set_value(GPIO.HIGH);

            await this.bus.i2cWrite(this.addr, buf.length, buf);

            buf = Buffer.alloc(count, 0);
            result = await this.bus.i2cRead(this.addr, count, buf);
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

        if(result.bytesRead < count)
            throw new Error("I2C Read failed, expected " + count + " bytes, got " + result.bytesRead);

        return result.buffer;
    }
    async read_register(reg)
    {
        let buf = await this.read_burst(reg, 1);

        return buf.readUInt8(0);
    }

    async get_unique_id()
    {
        let buf = await this.read_burst(0xF8, 8);

        return buf.readUInt32LE(4) + "-" + buf.readUInt32LE(0);
    }
    async get_software_version()
    {
        let buf = await this.read_burst(0xF4, 2);

        return buf.readUInt16LE(0);
    }
    async get_chip_temperatures()
    {
        let buf = await this.read_burst(0xE0, 8);

        return {
            emu: buf.readFloatLE(0),
            adc: buf.readFloatLE(4)
        };
    }
    async get_chip_voltages()
    {
        let buf = await this.read_burst(0xD0, 16);

        return {
            avdd: buf.readFloatLE(0),
            dvdd: buf.readFloatLE(4),
            iovdd: buf.readFloatLE(8),
            core: buf.readFloatLE(12)
        };
    }
    async get_system_voltages()
    {
        let buf = await this.read_burst(0xC0, 4);

        return {
            vin: buf.readFloatLE(0)
        };
    }

    // TODO
}

module.exports = RelayController;