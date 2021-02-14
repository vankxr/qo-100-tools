const Util = require("util");
const FileSystem = require("fs").promises;
const ChildProcess = require("child_process");
const Exec = Util.promisify(ChildProcess.exec);

class GPIO
{
    static INPUT = "in";
    static OUTPUT = "out";
    static PULLUP = "up";
    static PULLDOWN = "down";
    static NOPULL = "off";
    static HIGH = "1";
    static LOW = "0";
    static ANY = "X";

    static root_path = "/sys/class/gpio/";

    static async export(pin)
    {
        if(typeof(pin) != "string")
            throw new Error("Invalid pin name");

        if(pin[0] != "P")
            throw new Error("Invalid pin name");

        let port_number = pin.charCodeAt(1);

        if(port_number < 65 || port_number > 90)
            throw new Error("Invalid pin name");

        let pin_number = parseInt(pin.substring(2));

        if(pin_number < 0 || pin_number > 31)
            throw new Error("Invalid pin name");

        let gpio_index = (port_number - 65) * 32 + pin_number;

        try
        {
            await FileSystem.access(GPIO.root_path + "gpio" + gpio_index);
        }
        catch(e)
        {
            await FileSystem.writeFile(GPIO.root_path + "export", gpio_index.toString());
        }

        return new GPIO(pin, gpio_index);
    }

    pin;
    index;
    path;
    event;
    exported;

    constructor(pin, index)
    {
        this.pin = pin;
        this.index = index;
        this.path = GPIO.root_path + "gpio" + index + "/";
        this.event = {
            timer: null,
            callback: null,
            args: [],
            value: GPIO.LOW,
            last_value: GPIO.LOW
        };
        this.exported = true;
    }

    async unexport()
    {
        if(!this.exported)
            throw new Error("GPIO not exported");

        if(this.event.timer)
            clearInterval(this.event.timer);

        await FileSystem.writeFile(GPIO.root_path + "unexport", this.index.toString());

        this.exported = false;
    }
    get_pin_name()
    {
        return this.pin;
    }
    get_pin_index()
    {
        return this.index;
    }

    async set_direction(direction)
    {
        if(!this.exported)
            throw new Error("GPIO not exported");

        if(direction !== GPIO.INPUT && direction !== GPIO.OUTPUT)
            throw new Error("Invalid direction");

        await FileSystem.writeFile(this.path + "direction", direction);
    }
    async set_pull(pull)
    {
        if(!this.exported)
            throw new Error("GPIO not exported");

        if(pull !== GPIO.PULLUP && pull !== GPIO.PULLDOWN && pull !== GPIO.NOPULL)
            throw new Error("Invalid pull type");

        try
        {
            await FileSystem.access("/usr/bin/hx_soc_gpio_pull_cfg", require("fs").constants.X_OK);
        }
        catch(e)
        {
            throw new Error("H2+/H3 SoC GPIO pull utility executable does not exist or is not executable: " + e);
        }

        try
        {
            const {stdout, stderr} = await Exec(Util.format("/usr/bin/hx_soc_gpio_pull_cfg %d %s", this.index, pull.toUpperCase()));
        }
        catch(e)
        {
            throw new Error("H2+/H3 SoC GPIO pull utility executable failed: " + e);
        }
    }
    async set_value(value)
    {
        if(!this.exported)
            throw new Error("GPIO not exported");

        if(value !== GPIO.HIGH && value !== GPIO.LOW)
            throw new Error("Invalid value");

        await FileSystem.writeFile(this.path + "value", value);
    }
    async get_value()
    {
        if(!this.exported)
            throw new Error("GPIO not exported");

        let value = (await FileSystem.readFile(this.path + "value")).toString("utf-8");

        if(value !== GPIO.HIGH && value !== GPIO.LOW)
            throw new Error("Invalid value returned");

        return value;
    }

    async enable_event_polling(value, interval, callback, ...args)
    {
        if(this.event.timer)
            throw new Error("Event polling already enabled, disable first");

        if(value !== GPIO.HIGH && value !== GPIO.LOW && value !== GPIO.ANY)
            throw new Error("Invalid value");

        if(typeof(callback) !== "function")
            throw new Error("Invalid callback");

        if(interval < 0)
            throw new Error("Invalid interval");

        this.event.callback = callback.bind(this);
        this.event.args = args;
        this.event.last_value = await this.get_value();
        this.event.value = value;
        this.event.timer = setInterval(this.poll, interval);
    }
    async disable_event_polling()
    {
        if(!this.event.timer)
            throw new Error("Event polling not enabled");

        clearInterval(this.event.timer);

        this.event.timer = null;
        this.event.callback = null;
        this.event.args = [];
    }

    async poll()
    {
        try
        {
            let value = await this.get_value();

            if(value != this.event.last_value)
            {
                this.event.last_value = value;

                if((this.event.value === GPIO.ANY || value === this.event.value) && typeof(this.event.callback) == "function")
                    this.event.callback(null, value, ...this.event.args);
            }
        }
        catch (e)
        {
            if(typeof(this.event.callback) == "function")
                this.event.callback(e, null, ...this.event.args);
        }
    }
}

module.exports = GPIO;