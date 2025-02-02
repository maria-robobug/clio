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

#include "web/dosguard/IntervalSweepHandler.hpp"

#include "util/newconfig/ConfigDefinition.hpp"
#include "web/dosguard/DOSGuardInterface.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/system/detail/error_code.hpp>

#include <algorithm>
#include <chrono>

namespace web::dosguard {

IntervalSweepHandler::IntervalSweepHandler(
    util::config::ClioConfigDefinition const& config,
    boost::asio::io_context& ctx,
    BaseDOSGuard& dosGuard
)
    : repeat_{ctx}
{
    auto const sweepInterval{std::max(
        std::chrono::milliseconds{1u},
        util::config::ClioConfigDefinition::toMilliseconds(config.get<double>("dos_guard.sweep_interval"))
    )};
    repeat_.start(sweepInterval, [&dosGuard] { dosGuard.clear(); });
}

}  // namespace web::dosguard
