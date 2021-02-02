class MCP3421
{
    bus;
    addr;

    constructor(bus, addr)
    {
        this.bus = bus;

        this.addr = 0x68 | (addr & 0x07);
    }

    async probe()
    {
        const release = await this.bus.mutex.acquire();

        try
        {
            if((await this.bus.scan(this.addr)).indexOf(this.addr) === -1)
                throw new Error("Could not find MCP3421 at address 0x" + this.addr.toString(16));
        }
        finally
        {
            release();
        }

        return true;
    }


    // TODO
}

module.exports = MCP3421;