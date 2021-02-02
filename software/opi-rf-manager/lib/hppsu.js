const GPIO = require.main.require("./lib/gpio");

class HPPSU
{
    bus;
    bus_enable_gpio;
    psu_addr;
    ee_addr;

    constructor(bus, addr, bus_enable_gpio)
    {
        this.bus = bus;
        this.bus_enable_gpio = bus_enable_gpio;

        this.psu_addr = 0x58 | (addr & 0x07);
        this.ee_addr = 0x50 | (addr & 0x07);
    }

    async probe()
    {
        const release = await this.bus.mutex.acquire();

        try
        {
            if(this.bus_enable_gpio)
                await this.bus_enable_gpio.set_value(GPIO.HIGH);

            if((await this.bus.scan(this.psu_addr)).indexOf(this.psu_addr) === -1)
                throw new Error("Could not find PSU at address 0x" + this.psu_addr.toString(16));

            if((await this.bus.scan(this.ee_addr)).indexOf(this.ee_addr) === -1)
                throw new Error("Could not find EEPROM at address 0x" + this.ee_addr.toString(16));
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

    calc_checksum(buf)
    {
        let cs = (this.psu_addr << 1);

        for(let i = 0; i < buf.length - 1; i++)
            cs += buf.readUInt8(i);

        buf.writeUInt8(((0xFF - cs) + 1) & 0xFF, buf.length - 1);
    }
    check_checksum(buf)
    {
        let cs = 0;

        for(let i = 0; i < buf.length; i++)
            cs += buf.readUInt8(i);

        if((((0xFF - cs) + 1) & 0xFF) === 0)
            return true;

        return false;
    }

    async write_register(reg, data)
    {
        let buf = Buffer.alloc(4, 0);

        buf.writeUInt8(reg, 0);
        buf.writeUInt16LE(data, 1);

        this.calc_checksum(buf);

        const release = await this.bus.mutex.acquire();

        try
        {
            if(this.bus_enable_gpio)
                await this.bus_enable_gpio.set_value(GPIO.HIGH);

            await this.bus.i2cWrite(this.psu_addr, buf.length, buf);
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
    async read_register(reg)
    {
        let buf = Buffer.alloc(2, 0);

        buf.writeUInt8(reg, 0);

        this.calc_checksum(buf);

        let result;
        const release = await this.bus.mutex.acquire();

        try
        {
            if(this.bus_enable_gpio)
                await this.bus_enable_gpio.set_value(GPIO.HIGH);

            await this.bus.i2cWrite(this.psu_addr, buf.length, buf);

            buf = Buffer.alloc(3, 0);
            result = await this.bus.i2cRead(this.psu_addr, 3, buf);
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

        if(result.bytesRead < 3)
            throw new Error("I2C Read failed, expected " + 3 + " bytes, got " + result.bytesRead);

        if(!this.check_checksum(result.buffer))
            throw new Error("Checksum does not match");

        return result.buffer.readUInt16LE(0);
    }
    async read_mcu_eeprom(mem_addr)
    {
        await this.write_register(0x56, mem_addr);

        return await this.read_register(0x56) >> 8;
    }
    async read_eeprom(mem_addr, count)
    {
        let buf = Buffer.alloc(1, 0);

        buf.writeUInt8(mem_addr, 0);

        let result;
        const release = await this.bus.mutex.acquire();

        try
        {
            if(this.bus_enable_gpio)
                await this.bus_enable_gpio.set_value(GPIO.HIGH);

            await this.bus.i2cWrite(this.ee_addr, buf.length, buf);

            buf = Buffer.alloc(count, 0);
            result = await this.bus.i2cRead(this.ee_addr, count, buf);
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

    async get_spn()
    {
        return (await this.read_eeprom(0x12, 10)).toString("utf8");
    }
    async get_date()
    {
        return (await this.read_eeprom(0x1D, 8)).toString("utf8");
    }
    async get_name()
    {
        return (await this.read_eeprom(0x32, 26)).toString("utf8");
    }
    async get_ct()
    {
        return (await this.read_eeprom(0x5B, 14)).toString("utf8");
    }

    async get_id()
    {
        return await this.read_register(0x00);

        // 0x2000 - HSTNS-PL18 - 750W
        // 0x2100 - HSTNS-PD11 - 1200W
    }

    async get_input_voltage()
    {
        return await this.read_register(0x08) / 32;
    }
    async get_input_current()
    {
        return await this.read_register(0x0A) / 64;
    }
    async get_peak_input_current()
    {
        return await this.read_register(0x34) / 64;
    }
    async clear_peak_input_current()
    {
        await this.write_register(0x34, 0);

        // write 0x0000 to 0x34 - clear peak input current
    }
    async get_input_power()
    {
        let ret = await this.read_register(0x0C);

        if(ret <= 50)
        {
            let in_v = await this.get_input_voltage();
            let in_a = await this.get_input_current();

            ret = in_v * in_a;
        }

        return ret;
    }
    async get_peak_input_power()
    {
        let ret = await this.read_register(0x32);

        if(ret <= 50)
        {
            let in_v = await this.get_input_voltage();
            let in_a = await this.get_peak_input_current();

            ret = in_v * in_a;
        }

        return ret;
    }
    async clear_peak_input_power()
    {
        await this.write_register(0x32, 0);

        // write 0x0000 to 0x32 - clear peak input power
    }
    async get_input_energy()
    {
        let lsb = await this.read_register(0x2C);
        let msb = await this.read_register(0x2E);

        return ((msb << 16) | lsb) / 7200;
    }
    async get_input_undervoltage_threshold()
    {
        return await this.read_register(0x44) / 32;
    }
    async set_input_undervoltage_threshold(voltage)
    {
        if(voltage < 0 || voltage > 65535/32)
            throw new Error("Voltage threshold out of bounds");

        await this.write_register(0x44, voltage * 32);
    }
    async get_input_overvoltage_threshold()
    {
        return await this.read_register(0x46) / 32;
    }
    async set_input_overvoltage_threshold(voltage)
    {
        if(voltage < 0 || voltage > 65535/32)
            throw new Error("Voltage threshold out of bounds");

        await this.write_register(0x46, voltage * 32);
    }

    async get_output_voltage()
    {
        return await this.read_register(0x0E) / 256;
    }
    async get_output_current()
    {
        return await this.read_register(0x10) / 32;
    }
    async get_peak_output_current()
    {
        return await this.read_register(0x36) / 32;
    }
    async clear_peak_output_current()
    {
        await this.write_register(0x36, 0);

        // write 0x0000 to 0x36 - clear peak output current
    }
    async get_output_power()
    {
        let ret = await this.read_register(0x12) * 2;

        if(ret <= 72)
        {
            let out_v = await this.get_output_voltage();
            let out_a = await this.get_output_current();

            ret = out_v * out_a;
        }

        return ret;
    }
    async get_output_undervoltage_threshold()
    {
        return await this.read_register(0x48) / 256;
    }
    async set_output_undervoltage_threshold(voltage)
    {
        if(voltage < 0 || voltage > 65535/256)
            throw new Error("Voltage threshold out of bounds");

        await this.write_register(0x48, voltage * 256);
    }
    async get_output_overvoltage_threshold()
    {
        return await this.read_register(0x4A) / 256;
    }
    async set_output_overvoltage_threshold(voltage)
    {
        if(voltage < 0 || voltage > 65535/256)
            throw new Error("Voltage threshold out of bounds");

        await this.write_register(0x4A, voltage * 256);
    }

    async get_intake_temperature()
    {
        return await this.read_register(0x1A) / 64;
    }
    async get_internal_temperature()
    {
        return await this.read_register(0x1C) / 64;
    }

    async get_fan_speed()
    {
        return await this.read_register(0x1E);
    }
    async get_fan_target_speed()
    {
        return await this.read_register(0x40);
    }
    async set_fan_target_speed(rpm)
    {
        await this.write_register(0x40, rpm);

        // write 0xXXXX to 0x40 - set surprise_more_flags bit 5, probably signal that fan speed has changed
    }

    async get_total_on_time()
    {
        let xlsb = await this.read_mcu_eeprom(0x19);
        let lsb = await this.read_mcu_eeprom(0x1A);
        let msb = await this.read_mcu_eeprom(0x1B);

        return (msb << 16) | (lsb << 8) | xlsb;
    }
    async get_on_time()
    {
        return await this.read_register(0x30) / 2;
    }
    async clear_on_time_and_energy()
    {
        await this.write_register(0x30, 0);

        // write 0x0000 to 0x30 - set i2c_flags1 bit 2 and 7 (requests clear of on_time and energy)
    }

    async get_status_flags()
    {
        return await this.read_register(0x02);

        // Bit 0 - Main output enabled
        // Bit 1 - Seems to indicate whether input voltage is present, something like ready flag (?)
        // Bit 2 - #ENABLE pin status inverted
        // Bit 4 - Is always set but is not mentioned in disassembly (?)
        // Bit 17-16 - 00: Invalid input voltage, 01: xxx V < Input voltage < 108V (100V nominal), 10: 108V < Input voltage < 132V (120V nominal), 11: 179V < Input voltage < 264V
    }

    async main_output_enabled()
    {
        return (await this.get_status_flags() & 0x05) === 0x05;
    }
    async input_present()
    {
        return (await this.get_status_flags() & 0x02) === 0x02;
    }

    /*
    write 0xXXXX to 0x3A - set yet_more_flags bit 5, check written data bit 5 is clear, ...TODO
    write 0xXXXX (not zero) to 0x3C - set yet_more_flags bit 7, set interesting_ctrl_byte_set_cmd3b, bit 6, copy written data to written_by_cmd_3d

    write 0xXXXX to 0x54 - set some_major_flags bit 5

    eeprom at address 0x1F has 0x2E
    */
}

module.exports = HPPSU;