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

    calc_checksum(buf)
    {
        if(!(buf instanceof Buffer))
            throw new Error("Invalid buffer");

        let cs = this.addr;

        for(let i = 0; i < buf.length - 1; i++)
            cs += buf.readUInt8(i);

        buf.writeUInt8(((0xFF - cs) + 1) & 0xFF, buf.length - 1);
    }
    check_checksum(buf)
    {
        if(!(buf instanceof Buffer))
            throw new Error("Invalid buffer");

        let cs = this.addr;

        for(let i = 0; i < buf.length; i++)
            cs += buf.readUInt8(i);

        if((((0xFF - cs) + 1) & 0xFF) === 0)
            return true;

        return false;
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

        if(data.length > 256 - 3)
            throw new Error("Data too long");

        let buf = Buffer.alloc(data.length + 3, 0);

        buf.writeUInt8(reg, 0);
        buf.writeUInt8(data.length, 1);
        data.copy(buf, 2, 0);

        this.calc_checksum(buf);

        await super.write(buf);
    }
    async read(reg, count = 1)
    {
        if(typeof(reg) !== "number" || isNaN(reg) || reg < 0 || reg > 255)
            throw new Error("Invalid register");

        if(typeof(count) !== "number" || isNaN(count) || count < 1 || count > 255)
            throw new Error("Invalid count");

        let buf = Buffer.alloc(4, 0);

        buf.writeUInt8(reg, 0);
        buf.writeUInt8(0xFF, 1);
        buf.writeUInt8(count, 2);

        this.calc_checksum(buf);

        await super.write(buf);

        let result = await super.read(count + 1);

        if(!this.check_checksum(result))
            throw new Error("Checksum does not match");

        return count === 1 ? result.readUInt8(0) : result.subarray(0, count);
    }

    async reset()
    {
        await this.write(0x01, 0x80);
    }
    async get_unique_id()
    {
        let buf = await this.read(0xF8, 8);

        return buf.readUInt32LE(4).toString(16).toUpperCase() + "-" + buf.readUInt32LE(0).toString(16).toUpperCase();
    }
    async get_software_version()
    {
        let buf = await this.read(0xF4, 2);

        return buf.readUInt16LE(0);
    }
    async get_reset_state()
    {
        let reg = await this.read(0xE8);

        return (reg & 0xC0) >> 6;
    }
    async get_reset_reason()
    {
        let reg = await this.read(0xE8);

        return reg & 0x3F;
    }
    async get_reset_reason_str()
    {
        switch(await this.get_reset_reason())
        {
            case 0x00: return "None";
            case 0x01: return "Power-on reset";
            case 0x02: return "Analog VDD (AVDD) brown-out reset";
            case 0x03: return "Digital VDD (DVDD) brown-out reset";
            case 0x04: return "Core VDD (DECOUPLE) brown-out reset";
            case 0x05: return "External pin reset";
            case 0x06: return "Core lockup reset";
            case 0x07: return "Software reset";
            case 0x08: return "Watchdog timer reset";
            case 0x09: return "EM4 wakeup reset";
        }
    }
    async get_uptime()
    {
        let buf = await this.read(0xE0, 8);

        return buf.readUInt32LE(0); // TODO: Support 64-bit uptime
    }
    async get_chip_temperatures()
    {
        let buf = await this.read(0xD0, 8);

        return {
            emu: buf.readFloatLE(0),
            adc: buf.readFloatLE(4)
        };
    }
    async get_chip_voltages()
    {
        let buf = await this.read(0xC0, 16);

        return {
            avdd: buf.readFloatLE(0),
            dvdd: buf.readFloatLE(4),
            iovdd: buf.readFloatLE(8),
            core: buf.readFloatLE(12)
        };
    }
    async get_system_voltages()
    {
        let buf = await this.read(0xB0, 4);

        return {
            vin: buf.readFloatLE(0)
        };
    }

    async set_relay_undervoltage_point(voltage)
    {
        if(isNaN(voltage) || voltage < 500 || voltage > 27000)
            throw new Error("Voltage out of bounds");

        let buf = Buffer.alloc(4);

        buf.writeFloatLE(voltage, 0);

        await this.write(0x1C, buf);
    }
    async get_relay_undervoltage_point()
    {
        let buf = await this.read(0x1C, 4);

        return buf.readFloatLE(0);
    }
    async set_relay_undervoltage_status(status)
    {
        if(typeof(status) != "boolean")
            throw new Error("Invalid status");

        if(status)
            await this.write(0x01, (await this.read(0x01)) | 0x01);
        else
            await this.write(0x01, (await this.read(0x01)) & ~0x01);
    }
    async get_relay_undervoltage_status()
    {
        return !!((await this.read(0x01)) & 0x01);
    }
    async was_relay_undervoltage_triggered()
    {
        return !!((await this.read(0x00)) & 0x01);
    }
    async is_relay_undervoltage()
    {
        return !((await this.read(0x00)) & 0x02);
    }

    async set_relay_status(index, status)
    {
        if(isNaN(index) || !Number.isInteger(index) || index < 0 || index > 11)
            throw new Error("Invalid index");

        if(typeof(status) != "boolean")
            throw new Error("Invalid status");

        let buf = Buffer.alloc(2);

        buf.writeUInt16LE(1 << index, 0);

        await this.write(status ? 0x04 : 0x06, buf);
    }
    async get_relay_status(index = -1)
    {
        let buf = await this.read(0x02, 2);
        let rstatus = buf.readUInt16LE(0);

        return (!Number.isInteger(index) || index < 0 || index > 11) ? rstatus : !!(rstatus & (1 << index));
    }
    async set_relay_duty_cycle(index, dc)
    {
        if(isNaN(index) || !Number.isInteger(index) || index < 0 || index > 11)
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
        if(isNaN(index) || !Number.isInteger(index) || index < 0 || index > 11)
            throw new Error("Invalid index");

        return (await this.read(0x10 + index)) / 256;
    }
    async set_relay_voltage(index, voltage)
    {
        if(isNaN(index) || !Number.isInteger(index) || index < 0 || index > 11)
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
        if(isNaN(index) || !Number.isInteger(index) || index < 0 || index > 11)
            throw new Error("Invalid index");

        let vin = (await this.get_system_voltages()).vin;
        let dc = await this.get_relay_duty_cycle(index);

        return vin * dc;
    }
}

module.exports = RelayController;