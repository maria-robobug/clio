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

#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/Errors.hpp"
#include "rpc/Factories.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/impl/APIVersionParser.hpp"
#include "util/Assert.hpp"
#include "util/CoroutineGroup.hpp"
#include "util/JsonUtils.hpp"
#include "util/Profiler.hpp"
#include "util/Taggable.hpp"
#include "util/log/Logger.hpp"
#include "web/SubscriptionContextInterface.hpp"
#include "web/ng/Connection.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/Response.hpp"
#include "web/ng/impl/ErrorHandling.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <boost/system/system_error.hpp>
#include <xrpl/protocol/jss.h>

#include <chrono>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <ratio>
#include <stdexcept>
#include <string>
#include <utility>

namespace web::ng {

/**
 * @brief The server handler for RPC requests called by web server.
 *
 * Note: see @ref web::SomeServerHandler concept
 */
template <typename RPCEngineType, typename ETLType>
class RPCServerHandler {
    std::shared_ptr<BackendInterface const> const backend_;
    std::shared_ptr<RPCEngineType> const rpcEngine_;
    std::shared_ptr<ETLType const> const etl_;
    util::TagDecoratorFactory const tagFactory_;
    rpc::impl::ProductionAPIVersionParser apiVersionParser_;  // can be injected if needed

    util::Logger log_{"RPC"};
    util::Logger perfLog_{"Performance"};

public:
    /**
     * @brief Create a new server handler.
     *
     * @param config Clio config to use
     * @param backend The backend to use
     * @param rpcEngine The RPC engine to use
     * @param etl The ETL to use
     */
    RPCServerHandler(
        util::config::ClioConfigDefinition const& config,
        std::shared_ptr<BackendInterface const> const& backend,
        std::shared_ptr<RPCEngineType> const& rpcEngine,
        std::shared_ptr<ETLType const> const& etl
    )
        : backend_(backend)
        , rpcEngine_(rpcEngine)
        , etl_(etl)
        , tagFactory_(config)
        , apiVersionParser_(config.getObject("api_version"))
    {
    }

