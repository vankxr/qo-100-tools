const FileSystem = require('fs');
const I2C = require("i2c-bus");
const HPPSU = require("./lib/hppsu");
const Logger = require("./util/logger");
const delay = require("./util/delay");

const cl = new Logger(__dirname + "/log", "console_");
const el = new Logger(__dirname + "/log/error", "console_error_");

async function main()
{
    const I2C0 = await I2C.openPromisified(0);
    const PSU0 = new HPPSU(I2C0, 7);

    try
    {
        let found = await PSU0.probe();

        if(found)
            cl.ctprint("green", null, "PSU and EEPROM found!");
    }
    catch (e)
    {
        return el.ctprint("red", null, e);
    }

    cl.ctprint(null, null, "PSU SPN: %s", await PSU0.get_spn());
    cl.ctprint(null, null, "PSU Date: %s", await PSU0.get_date());
    cl.ctprint(null, null, "PSU Name: %s", await PSU0.get_name());
    cl.ctprint(null, null, "PSU CT: %s", await PSU0.get_ct());
    cl.ctprint(null, null, "-------------------------------------");
    cl.ctprint(null, null, "Input Voltage: %d V", await PSU0.get_input_voltage());
    cl.ctprint(null, null, "Input Current: %d A (max. %d A)", await PSU0.get_input_current(), await PSU0.get_peak_input_current());
    cl.ctprint(null, null, "Input Power: %d W (max. %d W)", await PSU0.get_input_power(), await PSU0.get_peak_input_power());
    cl.ctprint(null, null, "Input Energy: %d Wh", await PSU0.get_input_energy());
    cl.ctprint(null, null, "Output Voltage: %d V", await PSU0.get_output_voltage());
    cl.ctprint(null, null, "Output Current: %d A (max. %d A)", await PSU0.get_output_current(), await PSU0.get_peak_output_current());
    cl.ctprint(null, null, "Output Power: %d W", await PSU0.get_output_power());
    cl.ctprint(null, null, "Intake Temperature: %d C", await PSU0.get_intake_temperature());
    cl.ctprint(null, null, "Internal Temperature: %d C", await PSU0.get_internal_temperature());
    cl.ctprint(null, null, "Fan speed: %d RPM", await PSU0.get_fan_speed());
    cl.ctprint(null, null, "Fan target speed: %d RPM", await PSU0.get_fan_target_speed());
    cl.ctprint(null, null, "On time: %d s", await PSU0.get_on_time());
    cl.ctprint(null, null, "Total on time: %d hours", await PSU0.get_total_on_time() / 60 / 24);
    cl.ctprint(null, null, "Status flags: 0x" + (await PSU0.get_status_flags()).toString(16));
    cl.ctprint(null, null, "PSU ON: " + await PSU0.main_output_enabled());

    await PSU0.set_fan_target_speed(0);

    //await PSU0.clear_on_time_and_energy();
    //await PSU0.clear_peak_input_current();
}

main();
