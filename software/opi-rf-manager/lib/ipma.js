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

        let stations = JSON.parse(res.body);
        let ret = [];

        for(const station of stations)
        {
            try
            {
                ret.push(new IPMAStation(station));
            }
            catch(e)
            {
                continue;
            }
        }

        return ret;
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

        let locations = res.body.data;
        let ret = [];

        for(const location of locations)
        {
            try
            {
                ret.push(new IPMALocation(location));
            }
            catch(e)
            {
                continue;
            }
        }

        return ret;
    }
}
class IPMAStation
{
    id;
    name;
    location;

    constructor(station)
    {
        if(!station.geometry)
            throw new Error("No station location data found");

        if(!station.geometry.coordinates)
            throw new Error("No station location data found");

        if(station.geometry.coordinates.length !== 2)
            throw new Error("Invalid station location data found");

        if(!station.properties)
            throw new Error("No station properties found");

        if(!station.properties.idEstacao)
            throw new Error("Invalid station properties found");

        if(!station.properties.localEstacao)
            throw new Error("Invalid station properties found");

        this.id = parseInt(station.properties.idEstacao.toString());
        this.name = station.properties.localEstacao;
        this.location = {
            lat: parseFloat(station.geometry.coordinates[1].toString()),
            lon: parseFloat(station.geometry.coordinates[0].toString())
        };
    }

    async get_observation_data()
    {
        if(this.id <= 0)
            throw new Error("Station ID not set or invalid");

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
            let observation = res.body[observation_time][this.id.toString()];

            if(!observation)
                continue;

            observation.time = new Date(observation_time);
            observation.missing_data = [];

            for(const key in observation)
                if(parseInt(observation[key].toString()) == -99)
                    observation.missing_data.push(key);

            ret.push(observation);
        }

        return ret;
    }
    async get_latest_observation_data()
    {
        let observations = await this.get_observation_data();

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
}
class IPMALocation
{
    id;
    name;
    location;

    constructor(location)
    {
        if(!location.latitude)
            throw new Error("No location data found");

        if(!location.longitude)
            throw new Error("No location data found");

        if(!location.globalIdLocal)
            throw new Error("No ID found");

        if(!location.local)
            throw new Error("No location name found");

        this.id = parseInt(location.globalIdLocal.toString());
        this.name = location.local;
        this.location = {
            lat: parseFloat(location.latitude.toString()),
            lon: parseFloat(location.longitude.toString())
        };
    }

    async get_forecast_data(day = 0)
    {
        if(this.id <= 0)
            throw new Error("Location ID not set or invalid");

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
            if(forecast.globalIdLocal !== this.id)
                continue;

            forecast.date = forecast_date;

            return forecast;
        }

        throw new Error("Forecast not found");
    }
}

module.exports = {
    IPMA,
    IPMAStation,
    IPMALocation
};