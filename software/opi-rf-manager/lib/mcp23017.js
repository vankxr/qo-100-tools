const GPIO = require.main.require("./lib/gpio");
const { I2C, I2CDevice } = require.main.require("./lib/i2c");

class MCP23017GPIO extends GPIO
{
    event = {
        callback: null,
    };
    parent;
    reg_mask;

    static async export(pin)
    {
        // Compatibility method only

        throw new Error("MCP23017 GPIOs cannot be exported");
    }

    constructor(parent, pin, index)
    {
        super(pin, index);

        this.event = {
            callback: null,
            args: []
        };
        this.parent = parent;
        this.reg_mask = pin[1] === "B" ? 0x10 : 0x00;
    }

    async unexport()
    {
        // Compatibility method only

        throw new Error("MCP23017 GPIOs cannot be unexported");
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
        if(direction !== GPIO.INPUT && direction !== GPIO.OUTPUT)
            throw new Error("Invalid direction");

        let iodir = await this.parent.read(0x00 | this.reg_mask);

        if(direction === GPIO.INPUT)
            iodir |= (1 << (this.index % 8));
        else if(direction === GPIO.OUTPUT)
            iodir &= ~(1 << (this.index % 8));

        await this.parent.write(0x00 | this.reg_mask, iodir);
    }
    async set_pull(pull)
    {
        if(pull !== GPIO.PULLUP && pull !== GPIO.PULLDOWN && pull !== GPIO.NOPULL)
            throw new Error("Invalid pull type");

        if(pull === GPIO.PULLDOWN)
            throw new Error("Pull-down not supported");

        let gppu = await this.parent.read(0x06 | this.reg_mask);

        if(pull === GPIO.PULLUP)
            gppu |= (1 << (this.index % 8));
        else if(pull === GPIO.NOPULL)
            gppu &= ~(1 << (this.index % 8));

        await this.parent.write(0x06 | this.reg_mask, gppu);
    }
    async set_value(value)
    {
        if(value !== GPIO.HIGH && value !== GPIO.LOW)
            throw new Error("Invalid value");

        let olat = await this.parent.read(0x0A | this.reg_mask);

        if(value === GPIO.HIGH)
            olat |= (1 << (this.index % 8));
        else if(value === GPIO.LOW)
            olat &= ~(1 << (this.index % 8));

        await this.parent.write(0x0A | this.reg_mask, olat);
    }
    async get_value()
    {
        let gpio = await this.parent.read(0x09 | this.reg_mask);

        if(gpio & (1 << (this.index % 8)))
            return GPIO.HIGH;
        else
            return GPIO.LOW;
    }

    async enable_event_polling(value, interval, callback)
    {
        if(!this.parent.irq_gpio)
            throw new Error("Parent MCP23017 does not have a valid IRQ GPIO configured");

        if(this.event.callback)
            throw new Error("Event polling already enabled, disable first");

        if(value !== GPIO.HIGH && value !== GPIO.LOW && value !== GPIO.ANY)
            throw new Error("Invalid value");

        if(typeof(callback) !== "function")
            throw new Error("Invalid callback");

        if(interval < 0)
            throw new Error("Invalid interval");

        this.event.callback = callback.bind(this);
        this.event.args = args;

        let regs = await this.parent.read(0x02 | this.reg_mask, 3);
        let gpinten_op = await this.parent.read(0x02 | (this.reg_mask ^ 0x10)); // GPINTEN register from the other port
        let gpinten = regs.readUInt8(0);
        let defval = regs.readUInt8(1);
        let intcon = regs.readUInt8(2);

        if(value === GPIO.ANY)
        {
            regs.writeUInt8(intcon & ~(1 << (this.index % 8)), 2);
        }
        else
        {
            if(value === GPIO.LOW)
                regs.writeUInt8(defval | (1 << (this.index % 8)), 1);
            else if(value === GPIO.HIGH)
                regs.writeUInt8(defval & ~(1 << (this.index % 8)), 1);

            regs.writeUInt8(intcon | (1 << (this.index % 8)), 2);
        }

        regs.writeUInt8(gpinten | (1 << (this.index % 8)), 0);

        if(gpinten_op === 0x00 && gpinten === 0x00) // No other GPIO has interrupts enabled
            this.parent.irq_gpio.enable_event_polling(GPIO.LOW, interval, MCP23017.isr, this.parent);

        await this.parent.write(0x02 | this.reg_mask, regs);
    }
    async disable_event_polling()
    {
        if(!this.event.callback)
            throw new Error("Event polling not enabled");

        let gpinten = await this.parent.read(0x02 | this.reg_mask);
        let gpinten_op = await this.parent.read(0x02 | (this.reg_mask ^ 0x10)); // GPINTEN register from the other port

        gpinten &= ~(1 << (this.index % 8));

        await this.parent.write(0x02 | this.reg_mask, gpinten);

        this.event.callback = null;
        this.event.args = [];

        if(gpinten_op === 0x00 && gpinten === 0x00) // No other GPIO has interrupts enabled
            this.parent.irq_gpio.disable_event_polling();
    }
}

