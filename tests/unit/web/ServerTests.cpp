//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include "util/AssignRandomPort.hpp"
#include "util/LoggerFixtures.hpp"
#include "util/MockPrometheus.hpp"
#include "util/TestHttpClient.hpp"
#include "util/TestWebSocketClient.hpp"
#include "util/TmpFile.hpp"
#include "util/newconfig/Array.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigFileJson.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"
#include "util/prometheus/Label.hpp"
#include "util/prometheus/Prometheus.hpp"
#include "web/AdminVerificationStrategy.hpp"
#include "web/Server.hpp"
#include "web/dosguard/DOSGuard.hpp"
#include "web/dosguard/DOSGuardInterface.hpp"
#include "web/dosguard/IntervalSweepHandler.hpp"
#include "web/dosguard/WhitelistHandler.hpp"
#include "web/interface/ConnectionBase.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/websocket/error.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <boost/system/system_error.hpp>
#include <fmt/core.h>
#include <gtest/gtest.h>
#include <test_data/SslCert.hpp>

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

using namespace util;
using namespace util::config;
using namespace web::impl;
using namespace web;

static boost::json::value
generateJSONWithDynamicPort(std::string_view port)
{
    return boost::json::parse(fmt::format(
        R"JSON({{
            "server": {{
                "ip": "0.0.0.0",
                "port": {}
            }},
            "dos_guard": {{
                "max_fetches": 100,
                "sweep_interval": 1000,
                "max_connections": 2,
                "max_requests": 3,
                "whitelist": ["127.0.0.1"]
            }}
        }})JSON",
        port
    ));
}

static boost::json::value
generateJSONDataOverload(std::string_view port)
{
    return boost::json::parse(fmt::format(
        R"JSON({{
            "server": {{
                "ip": "0.0.0.0",
                "port": {}
            }},
            "dos_guard": {{
                "max_fetches": 100,
                "sweep_interval": 1000,
                "max_connections": 2,
                "max_requests": 1
            }}
        }})JSON",
        port
    ));
}

inline static ClioConfigDefinition
getParseServerConfig(boost::json::value val)
{
    ConfigFileJson const jsonVal{val.as_object()};
    auto config = ClioConfigDefinition{
        {"server.ip", ConfigValue{ConfigType::String}},
        {"server.port", ConfigValue{ConfigType::Integer}},
        {"server.admin_password", ConfigValue{ConfigType::String}.optional()},
        {"server.local_admin", ConfigValue{ConfigType::Boolean}.optional()},
        {"server.ws_max_sending_queue_size", ConfigValue{ConfigType::Integer}.defaultValue(1500)},
        {"log_tag_style", ConfigValue{ConfigType::String}.defaultValue("uint")},
        {"dos_guard.max_fetches", ConfigValue{ConfigType::Integer}},
        {"dos_guard.sweep_interval", ConfigValue{ConfigType::Integer}},
        {"dos_guard.max_connections", ConfigValue{ConfigType::Integer}},
        {"dos_guard.max_requests", ConfigValue{ConfigType::Integer}},
        {"dos_guard.whitelist.[]", Array{ConfigValue{ConfigType::String}.optional()}},
        {"ssl_key_file", ConfigValue{ConfigType::String}.optional()},
        {"ssl_cert_file", ConfigValue{ConfigType::String}.optional()},
    };
    auto const errors = config.parse(jsonVal);
    [&]() { ASSERT_FALSE(errors.has_value()); }();
    return config;
};

struct WebServerTest : NoLoggerFixture {
    ~WebServerTest() override
    {
        work_.reset();
        ctx.stop();
        if (runner_->joinable())
            runner_->join();
    }

    WebServerTest()
    {
        work_.emplace(ctx);  // make sure ctx does not stop on its own
        runner_.emplace([this] { ctx.run(); });
    }

    boost::json::value
    addSslConfig(boost::json::value config) const
    {
        config.as_object()["ssl_key_file"] = sslKeyFile.path;
        config.as_object()["ssl_cert_file"] = sslCertFile.path;
        return config;
    }

