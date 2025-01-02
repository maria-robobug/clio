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

#include "util/SignalsHandler.hpp"

#include "util/Assert.hpp"
#include "util/log/Logger.hpp"
#include "util/newconfig/ConfigDefinition.hpp"

#include <chrono>
#include <csignal>
#include <functional>
#include <optional>
#include <utility>

namespace util {

namespace impl {

class SignalsHandlerStatic {
    static SignalsHandler* installedHandler;

public:
    static void
    registerHandler(SignalsHandler& handler)
    {
        ASSERT(installedHandler == nullptr, "There could be only one instance of SignalsHandler");
        installedHandler = &handler;
    }

    static void
    resetHandler()
    {
        installedHandler = nullptr;
    }

    static void
    handleSignal(int signal)
    {
        ASSERT(installedHandler != nullptr, "SignalsHandler is not initialized");
        installedHandler->stopHandler_(signal);
    }

    static void
    handleSecondSignal(int signal)
    {
        ASSERT(installedHandler != nullptr, "SignalsHandler is not initialized");
        installedHandler->secondSignalHandler_(signal);
    }
};

SignalsHandler* SignalsHandlerStatic::installedHandler = nullptr;

}  // namespace impl

SignalsHandler::SignalsHandler(config::ClioConfigDefinition const& config, std::function<void()> forceExitHandler)
    : gracefulPeriod_(0)
    , context_(1)
    , stopHandler_([this, forceExitHandler](int) mutable {
        LOG(LogService::info()) << "Got stop signal. Stopping Clio. Graceful period is "
                                << std::chrono::duration_cast<std::chrono::milliseconds>(gracefulPeriod_).count()
                                << " milliseconds.";
        setHandler(impl::SignalsHandlerStatic::handleSecondSignal);
        timer_.emplace(context_.scheduleAfter(
            gracefulPeriod_,
            [forceExitHandler = std::move(forceExitHandler)](auto&& stopToken, bool canceled) {
                // TODO: Update this after https://github.com/XRPLF/clio/issues/1380
                if (not stopToken.isStopRequested() and not canceled) {
                    LOG(LogService::warn()) << "Force exit at the end of graceful period.";
                    forceExitHandler();
                }
            }
        ));
        stopSignal_();
    })
    , secondSignalHandler_([this, forceExitHandler = std::move(forceExitHandler)](int) {
        LOG(LogService::warn()) << "Force exit on second signal.";
        forceExitHandler();
        cancelTimer();
        setHandler();
    })
{
    impl::SignalsHandlerStatic::registerHandler(*this);

    gracefulPeriod_ = util::config::ClioConfigDefinition::toMilliseconds(config.get<float>("graceful_period"));
    setHandler(impl::SignalsHandlerStatic::handleSignal);
}

SignalsHandler::~SignalsHandler()
{
    cancelTimer();
    setHandler();
    impl::SignalsHandlerStatic::resetHandler();  // This is needed mostly for tests to reset static state
}

void
SignalsHandler::cancelTimer()
{
    if (timer_.has_value())
        timer_->abort();
}

void
SignalsHandler::setHandler(void (*handler)(int))
{
    for (int const signal : kHANDLED_SIGNALS) {
        std::signal(signal, handler == nullptr ? SIG_DFL : handler);
    }
}

}  // namespace util
