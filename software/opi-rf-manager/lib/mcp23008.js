const GPIO = require.main.require("./lib/gpio");

class MCP23008_GPIO
{
    static INPUT = "in";
    static OUTPUT = "out";
    static PULLUP = "up";
    static PULLDOWN = "down";
    static NOPULL = "off";
    static HIGH = "1";
    static LOW = "0";
    static ANY = "X";

    pin;
    index;
    event = {
        callback: null,
    };
    parent;

    constructor(parent, pin, index)
    {
        this.pin = pin;
        this.index = index;
        this.parent = parent;
    }

    async unexport()
    {
        // Compatibility method only
    }
    get_pin_name()
    {
        return this.pin;
    }
    get_pin_index()
    {
        return this.index;
    }

    async set_direction(direction)
    {
        if(direction !== MCP23008_GPIO.INPUT && direction !== MCP23008_GPIO.OUTPUT)
            throw new Error("Invalid direction");

        let iodir = await this.parent.read_register(0x00);

        if(direction === MCP23008_GPIO.INPUT)
            iodir |= (1 << (this.index % 8));
        else if(direction === MCP23008_GPIO.OUTPUT)
            iodir &= ~(1 << (this.index % 8));

        await this.parent.write_register(0x00, iodir);
    }
    async set_pull(pull)
    {
        if(pull !== MCP23008_GPIO.PULLUP && pull !== MCP23008_GPIO.PULLDOWN && pull !== MCP23008_GPIO.NOPULL)
            throw new Error("Invalid pull type");

        if(pull === MCP23008_GPIO.PULLDOWN)
            throw new Error("Pull-down not supported");

        let gppu = await this.parent.read_register(0x06);

        if(pull === MCP23008_GPIO.PULLUP)
            gppu |= (1 << (this.index % 8));
        else if(pull === MCP23008_GPIO.NOPULL)
            gppu &= ~(1 << (this.index % 8));

        await this.parent.write_register(0x06, gppu);
    }
    async set_value(value)
    {
        if(value !== MCP23008_GPIO.HIGH && value !== MCP23008_GPIO.LOW)
            throw new Error("Invalid value");

        let olat = await this.parent.read_register(0x0A);

        if(value === MCP23008_GPIO.HIGH)
            olat |= (1 << (this.index % 8));
        else if(value === MCP23008_GPIO.LOW)
            olat &= ~(1 << (this.index % 8));

        await this.parent.write_register(0x0A, olat);
    }
    async get_value()
    {
        let gpio = await this.parent.read_register(0x09);

        if(gpio & (1 << (this.index % 8)))
            return MCP23008_GPIO.HIGH;
        else
            return MCP23008_GPIO.LOW;
    }

    async enable_event_polling(value, interval, callback)
    {
        if(!this.parent.irq_gpio)
            throw new Error("Parent MCP23008 does not have a valid IRQ GPIO configured");

        if(this.event.callback)
            throw new Error("Event polling already enabled, disable first");

        if(value !== MCP23008_GPIO.HIGH && value !== MCP23008_GPIO.LOW && value !== MCP23008_GPIO.ANY)
            throw new Error("Invalid value");

        if(typeof(callback) !== "function")
            throw new Error("Invalid callback");

        if(interval < 0)
            throw new Error("Invalid interval");

        this.event.callback = callback;

        let regs = await this.parent.read_burst(0x02, 3);
        let gpinten = regs.readUInt8(0);
        let defval = regs.readUInt8(1);
        let intcon = regs.readUInt8(2);

        if(value === MCP23008_GPIO.ANY)
        {
            regs.writeUInt8(intcon & ~(1 << (this.index % 8)), 2);
        }
        else
        {
            if(value === MCP23008_GPIO.LOW)
                regs.writeUInt8(defval | (1 << (this.index % 8)), 1);
            else if(value === MCP23008_GPIO.HIGH)
                regs.writeUInt8(defval & ~(1 << (this.index % 8)), 1);

            regs.writeUInt8(intcon | (1 << (this.index % 8)), 2);
        }

        regs.writeUInt8(gpinten | (1 << (this.index % 8)), 0);

        if(gpinten === 0x00) // No other GPIO has interrupts enabled
            this.parent.irq_gpio.enable_event_polling(GPIO.LOW, interval, this.parent.isr);

        await this.parent.write_burst(0x02, regs);
    }
    async disable_event_polling()
    {
        if(!this.event.callback)
            throw new Error("Event polling not enabled");

        let gpinten = await this.parent.read_register(0x02);

        gpinten &= ~(1 << (this.index % 8));

        await this.parent.write_register(0x02, gpinten);

        if(gpinten === 0x00) // No other GPIO has interrupts enabled
            this.parent.irq_gpio.disable_event_polling();
    }
}

