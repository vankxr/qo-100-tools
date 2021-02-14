const MCP3421 = require.main.require("./lib/mcp3421");

class LTC5597
{
    adc;
    offset;
    calibration = {};
    current_calibration = [];

    constructor(bus, addr, bus_enable_gpio)
    {
        this.adc = new MCP3421(bus, addr, bus_enable_gpio);
        this.offset = 0;
    }

    async probe()
    {
        await this.adc.probe();
    }

    set_offset(offset)
    {
        this.offset = offset;
    }
    get_offset()
    {
        return this.offset;
    }

    async config(resolution, continuous)
    {
        await this.adc.config(resolution, continuous);
    }

    load_calibration(calibration)
    {
        if(typeof(calibration) !== "object")
            return false;

        if(!calibration.pdata || !Array.isArray(calibration.pdata) || !calibration.pdata.length)
            return false;

        for(const ppoint of calibration.pdata)
        {
            if(typeof(ppoint) !== "number")
                return false;
        }

        if(!calibration.fdata || !Array.isArray(calibration.fdata) || !calibration.fdata.length)
            return false;

        for(const fpoint of calibration.fdata)
        {
            if(typeof(fpoint) !== "object")
                return false;

            if(typeof(fpoint.frequency) !== "number")
                return false;

            if(!fpoint.vdata || !Array.isArray(fpoint.vdata) || fpoint.vdata.length !== calibration.pdata.length)
                return false;

            for(const vpoint of fpoint.vdata)
            {
                if(typeof(vpoint) !== "number")
                    return false;
            }
        }

        this.calibration = calibration;

        return true;
    }
    set_frequency(frequency)
    {
        if(!this.calibration)
            throw new Error("No calibration loaded");

        if(this.calibration.fdata.length == 1)
        {
            this.current_calibration = this.calibration.fdata[0].vdata;

            return true;
        }

        this.current_calibration = [];

        let f0data;
        let f1data;

        for(let i = 1; i < this.calibration.fdata.length; i++)
        {
            f0data = this.calibration.fdata[i - 1];
            f1data = this.calibration.fdata[i];

            if(f0data.frequency <= frequency && f1data.frequency > frequency)
                break;
        }

        let df = f1data.frequency - f0data.frequency;

        for(let i = 0; i < this.calibration.pdata.length; i++)
        {
            let f0v = f0data.vdata[i];
            let f1v = f1data.vdata[i];

            let dv = f1v - f0v;
            let slope = dv / df;
            let interp = f0v + (frequency - f0data.frequency) * slope;

            this.current_calibration.push(interp);
        }
    }

    async get_power_level(gain, samples = 1)
    {
        if(this.current_calibration.length < 2)
            throw new Error("No current calibration loaded");

        let voltage = await this.adc.get_voltage(gain, samples);

        let v0;
        let p0;
        let v1;
        let p1;

        for(let i = 1; i < this.current_calibration.length; i++)
        {
            v0 = this.current_calibration[i - 1];
            p0 = this.calibration.pdata[i - 1];
            v1 = this.current_calibration[i];
            p1 = this.calibration.pdata[i];

            if(v0 <= voltage && v1 > voltage)
                break;
        }

        let dv = v1 - v0;
        let dp = p1 - p0;
        let slope = dp / dv;
        let interp = p0 + (voltage - v0) * slope;

        return interp + this.offset;
    }
}

module.exports = LTC5597;