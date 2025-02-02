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

#include "rpc/handlers/LedgerIndex.hpp"

#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"
#include "util/TimeUtils.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <xrpl/basics/chrono.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/jss.h>

#include <algorithm>
#include <cstdint>
#include <ranges>
#include <string>

namespace rpc {

LedgerIndexHandler::Result
LedgerIndexHandler::process(LedgerIndexHandler::Input input, Context const& ctx) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "LedgerIndex's ledger range must be available");

    auto const [minIndex, maxIndex] = *range;

    auto const fillOutputByIndex = [&](std::uint32_t index) {
        auto const ledger = sharedPtrBackend_->fetchLedgerBySequence(index, ctx.yield);
        return Output{
            .ledgerIndex = index,
            .ledgerHash = ripple::strHex(ledger->hash),
            .closeTimeIso = ripple::to_string_iso(ledger->closeTime)
        };
    };

    // if no date is provided, return the latest ledger
    if (!input.date)
        return fillOutputByIndex(maxIndex);

    auto const convertISOTimeStrToTicks = [](std::string const& isoTimeStr) {
        auto const systemTime = util::systemTpFromUtcStr(isoTimeStr, kDATE_FORMAT);
        // systemTime must be valid after validation passed
        return systemTime->time_since_epoch().count();
    };

    auto const ticks = convertISOTimeStrToTicks(*input.date);

    auto const earlierThan = [&](std::uint32_t ledgerIndex) {
        auto const header = sharedPtrBackend_->fetchLedgerBySequence(ledgerIndex, ctx.yield);
        auto const ledgerTime = util::systemTpFromLedgerCloseTime(header->closeTime);
        return ticks < ledgerTime.time_since_epoch().count();
    };

    // If the given date is earlier than the first valid ledger, return lgrNotFound
    if (earlierThan(minIndex))
        return Error{Status{RippledError::rpcLGR_NOT_FOUND, "ledgerNotInRange"}};

    auto const view = std::ranges::iota_view{minIndex, maxIndex + 1};

    auto const greaterEqLedgerIter = std::ranges::lower_bound(
        view, ticks, [&](std::uint32_t ledgerIndex, std::int64_t) { return not earlierThan(ledgerIndex); }
    );

    if (greaterEqLedgerIter != view.end())
        return fillOutputByIndex(std::max(static_cast<std::uint32_t>(*greaterEqLedgerIter) - 1, minIndex));

    return fillOutputByIndex(maxIndex);
}

LedgerIndexHandler::Input
tag_invoke(boost::json::value_to_tag<LedgerIndexHandler::Input>, boost::json::value const& jv)
{
    auto input = LedgerIndexHandler::Input{};

    if (jv.as_object().contains(JS(date)))
        input.date = jv.at(JS(date)).as_string();

    return input;
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, LedgerIndexHandler::Output const& output)
{
    jv = boost::json::object{
        {JS(ledger_index), output.ledgerIndex},
        {JS(ledger_hash), output.ledgerHash},
        {JS(closed), output.closeTimeIso},
        {JS(validated), true},
    };
}

}  // namespace rpc
