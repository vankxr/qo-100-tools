const FileSystem = require("fs");
const ReadLine = require('readline');
const Crypto = require("crypto");
const SSH2 = require("ssh2");
const ShellQuote = require("shell-quote");
const GPIO = require("./lib/gpio");
const { I2C, I2CDevice } = require("./lib/i2c");
const { OneWire, OneWireDevice } = require("./lib/onewire");
const IPMA = require("./lib/ipma");
const HPPSU = require("./lib/hppsu");
const DS2484 = require("./lib/ds2484");
const BME280 = require("./lib/bme280");
const MCP3221 = require("./lib/mcp3221");
const MCP3421 = require("./lib/mcp3421");
const MCP23008 = require("./lib/mcp23008");
const MCP23017 = require("./lib/mcp23017");
const LTC5597 = require("./lib/ltc5597");
const DS18B20 = require("./lib/ds18b20");
const RelayController = require("./lib/relay_controller");
const LogFileManager = require("./util/log_file_manager");
const Printer = require("./util/printer");
const delay = require("./util/delay");

let cl;
let ipma;
const gpios = {};
const buses = {};
const onewire_bus_controllers = [];
const relay_controllers = [];
const upconverters = [];
const lnb_controllers = [];
const pa_bias_controllers = [];
const power_supply_gpio_controllers = [];
const power_supplies = [];
const sensors = {};
const ssh_allowed_users = [];
const ssh_commands = {};
let ssh_server;

function log_init()
{
    const console_file = new LogFileManager(LogFileManager.DAILY, __dirname + "/log", "console_", ".log");

    console_file.start();

    const console_file_printer = new Printer(console_file);
    const console_stdout_printer = new Printer(process.stdout);

    console_stdout_printer.chain(console_file_printer);

    cl = console_stdout_printer;
}