class MCP23017 extends I2CDevice
{
    irq_gpio;
    gpios = [
        // PORT A
        new MCP23017GPIO(this, "PA0", 0),
        new MCP23017GPIO(this, "PA1", 1),
        new MCP23017GPIO(this, "PA2", 2),
        new MCP23017GPIO(this, "PA3", 3),
        new MCP23017GPIO(this, "PA4", 4),
        new MCP23017GPIO(this, "PA5", 5),
        new MCP23017GPIO(this, "PA6", 6),
        new MCP23017GPIO(this, "PA7", 7),
        // PORT B
        new MCP23017GPIO(this, "PB0", 32),
        new MCP23017GPIO(this, "PB1", 33),
        new MCP23017GPIO(this, "PB2", 34),
        new MCP23017GPIO(this, "PB3", 35),
        new MCP23017GPIO(this, "PB4", 36),
        new MCP23017GPIO(this, "PB5", 37),
        new MCP23017GPIO(this, "PB6", 38),
        new MCP23017GPIO(this, "PB7", 39)
    ];

    static async isr(self)
    {
        if(!(self instanceof MCP23017))
            return;

        try
        {
            let regs_a = await self.read(0x07, 2);
            let intf_a = regs_a.readUInt8(0);
            let intcap_a = regs_a.readUInt8(1);
            let regs_b = await self.read(0x17, 2);
            let intf_b = regs_b.readUInt8(0);
            let intcap_b = regs_b.readUInt8(1);

            for(let i = 0; i < 8; i++)
            {
                if(intf_a & (1 << i))
                {
                    let gpio = self.gpios[i];

                    if(typeof(gpio.event.callback) == "function")
                        gpio.event.callback(null, !!(intcap_a & (1 << i)), ...gpio.event.args);
                }

                if(intf_b & (1 << i))
                {
                    let gpio = self.gpios[i + 8];

                    if(typeof(gpio.event.callback) == "function")
                        gpio.event.callback(null, !!(intcap_b & (1 << i)), ...gpio.event.args);
                }
            }
        }
        catch (e)
        {

        }
    }

    constructor(bus, addr, bus_enable_gpio, irq_gpio)
    {
        if(bus instanceof I2CDevice)
        {
            super(bus.bus, bus.addr, bus.bus_enable_gpio);

            this.irq_gpio = addr;
        }
        else
        {
            super(bus, 0x20 | (addr & 0x07), bus_enable_gpio);

            this.irq_gpio = irq_gpio;
        }
    }

    async write(reg, data)
    {
        if(typeof(reg) !== "number" || isNaN(reg) || reg < 0 || reg > 255)
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
        if(typeof(reg) !== "number" || isNaN(reg) || reg < 0 || reg > 255)
            throw new Error("Invalid register");

        if(typeof(count) !== "number" || isNaN(count) || count < 1)
            throw new Error("Invalid count");

        await super.write(reg);

        return super.read(count);
    }

    async config()
    {
        await this.write(0x05, 0xC4);

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

        await this.write(0x00, regs); // PORT A
        await this.write(0x10, regs); // PORT B
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
}

module.exports = MCP23017;