    // this ctx is for dos timer
    boost::asio::io_context ctxSync;
    std::string const port = std::to_string(tests::util::generateFreePort());
    ClioConfigDefinition cfg{getParseServerConfig(generateJSONWithDynamicPort(port))};
    dosguard::WhitelistHandler whitelistHandler{cfg};
    dosguard::DOSGuard dosGuard{cfg, whitelistHandler};
    dosguard::IntervalSweepHandler sweepHandler{cfg, ctxSync, dosGuard};

    ClioConfigDefinition cfgOverload{getParseServerConfig(generateJSONDataOverload(port))};
    dosguard::WhitelistHandler whitelistHandlerOverload{cfgOverload};
    dosguard::DOSGuard dosGuardOverload{cfgOverload, whitelistHandlerOverload};
    dosguard::IntervalSweepHandler sweepHandlerOverload{cfgOverload, ctxSync, dosGuardOverload};
    // this ctx is for http server
    boost::asio::io_context ctx;

    TmpFile sslCertFile{tests::sslCertFile()};
    TmpFile sslKeyFile{tests::sslKeyFile()};

private:
    std::optional<boost::asio::io_service::work> work_;
    std::optional<std::thread> runner_;
};

class EchoExecutor {
public:
    void
    operator()(std::string const& reqStr, std::shared_ptr<web::ConnectionBase> const& ws)
    {
        ws->send(std::string(reqStr), http::status::ok);
    }

    void
    operator()(boost::beast::error_code /* ec */, std::shared_ptr<web::ConnectionBase> const& /* ws */)
    {
    }
};

class ExceptionExecutor {
public:
    void
    operator()(std::string const& /* req */, std::shared_ptr<web::ConnectionBase> const& /* ws */)
    {
        throw std::runtime_error("MyError");
    }

    void
    operator()(boost::beast::error_code /* ec */, std::shared_ptr<web::ConnectionBase> const& /* ws */)
    {
    }
};

namespace {

template <class Executor>
std::shared_ptr<web::HttpServer<Executor>>
makeServerSync(
    util::config::ClioConfigDefinition const& config,
    boost::asio::io_context& ioc,
    web::dosguard::DOSGuardInterface& dosGuard,
    std::shared_ptr<Executor> const& handler
)
{
    auto server = std::shared_ptr<web::HttpServer<Executor>>();
    std::mutex m;
    std::condition_variable cv;
    bool ready = false;
    boost::asio::dispatch(ioc.get_executor(), [&]() mutable {
        server = web::makeHttpServer(config, ioc, dosGuard, handler);
        {
            std::lock_guard const lk(m);
            ready = true;
        }
        cv.notify_one();
    });
    {
        std::unique_lock lk(m);
        cv.wait(lk, [&] { return ready; });
    }
    return server;
}

}  // namespace

TEST_F(WebServerTest, Http)
{
    auto const e = std::make_shared<EchoExecutor>();
    auto const server = makeServerSync(cfg, ctx, dosGuard, e);
    auto const [status, res] = HttpSyncClient::post("localhost", port, R"({"Hello":1})");
    EXPECT_EQ(res, R"({"Hello":1})");
    EXPECT_EQ(status, boost::beast::http::status::ok);
}

TEST_F(WebServerTest, Ws)
{
    auto e = std::make_shared<EchoExecutor>();
    auto const server = makeServerSync(cfg, ctx, dosGuard, e);
    WebSocketSyncClient wsClient;
    wsClient.connect("localhost", port);
    auto const res = wsClient.syncPost(R"({"Hello":1})");
    EXPECT_EQ(res, R"({"Hello":1})");
    wsClient.disconnect();
}

TEST_F(WebServerTest, HttpInternalError)
{
    auto const e = std::make_shared<ExceptionExecutor>();
    auto const server = makeServerSync(cfg, ctx, dosGuard, e);
    auto const [status, res] = HttpSyncClient::post("localhost", port, R"({})");
    EXPECT_EQ(
        res,
        R"({"error":"internal","error_code":73,"error_message":"Internal error.","status":"error","type":"response"})"
    );
    EXPECT_EQ(status, boost::beast::http::status::internal_server_error);
}

