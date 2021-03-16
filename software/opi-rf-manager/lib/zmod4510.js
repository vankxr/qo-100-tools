const { I2C, I2CDevice } = require.main.require("./lib/i2c");
const delay = require.main.require("./util/delay");

class ZMOD4510 extends I2CDevice
{
    static meas_conf = {
        start: 0x80,
        h: { addr: 0x40, len: 10 },
        d: { addr: 0x50, len: 6, data: [0x20, 0x05, 0xA0, 0x18, 0xC0, 0x1C] },
        m: { addr: 0x60, len: 1, data: [0x03] },
        s: { addr: 0x68, len: 30, data: [0x00, 0x00, 0x00, 0x08, 0x00, 0x10, 0x00, 0x01, 0x00, 0x09, 0x00, 0x11, 0x00, 0x02, 0x00, 0x0A, 0x00, 0x12, 0x00, 0x03, 0x00, 0x0B, 0x00, 0x13, 0x00, 0x04, 0x00, 0x0C, 0x80, 0x14] },
        r: { addr: 0x97, len: 30 }
    };
    static init_conf = {
        start: 0x80,
        h: { addr: 0x40, len: 2 },
        d: { addr: 0x50, len: 2, data: [0x00, 0xA4] },
        m: { addr: 0x60, len: 2, data: [0xC3, 0xE3] },
        s: { addr: 0x68, len: 4, data: [0x00, 0x00, 0x80, 0x40] },
        r: { addr: 0x97, len: 4 }
    };

    mox_er;
    mox_lr;
    conf;

    constructor(bus, bus_enable_gpio)
    {
        if(bus instanceof I2CDevice)
            super(bus.bus, bus.addr, bus.bus_enable_gpio);
        else
            super(bus, 0x33, bus_enable_gpio);
    }

    async probe()
    {
        await super.probe();

        let pid = await this.get_pid();

        if(pid !== 0x6320)
            throw new Error("Unknown ZMOD4510 PID 0x" + pid.toString(16));
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

    calc_factor(factor)
    {
        let hspf = (-(this.conf[2] * 256.0 + this.conf[3]) * ((this.conf[4] + 640.0) * (this.conf[5] + factor) - 512000.0)) / 12288000.0;

        if(hspf < 0 || hspf > 4096)
            throw new Error("Factor out of range");

        hspf = Math.floor(hspf);

        return Buffer.from([hspf >> 8, hspf & 0xFF]);
    }

    async config()
    {
        // Read info
        let i;

        for(i = 0; i < 1000; i++)
        {
            this.write(0x93, 0x00);

            await delay(200);

            if(!((await this.get_status()) & 0x80))
                break;
        }

        if(i >= 1000)
            throw new Error("Timed out waiting for sensor");

        this.conf = await this.read(0x20, 6);

        // Init sensor
        await this.read(0xB7);
        await this.write(ZMOD4510.init_conf.h.addr, this.calc_factor(80));
        await this.write(ZMOD4510.init_conf.d.addr, ZMOD4510.init_conf.d.data);
        await this.write(ZMOD4510.init_conf.m.addr, ZMOD4510.init_conf.m.data);
        await this.write(ZMOD4510.init_conf.s.addr, ZMOD4510.init_conf.s.data);
        await this.write(0x93, ZMOD4510.init_conf.start);

        while((await this.get_status()) & 0x80);

        let buf = await this.read(ZMOD4510.init_conf.r.addr, ZMOD4510.init_conf.r.len);

        this.mox_lr = buf.readUInt16BE(0);
        this.mox_er = buf.readUInt16BE(2);

        let status = await this.read(0xB7);

        if(status & 0x40)
            throw new Error("Access conflict");

        if(status & 0x80)
            throw new Error("POR event");

        // Init measurement
        await this.read(0xB7);

        await this.write(ZMOD4510.meas_conf.h.addr, Buffer.concat([this.calc_factor(-440), this.calc_factor(-490), this.calc_factor(-540), this.calc_factor(-590), this.calc_factor(-640)]));
        await this.write(ZMOD4510.meas_conf.d.addr, ZMOD4510.meas_conf.d.data);
        await this.write(ZMOD4510.meas_conf.m.addr, ZMOD4510.meas_conf.m.data);
        await this.write(ZMOD4510.meas_conf.s.addr, ZMOD4510.meas_conf.s.data);
    }
    async measure(polling_interval = 5000)
    {
        await this.write(0x93, ZMOD4510.meas_conf.start);

        while((await this.get_status()) & 0x80)
            await delay(polling_interval);
    }
    async get_status()
    {
        return this.read(0x94);
    }
    async get_general_purpose()
    {
        return this.read(0x26, 9);
    }
    async get_pid()
    {
        let buf = await this.read(0x00, 2);

        return buf.readUInt16BE(0);
    }

    async get_adc_results()
    {
        let buf = await this.read(ZMOD4510.meas_conf.r.addr, ZMOD4510.meas_conf.r.len);
        let status = await this.read(0xB7);

        if(status & 0x40)
            throw new Error("Access conflict");

        if(status & 0x80)
            throw new Error("POR event");

        return buf;
    }
    async get_rmox()
    {
        let adc_results = await this.get_adc_results();

        if(!adc_results || !adc_results.length || adc_results.length != ZMOD4510.meas_conf.r.len)
            throw new Error("Invalid ADC results");

        let rmox = [];

        for(let i = 0; i < ZMOD4510.meas_conf.r.len; i += 2)
        {
            let adc_value = adc_results.readUInt16BE(i);
            let cur_rmox;

            if(adc_value - this.rmox_lr < 0)
                cur_rmox = 1e-3;
            else if(this.mox_er - adc_value < 0)
                cur_rmox = 1e12;
            else
                cur_rmox = this.conf[0] * 1e3 * (adc_value - this.mox_lr) / (this.mox_er - adc_value);

            if(cur_rmox > 1e12)
                cur_rmox = 1e12;

            rmox.push(cur_rmox);
        }

        return rmox;
    }
}

module.exports = ZMOD4510;