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

#include "etl/impl/ForwardingSource.hpp"
#include "rpc/Errors.hpp"
#include "util/AsioContextTestFixture.hpp"
#include "util/TestWsServer.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <utility>

using namespace etl::impl;

struct ForwardingSourceTests : SyncAsioContextTest {
protected:
    TestWsServer server_{ctx_, "0.0.0.0"};
    ForwardingSource forwardingSource_{
        "127.0.0.1",
        server_.port(),
        std::chrono::milliseconds{20},
        std::chrono::milliseconds{20}
    };
};

TEST_F(ForwardingSourceTests, ConnectionFailed)
{
    runSpawn([&](boost::asio::yield_context yield) {
        auto result = forwardingSource_.forwardToRippled({}, {}, {}, yield);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error(), rpc::ClioError::EtlConnectionError);
    });
}

struct ForwardingSourceOperationsTests : ForwardingSourceTests {
    TestWsConnection
    serverConnection(boost::asio::yield_context yield)
    {
        // First connection attempt is SSL handshake so it will fail
        auto failedConnection = server_.acceptConnection(yield);
        [&]() { ASSERT_FALSE(failedConnection); }();

        auto connection = server_.acceptConnection(yield);
        [&]() { ASSERT_TRUE(connection) << connection.error().message(); }();
        return std::move(connection).value();
    }

protected:
    std::string const message_ = R"({"data": "some_data"})";
    boost::json::object const reply_ = {{"reply", "some_reply"}};
};

TEST_F(ForwardingSourceOperationsTests, XUserHeader)
{
    std::string const xUserValue = "some_user";
    boost::asio::spawn(ctx_, [&](boost::asio::yield_context yield) {
        auto connection = serverConnection(yield);
        auto headers = connection.headers();
        ASSERT_FALSE(headers.empty());
        auto it = std::ranges::find_if(headers, [](auto const& header) {
            return std::holds_alternative<std::string>(header.name) && std::get<std::string>(header.name) == "X-User";
        });
        ASSERT_FALSE(it == headers.end());
        EXPECT_EQ(std::get<std::string>(it->name), "X-User");
        EXPECT_EQ(it->value, xUserValue);
        connection.close(yield);
    });

    runSpawn([&](boost::asio::yield_context yield) {
        auto result =
            forwardingSource_.forwardToRippled(boost::json::parse(message_).as_object(), {}, xUserValue, yield);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error(), rpc::ClioError::EtlRequestError);
    });
}

TEST_F(ForwardingSourceOperationsTests, ReadFailed)
{
    boost::asio::spawn(ctx_, [&](boost::asio::yield_context yield) {
        auto connection = serverConnection(yield);
        connection.close(yield);
    });

    runSpawn([&](boost::asio::yield_context yield) {
        auto result = forwardingSource_.forwardToRippled(boost::json::parse(message_).as_object(), {}, {}, yield);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error(), rpc::ClioError::EtlRequestError);
    });
}

TEST_F(ForwardingSourceOperationsTests, ReadTimeout)
{
    TestWsConnectionPtr connection;
    boost::asio::spawn(ctx_, [&](boost::asio::yield_context yield) {
        connection = std::make_unique<TestWsConnection>(serverConnection(yield));
    });

    runSpawn([&](boost::asio::yield_context yield) {
        auto result = forwardingSource_.forwardToRippled(boost::json::parse(message_).as_object(), {}, {}, yield);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error(), rpc::ClioError::EtlRequestTimeout);
    });
}

TEST_F(ForwardingSourceOperationsTests, ParseFailed)
{
    boost::asio::spawn(ctx_, [&](boost::asio::yield_context yield) {
        auto connection = serverConnection(yield);

        auto receivedMessage = connection.receive(yield);
        [&]() { ASSERT_TRUE(receivedMessage); }();
        EXPECT_EQ(boost::json::parse(*receivedMessage), boost::json::parse(message_)) << *receivedMessage;

        auto sendError = connection.send("invalid_json", yield);
        [&]() { ASSERT_FALSE(sendError) << *sendError; }();

        connection.close(yield);
    });

    runSpawn([&](boost::asio::yield_context yield) {
        auto result = forwardingSource_.forwardToRippled(boost::json::parse(message_).as_object(), {}, {}, yield);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error(), rpc::ClioError::EtlInvalidResponse);
    });
}

TEST_F(ForwardingSourceOperationsTests, GotNotAnObject)
{
    boost::asio::spawn(ctx_, [&](boost::asio::yield_context yield) {
        auto connection = serverConnection(yield);

        auto receivedMessage = connection.receive(yield);
        [&]() { ASSERT_TRUE(receivedMessage); }();
        EXPECT_EQ(boost::json::parse(*receivedMessage), boost::json::parse(message_)) << *receivedMessage;

        auto sendError = connection.send(R"(["some_value"])", yield);

        [&]() { ASSERT_FALSE(sendError) << *sendError; }();

        connection.close(yield);
    });

    runSpawn([&](boost::asio::yield_context yield) {
        auto result = forwardingSource_.forwardToRippled(boost::json::parse(message_).as_object(), {}, {}, yield);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error(), rpc::ClioError::EtlInvalidResponse);
    });
}

TEST_F(ForwardingSourceOperationsTests, Success)
{
    boost::asio::spawn(ctx_, [&](boost::asio::yield_context yield) {
        auto connection = serverConnection(yield);

        auto receivedMessage = connection.receive(yield);
        [&]() { ASSERT_TRUE(receivedMessage); }();
        EXPECT_EQ(boost::json::parse(*receivedMessage), boost::json::parse(message_)) << *receivedMessage;

        auto sendError = connection.send(boost::json::serialize(reply_), yield);
        [&]() { ASSERT_FALSE(sendError) << *sendError; }();
    });

    runSpawn([&](boost::asio::yield_context yield) {
        auto result =
            forwardingSource_.forwardToRippled(boost::json::parse(message_).as_object(), "some_ip", {}, yield);
        [&]() { ASSERT_TRUE(result); }();
        auto expectedReply = reply_;
        expectedReply["forwarded"] = true;
        EXPECT_EQ(*result, expectedReply) << *result;
    });
}
