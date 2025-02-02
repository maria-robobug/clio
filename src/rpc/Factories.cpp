//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

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

#include "rpc/Factories.hpp"

#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/common/APIVersion.hpp"
#include "rpc/common/Types.hpp"
#include "util/Taggable.hpp"
#include "web/Context.hpp"
#include "web/SubscriptionContextInterface.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>

#include <expected>
#include <functional>
#include <string>
#include <utility>

using namespace std;
using namespace util;

namespace rpc {

std::expected<web::Context, Status>
makeWsContext(
    boost::asio::yield_context yc,
    boost::json::object const& request,
    web::SubscriptionContextPtr session,
    util::TagDecoratorFactory const& tagFactory,
    data::LedgerRange const& range,
    std::string const& clientIp,
    std::reference_wrapper<APIVersionParser const> apiVersionParser,
    bool isAdmin
)
{
    boost::json::value commandValue = nullptr;
    if (!request.contains("command") && request.contains("method")) {
        commandValue = request.at("method");
    } else if (request.contains("command") && !request.contains("method")) {
        commandValue = request.at("command");
    }

    if (!commandValue.is_string())
        return Error{{ClioError::RpcCommandIsMissing, "Method/Command is not specified or is not a string."}};

    auto const apiVersion = apiVersionParser.get().parse(request);
    if (!apiVersion)
        return Error{{ClioError::RpcInvalidApiVersion, apiVersion.error()}};

    auto const command = boost::json::value_to<std::string>(commandValue);
    return web::Context(yc, command, *apiVersion, request, std::move(session), tagFactory, range, clientIp, isAdmin);
}

std::expected<web::Context, Status>
makeHttpContext(
    boost::asio::yield_context yc,
    boost::json::object const& request,
    util::TagDecoratorFactory const& tagFactory,
    data::LedgerRange const& range,
    std::string const& clientIp,
    std::reference_wrapper<APIVersionParser const> apiVersionParser,
    bool const isAdmin
)
{
    if (!request.contains("method"))
        return Error{{ClioError::RpcCommandIsMissing}};

    if (!request.at("method").is_string())
        return Error{{ClioError::RpcCommandNotString}};

    if (request.at("method").as_string().empty())
        return Error{{ClioError::RpcCommandIsEmpty}};

    auto const command = boost::json::value_to<std::string>(request.at("method"));

    if (command == "subscribe" || command == "unsubscribe")
        return Error{{RippledError::rpcBAD_SYNTAX, "Subscribe and unsubscribe are only allowed for websocket."}};

    if (!request.at("params").is_array())
        return Error{{ClioError::RpcParamsUnparseable, "Missing params array."}};

    boost::json::array const& array = request.at("params").as_array();

    if (array.size() != 1 || !array.at(0).is_object())
        return Error{{ClioError::RpcParamsUnparseable}};

    auto const apiVersion = apiVersionParser.get().parse(request.at("params").as_array().at(0).as_object());
    if (!apiVersion)
        return Error{{ClioError::RpcInvalidApiVersion, apiVersion.error()}};

    return web::Context(
        yc, command, *apiVersion, array.at(0).as_object(), nullptr, tagFactory, range, clientIp, isAdmin
    );
}

}  // namespace rpc
