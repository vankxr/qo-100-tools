const https_request = require.main.require("./util/https_request");

class IPMA
{
    static async fetch_stations()
    {
        let req_options = {
            host: "api.ipma.pt",
            port: 443,
            path: "/open-data/observation/meteorology/stations/stations.json",
            method: "GET",
            timeout: 5000
        };

        let res = await https_request(req_options);

        if(res.statusCode != 200)
            throw new Error("Request unsuccessfull (" + res.statusCode + ")");

        if(!res.body)
            throw new Error("Invalid body");

        return JSON.parse(res.body);
    }
    static async fetch_locations()
    {
        let req_options = {
            host: "api.ipma.pt",
            port: 443,
            path: "/open-data/distrits-islands.json",
            method: "GET",
            timeout: 5000
        };

        let res = await https_request(req_options);

        if(res.statusCode != 200)
            throw new Error("Request unsuccessfull (" + res.statusCode + ")");

        if(!res.body)
            throw new Error("Invalid body");

        res.body = JSON.parse(res.body);

        if(!res.body.data)
            throw new Error("Invalid body");

        return res.body.data;
    }

    location;
    station;

    constructor(location, station)
    {
        this.location = location;
        this.station = station;
    }

    async get_forecast(day)
    {
        if(!this.location || !this.location.globalIdLocal)
            throw new Error("Location is not set or is invalid");

        if(!Number.isInteger(day) || day < 0 || day > 2)
            throw new Error("Forecast day must be an integer in the range 0-2");

        let req_options = {
            host: "api.ipma.pt",
            port: 443,
            path: "/open-data/forecast/meteorology/cities/daily/hp-daily-forecast-day" + day + ".json",
            method: "GET",
            timeout: 5000
        };

        let res = await https_request(req_options);

        if(res.statusCode != 200)
            throw new Error("Request unsuccessfull (" + res.statusCode + ")");

        if(!res.body)
            throw new Error("Invalid body");

        res.body = JSON.parse(res.body);

        if(!res.body.data)
            throw new Error("Invalid body");

        if(!res.body.forecastDate)
            throw new Error("Invalid body");

        let forecast_date = new Date(res.body.forecastDate);

        for(const forecast of res.body.data)
        {
            if(forecast.globalIdLocal !== this.location.globalIdLocal)
                continue;

            forecast.date = forecast_date;

            return forecast;
        }

        throw new Error("Forecast not found");
    }
    async get_surface_observations()
    {
        if(!this.station || !this.station.properties || !this.station.properties.idEstacao)
            throw new Error("Station is not set or is invalid");

        let req_options = {
            host: "api.ipma.pt",
            port: 443,
            path: "/open-data/observation/meteorology/stations/obs-surface.geojson",
            method: "GET",
            timeout: 5000
        };

        let res = await https_request(req_options);

        if(res.statusCode != 200)
            throw new Error("Request unsuccessfull (" + res.statusCode + ")");

        if(!res.body)
            throw new Error("Invalid body");

        res.body = JSON.parse(res.body);

        if(!res.body.features)
            throw new Error("Invalid body");

        let ret = [];

        for(const observation of res.body.features)
        {
            if(!observation.properties)
                continue;

            if(observation.properties.idEstacao !== this.station.properties.idEstacao)
                continue;

            if(observation.properties.time)
                observation.properties.time = new Date(observation.properties.time);

            ret.push(observation.properties);
        }

        return ret;
    }
    async get_latest_surface_observation()
    {
        let observations = await this.get_surface_observations();

        if(!observations.length)
            throw new Error("No observations found");

        let max_time = 0;
        let max_observation = null;

        for(const observation of observations)
        {
            if(!observation.time)
                continue;

            if(observation.time.getTime() > max_time)
            {
                max_time = observation.time.getTime();
                max_observation = observation;
            }
        }

        if(!max_observation)
            throw new Error("No valid observations found");

        return max_observation;
    }
    async get_daily_observations()
    {
        if(!this.station || !this.station.properties || !this.station.properties.idEstacao)
            throw new Error("Station is not set or is invalid");

        let req_options = {
            host: "api.ipma.pt",
            port: 443,
            path: "/open-data/observation/meteorology/stations/observations.json",
            method: "GET",
            timeout: 5000
        };

        let res = await https_request(req_options);

        if(res.statusCode != 200)
            throw new Error("Request unsuccessfull (" + res.statusCode + ")");

        if(!res.body)
            throw new Error("Invalid body");

        res.body = JSON.parse(res.body);

        let ret = [];

        for(const observation_time in res.body)
        {
            for(const observation_station in res.body[observation_time])
            {
                if(parseInt(observation_station) !== this.station.properties.idEstacao)
                    continue;

                let observation = res.body[observation_time][observation_station];

                observation.time = new Date(observation_time);

                ret.push(observation);
            }
        }

        return ret;
    }
}

module.exports = IPMA;