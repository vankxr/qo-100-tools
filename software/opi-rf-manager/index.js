const FileSystem = require("fs").promises;
const I2C = require("i2c-bus");
const IPMA = require("./lib/ipma");
const HPPSU = require("./lib/hppsu");
const BME280 = require("./lib/bme280");
const Logger = require("./util/logger");
const delay = require("./util/delay");

const cl = new Logger(__dirname + "/log", "console_");
const el = new Logger(__dirname + "/log/error", "console_error_");

async function main()
{
    let ipma_station = null;
    let ipma_location = null;

    try
    {
        let ipma_stations = await IPMA.fetch_stations();

        for(const station of ipma_stations)
        {
            if(!station.properties)
                continue;

            if(!station.properties.idEstacao)
                continue;

            if(!station.properties.localEstacao)
                continue;

            if(station.properties.localEstacao.indexOf("Leiria (Aeródromo)") === 0)
            {
                ipma_station = station;

                cl.ctprint("green", null, "IPMA station \"" + station.properties.localEstacao + "\" found!");

                break;
            }
        }

        if(!ipma_station)
            el.ctprint("yellow", null, "IPMA station not found!");

        let ipma_locations = await IPMA.fetch_locations();

        for(const location of ipma_locations)
        {
            if(!location.globalIdLocal)
                continue;

            if(!location.local)
                continue;

            if(location.local.indexOf("Leiria") === 0)
            {
                ipma_location = location;

                cl.ctprint("green", null, "IPMA location \"" + location.local + "\" found!");

                break;
            }
        }

        if(!ipma_location)
            el.ctprint("yellow", null, "IPMA location not found!");

        const IPMA0 = new IPMA(ipma_location, ipma_station);

        let sea_hpa = (await IPMA0.get_latest_surface_observation()).pressao;

        cl.ctprint(null, null, "IPMA Sea pressure: %d hPa", sea_hpa);

        BME280.set_sea_pressure(sea_hpa);
    }
    catch (e)
    {
        el.ctprint("red", null, e);
    }

    const I2C0 = await I2C.openPromisified(0);

    try
    {
        const BME = [new BME280(I2C0, 0), new BME280(I2C0, 1)];

        for(let i = 0; i < BME.length; i++)
        {
            let found = await BME[i].probe();

            if(found)
            {
                cl.ctprint("green", null, "BME280 #%d found!", i);

                await BME[i].read_calibration();
                await BME[i].config(
                    {
                        temperature: 16,
                        humidity: 16,
                        pressure: 16
                    },
                    16,
                    125
                );
                await BME[i].measure(false);

                cl.ctprint(null, null, "BME280 #%d Temperature: %d C", i, await BME[i].get_temperature());
                cl.ctprint(null, null, "BME280 #%d Humidity: %d %%RH", i, await BME[i].get_humidity());
                cl.ctprint(null, null, "BME280 #%d Pressure: %d hPa", i, await BME[i].get_pressure());
                cl.ctprint(null, null, "BME280 #%d Dew point: %d C", i, await BME[i].get_dew_point());
                cl.ctprint(null, null, "BME280 #%d Heat index: %d C", i, await BME[i].get_heat_index());
                cl.ctprint(null, null, "BME280 #%d Altitude: %d m", i, await BME[i].get_altitude());
            }
        }
    }
    catch (e)
    {
        el.ctprint("red", null, e);
    }

    try
    {
        const PSU = [new HPPSU(I2C0, 7)];

        for(let i = 0; i < PSU.length; i++)
        {
            let found = await PSU[i].probe();

            if(found)
            {
                cl.ctprint("green", null, "PSU #%d and EEPROM #%d found!", i, i);

                cl.ctprint(null, null, "PSU #%d ID: 0x%s", i, (await PSU[i].get_id()).toString(16));
                cl.ctprint(null, null, "PSU #%d SPN: %s", i, await PSU[i].get_spn());
                cl.ctprint(null, null, "PSU #%d Date: %s", i, await PSU[i].get_date());
                cl.ctprint(null, null, "PSU #%d Name: %s", i, await PSU[i].get_name());
                cl.ctprint(null, null, "PSU #%d CT: %s", i, await PSU[i].get_ct());
                cl.ctprint(null, null, "-------------------------------------");
                cl.ctprint(null, null, "PSU #%d Input Voltage: %d V", i, await PSU[i].get_input_voltage());
                cl.ctprint(null, null, "PSU #%d Input Undervoltage threshold: %d V", i, await PSU[i].get_input_undervoltage_threshold());
                cl.ctprint(null, null, "PSU #%d Input Overvoltage threshold: %d V", i, await PSU[i].get_input_overvoltage_threshold());
                cl.ctprint(null, null, "PSU #%d Input Current: %d A (max. %d A)", i, await PSU[i].get_input_current(), await PSU[i].get_peak_input_current());
                cl.ctprint(null, null, "PSU #%d Input Power: %d W (max. %d W)", i, await PSU[i].get_input_power(), await PSU[i].get_peak_input_power());
                cl.ctprint(null, null, "PSU #%d Input Energy: %d Wh", i, await PSU[i].get_input_energy());
                cl.ctprint(null, null, "PSU #%d Output Voltage: %d V", i, await PSU[i].get_output_voltage());
                cl.ctprint(null, null, "PSU #%d Output Undervoltage threshold: %d V", i, await PSU[i].get_output_undervoltage_threshold());
                cl.ctprint(null, null, "PSU #%d Output Overvoltage threshold: %d V", i, await PSU[i].get_output_overvoltage_threshold());
                cl.ctprint(null, null, "PSU #%d Output Current: %d A (max. %d A)", i, await PSU[i].get_output_current(), await PSU[i].get_peak_output_current());
                cl.ctprint(null, null, "PSU #%d Output Power: %d W", i, await PSU[i].get_output_power());
                cl.ctprint(null, null, "PSU #%d Intake Temperature: %d C", i, await PSU[i].get_intake_temperature());
                cl.ctprint(null, null, "PSU #%d Internal Temperature: %d C", i, await PSU[i].get_internal_temperature());
                cl.ctprint(null, null, "PSU #%d Fan speed: %d RPM", i, await PSU[i].get_fan_speed());
                cl.ctprint(null, null, "PSU #%d Fan target speed: %d RPM", i, await PSU[i].get_fan_target_speed());
                cl.ctprint(null, null, "PSU #%d On time: %d s", i, await PSU[i].get_on_time());
                cl.ctprint(null, null, "PSU #%d Total on time: %d hours", i, await PSU[i].get_total_on_time() / 60 / 24);
                cl.ctprint(null, null, "PSU #%d Status flags: 0x%s", i, (await PSU[i].get_status_flags()).toString(16));
                cl.ctprint(null, null, "PSU #%d ON: %s", i, await PSU[i].main_output_enabled());

                await PSU[i].set_fan_target_speed(0);

                //await PSU[i].clear_on_time_and_energy();
                //await PSU[i].clear_peak_input_current();
            }
        }
    }
    catch (e)
    {
        el.ctprint("red", null, e);
    }
}

main();