    /**
     * @brief The callback when server receives a request.
     *
     * @param request The request
     * @param connectionMetadata The connection metadata
     * @param subscriptionContext The subscription context
     * @param yield The yield context
     * @return The response
     */
    [[nodiscard]] Response
    operator()(
        Request const& request,
        ConnectionMetadata const& connectionMetadata,
        SubscriptionContextPtr subscriptionContext,
        boost::asio::yield_context yield
    )
    {
        std::optional<Response> response;
        util::CoroutineGroup coroutineGroup{yield, 1};
        auto const onTaskComplete = coroutineGroup.registerForeign();
        ASSERT(onTaskComplete.has_value(), "Coroutine group can't be full");

        bool const postSuccessful = rpcEngine_->post(
            [this,
             &request,
             &response,
             &onTaskComplete = onTaskComplete.value(),
             &connectionMetadata,
             subscriptionContext = std::move(subscriptionContext)](boost::asio::yield_context yield) mutable {
                try {
                    auto parsedRequest = boost::json::parse(request.message()).as_object();
                    LOG(perfLog_.debug()) << connectionMetadata.tag() << "Adding to work queue";

                    if (not connectionMetadata.wasUpgraded() and shouldReplaceParams(parsedRequest))
                        parsedRequest[JS(params)] = boost::json::array({boost::json::object{}});

                    response = handleRequest(
                        yield, request, std::move(parsedRequest), connectionMetadata, std::move(subscriptionContext)
                    );
                } catch (boost::system::system_error const& ex) {
                    // system_error thrown when json parsing failed
                    rpcEngine_->notifyBadSyntax();
                    response = impl::ErrorHelper{request}.makeJsonParsingError();
                    LOG(log_.warn()) << "Error parsing JSON: " << ex.what() << ". For request: " << request.message();
                } catch (std::invalid_argument const& ex) {
                    // thrown when json parses something that is not an object at top level
                    rpcEngine_->notifyBadSyntax();
                    LOG(log_.warn()) << "Invalid argument error: " << ex.what()
                                     << ". For request: " << request.message();
                    response = impl::ErrorHelper{request}.makeJsonParsingError();
                } catch (std::exception const& ex) {
                    LOG(perfLog_.error()) << connectionMetadata.tag() << "Caught exception: " << ex.what();
                    rpcEngine_->notifyInternalError();
                    response = impl::ErrorHelper{request}.makeInternalError();
                }

                // notify the coroutine group that the foreign task is done
                onTaskComplete();
            },
            connectionMetadata.ip()
        );

        if (not postSuccessful) {
            // onTaskComplete must be called to notify coroutineGroup that the foreign task is done
            onTaskComplete->operator()();
            rpcEngine_->notifyTooBusy();
            return impl::ErrorHelper{request}.makeTooBusyError();
        }

        // Put the coroutine to sleep until the foreign task is done
        coroutineGroup.asyncWait(yield);
        ASSERT(response.has_value(), "Woke up coroutine without setting response");
        return std::move(response).value();
    }

private:
    Response
    handleRequest(
        boost::asio::yield_context yield,
        Request const& rawRequest,
        boost::json::object&& request,
        ConnectionMetadata const& connectionMetadata,
        SubscriptionContextPtr subscriptionContext
    )
    {
        LOG(log_.info()) << connectionMetadata.tag() << (connectionMetadata.wasUpgraded() ? "ws" : "http")
                         << " received request from work queue: " << util::removeSecret(request)
                         << " ip = " << connectionMetadata.ip();

        try {
            auto const range = backend_->fetchLedgerRange();
            if (!range) {
                // for error that happened before the handler, we don't attach any warnings
                rpcEngine_->notifyNotReady();
                return impl::ErrorHelper{rawRequest, std::move(request)}.makeNotReadyError();
            }

            auto const context = [&] {
                if (connectionMetadata.wasUpgraded()) {
                    ASSERT(subscriptionContext != nullptr, "Subscription context must exist for a WS connecton");
                    return rpc::makeWsContext(
                        yield,
                        request,
                        std::move(subscriptionContext),
                        tagFactory_.with(connectionMetadata.tag()),
                        *range,
                        connectionMetadata.ip(),
                        std::cref(apiVersionParser_),
                        connectionMetadata.isAdmin()
                    );
                }
                return rpc::makeHttpContext(
                    yield,
                    request,
                    tagFactory_.with(connectionMetadata.tag()),
                    *range,
                    connectionMetadata.ip(),
                    std::cref(apiVersionParser_),
                    connectionMetadata.isAdmin()
                );
            }();

            if (!context) {
                auto const err = context.error();
                LOG(perfLog_.warn()) << connectionMetadata.tag() << "Could not create Web context: " << err;
                LOG(log_.warn()) << connectionMetadata.tag() << "Could not create Web context: " << err;

                // we count all those as BadSyntax - as the WS path would.
                // Although over HTTP these will yield a 400 status with a plain text response (for most).
                rpcEngine_->notifyBadSyntax();
                return impl::ErrorHelper(rawRequest, std::move(request)).makeError(err);
            }

            auto [result, timeDiff] = util::timed([&]() { return rpcEngine_->buildResponse(*context); });

            auto us = std::chrono::duration<int, std::milli>(timeDiff);
            rpc::logDuration(*context, us);

            boost::json::object response;

            if (auto const status = std::get_if<rpc::Status>(&result.response)) {
                // note: error statuses are counted/notified in buildResponse itself
                response = impl::ErrorHelper(rawRequest, request).composeError(*status);
                auto const responseStr = boost::json::serialize(response);

                LOG(perfLog_.debug()) << context->tag() << "Encountered error: " << responseStr;
                LOG(log_.debug()) << context->tag() << "Encountered error: " << responseStr;
            } else {
                // This can still technically be an error. Clio counts forwarded requests as successful.
                rpcEngine_->notifyComplete(context->method, us);

                auto& json = std::get<boost::json::object>(result.response);
                auto const isForwarded =
                    json.contains("forwarded") && json.at("forwarded").is_bool() && json.at("forwarded").as_bool();

                if (isForwarded)
                    json.erase("forwarded");

                // if the result is forwarded - just use it as is
                // if forwarded request has error, for http, error should be in "result"; for ws, error should
                // be at top
                if (isForwarded && (json.contains(JS(result)) || connectionMetadata.wasUpgraded())) {
                    for (auto const& [k, v] : json)
                        response.insert_or_assign(k, v);
                } else {
                    response[JS(result)] = json;
                }

                if (isForwarded)
                    response["forwarded"] = true;

                // for ws there is an additional field "status" in the response,
                // otherwise the "status" is in the "result" field
                if (connectionMetadata.wasUpgraded()) {
                    auto const appendFieldIfExist = [&](auto const& field) {
                        if (request.contains(field) and not request.at(field).is_null())
                            response[field] = request.at(field);
                    };

                    appendFieldIfExist(JS(id));
                    appendFieldIfExist(JS(api_version));

                    if (!response.contains(JS(error)))
                        response[JS(status)] = JS(success);

                    response[JS(type)] = JS(response);
                } else {
                    if (response.contains(JS(result)) && !response[JS(result)].as_object().contains(JS(error)))
                        response[JS(result)].as_object()[JS(status)] = JS(success);
                }
            }

            boost::json::array warnings = std::move(result.warnings);
            warnings.emplace_back(rpc::makeWarning(rpc::WarnRpcClio));

            if (etl_->lastCloseAgeSeconds() >= 60)
                warnings.emplace_back(rpc::makeWarning(rpc::WarnRpcOutdated));

            response["warnings"] = warnings;
            return Response{boost::beast::http::status::ok, response, rawRequest};
        } catch (std::exception const& ex) {
            // note: while we are catching this in buildResponse too, this is here to make sure
            // that any other code that may throw is outside of buildResponse is also worked around.
            LOG(perfLog_.error()) << connectionMetadata.tag() << "Caught exception: " << ex.what();
            LOG(log_.error()) << connectionMetadata.tag() << "Caught exception: " << ex.what();

            rpcEngine_->notifyInternalError();
            return impl::ErrorHelper(rawRequest, std::move(request)).makeInternalError();
        }
    }

