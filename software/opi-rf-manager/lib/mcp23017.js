const GPIO = require.main.require("./lib/gpio");

class MCP23017_GPIO
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
    reg_mask;

    constructor(parent, pin, index)
    {
        this.pin = pin;
        this.index = index;
        this.parent = parent;
        this.reg_mask = pin[1] === "B" ? 0x10 : 0x00;
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
        if(direction !== MCP23017_GPIO.INPUT && direction !== MCP23017_GPIO.OUTPUT)
            throw new Error("Invalid direction");

        let iodir = await this.parent.read_register(0x00 | this.reg_mask);

        if(direction === MCP23017_GPIO.INPUT)
            iodir |= (1 << (this.index % 8));
        else if(direction === MCP23017_GPIO.OUTPUT)
            iodir &= ~(1 << (this.index % 8));

        await this.parent.write_register(0x00 | this.reg_mask, iodir);
    }
    async set_pull(pull)
    {
        if(pull !== MCP23017_GPIO.PULLUP && pull !== MCP23017_GPIO.PULLDOWN && pull !== MCP23017_GPIO.NOPULL)
            throw new Error("Invalid pull type");

        if(pull === MCP23017_GPIO.PULLDOWN)
            throw new Error("Pull-down not supported");

        let gppu = await this.parent.read_register(0x06 | this.reg_mask);

        if(pull === MCP23017_GPIO.PULLUP)
            gppu |= (1 << (this.index % 8));
        else if(pull === MCP23017_GPIO.NOPULL)
            gppu &= ~(1 << (this.index % 8));

        await this.parent.write_register(0x06 | this.reg_mask, gppu);
    }
    async set_value(value)
    {
        if(value !== MCP23017_GPIO.HIGH && value !== MCP23017_GPIO.LOW)
            throw new Error("Invalid value");

        let olat = await this.parent.read_register(0x0A | this.reg_mask);

        if(value === MCP23017_GPIO.HIGH)
            olat |= (1 << (this.index % 8));
        else if(value === MCP23017_GPIO.LOW)
            olat &= ~(1 << (this.index % 8));

        await this.parent.write_register(0x0A | this.reg_mask, olat);
    }
    async get_value()
    {
        let gpio = await this.parent.read_register(0x09 | this.reg_mask);

        if(gpio & (1 << (this.index % 8)))
            return MCP23017_GPIO.HIGH;
        else
            return MCP23017_GPIO.LOW;
    }

    async enable_event_polling(value, interval, callback)
    {
        if(!this.parent.irq_gpio)
            throw new Error("Parent MCP23017 does not have a valid IRQ GPIO configured");

        if(this.event.callback)
            throw new Error("Event polling already enabled, disable first");

        if(value !== MCP23017_GPIO.HIGH && value !== MCP23017_GPIO.LOW && value !== MCP23017_GPIO.ANY)
            throw new Error("Invalid value");

        if(typeof(callback) !== "function")
            throw new Error("Invalid callback");

        if(interval < 0)
            throw new Error("Invalid interval");

        this.event.callback = callback;

        let regs = await this.parent.read_burst(0x02 | this.reg_mask, 3);
        let gpinten_op = await this.parent.read_register(0x02 | (this.reg_mask ^ 0x10)); // GPINTEN register from the other port
        let gpinten = regs.readUInt8(0);
        let defval = regs.readUInt8(1);
        let intcon = regs.readUInt8(2);

        if(value === MCP23017_GPIO.ANY)
        {
            regs.writeUInt8(intcon & ~(1 << (this.index % 8)), 2);
        }
        else
        {
            if(value === MCP23017_GPIO.LOW)
                regs.writeUInt8(defval | (1 << (this.index % 8)), 1);
            else if(value === MCP23017_GPIO.HIGH)
                regs.writeUInt8(defval & ~(1 << (this.index % 8)), 1);

            regs.writeUInt8(intcon | (1 << (this.index % 8)), 2);
        }

        regs.writeUInt8(gpinten | (1 << (this.index % 8)), 0);

        if(gpinten_op === 0x00 && gpinten === 0x00) // No other GPIO has interrupts enabled
            this.parent.irq_gpio.enable_event_polling(GPIO.LOW, interval, this.parent.isr);

        await this.parent.write_burst(0x02 | this.reg_mask, regs);
    }
    async disable_event_polling()
    {
        if(!this.event.callback)
            throw new Error("Event polling not enabled");

        let gpinten = await this.parent.read_register(0x02 | this.reg_mask);
        let gpinten_op = await this.parent.read_register(0x02 | (this.reg_mask ^ 0x10)); // GPINTEN register from the other port

        gpinten &= ~(1 << (this.index % 8));

        await this.parent.write_register(0x02 | this.reg_mask, gpinten);

        if(gpinten_op === 0x00 && gpinten === 0x00) // No other GPIO has interrupts enabled
            this.parent.irq_gpio.disable_event_polling();
    }
}

class MCP23017
{
    bus;
    bus_enable_gpio;
    addr;
    irq_gpio;
    gpios = [
        // PORT A
        new MCP23017_GPIO(this, "PA0", 0),
        new MCP23017_GPIO(this, "PA1", 1),
        new MCP23017_GPIO(this, "PA2", 2),
        new MCP23017_GPIO(this, "PA3", 3),
        new MCP23017_GPIO(this, "PA4", 4),
        new MCP23017_GPIO(this, "PA5", 5),
        new MCP23017_GPIO(this, "PA6", 6),
        new MCP23017_GPIO(this, "PA7", 7),
        // PORT B
        new MCP23017_GPIO(this, "PB0", 32),
        new MCP23017_GPIO(this, "PB1", 33),
        new MCP23017_GPIO(this, "PB2", 34),
        new MCP23017_GPIO(this, "PB3", 35),
        new MCP23017_GPIO(this, "PB4", 36),
        new MCP23017_GPIO(this, "PB5", 37),
        new MCP23017_GPIO(this, "PB6", 38),
        new MCP23017_GPIO(this, "PB7", 39)
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
                throw new Error("Could not find MCP23017 at address 0x" + this.addr.toString(16));
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
        await this.write_register(0x05, 0xC4);

        let regs = [
            0xFF, // IODIR
            0x00, // IPOL
            0x00, // GPINTEN
            0x00, // DEFVAL
            0x00, // INTCON
            0xC4, // IOCON
            0x00, // GPPU
            0x00, // INTF
            0x00, // INTCAP
            0x00, // GPIO
            0x00  // OLAT
        ];

        await this.write_burst(0x00, regs); // PORT A
        await this.write_burst(0x10, regs); // PORT B
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
            let regs_a = await this.read_burst(0x07, 2);
            let intf_a = regs_a.readUInt8(0);
            let intcap_a = regs_a.readUInt8(1);
            let regs_b = await this.read_burst(0x17, 2);
            let intf_b = regs_b.readUInt8(0);
            let intcap_b = regs_b.readUInt8(1);

            for(let i = 0; i < 8; i++)
            {
                if(intf_a & (1 << i))
                {
                    let gpio = this.gpios[i];

                    if(typeof(gpio.event.callback) == "function")
                        gpio.event.callback(null, !!(intcap_a & (1 << i)));
                }

                if(intf_b & (1 << i))
                {
                    let gpio = this.gpios[i + 8];

                    if(typeof(gpio.event.callback) == "function")
                        gpio.event.callback(null, !!(intcap_b & (1 << i)));
                }
            }
        }
        catch (e)
        {

        }
    }
}

module.exports = MCP23017;