TEST_F(WebServerTest, WsInternalError)
{
    auto e = std::make_shared<ExceptionExecutor>();
    auto const server = makeServerSync(cfg, ctx, dosGuard, e);
    WebSocketSyncClient wsClient;
    wsClient.connect("localhost", port);
    auto const res = wsClient.syncPost(R"({"id":"id1"})");
    wsClient.disconnect();
    EXPECT_EQ(
        res,
        R"({"error":"internal","error_code":73,"error_message":"Internal error.","status":"error","type":"response","id":"id1","request":{"id":"id1"}})"
    );
}

TEST_F(WebServerTest, WsInternalErrorNotJson)
{
    auto e = std::make_shared<ExceptionExecutor>();
    auto const server = makeServerSync(cfg, ctx, dosGuard, e);
    WebSocketSyncClient wsClient;
    wsClient.connect("localhost", port);
    auto const res = wsClient.syncPost("not json");
    wsClient.disconnect();
    EXPECT_EQ(
        res,
        R"({"error":"internal","error_code":73,"error_message":"Internal error.","status":"error","type":"response","request":"not json"})"
    );
}

TEST_F(WebServerTest, IncompleteSslConfig)
{
    auto const e = std::make_shared<EchoExecutor>();

    auto jsonConfig = generateJSONWithDynamicPort(port);
    jsonConfig.as_object()["ssl_key_file"] = sslKeyFile.path;

    auto const server = makeServerSync(getParseServerConfig(jsonConfig), ctx, dosGuard, e);
    EXPECT_EQ(server, nullptr);
}

TEST_F(WebServerTest, WrongSslConfig)
{
    auto const e = std::make_shared<EchoExecutor>();

    auto jsonConfig = generateJSONWithDynamicPort(port);
    jsonConfig.as_object()["ssl_key_file"] = sslKeyFile.path;
    jsonConfig.as_object()["ssl_cert_file"] = "wrong_path";

    auto const server = makeServerSync(getParseServerConfig(jsonConfig), ctx, dosGuard, e);
    EXPECT_EQ(server, nullptr);
}

TEST_F(WebServerTest, Https)
{
    auto const e = std::make_shared<EchoExecutor>();
    cfg = getParseServerConfig(addSslConfig(generateJSONWithDynamicPort(port)));
    auto const server = makeServerSync(cfg, ctx, dosGuard, e);
    auto const res = HttpsSyncClient::syncPost("localhost", port, R"({"Hello":1})");
    EXPECT_EQ(res, R"({"Hello":1})");
}

TEST_F(WebServerTest, Wss)
{
    auto e = std::make_shared<EchoExecutor>();
    cfg = getParseServerConfig(addSslConfig(generateJSONWithDynamicPort(port)));
    auto server = makeServerSync(cfg, ctx, dosGuard, e);
    WebServerSslSyncClient wsClient;
    wsClient.connect("localhost", port);
    auto const res = wsClient.syncPost(R"({"Hello":1})");
    EXPECT_EQ(res, R"({"Hello":1})");
    wsClient.disconnect();
}

TEST_F(WebServerTest, HttpRequestOverload)
{
    auto const e = std::make_shared<EchoExecutor>();
    auto const server = makeServerSync(cfg, ctx, dosGuardOverload, e);
    auto [status, res] = HttpSyncClient::post("localhost", port, R"({})");
    EXPECT_EQ(res, "{}");
    EXPECT_EQ(status, boost::beast::http::status::ok);

    std::tie(status, res) = HttpSyncClient::post("localhost", port, R"({})");
    EXPECT_EQ(
        res,
        R"({"error":"slowDown","error_code":10,"error_message":"You are placing too much load on the server.","status":"error","type":"response"})"
    );
    EXPECT_EQ(status, boost::beast::http::status::service_unavailable);
}

