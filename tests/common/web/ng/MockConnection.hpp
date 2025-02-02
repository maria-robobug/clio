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

#pragma once

#include "web/ng/Connection.hpp"
#include "web/ng/Error.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/Response.hpp"
#include "web/ng/impl/HttpConnection.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/context.hpp>
#include <gmock/gmock.h>

#include <chrono>
#include <memory>
#include <optional>

struct MockConnectionMetadataImpl : web::ng::ConnectionMetadata {
    using web::ng::ConnectionMetadata::ConnectionMetadata;
    MOCK_METHOD(bool, wasUpgraded, (), (const, override));
};

using MockConnectionMetadata = testing::NiceMock<MockConnectionMetadataImpl>;
using StrictMockConnectionMetadata = testing::StrictMock<MockConnectionMetadataImpl>;

struct MockConnectionImpl : web::ng::Connection {
    using web::ng::Connection::Connection;

    MOCK_METHOD(bool, wasUpgraded, (), (const, override));

    MOCK_METHOD(void, setTimeout, (std::chrono::steady_clock::duration), (override));

    using SendReturnType = std::optional<web::ng::Error>;
    MOCK_METHOD(SendReturnType, send, (web::ng::Response, boost::asio::yield_context), (override));

    using ReceiveReturnType = std::expected<web::ng::Request, web::ng::Error>;
    MOCK_METHOD(ReceiveReturnType, receive, (boost::asio::yield_context), (override));

    MOCK_METHOD(void, close, (boost::asio::yield_context), (override));
};

using MockConnection = testing::NiceMock<MockConnectionImpl>;
using MockConnectionPtr = std::unique_ptr<testing::NiceMock<MockConnectionImpl>>;

using StrictMockConnection = testing::StrictMock<MockConnectionImpl>;
using StrictMockConnectionPtr = std::unique_ptr<testing::StrictMock<MockConnectionImpl>>;
