const { I2C, I2CDevice } = require.main.require("./lib/i2c");

class PABiasController extends I2CDevice
{
    constructor(bus, bus_enable_gpio)
    {
        if(bus instanceof I2CDevice)
            super(bus.bus, bus.addr, bus.bus_enable_gpio);
        else
            super(bus, 0x3B, bus_enable_gpio);
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

    async reset()
    {
        await this.write(0x01, 0x80);
    }
    async get_unique_id()
    {
        let buf = await this.read(0xF8, 8);

        return buf.readUInt32LE(4) + "-" + buf.readUInt32LE(0);
    }
    async get_software_version()
    {
        let buf = await this.read(0xF4, 2);

        return buf.readUInt16LE(0);
    }
    async get_chip_temperatures()
    {
        let buf = await this.read(0xE0, 8);

        return {
            emu: buf.readFloatLE(0),
            adc: buf.readFloatLE(4)
        };
    }
    async get_system_temperatures()
    {
        let buf = await this.read(0xE8, 4);

        return {
            afe: buf.readFloatLE(0)
        };
    }
    async get_chip_voltages()
    {
        let buf = await this.read(0xD0, 16);

        return {
            avdd: buf.readFloatLE(0),
            dvdd: buf.readFloatLE(4),
            iovdd: buf.readFloatLE(8),
            core: buf.readFloatLE(12)
        };
    }
    async get_system_voltages()
    {
        let buf = await this.read(0xC0, 8);

        return {
            vin: buf.readFloatLE(0),
            v5v0: buf.readFloatLE(4)
        };
    }

    async set_tec_voltage(index, voltage)
    {
        if(isNaN(index) || index < 0 || index > 3)
            throw new Error("Invalid index");

        if(isNaN(voltage) || voltage < 0)
            throw new Error("Invalid voltage");

        let buf = Buffer.alloc(4);

        buf.writeFloatLE(voltage, 0);

        await this.write(0x60 + index * 4, buf);
    }
    async get_tec_voltage(index)
    {
        if(isNaN(index) || index < 0 || index > 3)
            throw new Error("Invalid index");

        let buf = await this.read(0x60 + index * 4, 4);

        return buf.readFloatLE(0);
    }

    async get_pa_telemetry(index)
    {
        if(isNaN(index) || index < 0 || index > 1)
            throw new Error("Invalid index");

        let buf = await this.read(0x10 + index * 32, 20);

        return {
            vgg_raw: buf.readFloatLE(0),
            vgg: buf.readFloatLE(4),
            vdd: buf.readFloatLE(8),
            idd: buf.readFloatLE(12),
            temperature: buf.readFloatLE(16)
        };
    }
}

module.exports = PABiasController;