TEST_F(WebServerTest, WsRequestOverload)
{
    auto e = std::make_shared<EchoExecutor>();
    auto const server = makeServerSync(cfg, ctx, dosGuardOverload, e);
    WebSocketSyncClient wsClient;
    wsClient.connect("localhost", port);
    auto res = wsClient.syncPost(R"({})");
    wsClient.disconnect();
    EXPECT_EQ(res, "{}");
    WebSocketSyncClient wsClient2;
    wsClient2.connect("localhost", port);
    res = wsClient2.syncPost(R"({})");
    wsClient2.disconnect();
    EXPECT_EQ(
        res,
        R"({"error":"slowDown","error_code":10,"error_message":"You are placing too much load on the server.","status":"error","type":"response","request":{}})"
    );
}

TEST_F(WebServerTest, HttpPayloadOverload)
{
    std::string const s100(100, 'a');
    auto const e = std::make_shared<EchoExecutor>();
    auto server = makeServerSync(cfg, ctx, dosGuardOverload, e);
    auto const [status, res] = HttpSyncClient::post("localhost", port, fmt::format(R"({{"payload":"{}"}})", s100));
    EXPECT_EQ(
        res,
        R"({"payload":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","warning":"load","warnings":[{"id":2003,"message":"You are about to be rate limited"}]})"
    );
    EXPECT_EQ(status, boost::beast::http::status::ok);
}

TEST_F(WebServerTest, WsPayloadOverload)
{
    std::string const s100(100, 'a');
    auto const e = std::make_shared<EchoExecutor>();
    auto server = makeServerSync(cfg, ctx, dosGuardOverload, e);
    WebSocketSyncClient wsClient;
    wsClient.connect("localhost", port);
    auto const res = wsClient.syncPost(fmt::format(R"({{"payload":"{}"}})", s100));
    wsClient.disconnect();
    EXPECT_EQ(
        res,
        R"({"payload":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","warning":"load","warnings":[{"id":2003,"message":"You are about to be rate limited"}]})"
    );
}

TEST_F(WebServerTest, WsTooManyConnection)
{
    auto const e = std::make_shared<EchoExecutor>();
    auto server = makeServerSync(cfg, ctx, dosGuardOverload, e);
    // max connection is 2, exception should happen when the third connection is made
    WebSocketSyncClient wsClient1;
    wsClient1.connect("localhost", port);
    WebSocketSyncClient wsClient2;
    wsClient2.connect("localhost", port);
    bool exceptionThrown = false;
    try {
        WebSocketSyncClient wsClient3;
        wsClient3.connect("localhost", port);
    } catch (boost::system::system_error const& ex) {
        exceptionThrown = true;
        EXPECT_EQ(ex.code(), boost::beast::websocket::error::upgrade_declined);
    }
    wsClient1.disconnect();
    wsClient2.disconnect();
    EXPECT_TRUE(exceptionThrown);
}

TEST_F(WebServerTest, HealthCheck)
{
    auto e = std::make_shared<ExceptionExecutor>();  // request handled before we get to executor
    auto const server = makeServerSync(cfg, ctx, dosGuard, e);
    auto const [status, res] = HttpSyncClient::get("localhost", port, "", "/health");

    EXPECT_FALSE(res.empty());
    EXPECT_EQ(status, boost::beast::http::status::ok);
}

TEST_F(WebServerTest, GetOtherThanHealthCheck)
{
    auto e = std::make_shared<ExceptionExecutor>();  // request handled before we get to executor
    auto const server = makeServerSync(cfg, ctx, dosGuard, e);
    auto const [status, res] = HttpSyncClient::get("localhost", port, "", "/");

    EXPECT_FALSE(res.empty());
    EXPECT_EQ(status, boost::beast::http::status::bad_request);
}

namespace {

std::string
jsonServerConfigWithAdminPassword(uint32_t const port)
{
    return fmt::format(
        R"JSON({{
            "server": {{
                "ip": "0.0.0.0",
                "port": {},
                "admin_password": "secret"
            }}
        }})JSON",
        port
    );
}

std::string
jsonServerConfigWithLocalAdmin(uint32_t const port)
{
    return fmt::format(
        R"JSON({{
            "server": {{
                "ip": "0.0.0.0",
                "port": {},
                "local_admin": true
            }}
        }})JSON",
        port
    );
}

