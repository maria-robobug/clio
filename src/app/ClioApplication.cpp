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

#include "app/ClioApplication.hpp"

#include "data/AmendmentCenter.hpp"
#include "data/BackendFactory.hpp"
#include "etl/ETLService.hpp"
#include "etl/LoadBalancer.hpp"
#include "etl/NetworkValidatedLedgers.hpp"
#include "feed/SubscriptionManager.hpp"
#include "rpc/Counters.hpp"
#include "rpc/Errors.hpp"
#include "rpc/RPCEngine.hpp"
#include "rpc/WorkQueue.hpp"
#include "rpc/common/impl/HandlerProvider.hpp"
#include "util/Assert.hpp"
#include "util/build/Build.hpp"
#include "util/config/Config.hpp"
#include "util/log/Logger.hpp"
#include "util/prometheus/Http.hpp"
#include "util/prometheus/Prometheus.hpp"
#include "web/AdminVerificationStrategy.hpp"
#include "web/RPCServerHandler.hpp"
#include "web/Server.hpp"
#include "web/SubscriptionContextInterface.hpp"
#include "web/dosguard/DOSGuard.hpp"
#include "web/dosguard/IntervalSweepHandler.hpp"
#include "web/dosguard/WhitelistHandler.hpp"
#include "web/ng/Connection.hpp"
#include "web/ng/RPCServerHandler.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/Response.hpp"
#include "web/ng/Server.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/http/status.hpp>

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

