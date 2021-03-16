const GPIO = require.main.require("./lib/gpio");
const { I2C, I2CDevice } = require.main.require("./lib/i2c");

class MCP23008GPIO extends GPIO
{
    event;
    parent;

    static async export(pin)
    {
        // Compatibility method only

        throw new Error("MCP23008 GPIOs cannot be exported");
    }

    constructor(parent, pin, index)
    {
        super(pin, index);

        this.event = {
            callback: null,
            args: []
        };
        this.parent = parent;
    }

    async unexport()
    {
        // Compatibility method only

        throw new Error("MCP23008 GPIOs cannot be unexported");
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

        let iodir = await this.parent.read(0x00);

        if(direction === GPIO.INPUT)
            iodir |= (1 << (this.index % 8));
        else if(direction === GPIO.OUTPUT)
            iodir &= ~(1 << (this.index % 8));

        await this.parent.write(0x00, iodir);
    }
    async set_pull(pull)
    {
        if(pull !== GPIO.PULLUP && pull !== GPIO.PULLDOWN && pull !== GPIO.NOPULL)
            throw new Error("Invalid pull type");

        if(pull === GPIO.PULLDOWN)
            throw new Error("Pull-down not supported");

        let gppu = await this.parent.read(0x06);

        if(pull === GPIO.PULLUP)
            gppu |= (1 << (this.index % 8));
        else if(pull === GPIO.NOPULL)
            gppu &= ~(1 << (this.index % 8));

        await this.parent.write(0x06, gppu);
    }
    async set_value(value)
    {
        if(value !== GPIO.HIGH && value !== GPIO.LOW)
            throw new Error("Invalid value");

        let olat = await this.parent.read(0x0A);

        if(value === GPIO.HIGH)
            olat |= (1 << (this.index % 8));
        else if(value === GPIO.LOW)
            olat &= ~(1 << (this.index % 8));

        await this.parent.write(0x0A, olat);
    }
    async get_value()
    {
        let gpio = await this.parent.read(0x09);

        if(gpio & (1 << (this.index % 8)))
            return GPIO.HIGH;
        else
            return GPIO.LOW;
    }

    async enable_event_polling(value, interval, callback, ...args)
    {
        if(!this.parent.irq_gpio)
            throw new Error("Parent MCP23008 does not have a valid IRQ GPIO configured");

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

        let regs = await this.parent.read(0x02, 3);
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

        if(gpinten === 0x00) // No other GPIO has interrupts enabled
            this.parent.irq_gpio.enable_event_polling(GPIO.LOW, interval, MCP23008.isr, this.parent);

        await this.parent.write(0x02, regs);
    }
    async disable_event_polling()
    {
        if(!this.event.callback)
            throw new Error("Event polling not enabled");

        let gpinten = await this.parent.read(0x02);

        gpinten &= ~(1 << (this.index % 8));

        await this.parent.write(0x02, gpinten);

        this.event.callback = null;
        this.event.args = [];

        if(gpinten === 0x00) // No other GPIO has interrupts enabled
            this.parent.irq_gpio.disable_event_polling();
    }
}

class MCP23008 extends I2CDevice
{
    irq_gpio;
    gpios = [
        // PORT A
        new MCP23008GPIO(this, "PA0", 0),
        new MCP23008GPIO(this, "PA1", 1),
        new MCP23008GPIO(this, "PA2", 2),
        new MCP23008GPIO(this, "PA3", 3),
        new MCP23008GPIO(this, "PA4", 4),
        new MCP23008GPIO(this, "PA5", 5),
        new MCP23008GPIO(this, "PA6", 6),
        new MCP23008GPIO(this, "PA7", 7)
    ];

    static async isr(self)
    {
        if(!(self instanceof MCP23008))
            return;

        try
        {
            let regs = await self.read(0x07, 2);
            let intf = regs.readUInt8(0);
            let intcap = regs.readUInt8(1);

            for(let i = 0; i < 8; i++)
            {
                if(intf & (1 << i))
                {
                    let gpio = self.gpios[i];

                    if(typeof(gpio.event.callback) == "function")
                        gpio.event.callback(null, !!(intcap & (1 << i)), ...gpio.event.args);
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

        await this.write(0x00, regs); // PORT A
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

module.exports = MCP23008;