std::string
jsonServerConfigWithBothAdminPasswordAndLocalAdminFalse(uint32_t const port)
{
    return fmt::format(
        R"JSON({{
            "server": {{
                "ip": "0.0.0.0",
                "port": {},
                "admin_password": "secret",
                "local_admin": false
            }}
        }})JSON",
        port
    );
}

std::string
jsonServerConfigWithNoSpecifiedAdmin(uint32_t const port)
{
    return fmt::format(
        R"JSON({{
            "server": {{
                "ip": "0.0.0.0",
                "port": {}
            }}
        }})JSON",
        port
    );
}

// get this value from online sha256 generator
constexpr auto kSECRET_SHA256 = "2bb80d537b1da3e38bd30361aa855686bde0eacd7162fef6a25fe97bf527a25b";

}  // namespace

class AdminCheckExecutor {
public:
    void
    operator()(std::string const& reqStr, std::shared_ptr<web::ConnectionBase> const& ws)
    {
        auto response = fmt::format("{} {}", reqStr, ws->isAdmin() ? "admin" : "user");
        ws->send(std::move(response), http::status::ok);
    }

    void
    operator()(boost::beast::error_code /* ec */, std::shared_ptr<web::ConnectionBase> const& /* ws */)
    {
    }
};

struct WebServerAdminTestParams {
    std::string config;
    std::vector<WebHeader> headers;
    std::string expectedResponse;
};

inline static ClioConfigDefinition
getParseAdminServerConfig(boost::json::value val)
{
    ConfigFileJson const jsonVal{val.as_object()};
    auto config = ClioConfigDefinition{
        {"server.ip", ConfigValue{ConfigType::String}},
        {"server.port", ConfigValue{ConfigType::Integer}},
        {"server.admin_password", ConfigValue{ConfigType::String}.optional()},
        {"server.local_admin", ConfigValue{ConfigType::Boolean}.optional()},
        {"server.processing_policy", ConfigValue{ConfigType::String}.defaultValue("parallel")},
        {"server.parallel_requests_limit", ConfigValue{ConfigType::Integer}.optional()},
        {"server.ws_max_sending_queue_size", ConfigValue{ConfigType::Integer}.defaultValue(1500)},
        {"ssl_cert_file", ConfigValue{ConfigType::String}.optional()},
        {"ssl_key_file", ConfigValue{ConfigType::String}.optional()},
        {"prometheus.enabled", ConfigValue{ConfigType::Boolean}.defaultValue(true)},
        {"prometheus.compress_reply", ConfigValue{ConfigType::Boolean}.defaultValue(true)},
        {"log_tag_style", ConfigValue{ConfigType::String}.defaultValue("uint")}
    };
    auto const errors = config.parse(jsonVal);
    [&]() { ASSERT_FALSE(errors.has_value()); }();
    return config;
};

class WebServerAdminTest : public WebServerTest, public ::testing::WithParamInterface<WebServerAdminTestParams> {};

TEST_P(WebServerAdminTest, WsAdminCheck)
{
    auto e = std::make_shared<AdminCheckExecutor>();
    ClioConfigDefinition const serverConfig{getParseAdminServerConfig(boost::json::parse(GetParam().config))};
    auto server = makeServerSync(serverConfig, ctx, dosGuardOverload, e);
    WebSocketSyncClient wsClient;
    uint32_t const webServerPort = serverConfig.get<uint32_t>("server.port");
    wsClient.connect("localhost", std::to_string(webServerPort), GetParam().headers);
    std::string const request = "Why hello";
    auto const res = wsClient.syncPost(request);
    wsClient.disconnect();
    EXPECT_EQ(res, fmt::format("{} {}", request, GetParam().expectedResponse));
}

TEST_P(WebServerAdminTest, HttpAdminCheck)
{
    auto const e = std::make_shared<AdminCheckExecutor>();
    ClioConfigDefinition const serverConfig{getParseAdminServerConfig(boost::json::parse(GetParam().config))};
    auto server = makeServerSync(serverConfig, ctx, dosGuardOverload, e);
    std::string const request = "Why hello";
    uint32_t const webServerPort = serverConfig.get<uint32_t>("server.port");
    auto const [status, res] =
        HttpSyncClient::post("localhost", std::to_string(webServerPort), request, GetParam().headers);

    EXPECT_EQ(res, fmt::format("{} {}", request, GetParam().expectedResponse));
    EXPECT_EQ(status, boost::beast::http::status::ok);
}

