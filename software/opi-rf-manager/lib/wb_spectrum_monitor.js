const WebSocket = require("ws");
const EventEmitter = require("events");

class WBSpectrumMonitor extends EventEmitter
{
    ws;
    keepaliveTimeout;

    static round_sr(bw)
    {
        if(bw < 0.022)
            return 0;
        else if(bw < 0.060)
            return 35;
        else if(bw < 0.086)
            return 66;
        else if(bw < 0.185)
            return 125;
        else if(bw < 0.277)
            return 250;
        else if(bw < 0.388)
            return 333;
        else if(bw < 0.700)
            return 500;
        else if(bw < 1.2)
            return 1000;
        else if(bw < 1.6)
            return 1500;
        else if(bw < 2.2)
            return 2000;
        else
            return Math.round(bw * 5) / 5;
    }
    static detect_signals(fft)
    {
        if(!(fft instanceof Buffer))
            throw new Error("Invalid FFT data");

        if(fft.length === 0 || fft.length % 2)
            throw new Error("Invalid FFT data");

        const fft_length = fft.length / 2; // Buffer size in byte, each bin is a word
        const fft_signal_threshold = 16500;
        const fft_full_scale_power = 16.7; // dB
        const fft_zero_scale_power = -3.35; // dB
        const fft_power_slope = (fft_full_scale_power - fft_zero_scale_power) / 65535;
        const fft_signal_start_freq = 10490.5; // MHz
        const fft_end_freq = 10499.5; // MHz
        const fft_freq_step = (fft_end_freq - fft_signal_start_freq) / fft_length; // MHz per FFT bin
        const fft_avg_count = 3;

        let data = {
            noise_power: 0,
            beacon: undefined,
            signals: []
        };

        let signal_start = 0;
        let noise_cnt = 0;

        for(let i = fft_avg_count - 1; i < fft_length; i++)
        {
            let sample = 0;

            for(let j = 0; j < fft_avg_count; j++)
                sample += fft.readUInt16LE((i - j) * 2);

            sample /= fft_avg_count;

            if(signal_start === 0)
            {
                if(sample >= fft_signal_threshold)
                {
                    signal_start = i;

                    continue;
                }

                data.noise_power += sample;
                noise_cnt++;

                continue;
            }

            if(sample < fft_signal_threshold || i === fft_length - 1)
            {
                let full_start_bin = signal_start;
                let full_end_bin = i;
                let full_bin_count = full_end_bin - full_start_bin;
                let full_start_freq = full_start_bin * fft_freq_step + fft_signal_start_freq;
                let full_end_freq = full_end_bin * fft_freq_step + fft_signal_start_freq;
                let full_center_freq = full_start_freq + (full_end_freq - full_start_freq) / 2;
                let full_bandwidth = full_end_freq - full_start_freq;
                let full_power = 0;
                let cnt = 0;

                for(let j = Math.floor(full_start_bin + 0.3 * full_bin_count); j < full_end_bin - 0.3 * full_bin_count; j++)
                {
                    full_power += fft.readUInt16LE(j * 2);
                    cnt++;
                }

                full_power /= cnt;

                let used_start_bin = full_start_bin;
                let used_end_bin = full_end_bin;
                let used_power_threshold = 0.75 * full_power;

                for(let j = full_start_bin; fft.readUInt16LE(j * 2) < used_power_threshold; j++)
                    used_start_bin = j;

                for(let j = full_end_bin; fft.readUInt16LE(j * 2) < used_power_threshold; j--)
                    used_end_bin = j;

                let used_bin_count = used_end_bin - used_start_bin;
                let used_start_freq = used_start_bin * fft_freq_step + fft_signal_start_freq;
                let used_end_freq = used_end_bin * fft_freq_step + fft_signal_start_freq;
                let used_center_freq = used_start_freq + (used_end_freq - used_start_freq) / 2;
                let used_bandwidth = used_end_freq - used_start_freq;
                let used_power = 0;

                for(let j = used_start_bin; j < used_end_bin; j++)
                    used_power += fft.readUInt16LE(j * 2);

                used_power /= used_bin_count;

                let signal = {
                    full_start_freq: full_start_freq,
                    full_end_freq: full_end_freq,
                    full_center_freq: full_center_freq,
                    full_bandwidth: full_bandwidth,
                    full_power: full_power * fft_power_slope + fft_zero_scale_power,
                    used_start_freq: used_start_freq,
                    used_end_freq: used_end_freq,
                    used_center_freq: used_center_freq,
                    used_bandwidth: used_bandwidth,
                    used_power: used_power * fft_power_slope + fft_zero_scale_power,
                    symbolrate: WBSpectrumMonitor.round_sr(used_bandwidth),
                    snr: 0,
                    sbr: 0,
                    out_of_band: full_end_bin === fft_length - 1,
                    over_powered: false
                };

                if(used_center_freq < 10492.0 && used_bandwidth >= 1) // Beacon
                    data.beacon = signal;
                else if(signal.symbolrate > 0)
                    data.signals.push(signal);

                signal_start = 0;
            }
        }

        if(noise_cnt)
            data.noise_power /= noise_cnt;

        data.noise_power = data.noise_power * fft_power_slope + fft_zero_scale_power;

        if(data.beacon)
            data.beacon.snr = data.beacon.full_power - data.noise_power;

        for(const signal of data.signals)
        {
            signal.snr = signal.full_power - data.noise_power;

            if(data.beacon)
            {
                signal.sbr = signal.full_power - data.beacon.full_power;
                signal.over_powered = signal.symbolrate > 500 && signal.sbr > -0.7;
            }
        }

        return data;
    }

    constructor()
    {
        super();

        this.ws_init();
    }

    ws_init()
    {
        if(this.ws)
            return;

        this.ws = new WebSocket(
            "wss://eshail.batc.org.uk/wb/fft",
            {
                handshakeTimeout: 5000,
                timeout: 1000
            }
        );

        this.keepaliveTimeout = setTimeout(
            function ()
            {
                this.emit("timeout");
                this.ws.terminate();
            }.bind(this),
            5000
        );

        this.ws.on("open", this.on_ws_open.bind(this));
        this.ws.on("message", this.on_ws_message.bind(this));
        this.ws.on("error", this.on_ws_error.bind(this));
        this.ws.on("close", this.on_ws_close.bind(this));
    }

    on_ws_open()
    {
        this.emit("open");
    }
    on_ws_message(data)
    {
        this.keepaliveTimeout.refresh();

        try
        {
            let signals = WBSpectrumMonitor.detect_signals(data);

            if(typeof(signals) == "object")
                this.emit("signals", signals);
        }
        catch (e)
        {
            this.emit("error", e);
        }
    }
    on_ws_error(e)
    {
        this.emit("error", e);
    }
    on_ws_close(code, reason)
    {
        this.emit("close", code, reason);

        clearTimeout(this.keepaliveTimeout);

        if(code > 1000 && code !== 1005)
        {
            setTimeout(this.ws_init.bind(this), 1000);

            this.ws = undefined;
        }
    }
}

module.exports = WBSpectrumMonitor;