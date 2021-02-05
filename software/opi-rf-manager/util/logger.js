const Colors = require('colors/safe');
const Util = require("util");
const FileSystem = require('fs');

class Logger
{
    file_path = "";
    file_prefix = "";

    constructor(file_path, file_prefix)
    {
        if(!file_path)
            throw new Error("Invalid file path");

        this.file_path = file_path;
        this.file_prefix = file_prefix;
    }

    cprint(color, pre, message, ...args)
    {
        let color_fn = undefined;

        if(typeof(color) === "string")
            color_fn = Colors[color.toLowerCase()];

        let out = "";

        if(pre)
            out += pre + " - ";

        out += message;
        out = Util.format(out, ...args);

        if(color_fn)
            out = color_fn(out);

        console.log(out);

        return out;
    }
    ctprint(color, pre, message, ...args)
    {
        let color_fn = undefined;

        if(typeof(color) === "string")
            color_fn = Colors[color.toLowerCase()];

        let log_time = new Date().toISOString().replace(/T/, ' ').replace(/\..+/, '');
        let out = "[" + log_time + "] ";

        if(pre)
            out += pre + " - ";

        out += message;
        out = Util.format(out, ...args);

        if(color_fn)
            out = color_fn(out);

        console.log(out);

        return out;
    }
    cfprint(color, pre, message, ...args)
    {
        let month = new Date().toISOString().match(/[0-9]{4}-[0-9]{2}/g)[0];
        let file_name = this.file_path + "/" + this.file_prefix + month.replace("-", "") + ".log";
        let text = cprint(color, pre, message, ...args);

        FileSystem.appendFileSync(file_name, text);
    }
    ctfprint(color, pre, message, ...args)
    {
        let month = new Date().toISOString().match(/[0-9]{4}-[0-9]{2}/g)[0];
        let file_name = this.file_path + "/" + this.file_prefix + month.replace("-", "") + ".log";
        let text = ctprint(color, pre, message, ...args);

        FileSystem.appendFileSync(file_name, text);
    }
}

module.exports = Logger;
