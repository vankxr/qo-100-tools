class MCP3221
{
    bus;
    addr;

    constructor(bus, addr)
    {
        this.bus = bus;

        this.addr = 0x48 | (addr & 0x07);
    }

    async probe()
    {
        const release = await this.bus.mutex.acquire();

        try
        {
            if((await this.bus.scan(this.addr)).indexOf(this.addr) === -1)
                throw new Error("Could not find MCP3221 at address 0x" + this.addr.toString(16));
        }
        finally
        {
            release();
        }

        return true;
    }

    async read_voltage(samples = 1)
    {
        let accum = 0;
        let buf = Buffer.alloc(2, 0);

        const release = await this.bus.mutex.acquire();

        try
        {
            for(let i = 0; i < samples; i++)
            {
                let result = await this.bus.i2cRead(this.addr, 2, buf);

                if(!result)
                    throw new Error("I2C Read failed");

                if(result.bytesRead < 2)
                    throw new Error("I2C Read failed, expected " + 2 + " bytes, got " + result.bytesRead);

                let voltage = 2500 * result.buffer.readUInt16BE(0) / 4096; // mV

                accum += voltage;
            }
        }
        finally
        {
            release();
        }

        accum /= samples;

        return accum;
    }
}

module.exports = MCP3221;