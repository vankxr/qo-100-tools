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
        if((await this.bus.scan(this.addr)).indexOf(this.addr) === -1)
            throw new Error("Could not find MCP3421 at address 0x" + this.addr.toString(16));

        return true;
    }


    // TODO
}

module.exports = MCP3221;