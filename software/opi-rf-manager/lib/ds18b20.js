const { OneWire, OneWireDevice } = require.main.require("./lib/onewire");

class DS18B20 extends OneWireDevice
{
    static check_scratchpad_crc(data)
    {
        if(!(data instanceof Buffer))
            return false;

        if(data.length !== 9)
            return false;

        let crc = 0x00;

        for(let i = 0; i < 9; i++)
        {
            crc ^= data.readUInt8(i);

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
        if(bus instanceof OneWireDevice)
            super(bus.bus, bus.rom);
        else
            super(bus, rom);
    }

    async read_scratchpad()
    {
        let data;
        const release = await this.bus.mutex.acquire();

        try
        {
            if(!(await this.bus.reset()))
                throw new Error("No device found on bus");

            await this.bus.rom_match(this);
            await this.bus.write(0xBE);

            data = await this.bus.read(9);

            if(!DS18B20.check_scratchpad_crc(data))
                throw new Error("Scratchpad CRC does not match");
        }
        finally
        {
            release();
        }

        return data;
    }
    async write_scratchpad(data)
    {
        if(Array.isArray(data))
            data = Buffer.from(data);

        if(!(data instanceof Buffer))
            throw new Error("Invalid scratchpad data");

        if(data.length !== 9)
            throw new Error("Invalid scratchpad data");

        if(!(await this.bus.reset()))
            throw new Error("No device found on bus");

        const release = await this.bus.mutex.acquire();

        try
        {
            await this.bus.rom_match(this);
            await this.bus.write(0x4E);
            await this.bus.write(data.slice(2, 5));
        }
        finally
        {
            release();
        }
    }
    async store_scratchpad()
    {
        if(!(await this.bus.reset()))
            throw new Error("No device found on bus");

        const release = await this.bus.mutex.acquire();

        try
        {
            await this.bus.rom_match(this);
            await this.bus.write(0x48);
        }
        finally
        {
            release();
        }
    }
    async recall_scratchpad()
    {
        if(!(await this.bus.reset()))
            throw new Error("No device found on bus");

        const release = await this.bus.mutex.acquire();

        try
        {
            await this.bus.rom_match(this);
            await this.bus.write(0xB8);

            while(!(await this.bus.bit()));
        }
        finally
        {
            release();
        }
    }

    async config(resolution = 12)
    {
        let scratch = await this.read_scratchpad();

        switch(resolution)
        {
            case 9:
                scratch.writeUInt8(0x00, 4);
            break;
            case 10:
                scratch.writeUInt8(0x20, 4);
            break;
            case 11:
                scratch.writeUInt8(0x40, 4);
            break;
            case 12:
                scratch.writeUInt8(0x60, 4);
            break;
        }

        await this.write_scratchpad(scratch);
    }
    async measure()
    {
        if(!(await this.bus.reset()))
            throw new Error("No device found on bus");

        const release = await this.bus.mutex.acquire();

        try
        {
            await this.bus.rom_match(this);
            await this.bus.write(0x44);

            while(!(await this.bus.bit()));
        }
        finally
        {
            release();
        }
    }
    async get_temperature()
    {
        let scratch = await this.read_scratchpad();

        let temp = scratch.readInt16LE(0);

        return temp * 0.0625;
    }
    async get_alarm()
    {
        let scratch = await this.read_scratchpad();

        let th = scratch.readInt8(2);
        let tl = scratch.readInt8(3);

        return {
            high: th,
            low: tl
        };
    }
    async set_alarm(high, low)
    {
        if(high > 128 || high < -127)
            throw new Error("Temperature out of range");

        if(low > 128 || low < -127)
            throw new Error("Temperature out of range");

        let scratch = await this.read_scratchpad();

        scratch.writeInt8(high, 2);
        scratch.writeInt8(low, 3);

        await this.write_scratchpad(scratch);
    }
    async is_parasidic_power()
    {
        if(!(await this.bus.reset()))
            throw new Error("No device found on bus");

        let result;
        const release = await this.bus.mutex.acquire();

        try
        {
            await this.bus.rom_match(this);
            await this.bus.write(0xB8);

            result = await this.bus.bit();
        }
        finally
        {
            release();
        }

        return result === 0;
    }
}

module.exports = DS18B20;