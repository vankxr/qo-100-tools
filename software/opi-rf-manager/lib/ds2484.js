const GPIO = require.main.require("./lib/gpio");
const { I2C, I2CDevice } = require.main.require("./lib/i2c");
const { OneWire, OneWireDevice } = require.main.require("./lib/onewire");

class DS2484OneWire extends OneWire
{
    parent;

    constructor(parent)
    {
        super();

        this.parent = parent;
    }

    async reset()
    {
        await this.parent.write(0xB4);

        let status = await this.parent.read(0xF0);

        while(status & 0x01)
            status = await this.parent.read(0xF0);

        return (status & 0x02) === 0x02;
    }
    async triplet(direction)
    {
        await this.parent.write(0x78, !!direction ? 0x80 : 0x00);

        let status = await this.parent.read(0xF0);

        while(status & 0x01)
            status = await this.parent.read(0xF0);

        return (status & 0x60) >> 5;
    }
    async bit(val = 1)
    {
        await this.parent.write(0x87, !!val ? 0x80 : 0x00);

        let status = await this.parent.read(0xF0);

        while(status & 0x01)
            status = await this.parent.read(0xF0);

        return (status & 0x20) === 0x20;
    }
    async write(data)
    {
        if(typeof(data) === "number")
            data = [data];

        if(Array.isArray(data))
            data = Buffer.from(data);

        if(!(data instanceof Buffer))
            throw new Error("Invalid data");

        for(let i = 0; i < data.length; i++)
        {
            await this.parent.write(0xA5, data[i]);

            let status = await this.parent.read(0xF0);

            while(status & 0x01)
                status = await this.parent.read(0xF0);
        }
    }
    async read(count = 1)
    {
        if(typeof(count) !== "number" || isNaN(count) || count < 1)
            throw new Error("Invalid count");

        let data = [];

        for(let i = 0; i < count; i++)
        {
            await this.parent.write(0x96);

            let status = await this.parent.read(0xF0);

            while(status & 0x01)
                status = await this.parent.read(0xF0);

            data.push(await this.parent.read(0xE1));
        }

        return count === 1 ? data[0] : Buffer.from(data);
    }
}

class DS2484 extends I2CDevice
{
    ow_bus = new DS2484OneWire(this);

    constructor(bus, bus_enable_gpio)
    {
        if(bus instanceof I2CDevice)
            super(bus.bus, bus.addr, bus.bus_enable_gpio);
        else
            super(bus, 0x18, bus_enable_gpio);
    }

    async write(cmd, arg)
    {
        if(typeof(cmd) !== "number")
            throw new Error("Invalid command");

        if(typeof(arg) !== "number" && arg !== undefined)
            throw new Error("Invalid argument");

        let buf;

        if(arg === undefined)
            buf = Buffer.from([cmd]);
        else
            buf = Buffer.from([cmd, arg]);

        await super.write(buf);
    }
    async read(reg)
    {
        if(typeof(reg) !== "number")
            throw new Error("Invalid register");

        await this.write(0xE1, reg);

        return super.read();
    }

    async config(active_pullup, power_down, strong_pullup, overdrive_speed)
    {
        await this.write(0xF0);

        let config = 0x00;

        config |= active_pullup ? 0x01 : 0x00;
        config |= power_down ? 0x02 : 0x00;
        config |= strong_pullup ? 0x04 : 0x00;
        config |= overdrive_speed ? 0x08 : 0x00;

        config |= ((~config & 0x0F) << 4);

        await this.write(0xD2, config);

        // TODO: 1-Wire configurations (DS pag. 13)
    }
    get_ow_bus()
    {
        return this.ow_bus;
    }
}

module.exports = DS2484;