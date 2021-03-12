const ZMOD4510 = require.main.require("./lib/zmod4510");

class OAQ
{
    static RCDA_STRATEGY_FIX = 0; /*!< hold rcda fixed */
    static RCDA_STRATEGY_SET = 1; /*!< set the rcda to the actal measurement */
    static RCDA_STRATEGY_ADJ = 2; /*!< adjust rcda to follow slow drifts */

    static GAS_DETECTION_STRATEGY_AUTO = 0;     /*!< use automatic gas discrimination */
    static GAS_DETECTION_STRATEGY_FORCEO3 = 1;  /*!< handle the measurements as O3 */
    static GAS_DETECTION_STRATEGY_FORCENO2 = 2;  /*!< handle the measurements as NO2 */

    static scaler_and_pca_o = [
        1.74133742, -1.05259192, -0.6739225, 6.25734425, 6.44056368,
        -2.85911536, 1.26398969, -0.07627178
    ];
    static scaler_and_pca_g = [
        0.098822169, 0.076355919, -0.24257118, -0.3427524, -0.36181408,
        -0.56742352, -0.59832847, -0.52399075, -0.81655341, -0.66967648,
        -0.55128223, 0.0052506132, -0.089478098, 1.5322995, 1.2153398,
        0.69527262, 0.80276036, -0.38943779, -0.84154904, -1.0249839,
        -1.9840645, -2.0471313, 1.0043662, -0.68624401, -0.17178419,
        -0.021966385, 0.080418855, -0.29417408, 0.053931851, 0.17823912,
        -0.10622692, 0.21712069, 0.28085747, -0.72911143, -0.67155647,
        1.605744, -1.6921251, -2.6079316, 0.18752402, -1.1799121,
        -1.1853565, 1.0869612, 0.42717779, 0.48912618, -1.0410491,
        -0.76051044, -2.2981009, 0.83406442, 1.7949145, -2.4980917,
        -0.17692336, 0.53374362, -3.6838586, -1.4608299, -0.90180105,
        0.11278431, 0.25221416, 3.2374511, 2.74702, -0.62844849,
        -8.571579, -1.8568134, 1.7693721, -5.4411964, 3.0048814,
        5.5464873, -0.031913839, -0.078795917, -3.0445056, 7.693006,
        -5.6150074, 1.6021262, -1.4109792, -2.7250714, 3.8859453,
        1.1784325, 0.053968702, 0.10485264, 0.061339337, 0.22992066,
        0.19881652, -8.227809, 8.450017, -1.3848327, 23.178757,
        -16.182943, -7.2538972, -5.0571938
    ];
    static svc_poly_coeffs = [
        -3.1019304, -2.3316905, 4.7303534, -0.80521905, 1.9767121,
        -2.0625834, 2.9635882, 1.1311389, -0.0052948906, -0.95747906,
        1.6330426, 2.2969685, -2.7378335, 1.1746475, -0.079822987,
        0.086946793, 3.4260488, -2.1125345, -0.959176, 0.066470027,
        3.703608, -3.8293538, 0.6122278, 3.7048388, -1.1051865,
        -1.0215594, 4.7507892, -3.3319578, -1.4624187, -0.2667965,
        0.40958887, -0.58006084, -0.7323609, 0.49243289, 0.26756704,
        4.6461277, -3.2305396, 0.94641966, 0.98231965, -0.29620481,
        -1.3983079, 2.0152678, -2.8249195, 3.7985516, -0.48653999,
        2.0379181, -3.2296391, -0.7920357, -1.0288268, -0.055438876,
        2.8394473, -0.80105704, 3.4359744, -1.5734187, -0.93969798,
        -0.98496193, 3.165663, 0.67869413, -2.4870875, 1.8048391,
        2.0739756, -1.7421837, -8.9348974, 0.56574947, -2.32111,
        1.2667328, -6.863461, 2.2387428, -1.7676632, 2.0244179,
        0.11134893, -7.9828277, 0.40084094, 0.44028839, -4.6814523,
        -0.712448, -3.60025, 1.2042403, -2.565119, 0.54992688,
        3.9262941, -0.77998477, 0.05671934, 0.033889253, -2.9563978,
        0.88960683, -0.44980583, -0.52007324, 0.048948638, 0.32514209,
        -6.4129553, 3.8077362, 0.56695652, 0.071017198, 5.5018158,
        -1.9626614, -0.34950507, 2.4830499, 0.33693412, 0.4366965,
        -0.4496052, 4.5655513, -1.100323, -2.6142988, -0.9866693,
        6.0067482, 0.394243, -0.26413938, 3.6902361, -4.9410615,
        -5.2179823, -1.3120681, 1.9078605, 1.7019941, -0.22031876,
        4.0383296, 2.2260122, 1.9149201, 1.0517087, 1.5966017,
        0.71503288, 2.8193233, 0.67933881, 3.7416115, 3.3591559,
        -1.8144586, -0.090612598, -2.4134445, -0.219465, 0.52822685,
        -3.670917, 0.32494608, -4.2846279, -0.64396828, 0.85289752,
        -4.4223251, 1.5727997, -3.3692346, 0.027973272, 0.061191835,
        1.6783003, -2.2148285, -4.3223734, -2.6331933, -3.7221727,
        -1.2402098, 2.2283425, -0.074013151, -0.67211276, -2.4923341,
        0.12842907, -1.9091744, -2.24827, 3.5922465, -0.89560825,
        -0.45383194, 0.38090637, 0.64149636, -0.41870576, -0.55770171,
        -1.8514881, -0.69146079, -0.3754369, -0.050475389, 0.13151406,
        0.0, 0.0, 0.0
    ];
    static epa_no2_conc = [
        0x00000036, 0x00000065,
        0x00000169, 0x0000028A,
        0x000004E2, 0x00000672
    ];
    static epa_no2_aqi = [
        0x00000033, 0x00000065,
        0x00000097, 0x000000C9,
        0x0000012D, 0x00000191
    ];
    static epa_o3_conc = [
        0x0000007D, 0x000000A5,
        0x000000CD, 0x00000195,
        0x000001F9, 0x0000025C
    ];
    static epa_o3_aqi = [
        0x00000065, 0x00000097,
        0x000000C9, 0x0000012D,
        0x00000191, 0x000001F4
    ];

