const Colors = require('colors/safe');
const Util = require("util");
const Stream = require("stream");
const LogFileManager = require.main.require("./util/log_file_manager");

class Printer
{
    stream;
    stream_manager;
    next;

    constructor(stream)
    {
        if(stream instanceof Stream.Writable)
            this.stream = stream;
        else if(stream instanceof LogFileManager)
            this.stream_manager = stream;
        else
            throw new Error("Invalid stream");
    }

    get_stream()
    {
        if(this.stream)
            return this.stream;

        return this.stream_manager.get_stream();
    }

    chain(next)
    {
        if(next !== undefined && !(next instanceof Printer))
            throw new Error("Invalid chain link");

        this.next = next;

        return next;
    }

    sprint(time, color, pre, message, ...args)
    {
        let color_fn = undefined;

        if(typeof(color) === "string")
            color_fn = Colors[color.toLowerCase()];

        let out = "";

        if(time)
        {
            let log_time = new Date().toISOString().replace(/T/, ' ').replace(/\..+/, '');
            out += "[" + log_time + "] ";
        }

        if(pre)
            out += pre + " - ";

        out += message;
        out = Util.format(out, ...args);

        if(color_fn)
            out = color_fn(out);

        return out;
    }
    sprintln(time, color, pre, message, ...args)
    {
        return this.sprint(time, color, pre, message, ...args) + "\r\n";
    }

    print(...args)
    {
        this.get_stream().write(this.sprint(false, ...args));

        if(this.next)
            this.next.print(...args);
    }
    println(...args)
    {
        this.get_stream().write(this.sprintln(false, ...args));

        if(this.next)
            this.next.println(...args);
    }
    tprint(...args)
    {
        this.get_stream().write(this.sprint(true, ...args));

        if(this.next)
            this.next.tprint(...args);
    }
    tprintln(...args)
    {
        this.get_stream().write(this.sprintln(true, ...args));

        if(this.next)
            this.next.tprintln(...args);
    }
}

module.exports = Printer;
