const GPIO = require.main.require("./lib/gpio");
const { I2C, I2CDevice } = require.main.require("./lib/i2c");

class HPPSUMCU extends I2CDevice
{
    constructor(bus, addr, bus_enable_gpio)
    {
        if(bus instanceof I2CDevice)
            super(bus.bus, bus.addr, bus.bus_enable_gpio);
        else
            super(bus, 0x58 | (addr & 0x07), bus_enable_gpio);
    }

    calc_checksum(buf)
    {
        if(!(buf instanceof Buffer))
            throw new Error("Invalid buffer");

        let cs = (this.addr << 1);

        for(let i = 0; i < buf.length - 1; i++)
            cs += buf.readUInt8(i);

        buf.writeUInt8(((0xFF - cs) + 1) & 0xFF, buf.length - 1);
    }
    check_checksum(buf)
    {
        if(!(buf instanceof Buffer))
            throw new Error("Invalid buffer");

        let cs = 0;

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

        if(typeof(data) !== "number" || isNaN(data) || data < 0 || data > 65535)
            throw new Error("Invalid data");

        let buf = Buffer.alloc(4, 0);

        buf.writeUInt8(reg, 0);
        buf.writeUInt16LE(data, 1);

        this.calc_checksum(buf);

        await super.write(buf);
    }
    async read(reg)
    {
        if(typeof(reg) !== "number" || isNaN(reg) || reg < 0 || reg > 255)
            throw new Error("Invalid register");

        let buf = Buffer.alloc(2, 0);

        buf.writeUInt8(reg, 0);

        this.calc_checksum(buf);

        await super.write(buf);

        let result = await super.read(3);

        if(!this.check_checksum(result))
            throw new Error("Checksum does not match");

        return result.readUInt16LE(0);
    }
    async read_eeprom(mem_addr)
    {
        await this.write(0x56, mem_addr);

        return (await this.read(0x56)) >> 8;
    }
}
class HPPSUEEPROM extends I2CDevice
{
    constructor(bus, addr, bus_enable_gpio)
    {
        if(bus instanceof I2CDevice)
            super(bus.bus, bus.addr, bus.bus_enable_gpio);
        else
            super(bus, 0x50 | (addr & 0x07), bus_enable_gpio);
    }

    async write(mem_addr, data)
    {
        if(typeof(mem_addr) !== "number" || isNaN(mem_addr) || mem_addr < 0 || mem_addr > 255)
            throw new Error("Invalid memory address");

        if(typeof(data) === "number")
            data = [data];

        if(Array.isArray(data))
            data = Buffer.from(data);

        if(!(data instanceof Buffer))
            throw new Error("Invalid data");

        let buf = Buffer.alloc(data.length + 1, 0);

        buf.writeUInt8(mem_addr, 0);
        data.copy(buf, 1, 0);

        await super.write(buf);
    }
    async read(mem_addr, count)
    {
        if(typeof(mem_addr) !== "number" || isNaN(mem_addr) || mem_addr < 0 || mem_addr > 255)
            throw new Error("Invalid memory address");

        if(typeof(count) !== "number" || isNaN(count) || count < 1)
            throw new Error("Invalid count");

        await super.write(mem_addr);

        return super.read(count);
    }
}

class HPPSU
{
    mcu;
    eeprom;
    present_gpio;
    enable_gpio;

    constructor(bus, addr, bus_enable_gpio, present_gpio, enable_gpio)
    {
        this.mcu = new HPPSUMCU(bus, addr, bus_enable_gpio);
        this.eeprom = new HPPSUEEPROM(bus, addr, bus_enable_gpio);

        this.present_gpio = present_gpio;
        this.enable_gpio = enable_gpio;
    }

    async probe()
    {
        await this.mcu.probe();
        await this.eeprom.probe();
    }

    async get_spn()
    {
        return (await this.eeprom.read(0x12, 10)).toString("utf8");
    }
    async get_date()
    {
        return (await this.eeprom.read(0x1D, 8)).toString("utf8");
    }
    async get_name()
    {
        return (await this.eeprom.read(0x32, 26)).toString("utf8");
    }
    async get_ct()
    {
        return (await this.eeprom.read(0x5B, 14)).toString("utf8");
    }

    async get_id()
    {
        return await this.mcu.read(0x00);

        // 0x2000 - HSTNS-PL18 - 750W
        // 0x2100 - HSTNS-PD11 - 1200W
    }

    async get_input_voltage()
    {
        return await this.mcu.read(0x08) / 32;
    }
    async get_input_current()
    {
        return await this.mcu.read(0x0A) / 64;
    }
    async get_peak_input_current()
    {
        return await this.mcu.read(0x34) / 64;
    }
    async clear_peak_input_current()
    {
        await this.mcu.write(0x34, 0);

        // write 0x0000 to 0x34 - clear peak input current
    }
    async get_input_power()
    {
        let ret = await this.mcu.read(0x0C);

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
        let ret = await this.mcu.read(0x32);

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
        await this.mcu.write(0x32, 0);

        // write 0x0000 to 0x32 - clear peak input power
    }
    async get_input_energy()
    {
        let lsb = await this.mcu.read(0x2C);
        let msb = await this.mcu.read(0x2E);

        return ((msb << 16) | lsb) / 7200;
    }
    async get_input_undervoltage_threshold()
    {
        return await this.mcu.read(0x44) / 32;
    }
    async set_input_undervoltage_threshold(voltage)
    {
        if(isNaN(voltage) || voltage < 0 || voltage > 65535/32)
            throw new Error("Voltage threshold out of bounds");

        await this.mcu.write(0x44, voltage * 32);

        // write 0xXXXX to 0x44 - set byte_DATA_A4 bit 1, signal that input UV threshold has changed
    }
    async get_input_overvoltage_threshold()
    {
        return await this.mcu.read(0x46) / 32;
    }
    async set_input_overvoltage_threshold(voltage)
    {
        if(isNaN(voltage) || voltage < 0 || voltage > 65535/32)
            throw new Error("Voltage threshold out of bounds");

        await this.mcu.write(0x46, voltage * 32);

        // write 0xXXXX to 0x44 - set byte_DATA_A4 bit 2, signal that input OV threshold has changed
    }

