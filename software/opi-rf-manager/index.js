const FileSystem = require("fs");
const Util = require("util");
const ReadLine = require('readline');
const Crypto = require("crypto");
const SSH2 = require("ssh2");
const ShellQuote = require("shell-quote");
const GPIO = require("./lib/gpio");
const { I2C, I2CDevice } = require("./lib/i2c");
const { OneWire, OneWireDevice } = require("./lib/onewire");
const IPMA = require("./lib/ipma");
const WBSpectrumMonitor = require("./lib/wb_spectrum_monitor");
const OAQ = require("./lib/oaq");
const HPPSU = require("./lib/hppsu");
const DS2484 = require("./lib/ds2484");
const BME280 = require("./lib/bme280");
const ZMOD4510 = require("./lib/zmod4510");
const SI1133 = require("./lib/si1133");
const MCP3221 = require("./lib/mcp3221");
const MCP3421 = require("./lib/mcp3421");
const MCP23008 = require("./lib/mcp23008");
const MCP23017 = require("./lib/mcp23017");
const F2915 = require("./lib/f2915");
const LTC5597 = require("./lib/ltc5597");
const LTC4151 = require("./lib/ltc4151");
const DS18B20 = require("./lib/ds18b20");
const RelayController = require("./lib/relay_controller");
const PABiasController = require("./lib/pa_bias_controller");
const LNBController = require("./lib/lnb_controller");
const Upconverter = require("./lib/upconverter");
const LogFileManager = require("./util/log_file_manager");
const Printer = require("./util/printer");
const delay = require("./util/delay");

let cl;
let ipma;
let wb_spectrum_monitor;
let wb_signals = {};
const gpios = {};
const buses = {};
const devices = {};
const ssh_allowed_users = [];
const ssh_commands = {};
let ssh_server;

