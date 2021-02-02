class MCP23017
{
    bus;
    addr;

    constructor(bus, addr)
    {
        this.bus = bus;

        this.addr = 0x20 | (addr & 0x07);
    }

    async probe()
    {
        const release = await this.bus.mutex.acquire();

        try
        {
            if((await this.bus.scan(this.addr)).indexOf(this.addr) === -1)
                throw new Error("Could not find MCP23017 at address 0x" + this.addr.toString(16));
        }
        finally
        {
            release();
        }

        return true;
    }

    async write_register(reg, data)
    {
        let buf = Buffer.alloc(2, 0);

        buf.writeUInt8(reg, 0);
        buf.writeUInt8(data, 1);

        const release = await this.bus.mutex.acquire();

        try
        {
            await this.bus.i2cWrite(this.addr, buf.length, buf);
        }
        finally
        {
            release();
        }
    }
    async read_register(reg)
    {
        let buf = Buffer.alloc(1, 0);

        buf.writeUInt8(reg, 0);

        let result;
        const release = await this.bus.mutex.acquire();

        try
        {
            await this.bus.i2cWrite(this.addr, buf.length, buf);

            result = await this.bus.i2cRead(this.addr, 1, buf);
        }
        finally
        {
            release();
        }

        if(!result)
            throw new Error("I2C Read failed");

        if(result.bytesRead < 1)
            throw new Error("I2C Read failed, expected " + 1 + " bytes, got " + result.bytesRead);

        return result.buffer.readUInt8(0);
    }

    // TODO
}

module.exports = MCP23017;