    async get_output_voltage()
    {
        return await this.mcu.read(0x0E) / 256;
    }
    async get_output_current()
    {
        return await this.mcu.read(0x10) / 32;
    }
    async get_peak_output_current()
    {
        return await this.mcu.read(0x36) / 32;
    }
    async clear_peak_output_current()
    {
        await this.mcu.write(0x36, 0);

        // write 0x0000 to 0x36 - clear peak output current
    }
    async get_output_power()
    {
        let ret = await this.mcu.read(0x12) * 2;

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
        return await this.mcu.read(0x48) / 256;
    }
    async set_output_undervoltage_threshold(voltage)
    {
        if(isNaN(voltage) || voltage < 0 || voltage > 65535/256)
            throw new Error("Voltage threshold out of bounds");

        await this.mcu.write(0x48, voltage * 256);

        // write 0xXXXX to 0x48 - set byte_DATA_A4 bit 3, signal that output UV threshold has changed
    }
    async get_output_overvoltage_threshold()
    {
        return await this.mcu.read(0x4A) / 256;
    }
    async set_output_overvoltage_threshold(voltage)
    {
        if(isNaN(voltage) || voltage < 0 || voltage > 65535/256)
            throw new Error("Voltage threshold out of bounds");

        await this.mcu.write(0x4A, voltage * 256);

        // write 0xXXXX to 0x4A - set byte_DATA_A4 bit 4, signal that output OV threshold has changed
    }

    async get_intake_temperature()
    {
        return await this.mcu.read(0x1A) / 64;
    }
    async get_internal_temperature()
    {
        return await this.mcu.read(0x1C) / 64;
    }

    async get_fan_speed()
    {
        return await this.mcu.read(0x1E);
    }
    async get_fan_target_speed()
    {
        return await this.mcu.read(0x40);
    }
    async set_fan_target_speed(rpm)
    {
        if(isNaN(rpm) || rpm < 0 || rpm > 17500)
            throw new Error("RPM out of bounds");

        // Only values > 3300 actually work

        await this.mcu.write(0x40, rpm);

        // write 0xXXXX to 0x40 - set surprise_more_flags bit 5, probably signal that fan speed has changed
    }

    async get_total_on_time()
    {
        let xlsb = await this.mcu.read_eeprom(0x19);
        let lsb = await this.mcu.read_eeprom(0x1A);
        let msb = await this.mcu.read_eeprom(0x1B);

        return (msb << 16) | (lsb << 8) | xlsb;
    }
    async get_on_time()
    {
        return await this.mcu.read(0x30) / 2;
    }
    async clear_on_time_and_energy()
    {
        await this.mcu.write(0x30, 0);

        // write 0x0000 to 0x30 - set i2c_flags1 bit 2 and 7 (requests clear of on_time and energy)
    }

    async get_status_flags()
    {
        return await this.mcu.read(0x02);

        // Bit 0 - Main output enabled
        // Bit 1 - Seems to indicate whether input voltage is present, something like ready flag (?)
        // Bit 2 - #ENABLE pin status inverted
        // Bit 4 - Is always set but is not mentioned in disassembly (?)
        // Bit 9-8 - 00: Invalid input voltage, 01: Input voltage < 108V (100V nominal), 10: 108V < Input voltage < 132V (120V/127V nominal), 11: 179V < Input voltage < 264V (230V nominal)
    }

    async is_main_output_enabled()
    {
        return (await this.get_status_flags() & 0x05) === 0x05;
    }
    async is_input_present()
    {
        return (await this.get_status_flags() & 0x02) === 0x02;
    }

    /*
    write 0xXXXX to 0x3A - set yet_more_flags bit 5, check written data bit 5 is clear, ...TODO: check asm label cmd_not_4b
    write 0xXXXX (not zero) to 0x3C - set yet_more_flags bit 7, set interesting_ctrl_byte_set_cmd3b, bit 6, copy written data to written_by_cmd_3d

    write 0xXXXX to 0x50 - writes value to byte_DATA_EE and byte_DATA_EF // Related to temperature, maybe "warning" threshold
    write 0xXXXX to 0x52 - writes value to byte_DATA_A5 and byte_DATA_A6 // Related to temperature, maybe "critical" threshold

    write 0xXXXX to 0x54 - writes value to counter_for_eeprom_logging (LSB) and tag_for_eeprom_logging (MSB), set some_major_flags bit 5

    eeprom at address 0x1F has 0x2E
    */

    async is_present()
    {
        if(!this.present_gpio)
            throw new Error("No present GPIO defined");

        return (await this.present_gpio.get_value()) === GPIO.LOW;
    }
    async set_enable(enable)
    {
        if(!this.enable_gpio)
            throw new Error("No enable GPIO defined");

        if(enable)
            await this.enable_gpio.set_value(GPIO.LOW);

        await this.enable_gpio.set_direction(enable ? GPIO.OUTPUT : GPIO.INPUT);
    }
}

module.exports = HPPSU;