INSTANTIATE_TEST_CASE_P(
    WebServerAdminTestsSuit,
    WebServerAdminTest,
    ::testing::Values(
        WebServerAdminTestParams{
            .config = jsonServerConfigWithAdminPassword(tests::util::generateFreePort()),
            .headers = {},
            .expectedResponse = "user"
        },
        WebServerAdminTestParams{
            .config = jsonServerConfigWithAdminPassword(tests::util::generateFreePort()),
            .headers = {WebHeader(http::field::authorization, "")},
            .expectedResponse = "user"
        },
        WebServerAdminTestParams{
            .config = jsonServerConfigWithAdminPassword(tests::util::generateFreePort()),
            .headers = {WebHeader(http::field::authorization, "s")},
            .expectedResponse = "user"
        },
        WebServerAdminTestParams{
            .config = jsonServerConfigWithAdminPassword(tests::util::generateFreePort()),
            .headers = {WebHeader(http::field::authorization, kSECRET_SHA256)},
            .expectedResponse = "user"
        },
        WebServerAdminTestParams{
            .config = jsonServerConfigWithAdminPassword(tests::util::generateFreePort()),
            .headers = {WebHeader(
                http::field::authorization,
                fmt::format("{}{}", PasswordAdminVerificationStrategy::kPASSWORD_PREFIX, kSECRET_SHA256)
            )},
            .expectedResponse = "admin"
        },
        WebServerAdminTestParams{
            .config = jsonServerConfigWithBothAdminPasswordAndLocalAdminFalse(tests::util::generateFreePort()),
            .headers = {WebHeader(http::field::authorization, kSECRET_SHA256)},
            .expectedResponse = "user"
        },
        WebServerAdminTestParams{
            .config = jsonServerConfigWithBothAdminPasswordAndLocalAdminFalse(tests::util::generateFreePort()),
            .headers = {WebHeader(
                http::field::authorization,
                fmt::format("{}{}", PasswordAdminVerificationStrategy::kPASSWORD_PREFIX, kSECRET_SHA256)
            )},
            .expectedResponse = "admin"
        },
        WebServerAdminTestParams{
            .config = jsonServerConfigWithAdminPassword(tests::util::generateFreePort()),
            .headers = {WebHeader(
                http::field::authentication_info,
                fmt::format("{}{}", PasswordAdminVerificationStrategy::kPASSWORD_PREFIX, kSECRET_SHA256)
            )},
            .expectedResponse = "user"
        },
        WebServerAdminTestParams{
            .config = jsonServerConfigWithLocalAdmin(tests::util::generateFreePort()),
            .headers = {},
            .expectedResponse = "admin"
        },
        WebServerAdminTestParams{
            .config = jsonServerConfigWithNoSpecifiedAdmin(tests::util::generateFreePort()),
            .headers = {},
            .expectedResponse = "admin"
        }

    )
);

TEST_F(WebServerTest, AdminErrorCfgTestBothAdminPasswordAndLocalAdminSet)
{
    uint32_t webServerPort = tests::util::generateFreePort();
    std::string const jsonServerConfigWithBothAdminPasswordAndLocalAdmin = fmt::format(
        R"JSON({{
        "server":{{
                "ip": "0.0.0.0",
                "port": {},
                "admin_password": "secret",
                "local_admin": true
            }}
    }})JSON",
        webServerPort
    );

    auto const e = std::make_shared<AdminCheckExecutor>();
    ClioConfigDefinition const serverConfig{
        getParseAdminServerConfig(boost::json::parse(jsonServerConfigWithBothAdminPasswordAndLocalAdmin))
    };
    EXPECT_THROW(web::makeHttpServer(serverConfig, ctx, dosGuardOverload, e), std::logic_error);
}

