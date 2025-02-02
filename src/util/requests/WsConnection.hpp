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

#include "util/log/Logger.hpp"
#include "util/requests/Types.hpp"

#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/websocket/stream.hpp>

#include <chrono>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace util::requests {

/**
 * @brief Interface for WebSocket connections. It is used to hide SSL and plain connections behind the same interface.
 *
 * @note WsConnection must not be destroyed while there are pending asynchronous operations on it.
 */
class WsConnection {
public:
    virtual ~WsConnection() = default;

    /**
     * @brief Read a message from the WebSocket
     *
     * @param yield yield context
     * @param timeout timeout for the operation
     * @return Message or error
     */
    virtual std::expected<std::string, RequestError>
    read(
        boost::asio::yield_context yield,
        std::optional<std::chrono::steady_clock::duration> timeout = std::nullopt
    ) = 0;

    /**
     * @brief Write a message to the WebSocket
     *
     * @param message message to write
     * @param yield yield context
     * @param timeout timeout for the operation
     * @return Error if any
     */
    virtual std::optional<RequestError>
    write(
        std::string const& message,
        boost::asio::yield_context yield,
        std::optional<std::chrono::steady_clock::duration> timeout = std::nullopt
    ) = 0;

    /**
     * @brief Close the WebSocket
     *
     * @param yield yield context
     * @param timeout timeout for the operation
     * @return Error if any
     */
    virtual std::optional<RequestError>
    close(boost::asio::yield_context yield, std::chrono::steady_clock::duration timeout = kDEFAULT_TIMEOUT) = 0;

    static constexpr std::chrono::seconds kDEFAULT_TIMEOUT{5}; /**< Default timeout for connecting */
};
using WsConnectionPtr = std::unique_ptr<WsConnection>;

/**
 * @brief Builder for WebSocket connections
 */
class WsConnectionBuilder {
    util::Logger log_{"WsConnectionBuilder"};
    std::string host_;
    std::string port_;
    std::vector<HttpHeader> headers_;
    std::chrono::steady_clock::duration connectionTimeout_{kDEFAULT_TIMEOUT};
    std::chrono::steady_clock::duration wsHandshakeTimeout_{kDEFAULT_TIMEOUT};
    std::string target_{"/"};

public:
    /**
     * @brief Create a new connection builder
     *
     * @param host Host to connect to
     * @param port Port to connect to
     */
    WsConnectionBuilder(std::string host, std::string port);

    /**
     * @brief Add a header to the request
     *
     * @param header header to add
     * @return Reference to self
     */
    WsConnectionBuilder&
    addHeader(HttpHeader header);

    /**
     * @brief Add multiple headers to the request
     *
     * @param headers headers to add
     * @return Reference to self
     */
    WsConnectionBuilder&
    addHeaders(std::vector<HttpHeader> headers);

    /**
     * @brief Set the target of the request
     *
     * @param target target to set
     * @return Reference to self
     */
    WsConnectionBuilder&
    setTarget(std::string target);

    /**
     * @brief Set the timeout for connection establishing operations. Default is 5 seconds
     *
     * @param timeout timeout to set
     * @return Reference to self
     */
    WsConnectionBuilder&
    setConnectionTimeout(std::chrono::steady_clock::duration timeout);

    /**
     * @brief Set the timeout for WebSocket handshake. Default is 5 seconds
     *
     * @param timeout timeout to set
     * @return Reference to self
     */
    WsConnectionBuilder&
    setWsHandshakeTimeout(std::chrono::steady_clock::duration timeout);

    /**
     * @brief Connect to the host using SSL asynchronously
     *
     * @param yield yield context
     * @return WebSocket connection or error
     */
    std::expected<WsConnectionPtr, RequestError>
    sslConnect(boost::asio::yield_context yield) const;

    /**
     * @brief Connect to the host without SSL asynchronously
     *
     * @param yield yield context
     * @return WebSocket connection or error
     */
    std::expected<WsConnectionPtr, RequestError>
    plainConnect(boost::asio::yield_context yield) const;

    /**
     * @brief Connect to the host trying SSL first then plain if SSL fails
     *
     * @param yield yield context
     * @return WebSocket connection or error
     */
    std::expected<WsConnectionPtr, RequestError>
    connect(boost::asio::yield_context yield) const;

    static constexpr std::chrono::seconds kDEFAULT_TIMEOUT{5}; /**< Default timeout for connecting */

private:
    template <typename StreamDataType>
    std::expected<WsConnectionPtr, RequestError>
    connectImpl(StreamDataType&& streamData, boost::asio::yield_context yield) const;
};

}  // namespace util::requests
