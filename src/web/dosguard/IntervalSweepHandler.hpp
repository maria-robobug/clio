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

#pragma once

#include "util/Repeat.hpp"
#include "util/newconfig/ConfigDefinition.hpp"

#include <boost/asio/io_context.hpp>

namespace web::dosguard {

class BaseDOSGuard;

/**
 * @brief Sweep handler clearing context every sweep interval from config.
 */
class IntervalSweepHandler {
    util::Repeat repeat_;

public:
    /**
     * @brief Construct a new interval-based sweep handler.
     *
     * @param config Clio config to use
     * @param ctx The boost::asio::io_context to use
     * @param dosGuard The DOS guard to use
     */
    IntervalSweepHandler(
        util::config::ClioConfigDefinition const& config,
        boost::asio::io_context& ctx,
        web::dosguard::BaseDOSGuard& dosGuard
    );
};

}  // namespace web::dosguard
