const FileSystem = require("fs").promises;
const Mutex = require('async-mutex').Mutex;
const GPIO = require("./lib/gpio");
const { I2C, I2CDevice} = require("./lib/i2c");
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
const Logger = require("./util/logger");
const delay = require("./util/delay");

const cl = new Logger(__dirname + "/log", "console_");
const el = new Logger(__dirname + "/log/error", "console_error_");

async function main()
{
    // GPIOs
    const INTRUSION_GPIO = [
        await GPIO.export("PA0"),
        await GPIO.export("PA1")
    ];
    const EXT_BUS_GPIO = [
        await GPIO.export("PA6"),
        await GPIO.export("PA2")
    ];
    const PSU_BUS_GPIO = [
        await GPIO.export("PA7"),
        await GPIO.export("PA10")
    ];

    await INTRUSION_GPIO[0].set_direction(GPIO.INPUT);
    await INTRUSION_GPIO[0].set_pull(GPIO.PULLUP);
    await INTRUSION_GPIO[1].set_direction(GPIO.INPUT);
    await INTRUSION_GPIO[1].set_pull(GPIO.PULLUP);
    await EXT_BUS_GPIO[0].set_direction(GPIO.OUTPUT);
    await EXT_BUS_GPIO[0].set_value(GPIO.LOW);
    await EXT_BUS_GPIO[1].set_direction(GPIO.OUTPUT);
    await EXT_BUS_GPIO[1].set_value(GPIO.LOW);
    await PSU_BUS_GPIO[0].set_direction(GPIO.OUTPUT);
    await PSU_BUS_GPIO[0].set_value(GPIO.LOW);
    await PSU_BUS_GPIO[1].set_direction(GPIO.OUTPUT);
    await PSU_BUS_GPIO[1].set_value(GPIO.LOW);

    // Communication Interfaces
    const I2C_BUS = [
        await I2C.open(0),
        await I2C.open(1)
    ];

    // One Wire bus enumeration
    const OW_CONTROLLERS = [
        new DS2484(I2C_BUS[0])
    ];
    const OW_BUS = [null];

    let DS18B20_SENSORS = [];

    for(let i = 0; i < OW_CONTROLLERS.length; i++)
    {
        try
        {
            await OW_CONTROLLERS[i].probe();
        }
        catch (e)
        {
            el.ctprint("red", null, e);

            continue;
        }

        cl.ctprint("green", null, "DS2484 #%d found!", i);

        OW_CONTROLLERS[i].config();

        OW_BUS[i] = OW_CONTROLLERS[i].get_ow_bus();
        OW_BUS[i].mutex = new Mutex();

        let devices;

        try
        {
            devices = await OW_BUS[i].scan();
        }
        catch (e)
        {
            el.ctprint("red", null, "Error scanning OneWire bus %d: " + e, i);

            continue;
        }

        cl.ctprint(devices.length > 0 ? null : "yellow", null, "  Found %d devices on OneWire bus %d:", devices.length, i);

        for(device of devices)
        {
            cl.ctprint(null, null, "    %s", device);

            if(device.get_family_name() == "DS18B20")
            {
                let i = DS18B20_SENSORS.length;

                cl.ctprint("green", null, "      DS18B20 #%d found!", i);

                let sensor = new DS18B20(device);

                DS18B20_SENSORS.push(sensor);

                await sensor.config(12);
                await sensor.measure();

                cl.ctprint(null, null, "        DS18B20 #%d Temperature: %d C", i, await sensor.get_temperature());
                cl.ctprint(null, null, "        DS18B20 #%d High Alarm Temperature: %d C", i, (await sensor.get_alarm()).high);
                cl.ctprint(null, null, "        DS18B20 #%d Low Alarm Temperature: %d C", i, (await sensor.get_alarm()).low);
                cl.ctprint(null, null, "        DS18B20 #%d Parasidic powered: %s", i, await sensor.is_parasidic_power());
            }
        }
    }

    // IPMA
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

            //if(station.properties.localEstacao.indexOf("Leiria (Aeródromo)") === 0)
            if(station.properties.localEstacao.indexOf("Lisboa (G.Coutinho)") === 0)
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

            //if(location.local.indexOf("Leiria") === 0)
            if(location.local.indexOf("Lisboa") === 0)
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

        cl.ctprint(null, null, "  IPMA Sea pressure: %d hPa", sea_hpa);

        BME280.set_sea_pressure(sea_hpa);
    }
    catch (e)
    {
        el.ctprint("red", null, e);
    }

    // BME280
    const BME = [
        new BME280(I2C_BUS[0], 0, EXT_BUS_GPIO[0]),
        new BME280(I2C_BUS[0], 1, EXT_BUS_GPIO[0])
    ];

    for(let i = 0; i < BME.length; i++)
    {
        try
        {
            await BME[i].probe();
        }
        catch (e)
        {
            el.ctprint("red", null, e);

            continue;
        }

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

        cl.ctprint(null, null, "  BME280 #%d Temperature: %d C", i, await BME[i].get_temperature());
        cl.ctprint(null, null, "  BME280 #%d Humidity: %d %%RH", i, await BME[i].get_humidity());
        cl.ctprint(null, null, "  BME280 #%d Pressure: %d hPa", i, await BME[i].get_pressure());
        cl.ctprint(null, null, "  BME280 #%d Dew point: %d C", i, await BME[i].get_dew_point());
        cl.ctprint(null, null, "  BME280 #%d Heat index: %d C", i, await BME[i].get_heat_index());
        cl.ctprint(null, null, "  BME280 #%d Altitude: %d m", i, await BME[i].get_altitude());
    }

    // MCP3221
    const ADC = [
        new MCP3221(I2C_BUS[0], 0)
    ];

    for(let i = 0; i < ADC.length; i++)
    {
        try
        {
            await ADC[i].probe();
        }
        catch (e)
        {
            el.ctprint("red", null, e);

            continue;
        }

        cl.ctprint("green", null, "MCP3221 #%d found!", i);

        cl.ctprint(null, null, "  MCP3221 #%d Voltage: %d mV", i, (await ADC[i].get_voltage(50)) / (21 / (21 + 147)));
    }

    // LTC5597 (RFPowerMeter with MCP3421 ADC)
    const RFPowerMeter = [
        new LTC5597(I2C_BUS[0], 0, EXT_BUS_GPIO[0])
    ];

    for(let i = 0; i < RFPowerMeter.length; i++)
    {
        try
        {
            await RFPowerMeter[i].probe();
        }
        catch (e)
        {
            el.ctprint("red", null, e);

            continue;
        }

        cl.ctprint("green", null, "RFPowerMeter #%d found!", i);

        RFPowerMeter[i].load_calibration(
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
        RFPowerMeter[i].set_frequency(2400);

        await RFPowerMeter[i].config(14, true);

        cl.ctprint(null, null, "  RFPowerMeter #%d Power: %d dBm", i, await RFPowerMeter[i].get_power_level(2, 3));
    }

    // Relay Controller
    const RelayCon = [
        new RelayController(I2C_BUS[0], EXT_BUS_GPIO[1])
    ];

    for(let i = 0; i < RelayCon.length; i++)
    {
        try
        {
            await RelayCon[i].probe();
        }
        catch (e)
        {
            el.ctprint("red", null, e);

            continue;
        }

        cl.ctprint("green", null, "Relay Controller #%d found!", i);

        cl.ctprint(null, null, "  Relay Controller #%d Unique ID: %s", i, await RelayCon[i].get_unique_id());
        cl.ctprint(null, null, "  Relay Controller #%d Software Version: v%d", i, await RelayCon[i].get_software_version());
        cl.ctprint(null, null, "  Relay Controller #%d AVDD Voltage: %d mV", i, (await RelayCon[i].get_chip_voltages()).avdd);
        cl.ctprint(null, null, "  Relay Controller #%d DVDD Voltage: %d mV", i, (await RelayCon[i].get_chip_voltages()).dvdd);
        cl.ctprint(null, null, "  Relay Controller #%d IOVDD Voltage: %d mV", i, (await RelayCon[i].get_chip_voltages()).iovdd);
        cl.ctprint(null, null, "  Relay Controller #%d Core Voltage: %d mV", i, (await RelayCon[i].get_chip_voltages()).core);
        cl.ctprint(null, null, "  Relay Controller #%d VIN Voltage: %d mV", i, (await RelayCon[i].get_system_voltages()).vin);
        cl.ctprint(null, null, "  Relay Controller #%d ADC Temperature: %d C", i, (await RelayCon[i].get_chip_temperatures()).adc);
        cl.ctprint(null, null, "  Relay Controller #%d EMU Temperature: %d C", i, (await RelayCon[i].get_chip_temperatures()).emu);
    }

    // PSU GPIO Controlers
    const PSU_GPIO_CONTROLLER = [
        new MCP23008(I2C_BUS[1], 0, PSU_BUS_GPIO[0]),
        new MCP23008(I2C_BUS[1], 0, PSU_BUS_GPIO[1])
    ];
    const PSU_PRESENT_GPIO = [null, null];
    const PSU_ENABLE_GPIO = [null, null];

    for(let i = 0; i < PSU_GPIO_CONTROLLER.length; i++)
    {
        try
        {
            await PSU_GPIO_CONTROLLER[i].probe();
        }
        catch (e)
        {
            el.ctprint("red", null, e);

            continue;
        }

        cl.ctprint("green", null, "PSU GPIO Controller #%d found!", i);

        PSU_GPIO_CONTROLLER[i].config();

        PSU_PRESENT_GPIO[i] = PSU_GPIO_CONTROLLER[i].get_gpio("PA0");
        PSU_ENABLE_GPIO[i] = PSU_GPIO_CONTROLLER[i].get_gpio("PA4");

        PSU_PRESENT_GPIO[i].set_direction(GPIO.INPUT);
        PSU_PRESENT_GPIO[i].set_pull(GPIO.PULLUP);
        PSU_ENABLE_GPIO[i].set_direction(GPIO.INPUT);
        PSU_ENABLE_GPIO[i].set_pull(GPIO.PULLUP);
    }

    // HPPSU
    const PSU = [
        new HPPSU(I2C_BUS[1], 7, PSU_BUS_GPIO[0], PSU_PRESENT_GPIO[0], PSU_ENABLE_GPIO[0]),
        new HPPSU(I2C_BUS[1], 7, PSU_BUS_GPIO[1], PSU_PRESENT_GPIO[1], PSU_ENABLE_GPIO[1])
    ];

    for(let i = 0; i < PSU.length; i++)
    {
        let psu_present = true;

        try
        {
            psu_present = await PSU[i].is_present();

            cl.ctprint(psu_present ? "green" : "red", null, "PSU #%d Present: %s", i, psu_present);
        }
        catch (e)
        {
            el.ctprint("yellow", null, e);
        }

        if(!psu_present)
            continue;

        try
        {
            await PSU[i].probe();
        }
        catch (e)
        {
            el.ctprint("red", null, e);

            continue;
        }

        cl.ctprint("green", null, "PSU #%d and EEPROM #%d found!", i, i);

        //await PSU[i].set_enable(true);

        cl.ctprint(null, null, "  PSU #%d ID: 0x%s", i, (await PSU[i].get_id()).toString(16));
        cl.ctprint(null, null, "  PSU #%d SPN: %s", i, await PSU[i].get_spn());
        cl.ctprint(null, null, "  PSU #%d Date: %s", i, await PSU[i].get_date());
        cl.ctprint(null, null, "  PSU #%d Name: %s", i, await PSU[i].get_name());
        cl.ctprint(null, null, "  PSU #%d CT: %s", i, await PSU[i].get_ct());
        cl.ctprint(null, null, "  -------------------------------------");
        cl.ctprint(null, null, "  PSU #%d Input Voltage: %d V", i, await PSU[i].get_input_voltage());
        cl.ctprint(null, null, "  PSU #%d Input Undervoltage threshold: %d V", i, await PSU[i].get_input_undervoltage_threshold());
        cl.ctprint(null, null, "  PSU #%d Input Overvoltage threshold: %d V", i, await PSU[i].get_input_overvoltage_threshold());
        cl.ctprint(null, null, "  PSU #%d Input Current: %d A (max. %d A)", i, await PSU[i].get_input_current(), await PSU[i].get_peak_input_current());
        cl.ctprint(null, null, "  PSU #%d Input Power: %d W (max. %d W)", i, await PSU[i].get_input_power(), await PSU[i].get_peak_input_power());
        cl.ctprint(null, null, "  PSU #%d Input Energy: %d Wh", i, await PSU[i].get_input_energy());
        cl.ctprint(null, null, "  PSU #%d Output Voltage: %d V", i, await PSU[i].get_output_voltage());
        cl.ctprint(null, null, "  PSU #%d Output Undervoltage threshold: %d V", i, await PSU[i].get_output_undervoltage_threshold());
        cl.ctprint(null, null, "  PSU #%d Output Overvoltage threshold: %d V", i, await PSU[i].get_output_overvoltage_threshold());
        cl.ctprint(null, null, "  PSU #%d Output Current: %d A (max. %d A)", i, await PSU[i].get_output_current(), await PSU[i].get_peak_output_current());
        cl.ctprint(null, null, "  PSU #%d Output Power: %d W", i, await PSU[i].get_output_power());
        cl.ctprint(null, null, "  PSU #%d Intake Temperature: %d C", i, await PSU[i].get_intake_temperature());
        cl.ctprint(null, null, "  PSU #%d Internal Temperature: %d C", i, await PSU[i].get_internal_temperature());
        cl.ctprint(null, null, "  PSU #%d Fan speed: %d RPM", i, await PSU[i].get_fan_speed());
        cl.ctprint(null, null, "  PSU #%d Fan target speed: %d RPM", i, await PSU[i].get_fan_target_speed());
        cl.ctprint(null, null, "  PSU #%d On time: %d s", i, await PSU[i].get_on_time());
        cl.ctprint(null, null, "  PSU #%d Total on time: %d days", i, await PSU[i].get_total_on_time() / 60 / 24);
        cl.ctprint(null, null, "  PSU #%d Status flags: 0x%s", i, (await PSU[i].get_status_flags()).toString(16));
        cl.ctprint(null, null, "  PSU #%d ON: %s", i, await PSU[i].is_main_output_enabled());
        cl.ctprint(null, null, "  PSU #%d Input Present: %s", i, await PSU[i].is_input_present());

        await PSU[i].set_fan_target_speed(0);
        await PSU[i].set_enable(false);

        //await PSU[i].clear_on_time_and_energy();
        //await PSU[i].clear_peak_input_current();

        //while(1)
        {
            //cl.ctprint(null, null, "PSU #%d Input Voltage: %d V", i, await PSU[i].get_input_voltage());
            //cl.ctprint(null, null, "PSU #%d Status flags: 0x%s", i, (await PSU[i].get_status_flags()).toString(16));

            if(!(await PSU[i].is_input_present()))
            {
                cl.ctprint("cyan", null, "Power loss! Shutting down system...");

                require('child_process').exec("shutdown now");
                while(1);
            }

            await delay(500);
        }
    }
}

main();