TEST_F(WebServerTest, AdminErrorCfgTestBothAdminPasswordAndLocalAdminFalse)
{
    uint32_t webServerPort = tests::util::generateFreePort();
    std::string const jsonServerConfigWithNoAdminPasswordAndLocalAdminFalse = fmt::format(
        R"JSON({{
        "server": {{
            "ip": "0.0.0.0",
            "port": {},
            "local_admin": false
        }}
    }})JSON",
        webServerPort
    );

    auto const e = std::make_shared<AdminCheckExecutor>();
    ClioConfigDefinition const serverConfig{
        getParseAdminServerConfig(boost::json::parse(jsonServerConfigWithNoAdminPasswordAndLocalAdminFalse))
    };
    EXPECT_THROW(web::makeHttpServer(serverConfig, ctx, dosGuardOverload, e), std::logic_error);
}

struct WebServerPrometheusTest : util::prometheus::WithPrometheus, WebServerTest {};

TEST_F(WebServerPrometheusTest, rejectedWithoutAdminPassword)
{
    auto const e = std::make_shared<EchoExecutor>();
    uint32_t const webServerPort = tests::util::generateFreePort();
    ClioConfigDefinition const serverConfig{
        getParseAdminServerConfig(boost::json::parse(jsonServerConfigWithAdminPassword(webServerPort)))
    };
    auto server = makeServerSync(serverConfig, ctx, dosGuard, e);
    auto const [status, res] = HttpSyncClient::get("localhost", std::to_string(webServerPort), "", "/metrics");

    EXPECT_EQ(res, "Only admin is allowed to collect metrics");
    EXPECT_EQ(status, boost::beast::http::status::unauthorized);
}

TEST_F(WebServerPrometheusTest, rejectedIfPrometheusIsDisabled)
{
    uint32_t webServerPort = tests::util::generateFreePort();
    std::string const jsonServerConfigWithDisabledPrometheus = fmt::format(
        R"JSON({{
        "server":{{
                "ip": "0.0.0.0",
                "port": {},
                "admin_password": "secret",
                "ws_max_sending_queue_size": 1500
            }},
        "prometheus": {{ "enabled": false }}
    }})JSON",
        webServerPort
    );

    auto const e = std::make_shared<EchoExecutor>();
    ClioConfigDefinition const serverConfig{
        getParseAdminServerConfig(boost::json::parse(jsonServerConfigWithDisabledPrometheus))
    };
    PrometheusService::init(serverConfig);
    auto server = makeServerSync(serverConfig, ctx, dosGuard, e);
    auto const [status, res] = HttpSyncClient::get(
        "localhost",
        std::to_string(webServerPort),
        "",
        "/metrics",
        {WebHeader(
            http::field::authorization,
            fmt::format("{}{}", PasswordAdminVerificationStrategy::kPASSWORD_PREFIX, kSECRET_SHA256)
        )}
    );
    EXPECT_EQ(res, "Prometheus is disabled in clio config");
    EXPECT_EQ(status, boost::beast::http::status::forbidden);
}

TEST_F(WebServerPrometheusTest, validResponse)
{
    uint32_t const webServerPort = tests::util::generateFreePort();
    auto& testCounter = PrometheusService::counterInt("test_counter", util::prometheus::Labels());
    ++testCounter;
    auto const e = std::make_shared<EchoExecutor>();
    ClioConfigDefinition const serverConfig{
        getParseAdminServerConfig(boost::json::parse(jsonServerConfigWithAdminPassword(webServerPort)))
    };
    auto server = makeServerSync(serverConfig, ctx, dosGuard, e);
    auto const [status, res] = HttpSyncClient::get(
        "localhost",
        std::to_string(webServerPort),
        "",
        "/metrics",
        {WebHeader(
            http::field::authorization,
            fmt::format("{}{}", PasswordAdminVerificationStrategy::kPASSWORD_PREFIX, kSECRET_SHA256)
        )}
    );
    EXPECT_EQ(res, "# TYPE test_counter counter\ntest_counter 1\n\n");
    EXPECT_EQ(status, boost::beast::http::status::ok);
}
