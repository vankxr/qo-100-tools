const { I2C, I2CDevice } = require.main.require("./lib/i2c");

class BME280 extends I2CDevice
{
    static sea_hpa = 1013.25;

    calibration = {
        read: false,
        T1: undefined,
        T2: undefined,
        T3: undefined,
        P1: undefined,
        P2: undefined,
        P3: undefined,
        P4: undefined,
        P5: undefined,
        P6: undefined,
        P7: undefined,
        P8: undefined,
        P9: undefined,
        H1: undefined,
        H2: undefined,
        H3: undefined,
        H4: undefined,
        H5: undefined,
        H6: undefined
    };

    static set_sea_pressure(sea_hpa)
    {
        BME280.sea_hpa = sea_hpa;
    }

    constructor(bus, addr, bus_enable_gpio)
    {
        if(bus instanceof I2CDevice)
            super(bus.bus, bus.addr, bus.bus_enable_gpio);
        else
            super(bus, 0x76 | (addr & 0x01), bus_enable_gpio);

        this.calibration.read = false;
    }

    async probe()
    {
        await super.probe();

        let id = await this.get_chip_id();

        if(id !== 0x60)
            throw new Error("Unknown BME280 chip ID 0x" + id.toString(16));
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

    async read_calibration()
    {
        let buf = await this.read(0x88, 26);

        this.calibration.T1 = buf.readUInt16LE(0);
        this.calibration.T2 = buf.readInt16LE(2);
        this.calibration.T3 = buf.readInt16LE(4);

        this.calibration.P1 = buf.readUInt16LE(6);
        this.calibration.P2 = buf.readInt16LE(8);
        this.calibration.P3 = buf.readInt16LE(10);
        this.calibration.P4 = buf.readInt16LE(12);
        this.calibration.P5 = buf.readInt16LE(14);
        this.calibration.P6 = buf.readInt16LE(16);
        this.calibration.P7 = buf.readInt16LE(18);
        this.calibration.P8 = buf.readInt16LE(20);
        this.calibration.P9 = buf.readInt16LE(22);

        this.calibration.H1 = buf.readUInt8(25);

        buf = await this.read(0xE1, 7);

        this.calibration.H2 = buf.readInt16LE(0);
        this.calibration.H3 = buf.readUInt8(2);
        this.calibration.H4 = (buf.readUInt8(3) << 4) | (buf.readUInt8(4) & 0xF);
        this.calibration.H5 = (buf.readUInt8(5) << 4) | (buf.readUInt8(4) >> 4);
        this.calibration.H6 = buf.readInt8(6);

        this.calibration.read = true;
    }

    async config(os, filter, standby)
    {
        if(typeof(os) !== "object")
            return;

        let ctrl_hum = 0x00;
        let ctrl_meas = 0x00;
        let config = 0x00;

        switch(standby)
        {
            case 0.5:
                config |= 0x00;
            break;
            case 62.5:
                config |= 0x20;
            break;
            case 125:
                config |= 0x40;
            break;
            case 250:
                config |= 0x60;
            break;
            case 500:
                config |= 0x80;
            break;
            case 1000:
                config |= 0xA0;
            break;
            case 10:
                config |= 0xC0;
            break;
            case 20:
                config |= 0xE0;
            break;
        }

        switch(filter)
        {
            case 0:
                config |= 0x00;
            break;
            case 2:
                config |= 0x04;
            break;
            case 4:
                config |= 0x08;
            break;
            case 8:
                config |= 0x0C;
            break;
            case 16:
                config |= 0x10;
            break;
        }

        switch(os.temperature)
        {
            case 0:
                ctrl_meas |= 0x00;
            break;
            case 1:
                ctrl_meas |= 0x20;
            break;
            case 2:
                ctrl_meas |= 0x40;
            break;
            case 4:
                ctrl_meas |= 0x60;
            break;
            case 8:
                ctrl_meas |= 0x80;
            break;
            case 16:
                ctrl_meas |= 0xA0;
            break;
        }

        switch(os.humidity)
        {
            case 0:
                ctrl_hum |= 0x00;
            break;
            case 1:
                ctrl_hum |= 0x01;
            break;
            case 2:
                ctrl_hum |= 0x02;
            break;
            case 4:
                ctrl_hum |= 0x03;
            break;
            case 8:
                ctrl_hum |= 0x04;
            break;
            case 16:
                ctrl_hum |= 0x05;
            break;
        }

        switch(os.pressure)
        {
            case 0:
                ctrl_meas |= 0x00;
            break;
            case 1:
                ctrl_meas |= 0x04;
            break;
            case 2:
                ctrl_meas |= 0x08;
            break;
            case 4:
                ctrl_meas |= 0x0C;
            break;
            case 8:
                ctrl_meas |= 0x10;
            break;
            case 16:
                ctrl_meas |= 0x14;
            break;
        }

        await this.write(0xF2, ctrl_hum);
        await this.write(0xF4, ctrl_meas);
        await this.write(0xF5, config);
    }
    async measure(forced)
    {
        let config = await this.read(0xF4);

        if(!forced)
        {
            await this.write(0xF4, (config & 0xFC) | 0x03);

            return;
        }
        else
        {
            await this.write(0xF4, (config & 0xFC) | 0x01);

            //while(((await this.read(0xF4)) & 0x03) != 0x00);
        }

        while((await this.read(0xF3)) & 0x08);
    }
    async sleep()
    {
        let config = await this.read(0xF4);

        await this.write(0xF4, config & 0xFC);
    }
    async get_status()
    {
        return this.read(0xF3);
    }
    async get_chip_id()
    {
        return this.read(0xD0);
    }
    async reset()
    {
        await this.write(0xE0, 0xB6);
    }

    async get_data()
    {
        let buf = await this.read(0xF7, 8);

        // Temperature
        let adc_T = ((buf.readUInt16BE(3) << 8) | buf.readUInt8(5)) >> 4;
        let v1_T = (adc_T / 16384 - this.calibration.T1 / 1024) * this.calibration.T2;
        let v2_T = Math.pow(adc_T / 131072 - this.calibration.T1 / 8192, 2) * this.calibration.T3;
        let T_fine = v1_T + v2_T;
        let T = T_fine / 5120;

        // Humidity
        let adc_H = buf.readUInt16BE(6);
        let v1_H = T_fine - 76800;
        v1_H = (adc_H - (this.calibration.H4 * 64 + this.calibration.H5 / 16384 * v1_H)) * (this.calibration.H2 / 65536 * (1 + this.calibration.H6 / 67108864 * v1_H * (1 + this.calibration.H3 / 67108864 * v1_H)));
        v1_H *= (1 - this.calibration.H1 * v1_H / 524288);

        let H = v1_H;

        if(H > 100)
            H = 100;

        if(H < 0)
            H = 0;

        // Pressure
        let adc_P = ((buf.readUInt16BE(0) << 8) | buf.readUInt8(2)) >> 4;
        let v1_P = T_fine / 2 - 64000;
        let v2_P = Math.pow(v1_P, 2) * this.calibration.P6 / 32768;
        v2_P += v1_P * this.calibration.P5 * 2;
        v2_P = v2_P / 4 + this.calibration.P4 * 65536;
        v1_P = (this.calibration.P3 * Math.pow(v1_P, 2) / 524288 + this.calibration.P2 * v1_P) / 524288;
        v1_P = (1 + v1_P / 32768) * this.calibration.P1;

        let P = 1048576 - adc_P;
        P = (P - v2_P / 4096) * 6250 / v1_P;
        v1_P = this.calibration.P9 * Math.pow(P, 2) / 2147483648;
        v2_P = P * this.calibration.P8 / 32768;
        P += (v1_P + v2_P + this.calibration.P7) / 16;

        return {
            temperature: T,
            humidity: H,
            pressure: P / 100
        };
    }
    async get_temperature()
    {
        return (await this.get_data()).temperature;
    }
    async get_humidity()
    {
        return (await this.get_data()).humidity;
    }
    async get_pressure()
    {
        return (await this.get_data()).pressure;
    }

    async get_altitude()
    {
        let meas = await this.get_data();

        return (1.0 - Math.pow(meas.pressure / BME280.sea_hpa, (1 / 5.2553))) * 145366.45 * 0.3048;
    }
    async get_heat_index()
    {
        let meas = await this.get_data();

        return -8.784695 + 1.61139411 * meas.temperature + 2.33854900 * meas.humidity +
               -0.14611605 * meas.temperature * meas.humidity + -0.01230809 * Math.pow(meas.temperature, 2) +
               -0.01642482 * Math.pow(meas.humidity, 2) + 0.00221173 * Math.pow(meas.temperature, 2) * meas.humidity +
               0.00072546 * meas.temperature * Math.pow(meas.humidity, 2) +
               -0.00000358 * Math.pow(meas.temperature, 2) * Math.pow(meas.humidity, 2);
    }
    async get_dew_point()
    {
        let meas = await this.get_data();

        return 243.04 * (Math.log(meas.humidity / 100) + ((17.625 * meas.temperature) / (243.04 + meas.temperature))) /
               (17.625 - Math.log(meas.humidity / 100) - ((17.625 * meas.temperature) / (243.04 + meas.temperature)));
    }
}

module.exports = BME280;