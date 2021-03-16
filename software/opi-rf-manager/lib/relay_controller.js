const { I2C, I2CDevice } = require.main.require("./lib/i2c");

class RelayController extends I2CDevice
{
    constructor(bus, bus_enable_gpio)
    {
        if(bus instanceof I2CDevice)
            super(bus.bus, bus.addr, bus.bus_enable_gpio);
        else
            super(bus, 0x3A, bus_enable_gpio);
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
        let buf = await this.read(0xC0, 4);

        return {
            vin: buf.readFloatLE(0)
        };
    }

    async set_relay_undervoltage_protection(enable, voltage)
    {
        if(isNaN(voltage) || voltage < 500 || voltage > 27000)
            throw new Error("Voltage out of bounds");

        if(!enable)
            await this.write(0x01, (await this.read(0x01)) & ~0x01);

        let buf = Buffer.alloc(4);

        buf.writeFloatLE(voltage, 0);

        await this.write(0x1C, buf);

        if(enable)
            await this.write(0x01, (await this.read(0x01)) | 0x01);

        buf = await this.read(0x1C, 4);

        return buf.readFloatLE(0);
    }
    async was_undervoltage_protection_triggered()
    {
        return !!((await this.read(0x00)) & 0x01);
    }
    async is_undervoltage()
    {
        return !((await this.read(0x00)) & 0x02);
    }

    async set_relay_status(index, status)
    {
        if(isNaN(index) || index < 0 || index > 11)
            throw new Error("Invalid index");

        if(typeof(status) != "boolean")
            throw new Error("Invalid status");

        let buf = Buffer.alloc(4);

        buf.writeUInt16LE(1 << index, 0);

        await this.write(status ? 0x04 : 0x06, buf);
    }
    async get_relay_status(index = -1)
    {
        let buf = await this.read(0x02, 2);
        let rstatus = buf.readUInt16LE(0);

        return (index < 0 || index > 11) ? rstatus : !!(rstatus & (1 << index));
    }
    async set_relay_duty_cycle(index, dc)
    {
        if(isNaN(index) || index < 0 || index > 11)
            throw new Error("Invalid index");

        if(isNaN(dc) || dc < 0 || dc > 1)
            throw new Error("Invalid duty cycle");

        dc *= 256;

        if(dc > 255)
            dc = 255;

        await this.write(0x10 + index, dc);
    }
    async get_relay_duty_cycle(index)
    {
        if(isNaN(index) || index < 0 || index > 11)
            throw new Error("Invalid index");

        return (await this.read(0x10 + index)) / 256;
    }
    async set_relay_voltage(index, voltage)
    {
        if(isNaN(index) || index < 0 || index > 11)
            throw new Error("Invalid index");

        if(isNaN(voltage) || voltage < 0)
            throw new Error("Invalid voltage");

        let vin = (await this.get_system_voltages()).vin;
        let dc = voltage / vin;

        if(dc > 1)
            dc = 1;

        await this.set_relay_duty_cycle(index, dc);
    }
    async get_relay_voltage(index)
    {
        if(isNaN(index) || index < 0 || index > 11)
            throw new Error("Invalid index");

        let vin = (await this.get_system_voltages()).vin;
        let dc = await this.get_relay_duty_cycle(index);

        return vin * dc;
    }
}

module.exports = RelayController;