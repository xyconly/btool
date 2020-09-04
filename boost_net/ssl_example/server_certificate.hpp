//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_EXAMPLE_COMMON_SERVER_CERTIFICATE_HPP
#define BOOST_BEAST_EXAMPLE_COMMON_SERVER_CERTIFICATE_HPP

#include <boost/asio/buffer.hpp>
#include <boost/asio/ssl/context.hpp>
#include <cstddef>
#include <memory>

/*  Load a signed certificate into the ssl context, and configure
    the context for use with a server.

    For this to work with the browser or operating system, it is
    necessary to import the "Beast Test CA" certificate into
    the local certificate store, browser, or operating system
    depending on your environment Please see the documentation
    accompanying the Beast certificate for more details.
*/
inline
void
load_server_certificate(boost::asio::ssl::context& ctx)
{
    /*
        The certificate was generated from CMD.EXE on Windows 10 using:

        winpty openssl dhparam -out dh.pem 2048
        winpty openssl req -newkey rsa:2048 -nodes -keyout key.pem -x509 -days 10000 -out cert.pem -subj "//C=US\ST=CA\L=Los Angeles\O=Beast\CN=www.example.com"
    */

    std::string const cert =
        "-----BEGIN CERTIFICATE-----\n"
        "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\n"
        "GuQcX9ZzQ0mbd6xqD0ycgDSzQyU8\n"
        "-----END CERTIFICATE-----\n"
        "-----BEGIN CERTIFICATE-----\n"
        "MIIEsTCCA5mgAwIBAgIQCKWiRs1LXIyD1wK0u6tTSTANBgkqhkiG9w0BAQsFADBh\n"
        "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
        "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n"
        "QTAeFw0xNzExMDYxMjIzMzNaFw0yNzExMDYxMjIzMzNaMF4xCzAJBgNVBAYTAlVT\n"
        "MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n"
        "b20xHTAbBgNVBAMTFFJhcGlkU1NMIFJTQSBDQSAyMDE4MIIBIjANBgkqhkiG9w0B\n"
        "AQEFAAOCAQ8AMIIBCgKCAQEA5S2oihEo9nnpezoziDtx4WWLLCll/e0t1EYemE5n\n"
        "+MgP5viaHLy+VpHP+ndX5D18INIuuAV8wFq26KF5U0WNIZiQp6mLtIWjUeWDPA28\n"
        "OeyhTlj9TLk2beytbtFU6ypbpWUltmvY5V8ngspC7nFRNCjpfnDED2kRyJzO8yoK\n"
        "MFz4J4JE8N7NA1uJwUEFMUvHLs0scLoPZkKcewIRm1RV2AxmFQxJkdf7YN9Pckki\n"
        "f2Xgm3b48BZn0zf0qXsSeGu84ua9gwzjzI7tbTBjayTpT+/XpWuBVv6fvarI6bik\n"
        "KB859OSGQuw73XXgeuFwEPHTIRoUtkzu3/EQ+LtwznkkdQIDAQABo4IBZjCCAWIw\n"
        "HQYDVR0OBBYEFFPKF1n8a8ADIS8aruSqqByCVtp1MB8GA1UdIwQYMBaAFAPeUDVW\n"
        "0Uy7ZvCj4hsbw5eyPdFVMA4GA1UdDwEB/wQEAwIBhjAdBgNVHSUEFjAUBggrBgEF\n"
        "BQcDAQYIKwYBBQUHAwIwEgYDVR0TAQH/BAgwBgEB/wIBADA0BggrBgEFBQcBAQQo\n"
        "MCYwJAYIKwYBBQUHMAGGGGh0dHA6Ly9vY3NwLmRpZ2ljZXJ0LmNvbTBCBgNVHR8E\n"
        "OzA5MDegNaAzhjFodHRwOi8vY3JsMy5kaWdpY2VydC5jb20vRGlnaUNlcnRHbG9i\n"
        "YWxSb290Q0EuY3JsMGMGA1UdIARcMFowNwYJYIZIAYb9bAECMCowKAYIKwYBBQUH\n"
        "AgEWHGh0dHBzOi8vd3d3LmRpZ2ljZXJ0LmNvbS9DUFMwCwYJYIZIAYb9bAEBMAgG\n"
        "BmeBDAECATAIBgZngQwBAgIwDQYJKoZIhvcNAQELBQADggEBAH4jx/LKNW5ZklFc\n"
        "YWs8Ejbm0nyzKeZC2KOVYR7P8gevKyslWm4Xo4BSzKr235FsJ4aFt6yAiv1eY0tZ\n"
        "/ZN18bOGSGStoEc/JE4ocIzr8P5Mg11kRYHbmgYnr1Rxeki5mSeb39DGxTpJD4kG\n"
        "hs5lXNoo4conUiiJwKaqH7vh2baryd8pMISag83JUqyVGc2tWPpO0329/CWq2kry\n"
        "qv66OSMjwulUz0dXf4OHQasR7CNfIr+4KScc6ABlQ5RDF86PGeE6kdwSQkFiB/cQ\n"
        "ysNyq0jEDQTkfa2pjmuWtMCNbBnhFXBYejfubIhaUbEv2FOQB3dCav+FPg5eEveX\n"
        "TVyMnGo=\n"
        "-----END CERTIFICATE-----\n"
        "-----BEGIN CERTIFICATE-----\n"
        "MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh\n"
        "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
        "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n"
        "QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT\n"
        "MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n"
        "b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG\n"
        "9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB\n"
        "CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97\n"
        "nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt\n"
        "43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P\n"
        "T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4\n"
        "gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO\n"
        "BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR\n"
        "TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw\n"
        "DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr\n"
        "hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg\n"
        "06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF\n"
        "PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls\n"
        "YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk\n"
        "CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=\n"
        "-----END CERTIFICATE-----\n";

    std::string const key =
        "-----BEGIN PRIVATE KEY-----\n"
        "MIIEuwIBADANBgkqhkiG9w0BAQEFAASCBKUwggShAgEAAoIBAQCuc1/uxT02F78H\n"
        "nkg0VvAacn3hW3EG2Jix2K4csJq+EAbi3M5wPbY4IA6fBFM2PuDJAOs92XRqpJko\n"
        "TMkq0ExotcBGi8nslN8q7JBF0T0/Yuv+49arsiWzbb7awTgBymWkNZUYuKDxxit4\n"
        "O8r5dIAQYTLIjSJSV1weJITRKQ5Ixb6jyJm3GkmmHS5zUcLVNCy4V+zEM9LZNZxg\n"
        "dtX09eK089h1qi6ZSXok3gtme4bkEtbyxJ1tPO/0dJSRpI7sscHvFatPXeuQ9ejk\n"
        "Oltmo3C4FGgAGOYBfYTzpMmZ8qgeSyRRItHQTJSElDgGH7VyfFUrm35l955Ayjel\n"
        "Rz+UgICtAgMBAAECggEBAI9r21r6XbCzJSKmRsaBEzUrG8LC4tG2ERNmCP8dUpsf\n"
        "ogmxbydoFP9fm6WwcJkQaM3YK47V0Jg8ao5vUpQYXDlZ85IMKx6N5kvr4VEFDU2n\n"
        "jk4oEr1l4Qj7GQXQBLO0KowuYT3JzMf5HJPk1KLx3LeasZ2gKq93kwxVCbzL/Cpm\n"
        "zSuQAQuwtk3QRriZIksYLRY9XbJZ3CjEJwnE8L1nFIjBmIdPE4VNLhY1GVVIKSwq\n"
        "EVBD6JJ1i2FK7xYvzL86JR0xTSyi6nkPPMFb/TYsdqyMwDMmNvX+oMw58r/7gfFY\n"
        "/rQHOQ84x2gZ+43x6cwCrzo5HCwgt6gFwLheLUhEoSECgYEA1bdzvByFqJHCnJw6\n"
        "SsDWryS+bwYHNyr8zON/+M87qNcFtx0m4HmYXtMH732ZzinQKk+M26kTsO2N3HSM\n"
        "npIIAR4kHNCEhtGH3VPnzJhKT1eoRZjafRYMfs1CSkl0Mmu/xqq2H7WpeN7ia1/5\n"
        "AnTpg3xTEDGuCKqMYlC/bmFkLWMCgYEA0Pcjz3JH/aROTw1B1j/Moc7Ym5UWTZhA\n"
        "HANP6QiTBzMoi87+QV6Yf3YB0rp2xIc8L8Mo5A8z3IJxatVME0CX3iAWuDPka7kJ\n"
        "NsdbpiwS5vMqDLzjhFfbzQt12XdF2mbjxxrl9nRBxRfovrzRQMd2o1OjZaCEjvg0\n"
        "5bi7Li4yvq8CgYAdFb1bcWpDOasJkz1fpQTSiyabh29984y5+ZAV9WYCIVk2xXHg\n"
        "BMxWw1OGJUrEQu6Ag5kA3+69Gmc/BGGKxwbt2ANEJKCUlHlwBpY6QtOCHsTYy+eY\n"
        "NGL48sg9weddYUqVJ/BnOlrZB0Q7JrGFwxFwgn/vaUNtDIDUdHbI99ohTwKBgGLy\n"
        "Iwq/WkdH3ayg4mPIoeScRQSme4ESbojVKKl3Xecy2igZQ1tAp4TzI7ncgRBd2Knl\n"
        "Bx+18yCew4WKqhMTqtWK7DccmzRG0Y4Wp9bvV6Pz0B1n83NfBrz4iD0ItRLNVV2Z\n"
        "5vnj4qCoyZRHY+4AhLShjeU5NIteC+4aoscjKPQRAn84hpYWm+2GeR49LnFczXcW\n"
        "WJ1bhXDq9MtXg85wlzXMQLCk4C2kQyKxBkMj9TyMfIF7UUPKFippf7CG5A23KGn9\n"
        "aufnINBjQS5vhck3Cd6Y1Y10cgqUQOvGmpiE2RYtEUQi9BgVtzpFU/nbc5RKsNK4\n"
        "2Qible3+q7jruhPRuFvl\n"
        "-----END PRIVATE KEY-----\n";

    //std::string const dh =
    //    "-----BEGIN DH PARAMETERS-----\n"
    //    "MIIBCAKCAQEArzQc5mpm0Fs8yahDeySj31JZlwEphUdZ9StM2D8+Fo7TMduGtSi+\n"
    //    "/HRWVwHcTFAgrxVdm+dl474mOUqqaz4MpzIb6+6OVfWHbQJmXPepZKyu4LgUPvY/\n"
    //    "4q3/iDMjIS0fLOu/bLuObwU5ccZmDgfhmz1GanRlTQOiYRty3FiOATWZBRh6uv4u\n"
    //    "tff4A9Bm3V9tLx9S6djq31w31Gl7OQhryodW28kc16t9TvO1BzcV3HjRPwpe701X\n"
    //    "oEEZdnZWANkkpR/m/pfgdmGPU66S2sXMHgsliViQWpDCYeehrvFRHEdR9NV+XJfC\n"
    //    "QMUk26jPTIVTLfXmmwU0u8vUkpR7LQKkwwIBAg==\n"
    //    "-----END DH PARAMETERS-----\n";

    ctx.set_password_callback(
        [](std::size_t,
            boost::asio::ssl::context_base::password_purpose)
        {
            return "test";
        });

    ctx.set_options(
        boost::asio::ssl::context::default_workarounds |
        boost::asio::ssl::context::no_sslv2 |
        boost::asio::ssl::context::single_dh_use);

    ctx.use_certificate_chain(
        boost::asio::buffer(cert.data(), cert.size()));

    ctx.use_private_key(
        boost::asio::buffer(key.data(), key.size()),
        boost::asio::ssl::context::file_format::pem);

    //ctx.use_tmp_dh(
    //    boost::asio::buffer(dh.data(), dh.size()));
}