    bool
    shouldReplaceParams(boost::json::object const& req) const
    {
        auto const hasParams = req.contains(JS(params));
        auto const paramsIsArray = hasParams and req.at(JS(params)).is_array();
        auto const paramsIsEmptyString =
            hasParams and req.at(JS(params)).is_string() and req.at(JS(params)).as_string().empty();
        auto const paramsIsEmptyObject =
            hasParams and req.at(JS(params)).is_object() and req.at(JS(params)).as_object().empty();
        auto const paramsIsNull = hasParams and req.at(JS(params)).is_null();
        auto const arrayIsEmpty = paramsIsArray and req.at(JS(params)).as_array().empty();
        auto const arrayIsNotEmpty = paramsIsArray and not req.at(JS(params)).as_array().empty();
        auto const firstArgIsNull = arrayIsNotEmpty and req.at(JS(params)).as_array().at(0).is_null();
        auto const firstArgIsEmptyString = arrayIsNotEmpty and req.at(JS(params)).as_array().at(0).is_string() and
            req.at(JS(params)).as_array().at(0).as_string().empty();

        // Note: all this compatibility dance is to match `rippled` as close as possible
        return not hasParams or paramsIsEmptyString or paramsIsNull or paramsIsEmptyObject or arrayIsEmpty or
            firstArgIsEmptyString or firstArgIsNull;
    }
};

}  // namespace web::ng
