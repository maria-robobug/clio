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
#include "util/MockPrometheus.hpp"
#include "util/NameGenerator.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"
#include "util/prometheus/Http.hpp"
#include "util/prometheus/Label.hpp"
#include "util/prometheus/Prometheus.hpp"

#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <fmt/core.h>
#include <gtest/gtest.h>

#include <string>

using namespace util::prometheus;
using namespace util::config;
namespace http = boost::beast::http;

struct PrometheusCheckRequestTestsParams {
    std::string testName;
    http::verb method;
    std::string target;
    bool isAdmin;
    bool prometheusEnabled;
    bool expected;
};

struct PrometheusCheckRequestTests : public ::testing::TestWithParam<PrometheusCheckRequestTestsParams> {};

TEST_P(PrometheusCheckRequestTests, isPrometheusRequest)
{
    ClioConfigDefinition const config{
        {"prometheus.enabled", ConfigValue{ConfigType::Boolean}.defaultValue(GetParam().prometheusEnabled)},
        {"prometheus.compress_reply", ConfigValue{ConfigType::Boolean}.defaultValue(true)}
    };
    PrometheusService::init(config);
    boost::beast::http::request<boost::beast::http::string_body> req;
    req.method(GetParam().method);
    req.target(GetParam().target);
    EXPECT_EQ(handlePrometheusRequest(req, GetParam().isAdmin).has_value(), GetParam().expected);
}

INSTANTIATE_TEST_CASE_P(
    PrometheusHttpTests,
    PrometheusCheckRequestTests,
    ::testing::ValuesIn({
        PrometheusCheckRequestTestsParams{
            .testName = "validRequest",
            .method = http::verb::get,
            .target = "/metrics",
            .isAdmin = true,
            .prometheusEnabled = true,
            .expected = true
        },
        PrometheusCheckRequestTestsParams{
            .testName = "validRequestPrometheusDisabled",
            .method = http::verb::get,
            .target = "/metrics",
            .isAdmin = true,
            .prometheusEnabled = false,
            .expected = true
        },
        PrometheusCheckRequestTestsParams{
            .testName = "notAdmin",
            .method = http::verb::get,
            .target = "/metrics",
            .isAdmin = false,
            .prometheusEnabled = true,
            .expected = true
        },
        PrometheusCheckRequestTestsParams{
            .testName = "wrongMethod",
            .method = http::verb::post,
            .target = "/metrics",
            .isAdmin = true,
            .prometheusEnabled = true,
            .expected = false
        },
        PrometheusCheckRequestTestsParams{
            .testName = "wrongTarget",
            .method = http::verb::get,
            .target = "/",
            .isAdmin = true,
            .prometheusEnabled = true,
            .expected = false
        },
    }),
    tests::util::kNAME_GENERATOR
);

struct PrometheusHandleRequestTests : util::prometheus::WithPrometheus {
    http::request<http::string_body> const req{http::verb::get, "/metrics", 11};
};

TEST_F(PrometheusHandleRequestTests, emptyResponse)
{
    auto response = handlePrometheusRequest(req, true);
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->result(), http::status::ok);
    EXPECT_EQ(response->operator[](http::field::content_type), "text/plain; version=0.0.4");
    EXPECT_EQ(response->body(), "");
}

TEST_F(PrometheusHandleRequestTests, prometheusDisabled)
{
    ClioConfigDefinition const config{
        {"prometheus.enabled", ConfigValue{ConfigType::Boolean}.defaultValue(false)},
        {"prometheus.compress_reply", ConfigValue{ConfigType::Boolean}.defaultValue(true)}
    };
    PrometheusService::init(config);
    auto response = handlePrometheusRequest(req, true);
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->result(), http::status::forbidden);
}

TEST_F(PrometheusHandleRequestTests, notAdmin)
{
    auto response = handlePrometheusRequest(req, false);
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->result(), http::status::unauthorized);
}

