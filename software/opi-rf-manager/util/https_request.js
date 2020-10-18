const HTTPS = require("https");

module.exports = async function (options, body)
{
    return new Promise(
        function (resolve, reject)
        {
            const req = HTTPS.request(
                options,
                function (res)
                {
                    res.body = [];

                    res.on('data',
                        function (chunk)
                        {
                            res.body.push(chunk);
                        }
                    );

                    res.on('end',
                        function()
                        {
                            res.body = Buffer.concat(res.body);

                            return resolve(res);
                        }
                    );
                }
            )

            req.on("error", reject);

            if(body)
                req.write(body);

            req.end();
        }
    );
}
