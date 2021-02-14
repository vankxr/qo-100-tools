const Util = require("util");
const Mutex = require('async-mutex').Mutex;

class OneWireDevice
{
    // From https://owfs.org/index_php_page_family-code-list.html
    static Families = {
        0x01: "DS2401",
        0x02: "DS1991",
        0x04: "DS2404",
        0x05: "DS2405",
        0x06: "DS1993",
        0x08: "DS1992",
        0x09: "DS2502",
        0x0B: "DS2505",
        0x0F: "DS2506",
        0x10: "DS18S20",
        0x12: "DS2406",
        0x14: "DS2430A",
        0x16: "DS1954",
        0x18: "DS1963S",
        0x1A: "DS1963L",
        0x1B: "DS2436",
        0x1C: "DS28E04-100",
        0x1D: "DS2423",
        0x1E: "DS2437",
        0x1F: "DS2409",
        0x20: "DS2450",
        0x21: "DS1921",
        0x22: "DS1922",
        0x23: "DS2433",
        0x24: "DS2415",
        0x26: "DS2438",
        0x27: "DS2417",
        0x28: "DS18B20",
        0x29: "DS2408",
        0x2C: "DS2890",
        0x2D: "DS2431",
        0x2E: "DS2770",
        0x30: "DS2760",
        0x31: "DS2720",
        0x32: "DS2780",
        0x33: "DS1961s",
        0x34: "DS2703",
        0x35: "DS2755",
        0x36: "DS2740",
        0x37: "DS1977",
        0x3A: "DS2413",
        0x3B: "DS1825",
        0x3D: "DS2781",
        0x41: "DS1923",
        0x42: "DS28EA00",
        0x43: "DS28EC20",
        0x44: "DS28E10",
        0x51: "DS2751",
        0x7E: "EDS00xx",
        0x81: "USB ID",
        0x82: "DS1425",
        0x89: "DS1982U",
        0x8B: "DS1985U",
        0x8F: "DS1986U",
        0xA0: "mRS001",
        0xA1: "mVM001",
        0xA2: "mCM001",
        0xA6: "mTS017",
        0xB1: "mTC001",
        0xB2: "mAM001",
        0xB3: "mTC002",
        0xEE: "UVI",
        0xEF: "Moisture Hub",
        0xFC: "BAE0910",
        0xFF: "LCD"
    };

    bus;
    rom;
    family;
    uid;

    static check_rom_crc(rom)
    {
        let crc = 0x00;

        for(let i = 0; i < 8; i++)
        {
            crc ^= rom.readUInt8(i);

            for(let j = 0; j < 8; j++)
            {
                if(crc & 0x01)
                    crc = (crc >> 1) ^ 0x8C;
                else
                    crc >>= 1;
            }
        }

        return crc === 0x00;
    }

    constructor(bus, rom)
    {
        if(!(bus instanceof OneWire))
            throw new Error("Invalid OneWire Bus instance");

        if(!(rom instanceof Buffer))
            throw new Error("Invalid OneWire device ROM");

        if(rom.length !== 8)
            throw new Error("Invalid OneWire device ROM");

        if(!OneWireDevice.check_rom_crc(rom))
            throw new Error("Invalid OneWire device ROM");

        this.bus = bus;
        this.rom = rom;
        this.family = rom.readUInt8(0);
        this.uid = BigInt(rom.readUInt32LE(1)) + BigInt(rom.readUInt16LE(5)) * BigInt(Math.pow(2, 32));
    }

    get_family()
    {
        return this.family;
    }
    get_family_name()
    {
        return OneWireDevice.Families[this.family] || "Unknown";
    }
    get_uid()
    {
        return this.uid;
    }

    toString()
    {
        return Util.format("Family: 0x%s (%s), Unique ID: 0x%s", this.family.toString(16), this.get_family_name(), this.uid.toString(16));
    }
}
class OneWire
{
    mutex;

    constructor()
    {
        this.mutex = new Mutex();
    }

    async reset()
    {
        // Compatibility method only

        return 0;
    }
    async triplet(dir)
    {
        // Compatibility method only

        return 0;
    }
    async bit(val = 1)
    {
        // Compatibility method only

        return 0;
    }
    async write(data)
    {
        // Compatibility method only
    }
    async read(count)
    {
        // Compatibility method only

        return count === 1 ? 0x00 : Buffer.from([0x00]);
    }

    async rom_match(rom)
    {
        if(rom instanceof OneWireDevice)
            rom = rom.rom;

        if(!(rom instanceof Buffer))
            throw new Error("Invalid OneWire device ROM");

        if(rom.length !== 8)
            throw new Error("Invalid OneWire device ROM");

        await this.write(0x55);
        await this.write(rom);
    }
    async rom_skip()
    {
        await this.write(0xCC);
    }

    async scan(alarm = false)
    {
        let devices = [];
        let last_mismatch = 0;

        while(true)
        {
            let cur_device = Buffer.alloc(8, 0);
            let last_zero = 0;
            let cur_bit = 1;
            let cur_mask = 1;
            let cur_byte = 0;

            if(!(await this.reset()))
                break; // No devices in the bus

            await this.write(alarm ? 0xEC : 0xF0);

            while(cur_bit < 65)
            {
                let dir = 0;

                if(cur_bit < last_mismatch)
                    dir = devices.length ? !!(pullDeviceID[devices.length - 1].readUInt8(cur_byte) & cur_mask) : 0;
                else
                    dir = (cur_bit == last_mismatch);

                let result = await this.triplet(dir);

                switch(result) // If bit = 0 and !bit = 0 (two devices with different bits)
                {
                    case 0b00:
                        if(!dir)
                            last_zero = cur_bit;
                    break;
                    case 0b01:
                        dir = 1;
                    break;
                    case 0b10:
                        dir = 0;
                    break;
                    case 0b11:
                        return devices; // No devices in the bus (can happen when searching for alarm)
                    default:
                        return devices; // Error ??
                }

                if(dir)
                    cur_device.writeUInt8(cur_device.readUInt8(cur_byte) | cur_mask, cur_byte);
                else
                    cur_device.writeUInt8(cur_device.readUInt8(cur_byte) & ~cur_mask, cur_byte);

                cur_bit++;
                cur_mask <<= 1;

                if(cur_mask === 0x100)
                {
                    cur_mask = 1;
                    cur_byte++;
                }
            }

            devices.push(new OneWireDevice(this, cur_device));

            if(last_zero === 0)
                break;

            last_mismatch = last_zero;
        }

        return devices;
    }
}

module.exports = {
    OneWire,
    OneWireDevice
};