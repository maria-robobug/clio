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

#include "feed/FeedTestUtil.hpp"
#include "feed/impl/LedgerFeed.hpp"
#include "util/TestObject.hpp"
#include "web/SubscriptionContextInterface.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/json/parse.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/protocol/Fees.h>

namespace {
constexpr auto kLEDGER_HASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
}  // namespace

using namespace feed::impl;
namespace json = boost::json;
using namespace testing;

using FeedLedgerTest = FeedBaseTest<LedgerFeed>;

TEST_F(FeedLedgerTest, SubPub)
{
    backend_->setRange(10, 30);
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(testing::Return(ledgerHeader));

    auto const feeBlob = createLegacyFeeSettingBlob(1, 2, 3, 4, 0);
    EXPECT_CALL(*backend_, doFetchLedgerObject).WillOnce(testing::Return(feeBlob));
    // check the function response
    // Information about the ledgers on hand and current fee schedule. This
    // includes the same fields as a ledger stream message, except that it omits
    // the type and txn_count fields
    static constexpr auto kLEDGER_RESPONSE =
        R"({
            "validated_ledgers":"10-30",
            "ledger_index":30,
            "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_time":0,
            "fee_base":1,
            "reserve_base":3,
            "reserve_inc":2
        })";
    boost::asio::io_context ioContext;
    boost::asio::spawn(ioContext, [this](boost::asio::yield_context yield) {
        EXPECT_CALL(*mockSessionPtr, onDisconnect);
        auto res = testFeedPtr->sub(yield, backend_, sessionPtr);
        // check the response
        EXPECT_EQ(res, json::parse(kLEDGER_RESPONSE));
    });
    ioContext.run();
    EXPECT_EQ(testFeedPtr->count(), 1);

    static constexpr auto kLEDGER_PUB =
        R"({
            "type":"ledgerClosed",
            "ledger_index":31,
            "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_time":0,
            "fee_base":0,
            "reserve_base":10,
            "reserve_inc":0,
            "validated_ledgers":"10-31",
            "txn_count":8
        })";

    // test publish
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kLEDGER_PUB))).Times(1);
    auto const ledgerHeader2 = createLedgerHeader(kLEDGER_HASH, 31);
    auto fee2 = ripple::Fees();
    fee2.reserve = 10;
    testFeedPtr->pub(ledgerHeader2, fee2, "10-31", 8);

    // test unsub, after unsub the send should not be called
    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->count(), 0);
    EXPECT_CALL(*mockSessionPtr, send(_)).Times(0);
    testFeedPtr->pub(ledgerHeader2, fee2, "10-31", 8);
}

TEST_F(FeedLedgerTest, AutoDisconnect)
{
    backend_->setRange(10, 30);
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(testing::Return(ledgerHeader));

    auto const feeBlob = createLegacyFeeSettingBlob(1, 2, 3, 4, 0);
    EXPECT_CALL(*backend_, doFetchLedgerObject).WillOnce(testing::Return(feeBlob));
    static constexpr auto kLEDGER_RESPONSE =
        R"({
            "validated_ledgers":"10-30",
            "ledger_index":30,
            "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_time":0,
            "fee_base":1,
            "reserve_base":3,
            "reserve_inc":2
        })";

    web::SubscriptionContextInterface::OnDisconnectSlot slot;
    EXPECT_CALL(*mockSessionPtr, onDisconnect).WillOnce(testing::SaveArg<0>(&slot));

    boost::asio::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto res = testFeedPtr->sub(yield, backend_, sessionPtr);
        // check the response
        EXPECT_EQ(res, json::parse(kLEDGER_RESPONSE));
    });
    EXPECT_EQ(testFeedPtr->count(), 1);
    EXPECT_CALL(*mockSessionPtr, send(_)).Times(0);

    ASSERT_TRUE(slot);
    slot(sessionPtr.get());
    sessionPtr.reset();

    EXPECT_EQ(testFeedPtr->count(), 0);

    auto const ledgerHeader2 = createLedgerHeader(kLEDGER_HASH, 31);
    auto fee2 = ripple::Fees();
    fee2.reserve = 10;
    // no error
    testFeedPtr->pub(ledgerHeader2, fee2, "10-31", 8);
}
