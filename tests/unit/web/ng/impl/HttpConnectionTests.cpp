//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "util/AsioContextTestFixture.hpp"
#include "util/Taggable.hpp"
#include "util/TestHttpClient.hpp"
#include "util/TestHttpServer.hpp"
#include "util/TestWebSocketClient.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/Response.hpp"
#include "web/ng/impl/HttpConnection.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <optional>
#include <ranges>
#include <utility>

using namespace web::ng::impl;
using namespace web::ng;
using namespace util::config;
namespace http = boost::beast::http;

struct HttpConnectionTests : SyncAsioContextTest {
    PlainHttpConnection
    acceptConnection(boost::asio::yield_context yield)
    {
        auto expectedSocket = httpServer_.accept(yield);
        [&]() { ASSERT_TRUE(expectedSocket.has_value()) << expectedSocket.error().message(); }();
        auto ip = expectedSocket->remote_endpoint().address().to_string();
        PlainHttpConnection connection{
            std::move(expectedSocket).value(), std::move(ip), boost::beast::flat_buffer{}, tagDecoratorFactory_
        };
        connection.setTimeout(std::chrono::milliseconds{100});
        return connection;
    }

protected:
    util::TagDecoratorFactory tagDecoratorFactory_{
        ClioConfigDefinition{{"log_tag_style", ConfigValue{ConfigType::String}.defaultValue("int")}}
    };
    TestHttpServer httpServer_{ctx_, "localhost"};
    HttpAsyncClient httpClient_{ctx_};
    http::request<http::string_body> request_{http::verb::post, "/some_target", 11, "some data"};
};

TEST_F(HttpConnectionTests, wasUpgraded)
{
    boost::asio::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto maybeError = httpClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError->message(); }();
    });

    runSpawn([this](boost::asio::yield_context yield) {
        auto connection = acceptConnection(yield);
        EXPECT_FALSE(connection.wasUpgraded());
    });
}

TEST_F(HttpConnectionTests, Receive)
{
    request_.set(boost::beast::http::field::user_agent, "test_client");

    boost::asio::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto maybeError = httpClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError->message(); }();

        maybeError = httpClient_.send(request_, yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError->message(); }();
    });

    runSpawn([this](boost::asio::yield_context yield) {
        auto connection = acceptConnection(yield);

        auto expectedRequest = connection.receive(yield);
        ASSERT_TRUE(expectedRequest.has_value()) << expectedRequest.error().message();
        ASSERT_TRUE(expectedRequest->isHttp());

        auto const& receivedRequest = expectedRequest.value().asHttpRequest()->get();
        EXPECT_EQ(receivedRequest.method(), request_.method());
        EXPECT_EQ(receivedRequest.target(), request_.target());
        EXPECT_EQ(receivedRequest.body(), request_.body());
        EXPECT_EQ(
            receivedRequest.at(boost::beast::http::field::user_agent),
            request_.at(boost::beast::http::field::user_agent)
        );
    });
}

TEST_F(HttpConnectionTests, ReceiveTimeout)
{
    boost::asio::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto maybeError = httpClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{1});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError->message(); }();
    });

    runSpawn([this](boost::asio::yield_context yield) {
        auto connection = acceptConnection(yield);
        connection.setTimeout(std::chrono::milliseconds{1});
        auto expectedRequest = connection.receive(yield);
        EXPECT_FALSE(expectedRequest.has_value());
    });
}

TEST_F(HttpConnectionTests, ReceiveClientDisconnected)
{
    boost::asio::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto maybeError = httpClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{1});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError->message(); }();
        httpClient_.disconnect();
    });

    runSpawn([this](boost::asio::yield_context yield) {
        auto connection = acceptConnection(yield);
        connection.setTimeout(std::chrono::milliseconds{1});
        auto expectedRequest = connection.receive(yield);
        EXPECT_FALSE(expectedRequest.has_value());
    });
}

TEST_F(HttpConnectionTests, Send)
{
    Request const request{request_};
    Response const response{http::status::ok, "some response data", request};

    boost::asio::spawn(ctx_, [this, response = response](boost::asio::yield_context yield) mutable {
        auto maybeError = httpClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError->message(); }();

        auto const expectedResponse = httpClient_.receive(yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_TRUE(expectedResponse.has_value()) << maybeError->message(); }();

        auto const receivedResponse = expectedResponse.value();
        auto const sentResponse = std::move(response).intoHttpResponse();
        EXPECT_EQ(receivedResponse.result(), sentResponse.result());
        EXPECT_EQ(receivedResponse.body(), sentResponse.body());
        EXPECT_EQ(receivedResponse.version(), request_.version());
        EXPECT_TRUE(receivedResponse.keep_alive());
    });

    runSpawn([this, &response](boost::asio::yield_context yield) {
        auto connection = acceptConnection(yield);
        auto maybeError = connection.send(response, yield);
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError->message(); }();
    });
}