async function ssh_server_init()
{
    ssh_server = new SSH2.Server(
        {
            hostKeys: [
                await FileSystem.promises.readFile(__dirname + "/keys/host")
            ]
        },
        ssh_server_connection_handler
    );

    // Users
    ssh_server_add_user("joao", null, (await FileSystem.promises.readFile(__dirname + "/keys/users/joao.pub")).toString().split("\n"));

    // Commands
    ssh_server_add_command(
        "help",
        "Prints this help information",
        async function (argv)
        {
            for(cmd of Object.keys(ssh_commands))
                this.println(null, null, cmd + " - " + ssh_commands[cmd].description);

            return 0;
        }
    );
    ssh_server_add_command(
        "me",
        "Prints your IP address",
        async function (argv)
        {
            this.println(null, null, "Your IP: " + this.stream.session.client.ip);

            return 0;
        }
    );
    ssh_server_add_command(
        "echo",
        "Prints the first argument",
        async function (argv)
        {
            this.println(null, null, (argv.length > 1 ? argv[1] : ""));

            return 0;
        }
    );
    ssh_server_add_command(
        "clear",
        "Clears the screen",
        async function (argv)
        {
            if(!this.stream.session.shell)
                throw new Error("Invalid shell");

            this.stream.session.shell.write(
                null,
                {
                    ctrl: true,
                    name: "l"
                }
            );

            return 0;
        }
    );
    ssh_server_add_command(
        "rsens",
        "Read sensor specified in the first argument",
        async function (argv)
        {
            let sensor_name = argv[1];

            if(typeof(sensor_name) != "string")
                throw new Error("Invalid sensor name");

            sensor_name = sensor_name.toLowerCase();

            let sensor = sensors[sensor_name];

            if(!sensor)
                throw new Error("Sensor not found");

            if(sensor instanceof DS18B20)
            {
                this.tprintln(null, "DS18B20", "Temperature: %d C", await sensor.get_temperature());
                this.tprintln(null, "DS18B20", "High Alarm Temperature: %d C", (await sensor.get_alarm()).high);
                this.tprintln(null, "DS18B20", "Low Alarm Temperature: %d C", (await sensor.get_alarm()).low);
                this.tprintln(null, "DS18B20", "Parasidic powered: %s", await sensor.is_parasidic_power());
            }
            else if(sensor instanceof BME280)
            {
                this.tprintln(null, "BME280", "Temperature: %d C", await sensor.get_temperature());
                this.tprintln(null, "BME280", "Humidity: %d %%RH", await sensor.get_humidity());
                this.tprintln(null, "BME280", "Pressure: %d hPa", await sensor.get_pressure());
                this.tprintln(null, "BME280", "Dew point: %d C", await sensor.get_dew_point());
                this.tprintln(null, "BME280", "Heat index: %d C", await sensor.get_heat_index());
                this.tprintln(null, "BME280", "Altitude: %d m", await sensor.get_altitude());
            }
            else if(sensor instanceof MCP3221)
            {
                let samples = argv[2] || 1;

                this.tprintln(null, "MCP3221", "Voltage: %d mV", await sensor.get_voltage(samples));
            }
            else if(sensor instanceof LTC5597)
            {
                let samples = argv[2] || 3;
                let gain = argv[3] || 2;

                this.tprintln(null, "LTC5597", "Power: %d dBm", await sensor.get_power_level(gain, samples));
            }
            else
            {
                throw new Error("Sensor not supported");
            }

            return 0;
        },
        function (line)
        {
            const sensor_names = Object.keys(sensors);
            const hits = sensor_names.filter(
                function (c)
                {
                    return c.startsWith(line);
                }
            );

            return hits.length ? hits : sensor_names;
        }
    );
    ssh_server_add_command(
        "rpsu",
        "Read telemetry from the PSU specified in the first argument",
        async function (argv)
        {
            let psu_name = argv[1];

            if(typeof(psu_name) != "string")
                throw new Error("Invalid PSU name");

            psu_name = psu_name.toLowerCase();

            if(psu_name.indexOf("psu") != 0)
                throw new Error("Invalid PSU name");

            let psu_index = parseInt(psu_name.slice(3));

            if(psu_index < 0 || psu_index >= power_supplies.length)
                throw new Error("Invalid PSU name");

            let psu = power_supplies[psu_index];

            if(!psu)
                throw new Error("PSU not found");

            if(psu instanceof HPPSU)
            {
                this.tprintln(null, "HPPSU", "PSU ID: 0x%s", (await psu.get_id()).toString(16));
                this.tprintln(null, "HPPSU", "PSU SPN: %s", await psu.get_spn());
                this.tprintln(null, "HPPSU", "PSU Date: %s", await psu.get_date());
                this.tprintln(null, "HPPSU", "PSU Name: %s", await psu.get_name());
                this.tprintln(null, "HPPSU", "PSU CT: %s", await psu.get_ct());
                this.tprintln(null, "HPPSU", "-------------------------------------");
                this.tprintln(null, "HPPSU", "PSU Input Voltage: %d V", await psu.get_input_voltage());
                this.tprintln(null, "HPPSU", "PSU Input Undervoltage threshold: %d V", await psu.get_input_undervoltage_threshold());
                this.tprintln(null, "HPPSU", "PSU Input Overvoltage threshold: %d V", await psu.get_input_overvoltage_threshold());
                this.tprintln(null, "HPPSU", "PSU Input Current: %d A (max. %d A)", await psu.get_input_current(), await psu.get_peak_input_current());
                this.tprintln(null, "HPPSU", "PSU Input Power: %d W (max. %d W)", await psu.get_input_power(), await psu.get_peak_input_power());
                this.tprintln(null, "HPPSU", "PSU Input Energy: %d Wh", await psu.get_input_energy());
                this.tprintln(null, "HPPSU", "PSU Output Voltage: %d V", await psu.get_output_voltage());
                this.tprintln(null, "HPPSU", "PSU Output Undervoltage threshold: %d V", await psu.get_output_undervoltage_threshold());
                this.tprintln(null, "HPPSU", "PSU Output Overvoltage threshold: %d V", await psu.get_output_overvoltage_threshold());
                this.tprintln(null, "HPPSU", "PSU Output Current: %d A (max. %d A)", await psu.get_output_current(), await psu.get_peak_output_current());
                this.tprintln(null, "HPPSU", "PSU Output Power: %d W", await psu.get_output_power());
                this.tprintln(null, "HPPSU", "PSU Intake Temperature: %d C", await psu.get_intake_temperature());
                this.tprintln(null, "HPPSU", "PSU Internal Temperature: %d C", await psu.get_internal_temperature());
                this.tprintln(null, "HPPSU", "PSU Fan speed: %d RPM", await psu.get_fan_speed());
                this.tprintln(null, "HPPSU", "PSU Fan target speed: %d RPM", await psu.get_fan_target_speed());
                this.tprintln(null, "HPPSU", "PSU On time: %d s", await psu.get_on_time());
                this.tprintln(null, "HPPSU", "PSU Total on time: %d days", await psu.get_total_on_time() / 60 / 24);
                this.tprintln(null, "HPPSU", "PSU Status flags: 0x%s", (await psu.get_status_flags()).toString(16));
                this.tprintln(null, "HPPSU", "PSU ON: %s", await psu.is_main_output_enabled());
                this.tprintln(null, "HPPSU", "PSU Input Present: %s", await psu.is_input_present());
            }
            else
            {
                throw new Error("PSU not supported");
            }

            return 0;
        },
        function (line)
        {
            const psu_names = ["psu0", "psu1"];
            const hits = psu_names.filter(
                function (c)
                {
                    return c.startsWith(line);
                }
            );

            return hits.length ? hits : psu_names;
        }
    );

    // Listen
    ssh_server.listen(
        2222,
        "0.0.0.0",
        function ()
        {
            cl.tprintln("grey", "SSH", "Listening on port %d!", this.address().port);
        }
    );
}
function ssh_server_add_user(username, password, keys)
{
    if(typeof(username) != "string" || !username.length)
        throw new Error("Invalid username");

    if(keys !== undefined && !Array.isArray(keys))
        keys = [keys];

    let user = {
        username: Buffer.from(username),
    };

    if(typeof(password) == "string" && password.length)
        user.password = Buffer.from(password);

    if(Array.isArray(keys))
    {
        user.keys = [];

        for(key of keys)
        {
            if(!key.length)
                continue;

            let parsed_key = SSH2.utils.parseKey(Buffer.isBuffer(key) ? key : Buffer.from(key));

            if(parsed_key instanceof Error)
                throw parsed_key;

            user.keys.push(parsed_key);
        }
    }

    ssh_allowed_users.push(user);
}
function ssh_server_add_command(cmd, description, handler, completer)
{
    if(typeof(cmd) != "string" || !cmd.length)
        throw new Error("Invalid command string");

    if(description && typeof(description) != "string")
        throw new Error("Invalid description string");

    if(typeof(handler) != "function")
        throw new Error("Invalid command handler");

    for(let i = 0; i < cmd.length; i++)
    {
        let c = cmd.charCodeAt(i);

        if(!(c >= 0x30 && c <= 0x39) && !(c >= 0x41 && c <= 0x5A) && !(c >= 0x61 && c <= 0x7A) && c != 0x5F)
            throw new Error("Invalid command string");
    }

    if(typeof(ssh_commands[cmd]) == "object")
        throw new Error("Command already exists");

    ssh_commands[cmd] = {
        description: description || "No description available",
        handler: handler,
        completer: undefined
    };

    if(typeof(completer) == "function")
        ssh_commands[cmd].completer = completer;
}
function ssh_server_complete_command(line)
{
    const tokens = line.split(" ");
    const command_names = Object.keys(ssh_commands);
    let hits = [];

    if(tokens.length > 1)
    {
        let cmd = ssh_commands[tokens[0]];

        if(cmd && typeof(cmd.completer) == "function")
            hits = cmd.completer(tokens.slice(1).join(" "));

        if(hits.length == 1)
            hits[0] = tokens[0] + " " + hits[0];
    }
    else
    {
        hits = command_names.filter(
            function (c)
            {
                return c.startsWith(line);
            }
        );
    }

    return [hits.length ? hits : command_names, line];
}
async function ssh_server_process_command_string(str)
{
    if(typeof(str) != "string")
        return;

    cl.tprintln("blue", "SSH", "Processing command string '%s' from client at %s...", str, this.stream.session.client.ip);

    let argv = ShellQuote.parse(str);
    let current_argv = [];

    for(arg of argv)
    {
        switch(typeof(arg))
        {
            case "string":
            {
                current_argv.push(arg);
            }
            break;
            case "object":
            {
                if(typeof(arg.op) != "string")
                    return;

                switch(arg.op)
                {
                    case ";": // Process the next regardless if the current succeeds
                    {
                        await ssh_server_process_command.bind(this)(current_argv);
                        current_argv = [];
                    }
                    break;
                    case "&&": // Process the next only if the current succeeds
                    {
                        if(!(await ssh_server_process_command.bind(this)(current_argv)))
                            return;

                        current_argv = [];
                    }
                    break;
                    case "||": // Process the next only if the current does not succeed
                    {
                        if(await ssh_server_process_command.bind(this)(current_argv))
                            return;

                        current_argv = [];
                    }
                    break;
                    default:
                    {
                        this.println("red", null, "Operator '%s' not supported!", arg.op);
                        cl.tprintln("red", "SSH", "Error processing operator '%s' from client at %s: Operator '%s' not supported!", arg.op, this.stream.session.client.ip, arg.op);

                        return;
                    }
                    break;
                }
            }
            break;
            default:
            {
                this.println("red", null, "Invalid argument!");
                cl.tprintln("red", "SSH", "Error processing operator '%s' from client at %s: Invalid argument!", arg.op, this.stream.session.client.ip);

                return;
            }
            break;
        }
    }

    await ssh_server_process_command.bind(this)(current_argv);
}
async function ssh_server_process_command(argv)
{
    if(!Array.isArray(argv))
        return false;

    if(!argv)
        return false;

    if(!argv.length)
        return true;

    cl.tprintln("blue", "SSH", "Processing command '%s' from client at %s...", ShellQuote.quote(argv), this.stream.session.client.ip);

    let cmd_name = argv[0];
    let cmd_handler = ssh_commands[cmd_name] ? ssh_commands[cmd_name].handler : null;

    if(typeof(cmd_handler) != "function")
    {
        this.println("red", null, "Command '%s' not found!", cmd_name);
        cl.tprintln("red", "SSH", "Error processing command '%s' from client at %s: Command '%s' not found!", ShellQuote.quote(argv), this.stream.session.client.ip, cmd_name);

        return false;
    }

    cmd_handler = cmd_handler.bind(this);

    let success = false;

    try
    {
        success = await cmd_handler(argv);

        cl.tprintln("green", "SSH", "Processed command '%s' from client at %s!", ShellQuote.quote(argv), this.stream.session.client.ip);
    }
    catch(e)
    {
        success = false;

        this.println("red", null, e);
        cl.tprintln("red", "SSH", "Error processing command '%s' from client at %s: " + e, ShellQuote.quote(argv), this.stream.session.client.ip);
    }

    return success;
}
function ssh_server_connection_handler(client)
{
    client.ip = client._sock._peername.address;

    cl.tprintln("magenta", "SSH", "New client connected! IP: %s!", client.ip);

    client.on("authentication", ssh_server_client_auth_handler);
    client.on("ready", ssh_server_client_ready_handler);

    client.on(
        "end",
        function()
        {
            cl.tprintln("magenta", "SSH", "Client at %s disconnected!", this.ip);
        }
    );
}
function ssh_server_client_auth_handler(ctx)
{
    cl.tprintln("magenta", "SSH", "Authenticating client at %s using %s...", this.ip, ctx.method);

    let incoming_username = Buffer.from(ctx.username);
    let found_user;

    for(user of ssh_allowed_users)
    {
        if(incoming_username.length !== user.username.length)
            continue;

        if(!Crypto.timingSafeEqual(user.username, incoming_username))
            continue;

        found_user = user; // Don't break if we find a user to keep the code timing safe
    }

    if(!found_user)
        return ctx.reject();

    switch(ctx.method)
    {
        case "password":
        {
            if(!user.password)
                return ctx.reject();

            let password = Buffer.from(ctx.password);

            if(password.length !== user.password.length)
                return ctx.reject();

            if(!Crypto.timingSafeEqual(password, user.password))
                return ctx.reject();
        }
        break;
        case "publickey":
        {
            let passed = false;

            for(key of user.keys)
            {
                let pub_ssh_key = key.getPublicSSH();

                if(ctx.key.algo !== key.type)
                    continue;

                if(ctx.key.data.length !== pub_ssh_key.length)
                    continue;

                if(!Crypto.timingSafeEqual(ctx.key.data, pub_ssh_key))
                    continue;

                if(ctx.signature && key.verify(ctx.blob, ctx.signature) !== true)
                    continue;

                passed = true; // Don't break if we find a user to keep the code timing safe
            }

            if(!passed)
                return ctx.reject();
        }
        break;
        default:
        {
            return ctx.reject();
        }
    }

    this.auth = {
        username: ctx.username,
        method: ctx.method
    };

    cl.tprintln("green", "SSH", "Client at %s authenticated as '%s' using %s!", this.ip, this.auth.username, this.auth.method);

    return ctx.accept();
}
function ssh_server_client_ready_handler()
{
    cl.tprintln("magenta", "SSH", "Client at %s ready!", this.ip);

    this.on("session", ssh_server_client_session_request_handler);
}
function ssh_server_client_session_request_handler(accept, reject)
{
    cl.tprintln("magenta", "SSH", "Client at %s requested a new session...", this.ip);

    if(this.session)
    {
        cl.tprintln("red", "SSH", "Session rejected for client at %s: Session already present", this.ip);

        return reject();
    }

    this.session = accept();
    this.session.client = this;

    this.session.on("pty", ssh_server_client_session_pty_request_handler);
    this.session.on("window-change", ssh_server_client_session_window_change_handler);
    this.session.on("shell", ssh_server_client_session_shell_request_handler);
    this.session.on("exec", ssh_server_client_session_exec_request_handler);

    cl.tprintln("green", "SSH", "Session created for client at %s!", this.ip);
}
function ssh_server_client_session_pty_request_handler(accept, reject, info)
{
    cl.tprintln("magenta", "SSH", "Client at %s requested a PTY with %d rows and %d columns!", this.client.ip, info.rows, info.cols);

    this.pty = info;

    if(accept)
        accept();
}
function ssh_server_client_session_window_change_handler(accept, reject, info)
{
    //cl.tprintln("magenta", "SSH", "Client at %s PTY size changed to %d rows and %d columns!", this.client.ip, info.rows, info.cols);

    if(this.pty)
    {
        this.pty.rows = info.rows;
        this.pty.cols = info.cols;
        this.pty.width = info.width;
        this.pty.height = info.height;
    }

    if(this.channel)
    {
        this.channel.columns = info.cols;
        this.channel.rows = info.rows;

        this.channel.emit("resize");
    }

    if(accept)
        accept();
}
function ssh_server_client_session_shell_request_handler(accept, reject)
{
    cl.tprintln("magenta", "SSH", "Client at %s requested an interactive shell!", this.client.ip);

    let channel = accept();
    channel.session = this;
    channel.printer = new Printer(channel);

    if(this.pty)
    {
        channel.columns = this.pty.cols;
        channel.rows = this.pty.rows;
        channel.isTTY = true;

        channel.emit("resize");
    }

    this.shell = ReadLine.createInterface(
        {
            input: channel,
            output: channel,
            terminal: true,
            completer: ssh_server_complete_command,
            history: [],
            historySize: 100,
            removeHistoryDuplicates: true,
            prompt: this.client.auth.username + "> "
        }
    );

    this.shell.prompt();

    this.shell.on(
        "line",
        async function (line)
        {
            let cmd = line.trim();

            this.pause();

            await ssh_server_process_command_string.bind(this.output.printer)(cmd);

            this.resume();
            this.prompt();
        }
    );

    this.shell.on(
        "SIGINT",
        function ()
        {
            // Clear line
            this.write(
                null,
                {
                    ctrl: true,
                    name: "u"
                }
            );
            this.prompt();
        }
    );

    this.shell.on(
        "close",
        function()
        {
            this.output.write("\r\n");
            this.output.exit(0);
            this.output.end();
        }
    );

    this.shell.on(
        "history",
        async function (history)
        {
            // TODO: save history
        }
    );
}
async function ssh_server_client_session_exec_request_handler(accept, reject, info)
{
    let channel = accept();
    channel.session = this;
    channel.printer = new Printer(channel);

    let cmd = info.command.trim();
    await ssh_server_process_command_string.bind(channel.printer)(cmd);

    channel.exit(0);
    channel.end();
}