inline
void
load_root_certificates(boost::asio::ssl::context& ctx, boost::system::error_code& ec)
{
    std::string const cert =
        "-----BEGIN CERTIFICATE-----\n"
        "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\n"
        "GuQcX9ZzQ0mbd6xqD0ycgDSzQyU8\n"
        "-----END CERTIFICATE-----\n"
        "-----BEGIN CERTIFICATE-----\n"
        "MIIEsTCCA5mgAwIBAgIQCKWiRs1LXIyD1wK0u6tTSTANBgkqhkiG9w0BAQsFADBh\n"
        "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
        "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n"
        "QTAeFw0xNzExMDYxMjIzMzNaFw0yNzExMDYxMjIzMzNaMF4xCzAJBgNVBAYTAlVT\n"
        "MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n"
        "b20xHTAbBgNVBAMTFFJhcGlkU1NMIFJTQSBDQSAyMDE4MIIBIjANBgkqhkiG9w0B\n"
        "AQEFAAOCAQ8AMIIBCgKCAQEA5S2oihEo9nnpezoziDtx4WWLLCll/e0t1EYemE5n\n"
        "+MgP5viaHLy+VpHP+ndX5D18INIuuAV8wFq26KF5U0WNIZiQp6mLtIWjUeWDPA28\n"
        "OeyhTlj9TLk2beytbtFU6ypbpWUltmvY5V8ngspC7nFRNCjpfnDED2kRyJzO8yoK\n"
        "MFz4J4JE8N7NA1uJwUEFMUvHLs0scLoPZkKcewIRm1RV2AxmFQxJkdf7YN9Pckki\n"
        "f2Xgm3b48BZn0zf0qXsSeGu84ua9gwzjzI7tbTBjayTpT+/XpWuBVv6fvarI6bik\n"
        "KB859OSGQuw73XXgeuFwEPHTIRoUtkzu3/EQ+LtwznkkdQIDAQABo4IBZjCCAWIw\n"
        "HQYDVR0OBBYEFFPKF1n8a8ADIS8aruSqqByCVtp1MB8GA1UdIwQYMBaAFAPeUDVW\n"
        "0Uy7ZvCj4hsbw5eyPdFVMA4GA1UdDwEB/wQEAwIBhjAdBgNVHSUEFjAUBggrBgEF\n"
        "BQcDAQYIKwYBBQUHAwIwEgYDVR0TAQH/BAgwBgEB/wIBADA0BggrBgEFBQcBAQQo\n"
        "MCYwJAYIKwYBBQUHMAGGGGh0dHA6Ly9vY3NwLmRpZ2ljZXJ0LmNvbTBCBgNVHR8E\n"
        "OzA5MDegNaAzhjFodHRwOi8vY3JsMy5kaWdpY2VydC5jb20vRGlnaUNlcnRHbG9i\n"
        "YWxSb290Q0EuY3JsMGMGA1UdIARcMFowNwYJYIZIAYb9bAECMCowKAYIKwYBBQUH\n"
        "AgEWHGh0dHBzOi8vd3d3LmRpZ2ljZXJ0LmNvbS9DUFMwCwYJYIZIAYb9bAEBMAgG\n"
        "BmeBDAECATAIBgZngQwBAgIwDQYJKoZIhvcNAQELBQADggEBAH4jx/LKNW5ZklFc\n"
        "YWs8Ejbm0nyzKeZC2KOVYR7P8gevKyslWm4Xo4BSzKr235FsJ4aFt6yAiv1eY0tZ\n"
        "/ZN18bOGSGStoEc/JE4ocIzr8P5Mg11kRYHbmgYnr1Rxeki5mSeb39DGxTpJD4kG\n"
        "hs5lXNoo4conUiiJwKaqH7vh2baryd8pMISag83JUqyVGc2tWPpO0329/CWq2kry\n"
        "qv66OSMjwulUz0dXf4OHQasR7CNfIr+4KScc6ABlQ5RDF86PGeE6kdwSQkFiB/cQ\n"
        "ysNyq0jEDQTkfa2pjmuWtMCNbBnhFXBYejfubIhaUbEv2FOQB3dCav+FPg5eEveX\n"
        "TVyMnGo=\n"
        "-----END CERTIFICATE-----\n"
        "-----BEGIN CERTIFICATE-----\n"
        "MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh\n"
        "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
        "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n"
        "QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT\n"
        "MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n"
        "b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG\n"
        "9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB\n"
        "CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97\n"
        "nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt\n"
        "43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P\n"
        "T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4\n"
        "gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO\n"
        "BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR\n"
        "TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw\n"
        "DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr\n"
        "hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg\n"
        "06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF\n"
        "PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls\n"
        "YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk\n"
        "CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=\n"
        "-----END CERTIFICATE-----\n";

    ctx.add_certificate_authority(boost::asio::buffer(cert.data(), cert.size()), ec);
}

#endif