TEST_F(HttpConnectionTests, SendMultipleTimes)
{
    Request const request{request_};
    Response const response{http::status::ok, "some response data", request};

    boost::asio::spawn(ctx_, [this, response = response](boost::asio::yield_context yield) mutable {
        auto maybeError = httpClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError->message(); }();

        for ([[maybe_unused]] auto i : std::ranges::iota_view{0, 3}) {
            auto const expectedResponse = httpClient_.receive(yield, std::chrono::milliseconds{100});
            [&]() { ASSERT_TRUE(expectedResponse.has_value()) << maybeError->message(); }();

            auto const receivedResponse = expectedResponse.value();
            auto const sentResponse = Response{response}.intoHttpResponse();
            EXPECT_EQ(receivedResponse.result(), sentResponse.result());
            EXPECT_EQ(receivedResponse.body(), sentResponse.body());
            EXPECT_EQ(receivedResponse.version(), request_.version());
            EXPECT_TRUE(receivedResponse.keep_alive());
        }
    });

    runSpawn([this, &response](boost::asio::yield_context yield) {
        auto connection = acceptConnection(yield);

        for ([[maybe_unused]] auto i : std::ranges::iota_view{0, 3}) {
            auto maybeError = connection.send(response, yield);
            [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError->message(); }();
        }
    });
}

TEST_F(HttpConnectionTests, SendClientDisconnected)
{
    Response const response{http::status::ok, "some response data", Request{request_}};
    boost::asio::spawn(ctx_, [this, response = response](boost::asio::yield_context yield) mutable {
        auto maybeError = httpClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{1});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError->message(); }();
        httpClient_.disconnect();
    });
    runSpawn([this, &response](boost::asio::yield_context yield) {
        auto connection = acceptConnection(yield);
        connection.setTimeout(std::chrono::milliseconds{1});
        auto maybeError = connection.send(response, yield);
        size_t counter{1};
        while (not maybeError.has_value() and counter < 100) {
            ++counter;
            maybeError = connection.send(response, yield);
        }
        EXPECT_TRUE(maybeError.has_value());
        EXPECT_LT(counter, 100);
    });
}

TEST_F(HttpConnectionTests, Close)
{
    boost::asio::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto maybeError = httpClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError->message(); }();

        size_t counter{0};
        while (not maybeError.has_value() and counter < 100) {
            ++counter;
            maybeError = httpClient_.send(request_, yield, std::chrono::milliseconds{1});
        }
        EXPECT_TRUE(maybeError.has_value());
        EXPECT_LT(counter, 100);
    });

    runSpawn([this](boost::asio::yield_context yield) {
        auto connection = acceptConnection(yield);
        connection.setTimeout(std::chrono::milliseconds{1});
        connection.close(yield);
    });
}

TEST_F(HttpConnectionTests, IsUpgradeRequested_GotHttpRequest)
{
    boost::asio::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto maybeError = httpClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError->message(); }();

        maybeError = httpClient_.send(request_, yield, std::chrono::milliseconds{1});
        EXPECT_FALSE(maybeError.has_value()) << maybeError->message();
    });

    runSpawn([this](boost::asio::yield_context yield) {
        auto connection = acceptConnection(yield);
        auto result = connection.isUpgradeRequested(yield);
        [&]() { ASSERT_TRUE(result.has_value()) << result.error().message(); }();
        EXPECT_FALSE(result.value());
    });
}

TEST_F(HttpConnectionTests, IsUpgradeRequested_FailedToFetch)
{
    boost::asio::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto maybeError = httpClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError->message(); }();
    });

    runSpawn([this](boost::asio::yield_context yield) {
        auto connection = acceptConnection(yield);
        connection.setTimeout(std::chrono::milliseconds{1});
        auto result = connection.isUpgradeRequested(yield);
        EXPECT_FALSE(result.has_value());
    });
}

TEST_F(HttpConnectionTests, Upgrade)
{
    WebSocketAsyncClient wsClient{ctx_};

    boost::asio::spawn(ctx_, [this, &wsClient](boost::asio::yield_context yield) {
        auto maybeError = wsClient.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError->message(); }();
    });

    runSpawn([this](boost::asio::yield_context yield) {
        auto connection = acceptConnection(yield);
        auto const expectedResult = connection.isUpgradeRequested(yield);
        [&]() { ASSERT_TRUE(expectedResult.has_value()) << expectedResult.error().message(); }();
        [&]() { ASSERT_TRUE(expectedResult.value()); }();

        std::optional<boost::asio::ssl::context> sslContext;
        auto expectedWsConnection = connection.upgrade(sslContext, tagDecoratorFactory_, yield);
        [&]() { ASSERT_TRUE(expectedWsConnection.has_value()) << expectedWsConnection.error().message(); }();
    });
}

TEST_F(HttpConnectionTests, Ip)
{
    boost::asio::spawn(ctx_, [this](boost::asio::yield_context yield) mutable {
        auto maybeError = httpClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError->message(); }();
    });

    runSpawn([this](boost::asio::yield_context yield) {
        auto connection = acceptConnection(yield);
        EXPECT_TRUE(connection.ip() == "127.0.0.1" or connection.ip() == "::1") << connection.ip();
    });
}

TEST_F(HttpConnectionTests, isAdminSetAdmin)
{
    testing::StrictMock<testing::MockFunction<bool()>> adminSetter;
    EXPECT_CALL(adminSetter, Call).WillOnce(testing::Return(true));

    boost::asio::spawn(ctx_, [this](boost::asio::yield_context yield) mutable {
        auto maybeError = httpClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError->message(); }();
    });

    runSpawn([&](boost::asio::yield_context yield) {
        auto connection = acceptConnection(yield);
        EXPECT_FALSE(connection.isAdmin());

        connection.setIsAdmin(adminSetter.AsStdFunction());
        EXPECT_TRUE(connection.isAdmin());

        // Setter shouldn't not be called here because isAdmin is already set
        connection.setIsAdmin(adminSetter.AsStdFunction());
        EXPECT_TRUE(connection.isAdmin());
    });
}