async function bme280_update_sea_hpa()
{
    if(!(ipma instanceof IPMA))
        cl.tprintln("red", "IPMA", "No valid IPMA instance defined");

    let sea_hpa;

    try
    {
        sea_hpa = (await ipma.get_latest_surface_observation()).pressao;
    }
    catch(e)
    {
        cl.tprintln("red", "IPMA", "Error fetching from IPMA: " + e);
    }

    cl.tprintln("green", "IPMA", "Got IPMA Sea pressure: %d hPa", sea_hpa);

    BME280.set_sea_pressure(sea_hpa);
}

async function init()
{
    // Console and logging
    log_init();

    // GPIOs
    gpios.intrusion = [];
    gpios.intrusion.push(await GPIO.export("PA0"));
    gpios.intrusion.push(await GPIO.export("PA1"));
    gpios.ext_i2c_enable = [];
    gpios.ext_i2c_enable.push(await GPIO.export("PA6"));
    gpios.ext_i2c_enable.push(await GPIO.export("PA2"));
    gpios.psu_i2c_enable = [];
    gpios.psu_i2c_enable.push(await GPIO.export("PA7"));
    gpios.psu_i2c_enable.push(await GPIO.export("PA10"));

    await gpios.intrusion[0].set_direction(GPIO.INPUT);
    await gpios.intrusion[0].set_pull(GPIO.PULLUP);
    await gpios.intrusion[1].set_direction(GPIO.INPUT);
    await gpios.intrusion[1].set_pull(GPIO.PULLUP);
    await gpios.ext_i2c_enable[0].set_direction(GPIO.OUTPUT);
    await gpios.ext_i2c_enable[0].set_value(GPIO.LOW);
    await gpios.ext_i2c_enable[1].set_direction(GPIO.OUTPUT);
    await gpios.ext_i2c_enable[1].set_value(GPIO.LOW);
    await gpios.psu_i2c_enable[0].set_direction(GPIO.OUTPUT);
    await gpios.psu_i2c_enable[0].set_value(GPIO.LOW);
    await gpios.psu_i2c_enable[1].set_direction(GPIO.OUTPUT);
    await gpios.psu_i2c_enable[1].set_value(GPIO.LOW);

    // Communication Interfaces
    //// I2C
    buses.onewire = [];
    buses.i2c = [];
    buses.i2c.push(await I2C.open(0));
    buses.i2c.push(await I2C.open(1));

    //// OneWire bus Controllers
    onewire_bus_controllers.push(new DS2484(buses.i2c[0]));

    for(let i = 0; i < onewire_bus_controllers.length; i++)
    {
        const controller = onewire_bus_controllers[i];

        try
        {
            await controller.probe();
        }
        catch (e)
        {
            cl.tprintln("red", "DS2484", e);

            continue;
        }

        cl.tprintln("green", "DS2484", "DS2484 #%d found!", i);

        controller.config();

        buses.onewire.push(controller.get_ow_bus());
    }

    //// OneWire bus enumeration
    for(let i = 0; i < buses.onewire.length; i++)
    {
        let bus = buses.onewire[i];
        let devices;

        try
        {
            devices = await bus.scan();
        }
        catch (e)
        {
            cl.tprintln("red", "OneWire", "Error scanning OneWire bus %d: " + e, i);

            continue;
        }

        cl.tprintln(devices.length > 0 ? null : "yellow", "OneWire", "  Found %d devices on OneWire bus %d:", devices.length, i);

        for(device of devices)
        {
            cl.tprintln(null, "OneWire", "    %s", device);

            if(device.get_family_name() == "DS18B20")
            {
                cl.tprintln("green", "OneWire", "      DS18B20 #%d found!", i);

                let sensor = new DS18B20(device);

                // TODO: Check code and add accordingly to sensors array
                sensors["ds18b20_" + sensor.get_uid().toString(16)] = sensor;

                await sensor.config(12);
                await sensor.measure();

                cl.tprintln(null, "DS18B20", "        Temperature: %d C", await sensor.get_temperature());
                cl.tprintln(null, "DS18B20", "        High Alarm Temperature: %d C", (await sensor.get_alarm()).high);
                cl.tprintln(null, "DS18B20", "        Low Alarm Temperature: %d C", (await sensor.get_alarm()).low);
                cl.tprintln(null, "DS18B20", "        Parasidic powered: %s", await sensor.is_parasidic_power());
            }
        }
    }

    // IPMA
    try
    {
        let ipma_station = null;
        let ipma_location = null;

        let ipma_stations = await IPMA.fetch_stations();

        for(const station of ipma_stations)
        {
            if(!station.properties)
                continue;

            if(!station.properties.idEstacao)
                continue;

            if(!station.properties.localEstacao)
                continue;

            //if(station.properties.localEstacao.indexOf("Leiria (Aeródromo)") === 0)
            if(station.properties.localEstacao.indexOf("Lisboa (G.Coutinho)") === 0)
            {
                ipma_station = station;

                cl.tprintln("green", "IPMA", "IPMA station \"" + station.properties.localEstacao + "\" found!");

                break;
            }
        }

        if(!ipma_station)
            cl.tprintln("yellow", "IPMA", "IPMA station not found!");

        let ipma_locations = await IPMA.fetch_locations();

        for(const location of ipma_locations)
        {
            if(!location.globalIdLocal)
                continue;

            if(!location.local)
                continue;

            //if(location.local.indexOf("Leiria") === 0)
            if(location.local.indexOf("Lisboa") === 0)
            {
                ipma_location = location;

                cl.tprintln("green", "IPMA", "IPMA location \"" + location.local + "\" found!");

                break;
            }
        }

        if(!ipma_location)
            cl.tprintln("yellow", "IPMA", "IPMA location not found!");

        ipma = new IPMA(ipma_location, ipma_station);
    }
    catch (e)
    {
        cl.tprintln("red", "IPMA", e);
    }

    // Sensors
    //// BME280
    const bme280_sensors = [];
    bme280_sensors.push(new BME280(buses.i2c[0], 0, gpios.ext_i2c_enable[0]));
    bme280_sensors.push(new BME280(buses.i2c[0], 1, gpios.ext_i2c_enable[0]));

    for(let i = 0; i < bme280_sensors.length; i++)
    {
        let sensor = bme280_sensors[i];

        try
        {
            await sensor.probe();
        }
        catch (e)
        {
            cl.tprintln("red", "BME280", e);

            continue;
        }

        cl.tprintln("green", "BME280", "BME280 #%d found!", i);

        // TODO: Check code and add accordingly to sensors array
        sensors["bme280_" + i] = sensor;

        await sensor.read_calibration();
        await sensor.config(
            {
                temperature: 16,
                humidity: 16,
                pressure: 16
            },
            16,
            125
        );
        await sensor.measure(false);

        await delay(125 * 16);

        cl.tprintln(null, "BME280", "  BME280 #%d Temperature: %d C", i, await sensor.get_temperature());
        cl.tprintln(null, "BME280", "  BME280 #%d Humidity: %d %%RH", i, await sensor.get_humidity());
        cl.tprintln(null, "BME280", "  BME280 #%d Pressure: %d hPa", i, await sensor.get_pressure());
        cl.tprintln(null, "BME280", "  BME280 #%d Dew point: %d C", i, await sensor.get_dew_point());
        cl.tprintln(null, "BME280", "  BME280 #%d Heat index: %d C", i, await sensor.get_heat_index());
        cl.tprintln(null, "BME280", "  BME280 #%d Altitude: %d m", i, await sensor.get_altitude());
    }

    //// MCP3221 ADC
    const mcp3221_sensors = [];
    mcp3221_sensors.push(new MCP3221(buses.i2c[0], 0));

    for(let i = 0; i < mcp3221_sensors.length; i++)
    {
        let sensor = mcp3221_sensors[i];

        try
        {
            await sensor.probe();
        }
        catch (e)
        {
            cl.tprintln("red", "MCP3221", e);

            continue;
        }

        cl.tprintln("green", "MCP3221", "MCP3221 #%d found!", i);

        if(i == 0)
        {
            sensor.set_scale_factor((21e3 + 147e3) / 21e3); // Voltage divider

            sensors["vin_voltage_adc"] = sensor;
        }
        else
        {
            sensors["mcp3421_" + i] = sensor;
        }

        cl.tprintln(null, "MCP3221", "  MCP3221 #%d Voltage: %d mV", i, await sensor.get_voltage(10));
    }

    //// LTC5597 (RFPowerMeter with MCP3421 ADC)
    const ltc5597_sensor = [];
    ltc5597_sensor.push(new LTC5597(buses.i2c[0], 0, gpios.ext_i2c_enable[0]));

    for(let i = 0; i < ltc5597_sensor.length; i++)
    {
        let sensor = ltc5597_sensor[i];

        try
        {
            await sensor.probe();
        }
        catch (e)
        {
            cl.tprintln("red", "LTC5597", e);

            continue;
        }

        cl.tprintln("green", "LTC5597", "LTC5597 #%d found!", i);

        if(i == 0)
        {
            sensor.set_offset(18.1 + 20);

            sensors["tx_fwd_rf_power_sensor"] = sensor;
        }
        else
        {
            sensors["ltc5597_" + i] = sensor;
        }

        sensor.load_calibration(
            {
                pdata: [-40, -35, -30, -25, -20, -15, -10, -5, 0, 5],
                fdata: [
                    {
                        frequency: 100,
                        vdata: [1, 63.92, 206.92, 349.92, 492.92, 635.92, 778.92, 921.92, 1064.92, 1207.92]
                    },
                    {
                        frequency: 500,
                        vdata: [20.39, 158.89, 297.39, 435.89, 574.39, 712.89, 851.39, 989.89, 1128.39, 1266.89]
                    },
                    {
                        frequency: 1000,
                        vdata: [1, 138.2, 278.2, 418.2, 558.2, 698.2, 838.2, 978.2, 1118.2, 1258.2]
                    },
                    {
                        frequency: 2700,
                        vdata: [1, 120.7, 263.2, 405.7, 548.2, 690.7, 833.2, 975.7, 1118.2, 1260.7]
                    },
                    {
                        frequency: 5800,
                        vdata: [1, 134.01, 275.51, 417.01, 558.51, 700.01, 841.51, 983.01, 1124.51, 1266.01]
                    },
                    {
                        frequency: 10000,
                        vdata: [3.82, 144.82, 285.82, 426.82, 567.82, 708.82, 849.82, 990.82, 1131.82, 1272.82]
                    },
                    {
                        frequency: 15000,
                        vdata: [1, 120.28, 262.28, 404.28, 546.28, 688.28, 830.28, 972.28, 1114.28, 1256.28]
                    }
                ]
            }
        );
        sensor.set_frequency(525);

        await sensor.config(14, true);

        cl.tprintln(null, "LTC5597", "  LTC5597 #%d Power: %d dBm", i, await sensor.get_power_level(2, 3) + 18.1 + 20);
    }

    //// Controllers
    // Relay Controller
    relay_controllers.push(new RelayController(buses.i2c[0], gpios.ext_i2c_enable[1]));

    for(let i = 0; i < relay_controllers.length; i++)
    {
        let controller = relay_controllers[i];

        try
        {
            await controller.probe();
        }
        catch (e)
        {
            cl.tprintln("red", "RelayController", e);

            continue;
        }

        cl.tprintln("green", "RelayController", "Relay Controller #%d found!", i);

        cl.tprintln(null, "RelayController", "  Relay Controller #%d Unique ID: %s", i, await controller.get_unique_id());
        cl.tprintln(null, "RelayController", "  Relay Controller #%d Software Version: v%d", i, await controller.get_software_version());

        let chip_voltages = await controller.get_chip_voltages();
        cl.tprintln(null, "RelayController", "  Relay Controller #%d AVDD Voltage: %d mV", i, chip_voltages.avdd);
        cl.tprintln(null, "RelayController", "  Relay Controller #%d DVDD Voltage: %d mV", i, chip_voltages.dvdd);
        cl.tprintln(null, "RelayController", "  Relay Controller #%d IOVDD Voltage: %d mV", i, chip_voltages.iovdd);
        cl.tprintln(null, "RelayController", "  Relay Controller #%d Core Voltage: %d mV", i, chip_voltages.core);

        let system_voltages = await controller.get_system_voltages();
        cl.tprintln(null, "RelayController", "  Relay Controller #%d VIN Voltage: %d mV", i, system_voltages.vin);

        let chip_temperatures = await controller.get_chip_temperatures();
        cl.tprintln(null, "RelayController", "  Relay Controller #%d ADC Temperature: %d C", i, chip_temperatures.adc);
        cl.tprintln(null, "RelayController", "  Relay Controller #%d EMU Temperature: %d C", i, chip_temperatures.emu);
    }

    // TODO: Other controllers

    //// PSU
    // PSU GPIO Controlers
    power_supply_gpio_controllers.push(new MCP23008(buses.i2c[1], 0, gpios.psu_i2c_enable[0]));
    power_supply_gpio_controllers.push(new MCP23008(buses.i2c[1], 0, gpios.psu_i2c_enable[1]));

    for(let i = 0; i < power_supply_gpio_controllers.length; i++)
    {
        let gpio_controller = power_supply_gpio_controllers[i];

        try
        {
            await gpio_controller.probe();
        }
        catch (e)
        {
            cl.tprintln("red", "MCP23008", e);

            continue;
        }

        cl.tprintln("green", "MCP23008", "PSU GPIO Controller #%d found!", i);

        gpio_controller.config();

        let present_gpio = gpio_controller.get_gpio("PA0");
        let enable_gpio = gpio_controller.get_gpio("PA4");

        present_gpio.set_direction(GPIO.INPUT);
        present_gpio.set_pull(GPIO.PULLUP);
        enable_gpio.set_direction(GPIO.INPUT);
        enable_gpio.set_pull(GPIO.PULLUP);
    }

    // PSU
    power_supplies.push(new HPPSU(buses.i2c[1], 7, gpios.psu_i2c_enable[0], power_supply_gpio_controllers[0].get_gpio("PA0"), power_supply_gpio_controllers[0].get_gpio("PA4")));
    power_supplies.push(new HPPSU(buses.i2c[1], 7, gpios.psu_i2c_enable[1], power_supply_gpio_controllers[1].get_gpio("PA0"), power_supply_gpio_controllers[1].get_gpio("PA4")));

    for(let i = 0; i < power_supplies.length; i++)
    {
        let psu = power_supplies[i];
        let psu_present = true;

        try
        {
            psu_present = await psu.is_present();

            cl.tprintln(psu_present ? "green" : "red", "HPPSU", "PSU #%d Present: %s", i, psu_present);
        }
        catch (e)
        {
            cl.tprintln("yellow", "HPPSU", e);
        }

        if(!psu_present)
            continue;

        try
        {
            await psu.probe();
        }
        catch (e)
        {
            cl.tprintln("red", "HPPSU", e);

            continue;
        }

        cl.tprintln("green", "HPPSU", "PSU #%d and EEPROM #%d found!", i, i);

        //await psu.set_enable(true);

        cl.tprintln(null, "HPPSU", "  PSU #%d ID: 0x%s", i, (await psu.get_id()).toString(16));
        cl.tprintln(null, "HPPSU", "  PSU #%d SPN: %s", i, await psu.get_spn());
        cl.tprintln(null, "HPPSU", "  PSU #%d Date: %s", i, await psu.get_date());
        cl.tprintln(null, "HPPSU", "  PSU #%d Name: %s", i, await psu.get_name());
        cl.tprintln(null, "HPPSU", "  PSU #%d CT: %s", i, await psu.get_ct());
        cl.tprintln(null, "HPPSU", "  -------------------------------------");
        cl.tprintln(null, "HPPSU", "  PSU #%d Input Voltage: %d V", i, await psu.get_input_voltage());
        cl.tprintln(null, "HPPSU", "  PSU #%d Input Undervoltage threshold: %d V", i, await psu.get_input_undervoltage_threshold());
        cl.tprintln(null, "HPPSU", "  PSU #%d Input Overvoltage threshold: %d V", i, await psu.get_input_overvoltage_threshold());
        cl.tprintln(null, "HPPSU", "  PSU #%d Input Current: %d A (max. %d A)", i, await psu.get_input_current(), await psu.get_peak_input_current());
        cl.tprintln(null, "HPPSU", "  PSU #%d Input Power: %d W (max. %d W)", i, await psu.get_input_power(), await psu.get_peak_input_power());
        cl.tprintln(null, "HPPSU", "  PSU #%d Input Energy: %d Wh", i, await psu.get_input_energy());
        cl.tprintln(null, "HPPSU", "  PSU #%d Output Voltage: %d V", i, await psu.get_output_voltage());
        cl.tprintln(null, "HPPSU", "  PSU #%d Output Undervoltage threshold: %d V", i, await psu.get_output_undervoltage_threshold());
        cl.tprintln(null, "HPPSU", "  PSU #%d Output Overvoltage threshold: %d V", i, await psu.get_output_overvoltage_threshold());
        cl.tprintln(null, "HPPSU", "  PSU #%d Output Current: %d A (max. %d A)", i, await psu.get_output_current(), await psu.get_peak_output_current());
        cl.tprintln(null, "HPPSU", "  PSU #%d Output Power: %d W", i, await psu.get_output_power());
        cl.tprintln(null, "HPPSU", "  PSU #%d Intake Temperature: %d C", i, await psu.get_intake_temperature());
        cl.tprintln(null, "HPPSU", "  PSU #%d Internal Temperature: %d C", i, await psu.get_internal_temperature());
        cl.tprintln(null, "HPPSU", "  PSU #%d Fan speed: %d RPM", i, await psu.get_fan_speed());
        cl.tprintln(null, "HPPSU", "  PSU #%d Fan target speed: %d RPM", i, await psu.get_fan_target_speed());
        cl.tprintln(null, "HPPSU", "  PSU #%d On time: %d s", i, await psu.get_on_time());
        cl.tprintln(null, "HPPSU", "  PSU #%d Total on time: %d days", i, await psu.get_total_on_time() / 60 / 24);
        cl.tprintln(null, "HPPSU", "  PSU #%d Status flags: 0x%s", i, (await psu.get_status_flags()).toString(16));
        cl.tprintln(null, "HPPSU", "  PSU #%d ON: %s", i, await psu.is_main_output_enabled());
        cl.tprintln(null, "HPPSU", "  PSU #%d Input Present: %s", i, await psu.is_input_present());

        await psu.set_fan_target_speed(0);
        await psu.set_enable(false);

        //await psu.clear_on_time_and_energy();
        //await psu.clear_peak_input_current();

        //while(1)
        {
            //cl.tprintln(null, null, "PSU #%d Input Voltage: %d V", i, await psu.get_input_voltage());
            //cl.tprintln(null, null, "PSU #%d Status flags: 0x%s", i, (await psu.get_status_flags()).toString(16));
            /*
            if(!(await psu.is_input_present()))
            {
                cl.tprintln("cyan", null, "Power loss! Shutting down system...");

                require('child_process').exec("shutdown now");
                while(1);
            }

            await delay(500);
            */
        }
    }

    // SSH Server
    await ssh_server_init();
}
async function main()
{
    // Initialize
    await init();

    // BME280 sea level pressure for altitude calculation
    bme280_update_sea_hpa();
    setInterval(bme280_update_sea_hpa, 1200000); // Update every 20 minutes

    
}

try
{
    main();
}
catch(e)
{
    console.error(e);

    process.exit(1);
}