function log_init()
{
    const console_file = new LogFileManager(LogFileManager.DAILY, __dirname + "/log", "console_", ".log");

    console_file.start();

    const console_file_printer = new Printer(console_file, false);
    const console_stdout_printer = new Printer(process.stdout, true);

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
    ssh_server_add_user("root", null, (await FileSystem.promises.readFile(__dirname + "/keys/users/joao.pub")).toString().split("\n")); // Alias

    // Commands
    ssh_server_add_command(
        "help",
        "Prints this help information",
        async function (argv)
        {
            for(const cmd of Object.keys(ssh_commands))
                this.println(null, null, cmd + " - " + ssh_commands[cmd].description);

            return true;
        }
    );
    ssh_server_add_command(
        "me",
        "Prints your IP address",
        async function (argv)
        {
            this.println(null, null, "Your IP: " + this.stream.session.client.ip);
            this.println(null, null, "Your username: " + this.stream.session.client.auth.username);
            this.println(null, null, "Your auth method: " + this.stream.session.client.auth.method);

            return true;
        }
    );
    ssh_server_add_command(
        "echo",
        "Prints the first argument",
        async function (argv)
        {
            this.println(null, null, (argv.length > 1 ? argv[1] : ""));

            return true;
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

            return true;
        }
    );
    ssh_server_add_command(
        "rdev",
        "Read data from device specified in the first argument",
        async function (argv)
        {
            let device_param_name = argv[1];

            if(typeof(device_param_name) != "string")
                throw new Error("Invalid device parameter name");

            device_param_name = device_param_name.toLowerCase();

            let param_name_start = device_param_name.indexOf(".");

            if(param_name_start === 0)
                throw new Error("Invalid device parameter name");

            if(param_name_start == -1)
                param_name_start = device_param_name.length;

            let device_name = device_param_name.slice(0, param_name_start);
            let device = devices[device_name];

            if(!device)
                throw new Error("Device not found");

            let param_name = device_param_name.slice(param_name_start + 1);

            if(device instanceof HPPSU)
            {
                switch(param_name)
                {
                    case "":
                    {
                        this.tprintln(null, "HPPSU", "ID: 0x%s", (await device.get_id()).toString(16));
                        this.tprintln(null, "HPPSU", "SPN: %s", await device.get_spn());
                        this.tprintln(null, "HPPSU", "Date: %s", await device.get_date());
                        this.tprintln(null, "HPPSU", "Name: %s", await device.get_name());
                        this.tprintln(null, "HPPSU", "CT: %s", await device.get_ct());
                        this.tprintln(null, "HPPSU", "Input Present: %s", await device.is_input_present());
                        this.tprintln(null, "HPPSU", "Input Voltage: %d V", await device.get_input_voltage());
                        this.tprintln(null, "HPPSU", "Input Undervoltage threshold: %d V", await device.get_input_undervoltage_threshold());
                        this.tprintln(null, "HPPSU", "Input Overvoltage threshold: %d V", await device.get_input_overvoltage_threshold());
                        this.tprintln(null, "HPPSU", "Input Current: %d A (max. %d A)", await device.get_input_current(), await device.get_peak_input_current());
                        this.tprintln(null, "HPPSU", "Input Power: %d W (max. %d W)", await device.get_input_power(), await device.get_peak_input_power());
                        this.tprintln(null, "HPPSU", "Input Energy: %d Wh", await device.get_input_energy());
                        this.tprintln(null, "HPPSU", "Output Enabled: %s", await device.is_main_output_enabled());
                        this.tprintln(null, "HPPSU", "Output Voltage: %d V", await device.get_output_voltage());
                        this.tprintln(null, "HPPSU", "Output Undervoltage threshold: %d V", await device.get_output_undervoltage_threshold());
                        this.tprintln(null, "HPPSU", "Output Overvoltage threshold: %d V", await device.get_output_overvoltage_threshold());
                        this.tprintln(null, "HPPSU", "Output Current: %d A (max. %d A)", await device.get_output_current(), await device.get_peak_output_current());
                        this.tprintln(null, "HPPSU", "Output Power: %d W", await device.get_output_power());
                        this.tprintln(null, "HPPSU", "Intake Temperature: %d C", await device.get_intake_temperature());
                        this.tprintln(null, "HPPSU", "Internal Temperature: %d C", await device.get_internal_temperature());
                        this.tprintln(null, "HPPSU", "Fan speed: %d RPM", await device.get_fan_speed());
                        this.tprintln(null, "HPPSU", "Fan target speed: %d RPM", await device.get_fan_target_speed());
                        this.tprintln(null, "HPPSU", "On time: %d s", await device.get_on_time());
                        this.tprintln(null, "HPPSU", "Total on time: %d days", await device.get_total_on_time() / 60 / 24);
                        this.tprintln(null, "HPPSU", "Status flags: 0x%s", (await device.get_status_flags()).toString(16));
                    }
                    break;
                    case "info":
                    {
                        this.tprintln(null, "HPPSU", "ID: 0x%s", (await device.get_id()).toString(16));
                        this.tprintln(null, "HPPSU", "SPN: %s", await device.get_spn());
                        this.tprintln(null, "HPPSU", "Date: %s", await device.get_date());
                        this.tprintln(null, "HPPSU", "Name: %s", await device.get_name());
                        this.tprintln(null, "HPPSU", "CT: %s", await device.get_ct());
                    }
                    break;
                    case "status":
                    {
                        this.tprintln(null, "HPPSU", "Status flags: 0x%s", (await device.get_status_flags()).toString(16));
                        this.tprintln(null, "HPPSU", "Input Present: %s", await device.is_input_present());
                        this.tprintln(null, "HPPSU", "Output Enabled: %s", await device.is_main_output_enabled());
                    }
                    break;
                    case "input":
                    {
                        this.tprintln(null, "HPPSU", "Input Present: %s", await device.is_input_present());
                        this.tprintln(null, "HPPSU", "Input Voltage: %d V", await device.get_input_voltage());
                        this.tprintln(null, "HPPSU", "Input Undervoltage threshold: %d V", await device.get_input_undervoltage_threshold());
                        this.tprintln(null, "HPPSU", "Input Overvoltage threshold: %d V", await device.get_input_overvoltage_threshold());
                        this.tprintln(null, "HPPSU", "Input Current: %d A (max. %d A)", await device.get_input_current(), await device.get_peak_input_current());
                        this.tprintln(null, "HPPSU", "Input Power: %d W (max. %d W)", await device.get_input_power(), await device.get_peak_input_power());
                        this.tprintln(null, "HPPSU", "Input Energy: %d Wh", await device.get_input_energy());
                    }
                    break;
                    case "output":
                    {
                        this.tprintln(null, "HPPSU", "Output Enabled: %s", await device.is_main_output_enabled());
                        this.tprintln(null, "HPPSU", "Output Voltage: %d V", await device.get_output_voltage());
                        this.tprintln(null, "HPPSU", "Output Undervoltage threshold: %d V", await device.get_output_undervoltage_threshold());
                        this.tprintln(null, "HPPSU", "Output Overvoltage threshold: %d V", await device.get_output_overvoltage_threshold());
                        this.tprintln(null, "HPPSU", "Output Current: %d A (max. %d A)", await device.get_output_current(), await device.get_peak_output_current());
                        this.tprintln(null, "HPPSU", "Output Power: %d W", await device.get_output_power());
                    }
                    break;
                    case "temperature":
                    {
                        this.tprintln(null, "HPPSU", "Intake Temperature: %d C", await device.get_intake_temperature());
                        this.tprintln(null, "HPPSU", "Internal Temperature: %d C", await device.get_internal_temperature());
                    }
                    break;
                    case "fan_speed":
                    {
                        this.tprintln(null, "HPPSU", "Fan speed: %d RPM", await device.get_fan_speed());
                        this.tprintln(null, "HPPSU", "Fan target speed: %d RPM", await device.get_fan_target_speed());
                    }
                    break;
                    case "on_time":
                    {
                        this.tprintln(null, "HPPSU", "On time: %d s", await device.get_on_time());
                        this.tprintln(null, "HPPSU", "Total on time: %d days", await device.get_total_on_time() / 60 / 24);
                    }
                    break;
                    default:
                    {
                        throw new Error("PSU parameter not supported");
                    }
                    break;
                }
            }
            else if(device instanceof DS18B20)
            {
                await device.measure();

                switch(param_name)
                {
                    case "":
                    {
                        this.tprintln(null, "DS18B20", "Temperature: %d C", await device.get_temperature());
                        this.tprintln(null, "DS18B20", "High Alarm Temperature: %d C", (await device.get_alarm()).high);
                        this.tprintln(null, "DS18B20", "Low Alarm Temperature: %d C", (await device.get_alarm()).low);
                        this.tprintln(null, "DS18B20", "Parasidic powered: %s", await device.is_parasidic_power());
                    }
                    break;
                    default:
                    {
                        throw new Error("DS18B20 parameter not supported");
                    }
                    break;
                }
            }
            else if(device instanceof BME280)
            {
                switch(param_name)
                {
                    case "":
                    {
                        this.tprintln(null, "BME280", "Temperature: %d C", await device.get_temperature());
                        this.tprintln(null, "BME280", "Humidity: %d %%RH", await device.get_humidity());
                        this.tprintln(null, "BME280", "Pressure: %d hPa", await device.get_pressure());
                        this.tprintln(null, "BME280", "Dew point: %d C", await device.get_dew_point());
                        this.tprintln(null, "BME280", "Heat index: %d C", await device.get_heat_index());
                        this.tprintln(null, "BME280", "Altitude: %d m", await device.get_altitude());
                    }
                    break;
                    default:
                    {
                        throw new Error("BME280 parameter not supported");
                    }
                    break;
                }
            }
            else if(device instanceof OAQ)
            {
                switch(param_name)
                {
                    case "":
                    {
                        let stable = await device.is_stable();

                        if(!stable)
                        {
                            this.tprintln("yellow", "OAQ (ZMOD4510)", "Sensor not yet stable!");
                        }
                        else
                        {
                            this.tprintln(null, "OAQ (ZMOD4510)", "NO2 concentration: %d", await device.get_no2_concentration());
                            this.tprintln(null, "OAQ (ZMOD4510)", "O3 concentration: %d", await device.get_o3_concentration());
                            this.tprintln(null, "OAQ (ZMOD4510)", "Air Quality Index: %d (%s)", await device.get_aqi(), await device.get_loc());
                        }
                    }
                    break;
                    default:
                    {
                        throw new Error("OAQ parameter not supported");
                    }
                    break;
                }
            }
            else if(device instanceof SI1133)
            {
                switch(param_name)
                {
                    case "":
                    {
                        await device.measure();

                        this.tprintln(null, "SI1133", "Visible light: %d Lux", await device.get_lux());
                        this.tprintln(null, "SI1133", "UV Index: %d", await device.get_uv());
                    }
                    break;
                    default:
                    {
                        throw new Error("SI1133 parameter not supported");
                    }
                    break;
                }
            }
            else if(device instanceof MCP3221)
            {
                switch(param_name)
                {
                    case "":
                    {
                        let samples = argv[2] || 1;

                        this.tprintln(null, "MCP3221", "Voltage: %d mV", await device.get_voltage(samples));
                    }
                    break;
                    default:
                    {
                        throw new Error("MCP3221 parameter not supported");
                    }
                    break;
                }
            }
            else if(device instanceof LTC5597)
            {
                switch(param_name)
                {
                    case "":
                    {
                        let samples = argv[2] || 3;
                        let gain = argv[3] || 2;

                        this.tprintln(null, "LTC5597", "Power: %d dBm", await device.get_power_level(gain, samples));
                    }
                    break;
                    default:
                    {
                        throw new Error("LTC5597 parameter not supported");
                    }
                    break;
                }
            }
            else if(device instanceof F2915)
            {
                switch(param_name)
                {
                    case "":
                    {
                        this.tprintln(null, "F2915", "Powered: %s", await device.is_powered());
                        this.tprintln(null, "F2915", "Selected RF path: %d", await device.get_rf_path());
                    }
                    break;
                    default:
                    {
                        throw new Error("F2915 parameter not supported");
                    }
                    break;
                }
            }
            else if(device instanceof RelayController)
            {
                switch(param_name)
                {
                    case "":
                    {
                        this.tprintln(null, "Relay Controller", "Unique ID: %s", await device.get_unique_id());
                        this.tprintln(null, "Relay Controller", "Software Version: v%d", await device.get_software_version());

                        let chip_voltages = await device.get_chip_voltages();
                        this.tprintln(null, "Relay Controller", "AVDD Voltage: %d mV", chip_voltages.avdd);
                        this.tprintln(null, "Relay Controller", "DVDD Voltage: %d mV", chip_voltages.dvdd);
                        this.tprintln(null, "Relay Controller", "IOVDD Voltage: %d mV", chip_voltages.iovdd);
                        this.tprintln(null, "Relay Controller", "Core Voltage: %d mV", chip_voltages.core);

                        let system_voltages = await device.get_system_voltages();
                        this.tprintln(null, "Relay Controller", "VIN Voltage: %d mV", system_voltages.vin);

                        let uvth = await device.set_relay_undervoltage_protection(true, 20000);
                        this.tprintln(null, "Relay Controller", "VIN Undervoltage threshold: %d mV", uvth);
                        this.tprintln(null, "Relay Controller", "VIN Undervoltage status: %s", (await device.is_undervoltage()) ? "LOW" : "OK");

                        let chip_temperatures = await device.get_chip_temperatures();
                        this.tprintln(null, "Relay Controller", "ADC Temperature: %d C", chip_temperatures.adc);
                        this.tprintln(null, "Relay Controller", "EMU Temperature: %d C", chip_temperatures.emu);
                    }
                    break;
                    case "relay_status":
                    {
                        let i = parseInt(argv[2]);

                        if(isNaN(i) || i < 0 || i > 11)
                        {
                            this.tprintln(null, "Relay Controller", "Relays:");

                            let rstatus = await device.get_relay_status();

                            for(i = 0; i < 12; i++)
                            {
                                this.tprintln(null, "Relay Controller", "  Relay #%d status: %s", i, (rstatus & (1 << i)) ? "ON" : "OFF");
                                this.tprintln(null, "Relay Controller", "  Relay #%d duty cycle: %d %%", i, await device.get_relay_duty_cycle(i) * 100);
                                this.tprintln(null, "Relay Controller", "  Relay #%d voltage: %d mV", i, await device.get_relay_voltage(i));
                            }
                        }
                        else
                        {
                            this.tprintln(null, "Relay Controller", "Relay #%d status: %s", i, (await device.get_relay_status(i)) ? "ON" : "OFF");
                            this.tprintln(null, "Relay Controller", "Relay #%d duty cycle: %d %%", i, await device.get_relay_duty_cycle(i) * 100);
                            this.tprintln(null, "Relay Controller", "Relay #%d voltage: %d mV", i, await device.get_relay_voltage(i));
                        }
                    }
                    break;
                    default:
                    {
                        throw new Error("RelayController parameter not supported");
                    }
                    break;
                }
            }
            else
            {
                throw new Error("Device not supported");
            }

            return true;
        },
        function (line)
        {
            const param_names = [];

            for(const device_name in devices)
            {
                const device = devices[device_name];

                param_names.push(device_name);

                let device_param_names = [];

                if(device instanceof HPPSU)
                    device_param_names = ["info", "status", "input", "output", "temperature", "fan_speed", "on_time"];
                else if(device instanceof DS18B20)
                    device_param_names = [];
                else if(device instanceof BME280)
                    device_param_names = [];
                else if(device instanceof OAQ)
                    device_param_names = [];
                else if(device instanceof SI1133)
                    device_param_names = [];
                else if(device instanceof MCP3221)
                    device_param_names = [];
                else if(device instanceof LTC5597)
                    device_param_names = [];
                else if(device instanceof F2915)
                    device_param_names = [];
                else if(device instanceof RelayController)
                    device_param_names = ["relay_status"];

                for(const param of device_param_names)
                    param_names.push(device_name + "." + param);
            }

            const hits = param_names.filter(
                function (c)
                {
                    return c.startsWith(line);
                }
            );

            return hits.length ? hits : param_names;
        }
    );
    ssh_server_add_command(
        "wdev",
        "Modify parameters of the device specified in the first argument",
        async function (argv)
        {
            let device_param_name = argv[1];

            if(typeof(device_param_name) != "string")
                throw new Error("Invalid device parameter name");

            device_param_name = device_param_name.toLowerCase();

            let param_name_start = device_param_name.indexOf(".");

            if(param_name_start === 0)
                throw new Error("Invalid device parameter name");

            if(param_name_start == -1)
                param_name_start = device_param_name.length;

            let device_name = device_param_name.slice(0, param_name_start);
            let device = devices[device_name];

            if(!device)
                throw new Error("Device not found");

            let param_name = device_param_name.slice(param_name_start + 1);

            if(device instanceof HPPSU)
            {
                switch(param_name)
                {
                    case "clear_peak_input_current":
                    {
                        await device.clear_peak_input_current();

                        this.tprintln(null, "HPPSU", "Peak Input Current: %d A", await device.get_peak_input_current());
                    }
                    break;
                    case "clear_peak_input_power":
                    {
                        await device.clear_peak_input_power();

                        this.tprintln(null, "HPPSU", "Peak Input Power: %d W", await device.get_peak_input_power());
                    }
                    break;
                    case "clear_peak_output_current":
                    {
                        await device.clear_peak_output_current();

                        this.tprintln(null, "HPPSU", "Peak Output Current: %d A", await device.get_peak_output_current());
                    }
                    break;
                    case "fan_speed":
                    {
                        let speed = parseInt(argv[2]);

                        await device.set_fan_target_speed(speed);

                        this.tprintln(null, "HPPSU", "Fan speed: %d RPM", await device.get_fan_speed());
                        this.tprintln(null, "HPPSU", "Fan target speed: %d RPM", await device.get_fan_target_speed());
                    }
                    break;
                    case "clear_on_time_and_energy":
                    {
                        await device.clear_on_time_and_energy();

                        this.tprintln(null, "HPPSU", "On time: %d s", await device.get_on_time());
                    }
                    break;
                    case "turn_on":
                    {
                        await device.set_enable(true);
                        await delay(100);

                        this.tprintln(null, "HPPSU", "Output Enabled: %s", await device.is_main_output_enabled());
                    }
                    break;
                    case "turn_off":
                    {
                        await device.set_enable(false);
                        await delay(100);

                        this.tprintln(null, "HPPSU", "Output Enabled: %s", await device.is_main_output_enabled());
                    }
                    break;
                    default:
                    {
                        throw new Error("PSU parameter not supported");
                    }
                    break;
                }
            }
            else if(device instanceof DS18B20)
            {
                await device.measure();

                switch(param_name)
                {
                    default:
                    {
                        throw new Error("DS18B20 parameter not supported");
                    }
                    break;
                }
            }
            else if(device instanceof BME280)
            {
                switch(param_name)
                {
                    default:
                    {
                        throw new Error("BME280 parameter not supported");
                    }
                    break;
                }
            }
            else if(device instanceof OAQ)
            {
                switch(param_name)
                {
                    default:
                    {
                        throw new Error("OAQ parameter not supported");
                    }
                    break;
                }
            }
            else if(device instanceof SI1133)
            {
                switch(param_name)
                {
                    default:
                    {
                        throw new Error("SI1133 parameter not supported");
                    }
                    break;
                }
            }
            else if(device instanceof MCP3221)
            {
                switch(param_name)
                {
                    default:
                    {
                        throw new Error("MCP3221 parameter not supported");
                    }
                    break;
                }
            }
            else if(device instanceof LTC5597)
            {
                switch(param_name)
                {
                    default:
                    {
                        throw new Error("LTC5597 parameter not supported");
                    }
                    break;
                }
            }
            else if(device instanceof F2915)
            {
                switch(param_name)
                {
                    case "rf_path":
                    {
                        let path = parseInt(argv[2]);

                        await device.set_rf_path(path);

                        this.tprintln(null, "F2915", "Selected RF path: %d", await device.get_rf_path());
                    }
                    break;
                    default:
                    {
                        throw new Error("F2915 parameter not supported");
                    }
                    break;
                }
            }
            else if(device instanceof RelayController)
            {
                switch(param_name)
                {
                    case "turn_relay_on":
                    {
                        let i = parseInt(argv[2]);

                        if(isNaN(i) || i < 0 || i > 11)
                            throw new Error("Invalid relay index");

                        await device.set_relay_status(i, true);

                        this.tprintln(null, "Relay Controller", "Relay #%d status: %s", i, (await device.get_relay_status(i)) ? "ON" : "OFF");
                        this.tprintln(null, "Relay Controller", "Relay #%d duty cycle: %d %%", i, await device.get_relay_duty_cycle(i) * 100);
                        this.tprintln(null, "Relay Controller", "Relay #%d voltage: %d mV", i, await device.get_relay_voltage(i));
                    }
                    break;
                    case "turn_relay_off":
                    {
                        let i = parseInt(argv[2]);

                        if(isNaN(i) || i < 0 || i > 11)
                            throw new Error("Invalid relay index");

                        await device.set_relay_status(i, false);

                        this.tprintln(null, "Relay Controller", "Relay #%d status: %s", i, (await device.get_relay_status(i)) ? "ON" : "OFF");
                        this.tprintln(null, "Relay Controller", "Relay #%d duty cycle: %d %%", i, await device.get_relay_duty_cycle(i) * 100);
                        this.tprintln(null, "Relay Controller", "Relay #%d voltage: %d mV", i, await device.get_relay_voltage(i));
                    }
                    break;
                    case "set_relay_voltage":
                    {
                        let i = parseInt(argv[2]);

                        if(isNaN(i) || i < 0 || i > 11)
                            throw new Error("Invalid relay index");

                        let voltage = parseInt(argv[3]);

                        if(isNaN(voltage))
                            throw new Error("Invalid relay index");

                        await device.set_relay_voltage(i, voltage);

                        this.tprintln(null, "Relay Controller", "Relay #%d status: %s", i, (await device.get_relay_status(i)) ? "ON" : "OFF");
                        this.tprintln(null, "Relay Controller", "Relay #%d duty cycle: %d %%", i, await device.get_relay_duty_cycle(i) * 100);
                        this.tprintln(null, "Relay Controller", "Relay #%d voltage: %d mV", i, await device.get_relay_voltage(i));
                    }
                    break;
                    default:
                    {
                        throw new Error("RelayController parameter not supported");
                    }
                    break;
                }
            }
            else
            {
                throw new Error("Device not supported");
            }

            return true;
        },
        function (line)
        {
            const param_names = [];

            for(const device_name in devices)
            {
                const device = devices[device_name];

                let device_param_names = [];

                if(device instanceof HPPSU)
                    device_param_names = ["clear_peak_input_current", "clear_peak_input_power", "clear_peak_output_current", "fan_speed", "clear_on_time_and_energy", "turn_on", "turn_off"];
                else if(device instanceof DS18B20)
                    device_param_names = [];
                else if(device instanceof BME280)
                    device_param_names = [];
                else if(device instanceof OAQ)
                    device_param_names = [];
                else if(device instanceof SI1133)
                    device_param_names = [];
                else if(device instanceof MCP3221)
                    device_param_names = [];
                else if(device instanceof LTC5597)
                    device_param_names = [];
                else if(device instanceof F2915)
                    device_param_names = ["rf_path"];
                else if(device instanceof RelayController)
                    device_param_names = ["turn_relay_on", "turn_relay_off", "set_relay_voltage"];

                for(const param of device_param_names)
                    param_names.push(device_name + "." + param);
            }

            const hits = param_names.filter(
                function (c)
                {
                    return c.startsWith(line);
                }
            );

            return hits.length ? hits : param_names;
        }
    );
    ssh_server_add_command(
        "lswbsig",
        "Prints the latest signals detected on the Wideband Transponder",
        async function (argv)
        {
            if(wb_signals.noise_power !== undefined)
                this.tprintln("grey", null, "Noise floor: %d dB", wb_signals.noise_power.toFixed(2));

            if(wb_signals.beacon)
            {
                const signal = wb_signals.beacon;

                this.tprintln("magenta", null, "Beacon:");
                this.tprintln("magenta", null, "  Center frequency: %d MHz", signal.full_center_freq.toFixed(3));
                this.tprintln("magenta", null, "  Full bandwidth: %d MHz", signal.full_bandwidth.toFixed(3));
                this.tprintln("magenta", null, "  Full power: %d dB", signal.full_power.toFixed(2));
                this.tprintln("magenta", null, "  Used bandwidth: %d MHz", signal.used_bandwidth.toFixed(3));
                this.tprintln("magenta", null, "  Used power: %d dB", signal.used_power.toFixed(2));
                this.tprintln("magenta", null, "  Symbol rate: %d ksps", signal.symbolrate.toFixed(0));
                this.tprintln("magenta", null, "  SNR: %d dB", signal.snr.toFixed(2));
            }
            else
            {
                this.tprintln("red", null, "No beacon found!");

                return false;
            }

            if(!wb_signals.signals.length)
                this.tprintln(null, null, "No signals found!");

            for(const signal of wb_signals.signals)
            {
                let color = "brightGreen";

                if(signal.out_of_band)
                    color = "yellow";
                if(signal.over_powered)
                    color = "red";

                this.tprintln(color, null, "Signal:");
                this.tprintln(color, null, "  Center frequency: %d MHz", signal.full_center_freq.toFixed(3));
                this.tprintln(color, null, "  Full bandwidth: %d MHz", signal.full_bandwidth.toFixed(3));
                this.tprintln(color, null, "  Full power: %d dB", signal.full_power.toFixed(2));
                this.tprintln(color, null, "  Used bandwidth: %d MHz", signal.used_bandwidth.toFixed(3));
                this.tprintln(color, null, "  Used power: %d dB", signal.used_power.toFixed(2));
                this.tprintln(color, null, "  Symbol rate: %d ksps", signal.symbolrate.toFixed(0));
                this.tprintln(color, null, "  SNR: %d dB", signal.snr.toFixed(2));
                this.tprintln(color, null, "  SBR: %d dB", signal.sbr.toFixed(2));
            }

            return true;
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

        for(const key of keys)
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
        let cmd_name = tokens[0];
        let cmd = ssh_commands[cmd_name];

        if(cmd && typeof(cmd.completer) == "function")
            hits = cmd.completer(tokens.slice(1).join(" "));

        let checkpos = line.length - cmd_name.length - 1;
        let char = "";
        let equal = true;

        for(let i = 0; i < hits.length; i++)
        {
            if(!char)
                char = hits[i][checkpos];

            if(hits[i][checkpos] !== char)
            {
                equal = false;

                break;
            }
        }

        if(equal)
        {
            for(let i = 0; i < hits.length; i++)
                hits[i] = cmd_name + " " + hits[i];
        }
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

    if(hits.length == 1 && line == hits[0])
        hits[0] += " ";

    return [hits.length ? hits : command_names, line];
}
async function ssh_server_process_command_string(str)
{
    if(typeof(str) != "string")
        return;

    cl.tprintln("blue", "SSH", "Processing command string '%s' from client at %s...", str, this.stream.session.client.ip);

    let argv = ShellQuote.parse(str);
    let current_argv = [];

    for(const arg of argv)
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
function ssh_server_connection_handler(client, info)
{
    client.ip = info.ip;

    cl.tprintln("magenta", "SSH", "New client connected! IP: %s (%s)!", client.ip, info.header.identRaw);

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

    for(const user of ssh_allowed_users)
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

            for(const key of found_user.keys)
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
    channel.printer = new Printer(channel, true);

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

async function wb_spectrum_monitor_init()
{
    wb_spectrum_monitor = new WBSpectrumMonitor();

    wb_spectrum_monitor.on("open", wb_spectrum_monitor_open_handler);
    wb_spectrum_monitor.on("signals", wb_spectrum_monitor_signals_handler);
    wb_spectrum_monitor.on("error", wb_spectrum_monitor_error_handler);
    wb_spectrum_monitor.on("close", wb_spectrum_monitor_close_handler);
    wb_spectrum_monitor.on("timeout", wb_spectrum_monitor_timeout_handler);
}
function wb_spectrum_monitor_open_handler(ws)
{
    cl.tprintln("green", "WBTPX", "WebSocket listening!");
}
function wb_spectrum_monitor_signals_handler(signals)
{
    wb_signals = signals;
}
function wb_spectrum_monitor_error_handler(e)
{
    cl.tprintln("red", "WBTPX", "Error: " + e);
}
function wb_spectrum_monitor_close_handler(code, reason)
{
    cl.tprintln("yellow", "WBTPX", "WebSocket closed (%d)!", code);
}
function wb_spectrum_monitor_timeout_handler(ws)
{
    cl.tprintln("yellow", "WBTPX", "WebSocket timed out!");
}

async function ipma_init()
{
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

    // BME280 sea level pressure for altitude calculation
    ipma_fetch_sea_hpa();
    setInterval(ipma_fetch_sea_hpa, 1200000); // Update every 20 minutes
}
async function ipma_fetch_sea_hpa()
{
    if(!(ipma instanceof IPMA))
        cl.tprintln("red", "IPMA", "No valid IPMA instance defined");

    try
    {
        let sea_hpa = (await ipma.get_latest_surface_observation()).pressao;

        if(sea_hpa < 0)
            return cl.tprintln("yellow", "IPMA", "Got invalid Sea pressure from IPMA: %d hPa", sea_hpa);

        cl.tprintln("green", "IPMA", "Got IPMA Sea pressure: %d hPa", sea_hpa);

        BME280.set_sea_pressure(sea_hpa);
    }
    catch(e)
    {
        cl.tprintln("red", "IPMA", "Error fetching from IPMA: " + e);
    }
}

async function main()
{
    // Console and logging
    log_init();

    // IPMA
    await ipma_init();

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
    buses.i2c = [];
    buses.i2c.push(await I2C.open(0));
    buses.i2c.push(await I2C.open(1));

    //// OneWire bus Controllers
    buses.onewire = [];

    const onewire_bus_controllers = [];
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

        for(const device of devices)
        {
            cl.tprintln(null, "OneWire", "    %s", device);

            if(device.get_family_name() == "DS18B20")
            {
                cl.tprintln("green", "OneWire", "      DS18B20 #%d found!", i);

                let sensor = new DS18B20(device);

                // TODO: Check code and add accordingly to sensors array
                devices["ds18b20_" + sensor.get_uid().toString(16)] = sensor;

                await sensor.config(12);
                await sensor.measure();

                cl.tprintln(null, "DS18B20", "        Temperature: %d C", await sensor.get_temperature());
                cl.tprintln(null, "DS18B20", "        High Alarm Temperature: %d C", (await sensor.get_alarm()).high);
                cl.tprintln(null, "DS18B20", "        Low Alarm Temperature: %d C", (await sensor.get_alarm()).low);
                cl.tprintln(null, "DS18B20", "        Parasidic powered: %s", await sensor.is_parasidic_power());
            }
        }
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

        if(i == 0)
        {
            devices["thp_inside_sensor"] = sensor;
        }
        else if(i == 1)
        {
            devices["thp_outside_sensor"] = sensor;
        }
        else
        {
            devices["bme280_" + i] = sensor;
        }

        await sensor.measure(false);
        await delay(125 * 16);

        cl.tprintln(null, "BME280", "  Temperature: %d C", await sensor.get_temperature());
        cl.tprintln(null, "BME280", "  Humidity: %d %%RH", await sensor.get_humidity());
        cl.tprintln(null, "BME280", "  Pressure: %d hPa", await sensor.get_pressure());
        cl.tprintln(null, "BME280", "  Dew point: %d C", await sensor.get_dew_point());
        cl.tprintln(null, "BME280", "  Heat index: %d C", await sensor.get_heat_index());
        cl.tprintln(null, "BME280", "  Altitude: %d m", await sensor.get_altitude());
    }

    //// ZMOD4510
    const zmod4510_sensors = [];
    zmod4510_sensors.push(new ZMOD4510(buses.i2c[0], gpios.ext_i2c_enable[0]));

    for(let i = 0; i < zmod4510_sensors.length; i++)
    {
        let sensor = zmod4510_sensors[i];

        try
        {
            await sensor.probe();
        }
        catch (e)
        {
            cl.tprintln("red", "ZMOD4510", e);

            continue;
        }

        cl.tprintln("green", "ZMOD4510", "ZMOD4510 #%d found!", i);

        await sensor.config();

        let oaq = new OAQ(sensor);

        await oaq.config();
        await oaq.start_measuring();

        if(i == 0)
        {
            devices["oaq_out_sensor"] = oaq;
        }
        else
        {
            devices["zmod4510_oaq_" + i] = oaq;
        }
    }

    //// TODO: ZMOD4410 - not really needed...

    //// SI1133
    const si1133_sensors = [];
    si1133_sensors.push(new SI1133(buses.i2c[0], 0, gpios.ext_i2c_enable[0]));
    si1133_sensors.push(new SI1133(buses.i2c[0], 1, gpios.ext_i2c_enable[0]));

    for(let i = 0; i < si1133_sensors.length; i++)
    {
        let sensor = si1133_sensors[i];

        try
        {
            await sensor.probe();
        }
        catch (e)
        {
            cl.tprintln("red", "SI1133", e);

            continue;
        }

        cl.tprintln("green", "SI1133", "SI1133 #%d found!", i);

        await sensor.config();

        if(i == 0)
        {
            devices["light_inside_sensor"] = sensor;
        }
        else if(i == 1)
        {
            devices["light_outside_sensor"] = sensor;
        }
        else
        {
            devices["si1133_" + i] = sensor;
        }

        await sensor.measure();

        cl.tprintln(null, "SI1133", "  Visible light: %d Lux", await sensor.get_lux());
        cl.tprintln(null, "SI1133", "  UV Index: %d", await sensor.get_uv());
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
            sensor.set_reference(3000); // 3V reference voltage regulator
            sensor.set_scale_factor((21e3 + 147e3) / 21e3 * 1.04675); // Voltage divider and correction factor

            devices["vin_voltage_adc"] = sensor;
        }
        else
        {
            devices["mcp3421_" + i] = sensor;
        }

        cl.tprintln(null, "MCP3221", "  Voltage: %d mV", await sensor.get_voltage(10));
    }

    //// LTC5597 (RFPowerMeter with MCP3421 ADC)
    const ltc5597_sensors = [];
    ltc5597_sensors.push(new LTC5597(buses.i2c[0], 0, gpios.ext_i2c_enable[0]));

    for(let i = 0; i < ltc5597_sensors.length; i++)
    {
        let sensor = ltc5597_sensors[i];

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

            devices["tx_fwd_rf_power_sensor"] = sensor;
        }
        else
        {
            devices["ltc5597_" + i] = sensor;
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

        cl.tprintln(null, "LTC5597", "  Power: %d dBm", await sensor.get_power_level(2, 3) + 18.1 + 20);
    }

    //// LTC4151
    const ltc4151_sensors = [];
    ltc4151_sensors.push(new LTC4151(buses.i2c[0], 8, gpios.ext_i2c_enable[0]));

    for(let i = 0; i < ltc4151_sensors.length; i++)
    {
        let sensor = ltc4151_sensors[i];

        try
        {
            await sensor.probe();
        }
        catch (e)
        {
            cl.tprintln("red", "LTC4151", e);

            continue;
        }

        cl.tprintln("green", "LTC4151", "LTC4151 #%d found!", i);

        await sensor.config();

        sensor.set_current_shunt_value(0.012 / 3);
        sensor.set_adin_scale_factor((165 + 10) / 10); // Voltage divider factor

        if(false)
        {
            devices["ltc4151_" + i] = sensor;
        }
        else
        {
            devices["ltc4151_" + i] = sensor;
        }

        cl.tprintln(null, "LTC4151", "  VIN Voltage: %d mV", await sensor.get_vin_voltage(10));
        cl.tprintln(null, "LTC4151", "  Load Voltage: %d mV", await sensor.get_adin_voltage(10));
        cl.tprintln(null, "LTC4151", "  Load Current: %d mA", await sensor.get_current(10));
    }

    //// F2915 (RFSwitch with MCP23008 IO Controller)
    const f2915_rf_switches = [];
    f2915_rf_switches.push(new F2915(buses.i2c[0], 0, gpios.ext_i2c_enable[1]));
    f2915_rf_switches.push(new F2915(buses.i2c[0], 1, gpios.ext_i2c_enable[1]));
    f2915_rf_switches.push(new F2915(buses.i2c[0], 2, gpios.ext_i2c_enable[1]));

    for(let i = 0; i < f2915_rf_switches.length; i++)
    {
        let rf_switch = f2915_rf_switches[i];

        try
        {
            await rf_switch.probe();
        }
        catch (e)
        {
            cl.tprintln("red", "F2915", e);

            continue;
        }

        cl.tprintln("green", "F2915", "RF Switch #%d found!", i);

        await rf_switch.config();
        await rf_switch.set_power_enable(true);
        await rf_switch.set_rf_path(5);

        if(i == 0)
        {
            devices["tx_if_switch"] = rf_switch;
        }
        else
        {
            devices["f2915_rf_switch_" + i] = rf_switch;
        }

        cl.tprintln(null, "F2915", "  Powered: %s", await rf_switch.is_powered());
        cl.tprintln(null, "F2915", "  Selected RF path: %d", await rf_switch.get_rf_path());
    }

    //// Controllers
    // Relay Controller
    const relay_controllers = [];
    relay_controllers.push(new RelayController(buses.i2c[0], gpios.ext_i2c_enable[0]));

    for(let i = 0; i < relay_controllers.length; i++)
    {
        let controller = relay_controllers[i];

        try
        {
            await controller.probe();
        }
        catch (e)
        {
            cl.tprintln("red", "Relay Controller", e);

            continue;
        }

        cl.tprintln("green", "Relay Controller", "Relay Controller #%d found!", i);

        await controller.reset();
        await delay(5500);

        if(i == 0)
        {
            devices["relay_controller"] = controller;
        }
        else
        {
            devices["relay_controller_" + i] = controller;
        }

        cl.tprintln(null, "Relay Controller", "  Unique ID: %s", await controller.get_unique_id());
        cl.tprintln(null, "Relay Controller", "  Software Version: v%d", await controller.get_software_version());

        let chip_voltages = await controller.get_chip_voltages();
        cl.tprintln(null, "Relay Controller", "  AVDD Voltage: %d mV", chip_voltages.avdd);
        cl.tprintln(null, "Relay Controller", "  DVDD Voltage: %d mV", chip_voltages.dvdd);
        cl.tprintln(null, "Relay Controller", "  IOVDD Voltage: %d mV", chip_voltages.iovdd);
        cl.tprintln(null, "Relay Controller", "  Core Voltage: %d mV", chip_voltages.core);

        let system_voltages = await controller.get_system_voltages();
        cl.tprintln(null, "Relay Controller", "  VIN Voltage: %d mV", system_voltages.vin);

        let uvth = await controller.set_relay_undervoltage_protection(true, 20000);
        cl.tprintln(null, "Relay Controller", "  VIN Undervoltage threshold: %d mV", uvth);
        cl.tprintln(null, "Relay Controller", "  VIN Undervoltage status: %s", (await controller.is_undervoltage()) ? "LOW" : "OK");

        let chip_temperatures = await controller.get_chip_temperatures();
        cl.tprintln(null, "Relay Controller", "  ADC Temperature: %d C", chip_temperatures.adc);
        cl.tprintln(null, "Relay Controller", "  EMU Temperature: %d C", chip_temperatures.emu);

        cl.tprintln(null, "Relay Controller", "  Relays:");

        let rstatus = await controller.get_relay_status();

        for(let j = 0; j < 12; j++)
        {
            cl.tprintln(null, "Relay Controller", "    Relay #%d status: %s", j, (rstatus & (1 << j)) ? "ON" : "OFF");
            cl.tprintln(null, "Relay Controller", "    Relay #%d duty cycle: %d %%", j, await controller.get_relay_duty_cycle(j) * 100);
            cl.tprintln(null, "Relay Controller", "    Relay #%d voltage: %d mV", j, await controller.get_relay_voltage(j));
        }
    }

    // PA Bias Controller
    const pa_bias_controllers = [];
    pa_bias_controllers.push(new PABiasController(buses.i2c[0], gpios.ext_i2c_enable[0]));

    for(let i = 0; i < pa_bias_controllers.length; i++)
    {
        let controller = pa_bias_controllers[i];

        try
        {
            await controller.probe();
        }
        catch (e)
        {
            cl.tprintln("red", "PA Bias Controller", e);

            continue;
        }

        cl.tprintln("green", "PA Bias Controller", "PA Bias Controller #%d found!", i);

        await controller.reset();
        await delay(5500);

        if(i == 0)
        {
            devices["pa_bias_controller"] = controller;
        }
        else
        {
            devices["pa_bias_controller_" + i] = controller;
        }

        cl.tprintln(null, "PA Bias Controller", "  Unique ID: %s", await controller.get_unique_id());
        cl.tprintln(null, "PA Bias Controller", "  Software Version: v%d", await controller.get_software_version());

        let chip_voltages = await controller.get_chip_voltages();
        cl.tprintln(null, "PA Bias Controller", "  AVDD Voltage: %d mV", chip_voltages.avdd);
        cl.tprintln(null, "PA Bias Controller", "  DVDD Voltage: %d mV", chip_voltages.dvdd);
        cl.tprintln(null, "PA Bias Controller", "  IOVDD Voltage: %d mV", chip_voltages.iovdd);
        cl.tprintln(null, "PA Bias Controller", "  Core Voltage: %d mV", chip_voltages.core);

        let system_voltages = await controller.get_system_voltages();
        cl.tprintln(null, "PA Bias Controller", "  VIN Voltage: %d mV", system_voltages.vin);
        cl.tprintln(null, "PA Bias Controller", "  5V0 Voltage: %d mV", system_voltages.v5v0);

        let chip_temperatures = await controller.get_chip_temperatures();
        cl.tprintln(null, "PA Bias Controller", "  ADC Temperature: %d C", chip_temperatures.adc);
        cl.tprintln(null, "PA Bias Controller", "  EMU Temperature: %d C", chip_temperatures.emu);

        let system_temperatures = await controller.get_system_temperatures();
        cl.tprintln(null, "PA Bias Controller", "  AFE Temperature: %d C", system_temperatures.afe);

        cl.tprintln(null, "PA Bias Controller", "  TECs:");

        for(let j = 0; j < 4; j++)
        {
            cl.tprintln(null, "PA Bias Controller", "    TEC #%d status: %s", j, false ? "ON" : "OFF");
            cl.tprintln(null, "PA Bias Controller", "    TEC #%d voltage: %d mV", j, await controller.get_tec_voltage(j));
        }

        cl.tprintln(null, "PA Bias Controller", "  PAs:");

        for(let j = 0; j < 2; j++)
        {
            let data = await controller.get_pa_telemetry(j);

            cl.tprintln(null, "PA Bias Controller", "    PA #%d VGG Raw Voltage: %d mV", j, data.vgg_raw);
            cl.tprintln(null, "PA Bias Controller", "    PA #%d VGG Voltage: %d mV", j, data.vgg);
            cl.tprintln(null, "PA Bias Controller", "    PA #%d VDD Voltage: %d mV", j, data.vdd);
            cl.tprintln(null, "PA Bias Controller", "    PA #%d VDD Current: %d mA", j, data.idd);
            cl.tprintln(null, "PA Bias Controller", "    PA #%d Temperature: %d C", j, data.temperature);
        }
    }

    // LNB Controller
    const lnb_controllers = [];
    lnb_controllers.push(new LNBController(buses.i2c[0], gpios.ext_i2c_enable[0]));

    for(let i = 0; i < lnb_controllers.length; i++)
    {
        let controller = lnb_controllers[i];

        try
        {
            await controller.probe();
        }
        catch (e)
        {
            cl.tprintln("red", "LNB Controller", e);

            continue;
        }

        cl.tprintln("green", "LNB Controller", "LNB Controller #%d found!", i);

        await controller.reset();
        await delay(5500);

        if(i == 0)
        {
            devices["lnb_controller"] = controller;
        }
        else
        {
            devices["lnb_controller_" + i] = controller;
        }

        cl.tprintln(null, "LNB Controller", "  Unique ID: %s", await controller.get_unique_id());
        cl.tprintln(null, "LNB Controller", "  Software Version: v%d", await controller.get_software_version());

        let chip_voltages = await controller.get_chip_voltages();
        cl.tprintln(null, "LNB Controller", "  AVDD Voltage: %d mV", chip_voltages.avdd);
        cl.tprintln(null, "LNB Controller", "  DVDD Voltage: %d mV", chip_voltages.dvdd);
        cl.tprintln(null, "LNB Controller", "  IOVDD Voltage: %d mV", chip_voltages.iovdd);
        cl.tprintln(null, "LNB Controller", "  Core Voltage: %d mV", chip_voltages.core);

        let system_voltages = await controller.get_system_voltages();
        cl.tprintln(null, "LNB Controller", "  VIN Voltage: %d mV", system_voltages.vin);
        cl.tprintln(null, "LNB Controller", "  5V0 Voltage: %d mV", system_voltages.v5v0);

        let chip_temperatures = await controller.get_chip_temperatures();
        cl.tprintln(null, "LNB Controller", "  ADC Temperature: %d C", chip_temperatures.adc);
        cl.tprintln(null, "LNB Controller", "  EMU Temperature: %d C", chip_temperatures.emu);
    }

    // Upconverter
    const upconverters = [];
    upconverters.push(new Upconverter(buses.i2c[0], gpios.ext_i2c_enable[0]));

    for(let i = 0; i < upconverters.length; i++)
    {
        let controller = upconverters[i];

        try
        {
            await controller.probe();
        }
        catch (e)
        {
            cl.tprintln("red", "Upconverter", e);

            continue;
        }

        cl.tprintln("green", "Upconverter", "Upconverter #%d found!", i);

        await controller.reset();
        await delay(5500);

        if(i == 0)
        {
            devices["upconverter"] = controller;
        }
        else
        {
            devices["upconverter_" + i] = controller;
        }

        cl.tprintln(null, "Upconverter", "  Unique ID: %s", await controller.get_unique_id());
        cl.tprintln(null, "Upconverter", "  Software Version: v%d", await controller.get_software_version());

        let chip_voltages = await controller.get_chip_voltages();
        cl.tprintln(null, "Upconverter", "  AVDD Voltage: %d mV", chip_voltages.avdd);
        cl.tprintln(null, "Upconverter", "  DVDD Voltage: %d mV", chip_voltages.dvdd);
        cl.tprintln(null, "Upconverter", "  IOVDD Voltage: %d mV", chip_voltages.iovdd);
        cl.tprintln(null, "Upconverter", "  Core Voltage: %d mV", chip_voltages.core);

        let system_voltages = await controller.get_system_voltages();
        cl.tprintln(null, "Upconverter", "  VIN Voltage: %d mV", system_voltages.vin);
        cl.tprintln(null, "Upconverter", "  5V0 Voltage: %d mV", system_voltages.v5v0);

        let system_currents = await controller.get_system_currents();
        cl.tprintln(null, "Upconverter", "  5V0 Current: %d mA", system_currents.i5v0);

        let chip_temperatures = await controller.get_chip_temperatures();
        cl.tprintln(null, "Upconverter", "  ADC Temperature: %d C", chip_temperatures.adc);
        cl.tprintln(null, "Upconverter", "  EMU Temperature: %d C", chip_temperatures.emu);
    }

    //// PSU
    // PSU GPIO Controlers
    const psu_gpio_controllers = [];
    psu_gpio_controllers.push(new MCP23008(buses.i2c[1], 0, gpios.psu_i2c_enable[0]));
    psu_gpio_controllers.push(new MCP23008(buses.i2c[1], 0, gpios.psu_i2c_enable[1]));

    for(let i = 0; i < psu_gpio_controllers.length; i++)
    {
        let gpio_controller = psu_gpio_controllers[i];

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
    const psus = [];
    psus.push(new HPPSU(buses.i2c[1], 7, gpios.psu_i2c_enable[0], psu_gpio_controllers[0].get_gpio("PA0"), psu_gpio_controllers[0].get_gpio("PA4")));
    psus.push(new HPPSU(buses.i2c[1], 7, gpios.psu_i2c_enable[1], psu_gpio_controllers[1].get_gpio("PA0"), psu_gpio_controllers[1].get_gpio("PA4")));

    for(let i = 0; i < psus.length; i++)
    {
        let psu = psus[i];
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

        devices["psu" + i] = psu;

        cl.tprintln(null, "HPPSU", "  ID: 0x%s", (await psu.get_id()).toString(16));
        cl.tprintln(null, "HPPSU", "  SPN: %s", await psu.get_spn());
        cl.tprintln(null, "HPPSU", "  Date: %s", await psu.get_date());
        cl.tprintln(null, "HPPSU", "  Name: %s", await psu.get_name());
        cl.tprintln(null, "HPPSU", "  CT: %s", await psu.get_ct());
        cl.tprintln(null, "HPPSU", "  Input Present: %s", await psu.is_input_present());
        cl.tprintln(null, "HPPSU", "  Input Voltage: %d V", await psu.get_input_voltage());
        cl.tprintln(null, "HPPSU", "  Input Undervoltage threshold: %d V", await psu.get_input_undervoltage_threshold());
        cl.tprintln(null, "HPPSU", "  Input Overvoltage threshold: %d V", await psu.get_input_overvoltage_threshold());
        cl.tprintln(null, "HPPSU", "  Input Current: %d A (max. %d A)", await psu.get_input_current(), await psu.get_peak_input_current());
        cl.tprintln(null, "HPPSU", "  Input Power: %d W (max. %d W)", await psu.get_input_power(), await psu.get_peak_input_power());
        cl.tprintln(null, "HPPSU", "  Input Energy: %d Wh", await psu.get_input_energy());
        cl.tprintln(null, "HPPSU", "  Output Enabled: %s", await psu.is_main_output_enabled());
        cl.tprintln(null, "HPPSU", "  Output Voltage: %d V", await psu.get_output_voltage());
        cl.tprintln(null, "HPPSU", "  Output Undervoltage threshold: %d V", await psu.get_output_undervoltage_threshold());
        cl.tprintln(null, "HPPSU", "  Output Overvoltage threshold: %d V", await psu.get_output_overvoltage_threshold());
        cl.tprintln(null, "HPPSU", "  Output Current: %d A (max. %d A)", await psu.get_output_current(), await psu.get_peak_output_current());
        cl.tprintln(null, "HPPSU", "  Output Power: %d W", await psu.get_output_power());
        cl.tprintln(null, "HPPSU", "  Intake Temperature: %d C", await psu.get_intake_temperature());
        cl.tprintln(null, "HPPSU", "  Internal Temperature: %d C", await psu.get_internal_temperature());
        cl.tprintln(null, "HPPSU", "  Fan speed: %d RPM", await psu.get_fan_speed());
        cl.tprintln(null, "HPPSU", "  Fan target speed: %d RPM", await psu.get_fan_target_speed());
        cl.tprintln(null, "HPPSU", "  On time: %d s", await psu.get_on_time());
        cl.tprintln(null, "HPPSU", "  Total on time: %d days", await psu.get_total_on_time() / 60 / 24);
        cl.tprintln(null, "HPPSU", "  Status flags: 0x%s", (await psu.get_status_flags()).toString(16));

        await psu.set_enable(false);

        cl.tprintln("magenta", "HPPSU", "  Testing fan at max RPM...");
        await psu.set_fan_target_speed(17500);
        await delay(3000);
        let fan_speed = await psu.get_fan_speed();
        await psu.set_fan_target_speed(0);

        if(fan_speed >= 16000)
            cl.tprintln("green", "HPPSU", "    Fan test passed (%d)!", fan_speed);
        else
            cl.tprintln("yellow", "HPPSU", "    Fan test failed (%d)!", fan_speed);

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

    // Wideband Spectrum monitor
    await wb_spectrum_monitor_init();

    // SSH Server
    await ssh_server_init();
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