    static classify(rmox, rmox_mean)
    {
        let rmox_scaled = [];
        let rmox_div_by_mean = [];

        for(let i = 0; i < 11; i++)
            rmox_div_by_mean[i] = rmox[i + 1] / rmox_mean;

        for(let i = 0; i < 8; i++)
        {
            rmox_scaled[i] = OAQ.scaler_and_pca_o[i];

            for(let j = 0; j < 11; j++)
                rmox_scaled[i] += rmox_div_by_mean[j] * OAQ.scaler_and_pca_g[j + 11 * i];
        }

        let ret = 0.0;
        let svc_poly_index = 0;

        for(let i = 0; i < 9; i++)
        {
            let a;

            if(i)
                a = rmox_scaled[i - 1];
            else
                a = 1.0;

            for(let j = 0; j < i + 1; j++)
            {
                let b;

                if(j)
                    b = rmox_scaled[j - 1] * a;
                else
                    b = a;

                for(let k = 0; k < j + 1; k++)
                {
                    let c;

                    if(k)
                        c = rmox_scaled[k - 1] * b;
                    else
                        c = b;

                    ret += c * OAQ.svc_poly_coeffs[svc_poly_index++];
                }
            }
        }

        return ret;
    }
    static interp(conc, epa_conc, epa_aqi, imax)
    {
        for(let i = 0; i < imax; i++)
        {
            if(epa_conc[i] >= conc)
            {
                if(i)
                    return epa_aqi[i - 1] + (epa_aqi[i] - epa_aqi[i - 1]) / (epa_conc[i] - epa_conc[i - 1]) * (conc - epa_conc[i - 1]);
                else
                    return epa_aqi[0] / epa_conc[0] * conc;
            }
        }

        return epa_aqi[imax - 1];
    }

    sensor;
    aqi;
    stabilization_sample;
    measurement_data;
    trim_data_version;
    trim_b;
    trim_beta2;
    conc_no2;
    conc_o3;
    aqi_no2;
    aqi_o3;
    rcda_22;
    rcda_42;
    prob_no2;

