const Util = require("util");
const Mutex = require('async-mutex').Mutex;
const I2C_BUS = require("i2c-bus");
const GPIO = require.main.require("./lib/gpio");

class I2CDevice
{
    bus;
    bus_enable_gpio;
    addr;

    constructor(bus, addr, bus_enable_gpio)
    {
        if(!(bus instanceof I2C))
            throw new Error("Invalid I2C Bus instance");

        this.bus = bus;
        this.bus_enable_gpio = bus_enable_gpio;

        this.addr = addr;
    }

    get_address()
    {
        return this.addr;
    }

    async probe()
    {
        const release = await this.bus.mutex.acquire();

        try
        {
            if(this.bus_enable_gpio)
                await this.bus_enable_gpio.set_value(GPIO.HIGH);

            if((await this.bus.scan(this.addr)).indexOf(this.addr) === -1)
                throw new Error("Could not find I2C device at address 0x" + this.addr.toString(16).toUpperCase());
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
    async write(data)
    {
        const release = await this.bus.mutex.acquire();

        try
        {
            if(this.bus_enable_gpio)
                await this.bus_enable_gpio.set_value(GPIO.HIGH);

            await this.bus.write(this.addr, data);
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
    async read(count = 1)
    {
        let result;
        const release = await this.bus.mutex.acquire();

        try
        {
            if(this.bus_enable_gpio)
                await this.bus_enable_gpio.set_value(GPIO.HIGH);

            result = await this.bus.read(this.addr, count);
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

        return result;
    }

    toString()
    {
        return Util.format("Address: 0x%s", this.addr.toString(16));
    }
}
class I2C
{
    bus;
    mutex;

    static async open(num, options)
    {
        return new I2C(await I2C_BUS.openPromisified(num, options));
    }

    constructor(bus)
    {
        this.bus = bus;
        this.mutex = new Mutex();
    }

    async scan(addr)
    {
        return this.bus.scan(addr);
    }
    async write(addr, data)
    {
        if(typeof(data) === "number")
            data = [data];

        if(Array.isArray(data))
            data = Buffer.from(data);

        if(!(data instanceof Buffer))
            throw new Error("Invalid data");

        let result = await this.bus.i2cWrite(addr, data.length, data);

        if(!result)
            throw new Error("I2C Write failed");

        if(result.bytesWritten < data.length)
            throw new Error("I2C Write failed, expected " + data.length + " bytes, got " + result.bytesWritten);
    }
    async read(addr, count = 1)
    {
        if(typeof(count) !== "number" || count < 1)
            throw new Error("Invalid count");

        let result = await this.bus.i2cRead(addr, count, Buffer.alloc(count, 0));

        if(!result)
            throw new Error("I2C Read failed");

        if(result.bytesRead < count)
            throw new Error("I2C Read failed, expected " + count + " bytes, got " + result.bytesRead);

        return count === 1 ? result.buffer.readUInt8(0) : result.buffer;
    }
}

module.exports = {
    I2C,
    I2CDevice
};