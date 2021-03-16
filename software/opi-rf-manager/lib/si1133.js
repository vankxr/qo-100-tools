const { I2C, I2CDevice } = require.main.require("./lib/i2c");

class SI1133 extends I2CDevice
{
    constructor(bus, addr, bus_enable_gpio)
    {
        if(bus instanceof I2CDevice)
            super(bus.bus, bus.addr, bus.bus_enable_gpio);
        else
            super(bus, addr ? 0x55 : 0x52, bus_enable_gpio);
    }

    async probe()
    {
        await super.probe();

        let id = await this.get_chip_id();

        if(id !== 0x33)
            throw new Error("Unknown SI1133 chip ID 0x" + id.toString(16));
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
        await this.write(0x0B, 0x01);
    }
    async reset_ctr()
    {
        await this.write(0x0B, 0x00);
    }
    async wait_until_sleep()
    {
        for(let i = 0; i < 5; i++)
            if((await this.read(0x11)) & 0x20)
                break;
    }

    async send_command(cmd)
    {
        let ctr = (await this.read(0x11)) & 0x0F;

        for(let i = 0; i < 5; i++)
        {
            await this.wait_until_sleep();

            let lctr = (await this.read(0x11)) & 0x0F;

            if(ctr == lctr)
                break;

            ctr = lctr;
        }

        await this.write(0x0B, cmd);

        for(let i = 0; i < 5; i++)
        {
            await this.wait_until_sleep();

            let lctr = (await this.read(0x11)) & 0x0F;

            if(ctr != lctr)
                break;
        }
    }

    async write_param(param, value)
    {
        await this.write(0x0A, value);
        await this.send_command(0x80 | (param & 0x3F));
    }
    async read_param(param)
    {
        await this.send_command(0x40 | (param & 0x3F));

        return this.read(0x10);
    }

    async config()
    {
        await this.reset();

        await this.write_param(0x01, 0x0F); // CH_LIST

        await this.write_param(0x02, 0x78); // ADCCONFIG0
        await this.write_param(0x03, 0x71); // ADCSENS0
        await this.write_param(0x04, 0x40); // ADCPOST0
        await this.write_param(0x06, 0x4D); // ADCCONFIG1
        await this.write_param(0x07, 0xE1); // ADCSENS1
        await this.write_param(0x08, 0x40); // ADCPOST1
        await this.write_param(0x0A, 0x41); // ADCCONFIG2
        await this.write_param(0x0B, 0xE1); // ADCSENS2
        await this.write_param(0x0C, 0x50); // ADCPOST2
        await this.write_param(0x0E, 0x4D); // ADCCONFIG3
        await this.write_param(0x0F, 0x87); // ADCSENS3
        await this.write_param(0x10, 0x40); // ADCPOST3

        await this.write(0x0F, 0x0F);
    }
    async measure()
    {
        let chan_mask = await this.read_param(0x01);
        chan_mask &= await this.read(0x0F);

        if(!chan_mask)
            throw new Error("No channel enabled");

        await this.send_command(0x11);

        let irq = 0;

        do
            irq |= await this.read(0x12);
        while(irq != chan_mask);
    }
    async get_chip_id()
    {
        return this.read(0x00);
    }
    async get_hw_id()
    {
        return this.read(0x01);
    }
    async get_rev_id()
    {
        return this.read(0x02);
    }

    async get_raw_data()
    {
        let buf = await this.read(0x12, 13);
        let ret = {
            irq: buf.readUInt8(0),
            channels: []
        };

        for(let i = 1; i < buf.length; i += 3)
            ret.channels[(i - 1) / 3] = (buf.readInt16BE(i) << 8) | buf.readUInt8(i + 2);

        return ret;
    }
    async get_data()
    {
        let raw = await this.get_raw_data();
        let uv_raw = raw.channels[0];
        let large_white_h_raw = raw.channels[1];
        let med_ir_raw = raw.channels[2];
        let large_white_l_raw = raw.channels[3];

        return {
            lux: 0, // TODO: https://siliconlabs.github.io/Gecko_SDK_Doc/efm32pg12/html/si1133_8c_source.html
            uv: 0.0104 * (0.00391 * uv_raw * uv_raw + uv_raw)
        };
    }
    async get_lux()
    {
        return (await this.get_data()).lux;
    }
    async get_uv()
    {
        return (await this.get_data()).uv;
    }
}

module.exports = SI1133;