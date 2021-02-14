const FileSystem = require('fs');

class LogFileManager
{
    static YEARLY = 0;
    static MONTHLY = 1;
    static DAILY = 2;
    static HOURLY = 3;

    stream;
    file_name;
    path;
    prefix;
    suffix;
    mode;
    timer;

    static tick(self)
    {
        if(!(self instanceof LogFileManager))
            return;

        const regex = [
            /[0-9]{4}/g,
            /[0-9]{4}-[0-9]{2}/g,
            /[0-9]{4}-[0-9]{2}-[0-9]{2}/g,
            /[0-9]{4}-[0-9]{2}-[0-9]{2}_[0-9]{2}/g
        ];

        let month = new Date().toISOString().replace("T", "_").match(regex[self.mode])[0];
        let file_name = self.path + "/" + self.prefix + month + self.suffix;

        if(self.file_name != file_name)
        {
            if(self.stream)
                self.stream.end();

            self.file_name = file_name;
            self.stream = FileSystem.createWriteStream(
                self.file_name,
                {
                    flags: "a"
                }
            );
        }
    }

    constructor(mode, path, prefix, suffix)
    {
        if(mode < 0 || mode > 3)
            throw new Error("Invalid mode");

        this.mode = mode;
        this.path = __dirname;
        this.prefix = "";
        this.suffix = ".log";

        if(typeof(path) === "string")
            this.path = path;

        if(typeof(prefix) === "string")
            this.prefix = prefix;

        if(typeof(suffix) === "string")
            this.suffix = suffix;

        LogFileManager.tick(this);
    }

    get_stream()
    {
        return this.stream;
    }

    start(interval = 5000)
    {
        if(!this.timer)
            this.timer = setInterval(LogFileManager.tick, interval, this);
    }
    stop()
    {
        if(this.timer)
            clearInterval(this.timer);
    }

    destroy()
    {
        this.stop();

        if(this.stream)
            this.stream.end();
    }
}

module.exports = LogFileManager;
