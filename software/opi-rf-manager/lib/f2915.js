const { I2C, I2CDevice } = require.main.require("./lib/i2c");

class F2915 extends I2CDevice
{
    constructor(bus, addr, bus_enable_gpio)
    {
        if(bus instanceof I2CDevice)
            super(bus.bus, bus.addr, bus.bus_enable_gpio);
        else
            super(bus, 0x20 | (addr & 0x07), bus_enable_gpio);
    }

    async write(reg, data)
    {
        if(typeof(reg) !== "number" || reg < 0 || reg > 255)
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
        if(typeof(reg) !== "number" || reg < 0 || reg > 255)
            throw new Error("Invalid register");

        if(typeof(count) !== "number" || count < 1)
            throw new Error("Invalid count");

        await super.write(reg);

        return super.read(count);
    }

    async config()
    {
        let regs = [
            0x7F, // IODIR
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

        await this.write(0x00, regs);
    }

    async set_rf_path(path)
    {
        if(path < 0 || path > 5)
            throw new Error("Invalid RF path");

        await this.write(0x00, 0x63 | ((path & 0x07) << 2));
    }
    async get_rf_path()
    {
        let path = ((await this.read(0x09)) & 0x1C) >> 2;

        return path < 6 ? path : 0;
    }

    async is_powered()
    {
        return ((await this.read(0x09)) & 0x01) == 0x01;
    }
    async set_power_enable(enable)
    {
        await this.write(0x0A, enable ? 0x80 : 0x00);
    }
    async get_power_enable(enable)
    {
        return (await this.read(0x09)) & 0x80;
    }
}

module.exports = F2915;