TEST_F(PrometheusHandleRequestTests, responseWithCounter)
{
    auto const counterName = "test_counter";
    Labels const labels{{{"label1", "value1"}, Label{"label2", "value2"}}};
    auto const description = "test_description";

    auto& counter = PrometheusService::counterInt(counterName, labels, description);
    ++counter;
    counter += 3;

    auto response = handlePrometheusRequest(req, true);
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->result(), http::status::ok);
    EXPECT_EQ(response->operator[](http::field::content_type), "text/plain; version=0.0.4");
    auto const expectedBody =
        fmt::format("# HELP {0} {1}\n# TYPE {0} counter\n{0}{2} 4\n\n", counterName, description, labels.serialize());
    EXPECT_EQ(response->body(), expectedBody);
}

TEST_F(PrometheusHandleRequestTests, responseWithGauge)
{
    auto const gaugeName = "test_gauge";
    Labels const labels{{{"label2", "value2"}, {"label3", "value3"}}};
    auto const description = "test_description_gauge";

    auto& gauge = PrometheusService::gaugeInt(gaugeName, labels, description);
    ++gauge;
    gauge -= 3;

    auto response = handlePrometheusRequest(req, true);
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->result(), http::status::ok);
    EXPECT_EQ(response->operator[](http::field::content_type), "text/plain; version=0.0.4");
    auto const expectedBody =
        fmt::format("# HELP {0} {1}\n# TYPE {0} gauge\n{0}{2} -2\n\n", gaugeName, description, labels.serialize());
    EXPECT_EQ(response->body(), expectedBody);
}

TEST_F(PrometheusHandleRequestTests, responseWithCounterAndGauge)
{
    auto const counterName = "test_counter";
    Labels const counterLabels{{{"label1", "value1"}, {"label2", "value2"}}};
    auto const counterDescription = "test_description";

    auto& counter = PrometheusService::counterInt(counterName, counterLabels, counterDescription);
    ++counter;
    counter += 3;

    auto const gaugeName = "test_gauge";
    Labels const gaugeLabels{{{"label2", "value2"}, {"label3", "value3"}}};
    auto const gaugeDescription = "test_description_gauge";

    auto& gauge = PrometheusService::gaugeInt(gaugeName, gaugeLabels, gaugeDescription);
    ++gauge;
    gauge -= 3;

    auto response = handlePrometheusRequest(req, true);

    EXPECT_EQ(response->result(), http::status::ok);
    EXPECT_EQ(response->operator[](http::field::content_type), "text/plain; version=0.0.4");
    auto const expectedBody = fmt::format(
        "# HELP {3} {4}\n# TYPE {3} gauge\n{3}{5} -2\n\n"
        "# HELP {0} {1}\n# TYPE {0} counter\n{0}{2} 4\n\n",
        counterName,
        counterDescription,
        counterLabels.serialize(),
        gaugeName,
        gaugeDescription,
        gaugeLabels.serialize()
    );
    auto const anotherExpectedBody = fmt::format(
        "# HELP {0} {1}\n# TYPE {0} counter\n{0}{2} 4\n\n"
        "# HELP {3} {4}\n# TYPE {3} gauge\n{3}{5} -2\n\n",
        counterName,
        counterDescription,
        counterLabels.serialize(),
        gaugeName,
        gaugeDescription,
        gaugeLabels.serialize()
    );
    EXPECT_TRUE(response->body() == expectedBody || response->body() == anotherExpectedBody);
}

TEST_F(PrometheusHandleRequestTests, compressReply)
{
    PrometheusService::init(ClioConfigDefinition{
        {"prometheus.compress_reply", ConfigValue{ConfigType::Boolean}.defaultValue(true)},
        {"prometheus.enabled", ConfigValue{ConfigType::Boolean}.defaultValue(true)},
    });

    auto& gauge = PrometheusService::gaugeInt("test_gauge", Labels{});
    ++gauge;

    auto response = handlePrometheusRequest(req, true);
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->result(), http::status::ok);
    EXPECT_EQ(response->operator[](http::field::content_type), "text/plain; version=0.0.4");
    EXPECT_EQ(response->operator[](http::field::content_encoding), "gzip");
    EXPECT_GT(response->body().size(), 0ul);
}