class MCP23008
{
    bus;
    bus_enable_gpio;
    addr;
    irq_gpio;
    gpios = [
        // PORT A
        new MCP23008_GPIO(this, "PA0", 0),
        new MCP23008_GPIO(this, "PA1", 1),
        new MCP23008_GPIO(this, "PA2", 2),
        new MCP23008_GPIO(this, "PA3", 3),
        new MCP23008_GPIO(this, "PA4", 4),
        new MCP23008_GPIO(this, "PA5", 5),
        new MCP23008_GPIO(this, "PA6", 6),
        new MCP23008_GPIO(this, "PA7", 7)
    ];

    constructor(bus, addr, bus_enable_gpio, irq_gpio)
    {
        this.bus = bus;
        this.bus_enable_gpio = bus_enable_gpio;

        this.addr = 0x20 | (addr & 0x07);

        this.irq_gpio = irq_gpio;
    }

    async probe()
    {
        const release = await this.bus.mutex.acquire();

        try
        {
            if(this.bus_enable_gpio)
                await this.bus_enable_gpio.set_value(GPIO.HIGH);

            if((await this.bus.scan(this.addr)).indexOf(this.addr) === -1)
                throw new Error("Could not find MCP23008 at address 0x" + this.addr.toString(16));
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

    async config()
    {
        let regs = [
            0xFF, // IODIR
            0x00, // IPOL
            0x00, // GPINTEN
            0x00, // DEFVAL
            0x00, // INTCON
            0x04, // IOCON
            0x00, // GPPU
            0x00, // INTF
            0x00, // INTCAP
            0x00, // GPIO
            0x00  // OLAT
        ];

        await this.write_burst(0x00, regs); // PORT A
    }
    get_gpio(pin)
    {
        if(typeof(pin) != "string")
            throw new Error("Invalid pin name");

        if(pin[0] != "P")
            throw new Error("Invalid pin name");

        let port_number = pin.charCodeAt(1);

        if(port_number < 65 || port_number > 90)
            throw new Error("Invalid pin name");

        let pin_number = parseInt(pin.substring(2));

        if(pin_number < 0 || pin_number > 31)
            throw new Error("Invalid pin name");

        let gpio_index = (port_number - 65) * 32 + pin_number;
        let gpio_table_index = (port_number - 65) * 8 + pin_number;

        if(gpio_table_index >= this.gpios.length)
            throw new Error("Invalid pin name");

        return this.gpios[gpio_table_index];
    }

    async isr()
    {
        try
        {
            let regs = await this.read_burst(0x07, 2);
            let intf = regs.readUInt8(0);
            let intcap = regs.readUInt8(1);

            for(let i = 0; i < 8; i++)
            {
                if(intf & (1 << i))
                {
                    let gpio = this.gpios[i];

                    if(typeof(gpio.event.callback) == "function")
                        gpio.event.callback(null, !!(intcap & (1 << i)));
                }
            }
        }
        catch (e)
        {

        }
    }
}

module.exports = MCP23008;