    constructor(sensor, stabilization_samples = 10)
    {
        if(!(sensor instanceof ZMOD4510))
            throw new Error("Unsupported sensor");

        if(stabilization_samples < 0)
            throw new Error("Invalid stabilization sample count");

        this.sensor = sensor;
        this.aqi = -1;
        this.stabilization_sample = stabilization_samples;
        this.measurement_data = {
            enabled: false,
            rcda_strategy: OAQ.RCDA_STRATEGY_ADJ,
            gas_detection_strategy: OAQ.GAS_DETECTION_STRATEGY_AUTO,
            d_rising_m1: 4.9601944386079566e-05,
            d_falling_m1: 0.3934693402873666,
            d_class_m1: 0.024690087971667385
        };
    }

    async config()
    {
        let gp = await this.sensor.get_general_purpose();

        if(!Buffer.isBuffer(gp) && !Array.isArray(gp))
            throw new Error("Invalid sensor general purpose data");

        if(gp.length !== 9)
            throw new Error("Invalid sensor general purpose data");

        this.trim_data_version = gp[8];

        let v3 = Math.pow(10, (((gp[0] << 8) | gp[1]) / 65536) * 5 + 4);
        let v4 = Math.pow(10, (((gp[2] << 8) | gp[3]) / 65536) * 4 - 1.5);

        this.trim_b = v4 * v3;
        this.trim_beta2 = (((gp[4] << 8) | gp[5]) / 65536) * 3;
        this.conc_no2 = 0;
        this.conc_o3 = 0;
        this.aqi_no2 = 0;
        this.aqi_o3 = 0;
        this.rcda_22 = v3;
        this.rcda_42 = 15848932; // *(_DWORD *)(v6 + 12) = 1265751524;
        this.prob_no2 = 0.5; // *(_DWORD *)(v6 + 20) = 1056964608;
    }

    calc(rmox)
    {
        if(!Buffer.isBuffer(rmox) && !Array.isArray(rmox))
            throw new Error("Invalid Rmox data");

        if(rmox.length !== 15)
            throw new Error("Invalid Rmox data");

        if(this.stabilization_sample)
        {
            this.stabilization_sample--;

            return;
        }

        let rmox_valid = [];
        let rmox_mean = 0;

        for(let i = 0; i < 12; i++)
        {
            if(rmox[i + 3] > 1e12)
                rmox_valid[i] = 1e12;
            else if(rmox[i + 3] < 100)
                rmox_valid[i] = 100;
            else
                rmox_valid[i] = rmox[i + 3];

            rmox_mean += rmox_valid[i];
        }

        rmox_mean /= 12;

        if(this.measurement_data.rcda_strategy === OAQ.RCDA_STRATEGY_SET)
        {
            this.rcda_22 = rmox_valid[5];
            this.rcda_42 = rmox_valid[11];
        }
        else if(this.measurement_data.rcda_strategy === OAQ.RCDA_STRATEGY_ADJ)
        {
            let m1;

            if(rmox_valid[5] <= this.rcda_22)
                m1 = this.measurement_data.d_falling_m1;
            else
                m1 = this.measurement_data.d_rising_m1;

            this.rcda_22 *= Math.pow(rmox_valid[5] / this.rcda_22, m1);

            if(rmox_valid[11] <= this.rcda_42)
                m1 = this.measurement_data.d_falling_m1;
            else
                m1 = this.measurement_data.d_rising_m1;

            this.rcda_42 *= Math.pow(rmox_valid[11] / this.rcda_42, m1);
        }

        let new_conc_no2 = (rmox_valid[5] - this.rcda_22) / this.trim_b;

        if(new_conc_no2 < 0)
            new_conc_no2 = 0;

        new_conc_no2 = Math.pow(new_conc_no2, 1 / this.trim_beta2);

        if(this.trim_data_version > 0)
            new_conc_no2 *= 100;
        else
            new_conc_no2 *= 250;

        this.conc_no2 = (new_conc_no2 - this.conc_no2) * 0.18126924 + this.conc_no2;
        this.aqi_no2 = OAQ.interp(this.conc_no2, OAQ.epa_no2_conc, OAQ.epa_no2_aqi, 7);

        let new_conc_o3 = (rmox_valid[11] - this.rcda_42) / 56234132;

        if(new_conc_o3 < 0)
            new_conc_o3 = 0;

        new_conc_o3 *= 50;

        this.conc_o3 = (new_conc_o3 - this.conc_o3) * 0.18126924 + this.conc_o3;
        this.aqi_o3 = OAQ.interp(this.conc_o3, OAQ.epa_o3_conc, OAQ.epa_o3_aqi, 6);

        let mult;

        if(this.measurement_data.gas_detection_strategy === OAQ.GAS_DETECTION_STRATEGY_FORCEO3 || this.conc_no2 >= 10000.0)
        {
            mult = 0;
        }
        else if(this.measurement_data.gas_detection_strategy === OAQ.GAS_DETECTION_STRATEGY_FORCENO2)
        {
            mult = 1;
        }
        else
        {
            if(OAQ.classify(rmox_valid, rmox_mean) == 0)
                mult = 1;
            else
                mult = 0;
        }

        this.prob_no2 += (mult - this.prob_no2) * this.measurement_data.d_class_m1;
        this.aqi = (this.aqi_no2 - this.aqi_o3) * this.prob_no2 + this.aqi_o3;
    }

