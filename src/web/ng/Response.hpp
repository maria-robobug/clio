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

#include "web/ng/Request.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/json/object.hpp>

#include <string>
#include <variant>

namespace web::ng {

class Connection;

/**
 * @brief Represents an HTTP or Websocket response.
 */
class Response {
public:
    std::variant<boost::beast::http::response<boost::beast::http::string_body>, std::string> data;

public:
    /**
     * @brief Construct a Response from string. Content type will be text/html.
     *
     * @param status The HTTP status. It will be ignored if request is WebSocket.
     * @param message The message to send.
     * @param request The request that triggered this response. Used to determine whether the response should contain
     * HTTP or WebSocket data.
     */
    Response(boost::beast::http::status status, std::string message, Request const& request);

    /**
     * @brief Construct a Response from JSON object. Content type will be application/json.
     *
     * @param status The HTTP status. It will be ignored if request is WebSocket.
     * @param message The message to send.
     * @param request The request that triggered this response. Used to determine whether the response should contain
     * HTTP or WebSocket
     */
    Response(boost::beast::http::status status, boost::json::object const& message, Request const& request);

    /**
     * @brief Construct a Response from string. Content type will be text/html.
     *
     * @param status The HTTP status.
     * @param message The message to send.
     * @param connection The connection that triggered this response. Used to determine whether the response should
     * contain HTTP or WebSocket data.
     */
    Response(boost::beast::http::status status, boost::json::object const& message, Connection const& connection);

    /**
     * @brief Construct a Response from string. Content type will be text/html.
     *
     * @param status The HTTP status.
     * @param message The message to send.
     * @param connection The connection that triggered this response. Used to determine whether the response should
     * contain HTTP or WebSocket data.
     */
    Response(boost::beast::http::status status, std::string message, Connection const& connection);

    /**
     * @brief Construct a Response from HTTP response.
     *
     * @param response The HTTP response.
     * @param request The request that triggered this response. It must be an HTTP request.
     */
    Response(boost::beast::http::response<boost::beast::http::string_body> response, Request const& request);

    /**
     * @brief Get the message of the response.
     *
     * @return The message of the response.
     */
    std::string const&
    message() const;

    /**
     * @brief Replace existing message (or body) with new message.
     *
     * @param newMessage The new message.
     */
    void
    setMessage(std::string newMessage);

    /**
     * @brief Replace existing message (or body) with new message.
     *
     * @param newMessage The new message.
     */
    void
    setMessage(boost::json::object const& newMessage);

    /**
     * @brief Convert the Response to an HTTP response.
     * @note The Response must be constructed with an HTTP request.
     *
     * @return The HTTP response.
     */
    boost::beast::http::response<boost::beast::http::string_body>
    intoHttpResponse() &&;

    /**
     * @brief Get the message of the response as a const buffer.
     * @note The response must be constructed with a WebSocket request.
     *
     * @return The message of the response as a const buffer.
     */
    boost::asio::const_buffer
    asWsResponse() const&;
};

}  // namespace web::ng