namespace app {

namespace {

/**
 * @brief Start context threads
 *
 * @param ioc Context
 * @param numThreads Number of worker threads to start
 */
void
start(boost::asio::io_context& ioc, std::uint32_t numThreads)
{
    std::vector<std::thread> v;
    v.reserve(numThreads - 1);
    for (auto i = numThreads - 1; i > 0; --i)
        v.emplace_back([&ioc] { ioc.run(); });

    ioc.run();
    for (auto& t : v)
        t.join();
}

}  // namespace

ClioApplication::ClioApplication(util::Config const& config) : config_(config), signalsHandler_{config_}
{
    LOG(util::LogService::info()) << "Clio version: " << util::build::getClioFullVersionString();
    PrometheusService::init(config);
}

int
ClioApplication::run(bool const useNgWebServer)
{
    auto const threads = config_.valueOr("io_threads", 2);
    if (threads <= 0) {
        LOG(util::LogService::fatal()) << "io_threads is less than 1";
        return EXIT_FAILURE;
    }
    LOG(util::LogService::info()) << "Number of io threads = " << threads;

    // IO context to handle all incoming requests, as well as other things.
    // This is not the only io context in the application.
    boost::asio::io_context ioc{threads};

    // Rate limiter, to prevent abuse
    auto whitelistHandler = web::dosguard::WhitelistHandler{config_};
    auto dosGuard = web::dosguard::DOSGuard{config_, whitelistHandler};
    auto sweepHandler = web::dosguard::IntervalSweepHandler{config_, ioc, dosGuard};

    // Interface to the database
    auto backend = data::make_Backend(config_);

    // Manages clients subscribed to streams
    auto subscriptions = feed::SubscriptionManager::make_SubscriptionManager(config_, backend);

    // Tracks which ledgers have been validated by the network
    auto ledgers = etl::NetworkValidatedLedgers::make_ValidatedLedgers();

    // Handles the connection to one or more rippled nodes.
    // ETL uses the balancer to extract data.
    // The server uses the balancer to forward RPCs to a rippled node.
    // The balancer itself publishes to streams (transactions_proposed and accounts_proposed)
    auto balancer = etl::LoadBalancer::make_LoadBalancer(config_, ioc, backend, subscriptions, ledgers);

    // ETL is responsible for writing and publishing to streams. In read-only mode, ETL only publishes
    auto etl = etl::ETLService::make_ETLService(config_, ioc, backend, subscriptions, balancer, ledgers);

    auto workQueue = rpc::WorkQueue::make_WorkQueue(config_);
    auto counters = rpc::Counters::make_Counters(workQueue);
    auto const amendmentCenter = std::make_shared<data::AmendmentCenter const>(backend);
    auto const handlerProvider = std::make_shared<rpc::impl::ProductionHandlerProvider const>(
        config_, backend, subscriptions, balancer, etl, amendmentCenter, counters
    );

    using RPCEngineType = rpc::RPCEngine<etl::LoadBalancer, rpc::Counters>;
    auto const rpcEngine =
        RPCEngineType::make_RPCEngine(config_, backend, balancer, dosGuard, workQueue, counters, handlerProvider);

    if (useNgWebServer or config_.valueOr("server.__ng_web_server", false)) {
        web::ng::RPCServerHandler<RPCEngineType, etl::ETLService> handler{config_, backend, rpcEngine, etl};

        auto expectedAdminVerifier = web::make_AdminVerificationStrategy(config_);
        if (not expectedAdminVerifier.has_value()) {
            LOG(util::LogService::error()) << "Error creating admin verifier: " << expectedAdminVerifier.error();
            return EXIT_FAILURE;
        }
        auto const adminVerifier = std::move(expectedAdminVerifier).value();

        auto httpServer = web::ng::make_Server(config_, ioc);

        if (not httpServer.has_value()) {
            LOG(util::LogService::error()) << "Error creating web server: " << httpServer.error();
            return EXIT_FAILURE;
        }

        httpServer->onGet(
            "/metrics",
            [adminVerifier](
                web::ng::Request const& request,
                web::ng::ConnectionMetadata& connectionMetadata,
                web::SubscriptionContextPtr,
                boost::asio::yield_context
            ) -> web::ng::Response {
                auto const maybeHttpRequest = request.asHttpRequest();
                ASSERT(maybeHttpRequest.has_value(), "Got not a http request in Get");
                auto const& httpRequest = maybeHttpRequest->get();

                // FIXME(#1702): Using veb server thread to handle prometheus request. Better to post on work queue.
                auto maybeResponse = util::prometheus::handlePrometheusRequest(
                    httpRequest, adminVerifier->isAdmin(httpRequest, connectionMetadata.ip())
                );
                ASSERT(maybeResponse.has_value(), "Got unexpected request for Prometheus");
                return web::ng::Response{std::move(maybeResponse).value(), request};
            }
        );

        util::Logger webServerLog{"WebServer"};
        auto onRequest = [adminVerifier, &webServerLog, &handler](
                             web::ng::Request const& request,
                             web::ng::ConnectionMetadata& connectionMetadata,
                             web::SubscriptionContextPtr subscriptionContext,
                             boost::asio::yield_context yield
                         ) -> web::ng::Response {
            LOG(webServerLog.info()) << connectionMetadata.tag()
                                     << "Received request from ip = " << connectionMetadata.ip()
                                     << " - posting to WorkQueue";

            connectionMetadata.setIsAdmin([&adminVerifier, &request, &connectionMetadata]() {
                return adminVerifier->isAdmin(request.httpHeaders(), connectionMetadata.ip());
            });

            try {
                return handler(request, connectionMetadata, std::move(subscriptionContext), yield);
            } catch (std::exception const&) {
                return web::ng::Response{
                    boost::beast::http::status::internal_server_error,
                    rpc::makeError(rpc::RippledError::rpcINTERNAL),
                    request
                };
            }
        };

        httpServer->onPost("/", onRequest);
        httpServer->onWs(onRequest);

        auto const maybeError = httpServer->run();
        if (maybeError.has_value()) {
            LOG(util::LogService::error()) << "Error starting web server: " << *maybeError;
            return EXIT_FAILURE;
        }

        // Blocks until stopped.
        // When stopped, shared_ptrs fall out of scope
        // Calls destructors on all resources, and destructs in order
        start(ioc, threads);

        return EXIT_SUCCESS;
    }

    // Init the web server
    auto handler =
        std::make_shared<web::RPCServerHandler<RPCEngineType, etl::ETLService>>(config_, backend, rpcEngine, etl);

    auto const httpServer = web::make_HttpServer(config_, ioc, dosGuard, handler);

    // Blocks until stopped.
    // When stopped, shared_ptrs fall out of scope
    // Calls destructors on all resources, and destructs in order
    start(ioc, threads);

    return EXIT_SUCCESS;
}

}  // namespace app