    async sensor_data_ready_callback()
    {
        if(!this.measurement_data.enabled)
            return;

        this.calc(await this.sensor.get_rmox());

        this.sensor.measure().then(this.sensor_data_ready_callback.bind(this), this.sensor_measure_error_callback.bind(this));
    }
    sensor_measure_error_callback(e)
    {
        // Fail silently and just proceed to next measure

        if(this.measurement_data.enabled)
            this.sensor.measure().then(this.sensor_data_ready_callback.bind(this), this.sensor_measure_error_callback.bind(this));
    }

    start_measuring(rcda_strategy = this.measurement_data.rcda_strategy, gas_detection_strategy = this.measurement_data.gas_detection_strategy, d_rising_m1 = this.measurement_data.d_rising_m1, d_falling_m1 = this.measurement_data.d_falling_m1, d_class_m1 = this.measurement_data.d_class_m1)
    {
        if(this.measurement_data.enabled)
            throw new Error("Measurements already running");

        if(rcda_strategy < OAQ.RCDA_STRATEGY_FIX || rcda_strategy > OAQ.RCDA_STRATEGY_ADJ)
            throw new Error("Invalid RCDA strategy");

        if(gas_detection_strategy < OAQ.GAS_DETECTION_STRATEGY_AUTO || gas_detection_strategy > OAQ.GAS_DETECTION_STRATEGY_FORCENO2)
            throw new Error("Invalid Gas detection strategy");

        if(typeof(d_rising_m1) != "number" || typeof(d_falling_m1) != "number" || typeof(d_class_m1) != "number")
            throw new Error("Invalid damping factor");

        this.measurement_data.rcda_strategy = rcda_strategy;
        this.measurement_data.gas_detection_strategy = gas_detection_strategy;
        this.measurement_data.d_rising_m1 = d_rising_m1;
        this.measurement_data.d_falling_m1 = d_falling_m1;
        this.measurement_data.d_class_m1 = d_class_m1;
        this.measurement_data.enabled = true;

        this.sensor.measure().then(this.sensor_data_ready_callback.bind(this), this.sensor_measure_error_callback.bind(this));
    }
    stop_measuring()
    {
        this.measurement_data.enabled = false;
    }

    get_aqi()
    {
        return this.aqi;
    }
    get_loc()
    {
        let aqi = this.get_aqi();

        if(aqi >= 0 && aqi < 50)
            return "Good";
        else if(aqi > 50 && aqi < 100)
            return "Moderate";
        else if(aqi > 100 && aqi < 150)
            return "Unhealthy for Sensitive Groups";
        else if(aqi > 150 && aqi < 200)
            return "Unhealthy";
        else if(aqi > 200 && aqi < 300)
            return "Very unhealthy";
        else if(aqi > 300 && aqi < 500)
            return "Hazardous";
        else
            return "Out of scale";
    }
    get_no2_concentration()
    {
        return this.conc_no2;
    }
    get_o3_concentration()
    {
        return this.conc_o3;
    }
    is_stable()
    {
        return this.aqi >= 0 && this.stabilization_sample == 0;
    }
}

module.exports = OAQ;