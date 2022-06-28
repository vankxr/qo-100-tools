const { I2C, I2CDevice } = require.main.require("./lib/i2c");

class Upconverter extends I2CDevice
{
    static MOD_CW = 0;
    static MOD_QPSK = 1;
    static MOD_8PSK = 2;
    static MOD_16APSK = 3;
    static MOD_32APSK = 4;

    static get_modulation_name(mod)
    {
        if(typeof(mod) !== "number" || isNaN(mod) || !Number.isInteger(mod) || mod < 0 || mod > 4)
            return "Unknown";

        switch(mod)
        {
            case Upconverter.MOD_CW:
                return "CW";
            case Upconverter.MOD_QPSK:
                return "QPSK";
            case Upconverter.MOD_8PSK:
                return "8PSK";
            case Upconverter.MOD_16APSK:
                return "16APSK";
            case Upconverter.MOD_32APSK:
                return "32APSK";
            default:
                return "Unknown";
        }
    }

    constructor(bus, bus_enable_gpio)
    {
        if(bus instanceof I2CDevice)
            super(bus.bus, bus.addr, bus.bus_enable_gpio);
        else
            super(bus, 0x3D, bus_enable_gpio);
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

        return buf.readUInt32LE(4).toString(16).toUpperCase() + "-" + buf.readUInt32LE(0).toString(16).toUpperCase();
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
        let buf = await this.read(0xC0, 8);

        return {
            vin: buf.readFloatLE(0),
            v5v0: buf.readFloatLE(4)
        };
    }
    async get_system_currents()
    {
        let buf = await this.read(0xC8, 4);

        return {
            i5v0: buf.readFloatLE(0)
        };
    }

    async get_rf_power()
    {
        let buf = await this.read(0x38, 4);

        return buf.readFloatLE(0);
    }
    async set_rf_power_modulation(mod)
    {
        if(typeof(mod) !== "number" || isNaN(mod) || !Number.isInteger(mod) || mod < 0 || mod > 4)
            throw new Error("Invalid modulation");

        await this.write(0x31, ((await this.read(0x31)) & ~0x0F) | mod);
    }
    async get_rf_power_modulation()
    {
        return (await this.read(0x31)) & 0x0F;
    }
    async set_low_rf_power_threshold(pwr)
    {
        if(isNaN(pwr) || pwr < -10 || pwr > 40)
            throw new Error("RF Power out of bounds");

        let buf = Buffer.alloc(4);

        buf.writeFloatLE(pwr, 0);

        await this.write(0x34, buf);
    }
    async get_low_rf_power_threshold()
    {
        let buf = await this.read(0x34, 4);

        return buf.readFloatLE(0);
    }
    async set_low_rf_power_status(enable)
    {
        if(enable)
            await this.write(0x31, (await this.read(0x31)) | 0x10);
        else
            await this.write(0x31, (await this.read(0x31)) & ~0x10);
    }
    async get_low_rf_power_status()
    {
        return !!((await this.read(0x31)) & 0x10);
    }
    async was_low_rf_power_triggered()
    {
        return !!((await this.read(0x30)) & 0x01);
    }
    async is_low_rf_power()
    {
        return !((await this.read(0x30)) & 0x02);
    }

    async set_if_attenuation(att)
    {
        if(isNaN(att) || att < 0 || att > 32.75)
            throw new Error("Attenuation out of bounds");

        let buf = Buffer.alloc(4);

        buf.writeFloatLE(att, 0);

        await this.write(0x20, buf);
    }
    async get_if_attenuation()
    {
        let buf = await this.read(0x20, 4);

        return buf.readFloatLE(0);
    }
    async set_rf1_attenuation(att)
    {
        if(isNaN(att) || att < 0 || att > 32.75)
            throw new Error("Attenuation out of bounds");

        let buf = Buffer.alloc(4);

        buf.writeFloatLE(att, 0);

        await this.write(0x24, buf);
    }
    async get_rf1_attenuation()
    {
        let buf = await this.read(0x24, 4);

        return buf.readFloatLE(0);
    }
    async set_rf2_attenuation(att)
    {
        if(isNaN(att) || att < 0 || att > 32.75)
            throw new Error("Attenuation out of bounds");

        let buf = Buffer.alloc(4);

        buf.writeFloatLE(att, 0);

        await this.write(0x28, buf);
    }
    async get_rf2_attenuation()
    {
        let buf = await this.read(0x28, 4);

        return buf.readFloatLE(0);
    }

    async set_lo_frequency(freq)
    {
        if(isNaN(Number(freq)) || freq < 35000000n || freq > 4400000000n)
            throw new Error("Frequency out of bounds");

        let buf = Buffer.alloc(8);

        buf.writeBigUInt64LE(freq, 0);

        await this.write(0x10, buf);
    }
    async get_lo_frequency()
    {
        let buf = await this.read(0x10, 8);

        return buf.readBigUInt64LE(0);
    }
    async get_lo_ref_frequency()
    {
        let buf = await this.read(0x18, 4);

        return buf.readUInt32LE(0);
    }
    async get_lo_pfd_frequency()
    {
        let buf = await this.read(0x1C, 4);

        return buf.readUInt32LE(0);
    }
    async is_lo_pll_locked()
    {
        return !!((await this.read(0x00)) & 0x10);
    }
    async set_lo_pll_muted(muted)
    {
        if(muted)
            await this.write(0x01, (await this.read(0x01)) | 0x01);
        else
            await this.write(0x01, (await this.read(0x01)) & ~0x01);
    }
    async is_lo_pll_muted()
    {
        return !!((await this.read(0x00)) & 0x01);
    }

    async set_mixer_status(status)
    {
        if(typeof(status) != "boolean")
            throw new Error("Invalid status");

        if(status)
            await this.write(0x01, (await this.read(0x01)) | 0x02);
        else
            await this.write(0x01, (await this.read(0x01)) & ~0x02);
    }
    async get_mixer_status()
    {
        return !!((await this.read(0x00)) & 0x02);
    }

    async set_pa_stg1_2_status(status)
    {
        if(typeof(status) != "boolean")
            throw new Error("Invalid status");

        if(status)
            await this.write(0x01, (await this.read(0x01)) | 0x04);
        else
            await this.write(0x01, (await this.read(0x01)) & ~0x04);
    }
    async get_pa_stg1_2_status()
    {
        return !!((await this.read(0x00)) & 0x04);
    }

    async set_pa_stg3_status(status)
    {
        if(typeof(status) != "boolean")
            throw new Error("Invalid status");

        if(status)
            await this.write(0x01, (await this.read(0x01)) | 0x08);
        else
            await this.write(0x01, (await this.read(0x01)) & ~0x08);
    }
    async get_pa_stg3_status()
    {
        return !!((await this.read(0x00)) & 0x08);
    }
}

module.exports = Upconverter;