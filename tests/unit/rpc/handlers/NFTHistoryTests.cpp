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

#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/NFTHistory.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"

#include <boost/json/parse.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/STObject.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using namespace rpc;
namespace json = boost::json;
using namespace testing;

namespace {

constexpr auto kMIN_SEQ = 10;
constexpr auto kMAX_SEQ = 30;
constexpr auto kACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kLEDGER_HASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kNFT_ID = "00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004";

}  // namespace

struct RPCNFTHistoryHandlerTest : HandlerBaseTest {
    RPCNFTHistoryHandlerTest()
    {
        backend_->setRange(kMIN_SEQ, kMAX_SEQ);
    }
};

struct NFTHistoryParamTestCaseBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct NFTHistoryParameterTest : public RPCNFTHistoryHandlerTest,
                                 public WithParamInterface<NFTHistoryParamTestCaseBundle> {};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<NFTHistoryParamTestCaseBundle>{
        NFTHistoryParamTestCaseBundle{
            .testName = "MissingNFTID",
            .testJson = R"({})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'nft_id' missing"
        },
        NFTHistoryParamTestCaseBundle{
            .testName = "BinaryNotBool",
            .testJson = R"({"nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004", "binary": 1})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        NFTHistoryParamTestCaseBundle{
            .testName = "ForwardNotBool",
            .testJson =
                R"({"nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004", "forward": 1})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        NFTHistoryParamTestCaseBundle{
            .testName = "ledger_index_minNotInt",
            .testJson =
                R"({"nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004", "ledger_index_min": "x"})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        NFTHistoryParamTestCaseBundle{
            .testName = "ledger_index_maxNotInt",
            .testJson =
                R"({"nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004", "ledger_index_max": "x"})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        NFTHistoryParamTestCaseBundle{
            .testName = "ledger_indexInvalid",
            .testJson =
                R"({"nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004", "ledger_index": "x"})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledgerIndexMalformed"
        },
        NFTHistoryParamTestCaseBundle{
            .testName = "ledger_hashInvalid",
            .testJson =
                R"({"nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004", "ledger_hash": "x"})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledger_hashMalformed"
        },
        NFTHistoryParamTestCaseBundle{
            .testName = "ledger_hashNotString",
            .testJson =
                R"({"nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004", "ledger_hash": 123})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledger_hashNotString"
        },
        NFTHistoryParamTestCaseBundle{
            .testName = "limitNotInt",
            .testJson =
                R"({"nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004", "limit": "123"})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        NFTHistoryParamTestCaseBundle{
            .testName = "limitNagetive",
            .testJson = R"({"nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004", "limit": -1})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        NFTHistoryParamTestCaseBundle{
            .testName = "limitZero",
            .testJson = R"({"nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004", "limit": 0})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        NFTHistoryParamTestCaseBundle{
            .testName = "MarkerNotObject",
            .testJson =
                R"({"nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004", "marker": 101})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "invalidMarker"
        },
        NFTHistoryParamTestCaseBundle{
            .testName = "MarkerMissingSeq",
            .testJson = R"({
                "nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004",
                "marker": {"ledger": 123}
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'seq' missing"
        },
        NFTHistoryParamTestCaseBundle{
            .testName = "MarkerMissingLedger",
            .testJson = R"({
                "nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004",
                "marker":{"seq": 123}
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'ledger' missing"
        },
        NFTHistoryParamTestCaseBundle{
            .testName = "MarkerLedgerNotInt",
            .testJson = R"({
                "nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004",
                "marker": 
                {
                    "seq": "string",
                    "ledger": 1
                }
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        NFTHistoryParamTestCaseBundle{
            .testName = "MarkerSeqNotInt",
            .testJson = R"({
                "nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004",
                "marker": 
                {
                    "ledger": "string",
                    "seq": 1
                }
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        NFTHistoryParamTestCaseBundle{
            .testName = "LedgerIndexMinLessThanMinSeq",
            .testJson = R"({
                "nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004",
                "ledger_index_min": 9
            })",
            .expectedError = "lgrIdxMalformed",
            .expectedErrorMessage = "ledgerSeqMinOutOfRange"
        },
        NFTHistoryParamTestCaseBundle{
            .testName = "LedgerIndexMaxLargeThanMaxSeq",
            .testJson = R"({
                "nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004",
                "ledger_index_max": 31
            })",
            .expectedError = "lgrIdxMalformed",
            .expectedErrorMessage = "ledgerSeqMaxOutOfRange"
        },
        NFTHistoryParamTestCaseBundle{
            .testName = "LedgerIndexMaxLessThanLedgerIndexMin",
            .testJson = R"({
                "nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004",
                "ledger_index_max": 11,
                "ledger_index_min": 20
            })",
            .expectedError = "lgrIdxsInvalid",
            .expectedErrorMessage = "Ledger indexes invalid."
        },
        NFTHistoryParamTestCaseBundle{
            .testName = "LedgerIndexMaxMinAndLedgerIndex",
            .testJson = R"({
                "nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004", 
                "ledger_index_max": 20,
                "ledger_index_min": 11,
                "ledger_index": 10
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "containsLedgerSpecifierAndRange"
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCNFTHistoryGroup1,
    NFTHistoryParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::kNAME_GENERATOR
);

TEST_P(NFTHistoryParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NFTHistoryHandler{backend_}};
        auto const req = json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

static std::vector<TransactionAndMetadata>
genTransactions(uint32_t seq1, uint32_t seq2)
{
    auto transactions = std::vector<TransactionAndMetadata>{};
    auto trans1 = TransactionAndMetadata();
    ripple::STObject const obj = createPaymentTransactionObject(kACCOUNT, kACCOUNT2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = seq1;
    ripple::STObject const metaObj = createPaymentTransactionMetaObject(kACCOUNT, kACCOUNT2, 22, 23);
    trans1.metadata = metaObj.getSerializer().peekData();
    trans1.date = 1;
    transactions.push_back(trans1);

    auto trans2 = TransactionAndMetadata();
    ripple::STObject const obj2 = createPaymentTransactionObject(kACCOUNT, kACCOUNT2, 1, 1, 32);
    trans2.transaction = obj.getSerializer().peekData();
    trans2.ledgerSequence = seq2;
    ripple::STObject const metaObj2 = createPaymentTransactionMetaObject(kACCOUNT, kACCOUNT2, 22, 23);
    trans2.metadata = metaObj2.getSerializer().peekData();
    trans2.date = 2;
    transactions.push_back(trans2);
    return transactions;
}

TEST_F(RPCNFTHistoryHandlerTest, IndexSpecificForwardTrue)
{
    auto const transactions = genTransactions(kMIN_SEQ + 1, kMAX_SEQ - 1);
    auto const transCursor = TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchNFTTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchNFTTransactions(
            testing::_,
            testing::_,
            true,
            testing::Optional(testing::Eq(TransactionsCursor{kMIN_SEQ + 1, 0})),
            testing::_
        )
    )
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NFTHistoryHandler{backend_}};
        auto static const kINPUT = json::parse(fmt::format(
            R"({{
                "nft_id":"{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "forward": true
            }})",
            kNFT_ID,
            kMIN_SEQ + 1,
            kMAX_SEQ - 1
        ));
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("nft_id").as_string(), kNFT_ID);
        EXPECT_EQ(output.result->at("ledger_index_min").as_uint64(), kMIN_SEQ + 1);
        EXPECT_EQ(output.result->at("ledger_index_max").as_uint64(), kMAX_SEQ - 1);
        EXPECT_EQ(output.result->at("marker").as_object(), json::parse(R"({"ledger":12,"seq":34})"));
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 2);
        EXPECT_FALSE(output.result->as_object().contains("limit"));
    });
}

TEST_F(RPCNFTHistoryHandlerTest, IndexSpecificForwardFalseV1)
{
    constexpr auto kOUTPUT = R"({
                                "nft_id": "00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004",
                                "ledger_index_min": 11,
                                "ledger_index_max": 29,
                                "transactions":
                                [
                                    {
                                        "meta":
                                        {
                                            "AffectedNodes":
                                            [
                                                {
                                                    "ModifiedNode":{
                                                        "FinalFields":{
                                                            "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                                            "Balance": "22"
                                                        },
                                                        "LedgerEntryType": "AccountRoot"
                                                    }
                                                },
                                                {
                                                    "ModifiedNode":{
                                                        "FinalFields":{
                                                            "Account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                                                            "Balance": "23"
                                                        },
                                                        "LedgerEntryType": "AccountRoot"
                                                    }
                                                }
                                            ],
                                            "TransactionIndex": 0,
                                            "TransactionResult": "tesSUCCESS",
                                            "delivered_amount": "unavailable"
                                        },
                                        "tx":
                                        {
                                            "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                            "Amount": "1",
                                            "Destination": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                                            "Fee": "1",
                                            "Sequence": 32,
                                            "SigningPubKey": "74657374",
                                            "TransactionType": "Payment",
                                            "hash": "51D2AAA6B8E4E16EF22F6424854283D8391B56875858A711B8CE4D5B9A422CC2",
                                            "DeliverMax": "1",
                                            "ledger_index": 11,
                                            "date": 1
                                        },
                                        "validated": true
                                    },
                                    {
                                        "meta":
                                        {
                                            "AffectedNodes":
                                            [
                                                {
                                                    "ModifiedNode":{
                                                        "FinalFields":{
                                                            "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                                            "Balance": "22"
                                                        },
                                                        "LedgerEntryType": "AccountRoot"
                                                    }
                                                },
                                                {
                                                    "ModifiedNode":{
                                                        "FinalFields":{
                                                            "Account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                                                            "Balance": "23"
                                                        },
                                                        "LedgerEntryType": "AccountRoot"
                                                    }
                                                }
                                            ],
                                            "TransactionIndex": 0,
                                            "TransactionResult": "tesSUCCESS",
                                            "delivered_amount": "unavailable"
                                        },
                                        "tx":
                                        {
                                            "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                            "Amount": "1",
                                            "Destination": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                                            "Fee": "1",
                                            "Sequence": 32,
                                            "SigningPubKey": "74657374",
                                            "TransactionType": "Payment",
                                            "hash": "51D2AAA6B8E4E16EF22F6424854283D8391B56875858A711B8CE4D5B9A422CC2",
                                            "DeliverMax": "1",
                                            "ledger_index": 29,
                                            "date": 2
                                        },
                                        "validated": true
                                    }
                                ],
                                "validated": true,
                                "marker":
                                {
                                    "ledger": 12,
                                    "seq": 34
                                }
                                })";

    auto const transactions = genTransactions(kMIN_SEQ + 1, kMAX_SEQ - 1);
    auto const transCursor = TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchNFTTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchNFTTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{kMAX_SEQ - 1, INT32_MAX})),
            testing::_
        )
    )
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NFTHistoryHandler{backend_}};
        auto static const kINPUT = json::parse(fmt::format(
            R"({{
                "nft_id":"{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "forward": false
            }})",
            kNFT_ID,
            kMIN_SEQ + 1,
            kMAX_SEQ - 1
        ));
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), boost::json::parse(kOUTPUT));
    });
}

TEST_F(RPCNFTHistoryHandlerTest, IndexSpecificForwardFalseV2)
{
    constexpr auto kOUTPUT = R"({
                                "nft_id": "00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004",
                                "ledger_index_min": 11,
                                "ledger_index_max": 29,
                                "transactions":
                                [
                                    {
                                        "meta":
                                        {
                                            "AffectedNodes":
                                            [
                                                {
                                                    "ModifiedNode":
                                                    {
                                                        "FinalFields":
                                                        {
                                                            "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                                            "Balance": "22"
                                                        },
                                                        "LedgerEntryType": "AccountRoot"
                                                    }
                                                },
                                                {
                                                    "ModifiedNode":
                                                    {
                                                        "FinalFields":
                                                        {
                                                            "Account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                                                            "Balance": "23"
                                                        },
                                                        "LedgerEntryType": "AccountRoot"
                                                    }
                                                }
                                            ],
                                            "TransactionIndex": 0,
                                            "TransactionResult": "tesSUCCESS",
                                            "delivered_amount": "unavailable"
                                        },
                                        "tx_json":
                                        {
                                            "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                            "Destination": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                                            "Fee": "1",
                                            "Sequence": 32,
                                            "SigningPubKey": "74657374",
                                            "TransactionType": "Payment",
                                            "DeliverMax": "1",
                                            "ledger_index": 11,
                                            "date": 1
                                        },
                                        "hash": "51D2AAA6B8E4E16EF22F6424854283D8391B56875858A711B8CE4D5B9A422CC2",
                                        "ledger_index": 11,
                                        "close_time_iso": "2000-01-01T00:00:00Z",
                                        "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                                        "validated": true
                                    },
                                    {
                                        "meta":
                                        {
                                            "AffectedNodes":
                                            [
                                                {
                                                    "ModifiedNode":
                                                    {
                                                        "FinalFields":
                                                        {
                                                            "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                                            "Balance": "22"
                                                        },
                                                        "LedgerEntryType": "AccountRoot"
                                                    }
                                                },
                                                {
                                                    "ModifiedNode":
                                                    {
                                                        "FinalFields":
                                                        {
                                                            "Account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                                                            "Balance": "23"
                                                        },
                                                        "LedgerEntryType": "AccountRoot"
                                                    }
                                                }
                                            ],
                                            "TransactionIndex": 0,
                                            "TransactionResult": "tesSUCCESS",
                                            "delivered_amount": "unavailable"
                                        },
                                        "tx_json":
                                        {
                                            "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                            "Destination": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                                            "Fee": "1",
                                            "Sequence": 32,
                                            "SigningPubKey": "74657374",
                                            "TransactionType": "Payment",
                                            "DeliverMax": "1",
                                            "ledger_index": 29,
                                            "date": 2
                                        },
                                        "hash": "51D2AAA6B8E4E16EF22F6424854283D8391B56875858A711B8CE4D5B9A422CC2",
                                        "ledger_index": 29,
                                        "close_time_iso": "2000-01-01T00:00:00Z",
                                        "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                                        "validated": true
                                    }
                                ],
                                "validated": true,
                                "marker":
                                {
                                    "ledger": 12,
                                    "seq": 34
                                }
                                })";

    auto const transactions = genTransactions(kMIN_SEQ + 1, kMAX_SEQ - 1);
    auto const transCursor = TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    EXPECT_CALL(
        *backend_,
        fetchNFTTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{kMAX_SEQ - 1, INT32_MAX})),
            testing::_
        )
    )
        .WillOnce(Return(transCursor));

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(2);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NFTHistoryHandler{backend_}};
        auto static const kINPUT = json::parse(fmt::format(
            R"({{
                "nft_id":"{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "forward": false
            }})",
            kNFT_ID,
            kMIN_SEQ + 1,
            kMAX_SEQ - 1
        ));
        auto const output = handler.process(kINPUT, Context{.yield = yield, .apiVersion = 2u});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), boost::json::parse(kOUTPUT));
    });
}

TEST_F(RPCNFTHistoryHandlerTest, IndexNotSpecificForwardTrue)
{
    auto const transactions = genTransactions(kMIN_SEQ + 1, kMAX_SEQ - 1);
    auto const transCursor = TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchNFTTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchNFTTransactions(
            testing::_, testing::_, true, testing::Optional(testing::Eq(TransactionsCursor{kMIN_SEQ, 0})), testing::_
        )
    )
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NFTHistoryHandler{backend_}};
        auto static const kINPUT = json::parse(fmt::format(
            R"({{
                "nft_id":"{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "forward": true
            }})",
            kNFT_ID,
            -1,
            -1
        ));
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("nft_id").as_string(), kNFT_ID);
        EXPECT_EQ(output.result->at("ledger_index_min").as_uint64(), kMIN_SEQ);
        EXPECT_EQ(output.result->at("ledger_index_max").as_uint64(), kMAX_SEQ);
        EXPECT_EQ(output.result->at("marker").as_object(), json::parse(R"({"ledger":12,"seq":34})"));
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 2);
        EXPECT_FALSE(output.result->as_object().contains("limit"));
    });
}

TEST_F(RPCNFTHistoryHandlerTest, IndexNotSpecificForwardFalse)
{
    auto const transactions = genTransactions(kMIN_SEQ + 1, kMAX_SEQ - 1);
    auto const transCursor = TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchNFTTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchNFTTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{kMAX_SEQ, INT32_MAX})),
            testing::_
        )
    )
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NFTHistoryHandler{backend_}};
        auto static const kINPUT = json::parse(fmt::format(
            R"({{
                "nft_id":"{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "forward": false
            }})",
            kNFT_ID,
            -1,
            -1
        ));
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("nft_id").as_string(), kNFT_ID);
        EXPECT_EQ(output.result->at("ledger_index_min").as_uint64(), kMIN_SEQ);
        EXPECT_EQ(output.result->at("ledger_index_max").as_uint64(), kMAX_SEQ);
        EXPECT_EQ(output.result->at("marker").as_object(), json::parse(R"({"ledger":12,"seq":34})"));
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 2);
        EXPECT_FALSE(output.result->as_object().contains("limit"));
    });
}

TEST_F(RPCNFTHistoryHandlerTest, BinaryTrueV1)
{
    auto const transactions = genTransactions(kMIN_SEQ + 1, kMAX_SEQ - 1);
    auto const transCursor = TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchNFTTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchNFTTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{kMAX_SEQ, INT32_MAX})),
            testing::_
        )
    )
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NFTHistoryHandler{backend_}};
        auto static const kINPUT = json::parse(fmt::format(
            R"({{
                "nft_id":"{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "binary": true
            }})",
            kNFT_ID,
            -1,
            -1
        ));
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("nft_id").as_string(), kNFT_ID);
        EXPECT_EQ(output.result->at("ledger_index_min").as_uint64(), kMIN_SEQ);
        EXPECT_EQ(output.result->at("ledger_index_max").as_uint64(), kMAX_SEQ);
        EXPECT_EQ(output.result->at("marker").as_object(), json::parse(R"({"ledger":12,"seq":34})"));
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 2);
        EXPECT_EQ(
            output.result->at("transactions").as_array()[0].as_object().at("meta").as_string(),
            "201C00000000F8E5110061E762400000000000001681144B4E9C06F24296074F7B"
            "C48F92A97916C6DC5EA9E1E1E5110061E76240000000000000178114D31252CF90"
            "2EF8DD8451243869B38667CBD89DF3E1E1F1031000"
        );
        EXPECT_EQ(
            output.result->at("transactions").as_array()[0].as_object().at("tx_blob").as_string(),
            "120000240000002061400000000000000168400000000000000173047465737481"
            "144B4E9C06F24296074F7BC48F92A97916C6DC5EA98314D31252CF902EF8DD8451"
            "243869B38667CBD89DF3"
        );
        EXPECT_EQ(output.result->at("transactions").as_array()[0].as_object().at("date").as_uint64(), 1);

        EXPECT_FALSE(output.result->as_object().contains("limit"));
    });
}

TEST_F(RPCNFTHistoryHandlerTest, BinaryTrueV2)
{
    auto const transactions = genTransactions(kMIN_SEQ + 1, kMAX_SEQ - 1);
    auto const transCursor = TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    EXPECT_CALL(
        *backend_,
        fetchNFTTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{kMAX_SEQ, INT32_MAX})),
            testing::_
        )
    )
        .WillOnce(Return(transCursor));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NFTHistoryHandler{backend_}};
        auto static const kINPUT = json::parse(fmt::format(
            R"({{
                "nft_id":"{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "binary": true
            }})",
            kNFT_ID,
            -1,
            -1
        ));
        auto const output = handler.process(kINPUT, Context{.yield = yield, .apiVersion = 2u});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("nft_id").as_string(), kNFT_ID);
        EXPECT_EQ(output.result->at("ledger_index_min").as_uint64(), kMIN_SEQ);
        EXPECT_EQ(output.result->at("ledger_index_max").as_uint64(), kMAX_SEQ);
        EXPECT_EQ(output.result->at("marker").as_object(), json::parse(R"({"ledger":12,"seq":34})"));
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 2);
        EXPECT_EQ(
            output.result->at("transactions").as_array()[0].as_object().at("meta_blob").as_string(),
            "201C00000000F8E5110061E762400000000000001681144B4E9C06F24296074F7B"
            "C48F92A97916C6DC5EA9E1E1E5110061E76240000000000000178114D31252CF90"
            "2EF8DD8451243869B38667CBD89DF3E1E1F1031000"
        );
        EXPECT_EQ(
            output.result->at("transactions").as_array()[0].as_object().at("tx_blob").as_string(),
            "120000240000002061400000000000000168400000000000000173047465737481"
            "144B4E9C06F24296074F7BC48F92A97916C6DC5EA98314D31252CF902EF8DD8451"
            "243869B38667CBD89DF3"
        );
        EXPECT_EQ(output.result->at("transactions").as_array()[0].as_object().at("date").as_uint64(), 1);

        EXPECT_FALSE(output.result->as_object().contains("limit"));
    });
}

TEST_F(RPCNFTHistoryHandlerTest, LimitAndMarker)
{
    auto const transactions = genTransactions(kMIN_SEQ + 1, kMAX_SEQ - 1);
    auto const transCursor = TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchNFTTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchNFTTransactions(
            testing::_, testing::_, false, testing::Optional(testing::Eq(TransactionsCursor{10, 11})), testing::_
        )
    )
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NFTHistoryHandler{backend_}};
        auto static const kINPUT = json::parse(fmt::format(
            R"({{
                "nft_id":"{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "limit": 2,
                "forward": false,
                "marker": {{"ledger":10,"seq":11}}
            }})",
            kNFT_ID,
            -1,
            -1
        ));
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("nft_id").as_string(), kNFT_ID);
        EXPECT_EQ(output.result->at("ledger_index_min").as_uint64(), kMIN_SEQ);
        EXPECT_EQ(output.result->at("ledger_index_max").as_uint64(), kMAX_SEQ);
        EXPECT_EQ(output.result->at("limit").as_uint64(), 2);
        EXPECT_EQ(output.result->at("marker").as_object(), json::parse(R"({"ledger":12,"seq":34})"));
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 2);
    });
}

TEST_F(RPCNFTHistoryHandlerTest, SpecificLedgerIndex)
{
    // adjust the order for forward->false
    auto const transactions = genTransactions(kMAX_SEQ - 1, kMIN_SEQ + 1);
    auto const transCursor = TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchNFTTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchNFTTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{kMAX_SEQ - 1, INT32_MAX})),
            testing::_
        )
    )
        .Times(1);

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ - 1);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kMAX_SEQ - 1, _)).WillByDefault(Return(ledgerHeader));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NFTHistoryHandler{backend_}};
        auto static const kINPUT = json::parse(fmt::format(
            R"({{
                "nft_id":"{}",
                "ledger_index":{}
            }})",
            kNFT_ID,
            kMAX_SEQ - 1
        ));
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("nft_id").as_string(), kNFT_ID);
        EXPECT_EQ(output.result->at("ledger_index_min").as_uint64(), kMAX_SEQ - 1);
        EXPECT_EQ(output.result->at("ledger_index_max").as_uint64(), kMAX_SEQ - 1);
        EXPECT_FALSE(output.result->as_object().contains("limit"));
        EXPECT_FALSE(output.result->as_object().contains("marker"));
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 1);
    });
}

TEST_F(RPCNFTHistoryHandlerTest, SpecificNonexistLedgerIntIndex)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kMAX_SEQ - 1, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NFTHistoryHandler{backend_}};
        auto static const kINPUT = json::parse(fmt::format(
            R"({{
                "nft_id":"{}",
                "ledger_index":{}
            }})",
            kNFT_ID,
            kMAX_SEQ - 1
        ));
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCNFTHistoryHandlerTest, SpecificNonexistLedgerStringIndex)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kMAX_SEQ - 1, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NFTHistoryHandler{backend_}};
        auto static const kINPUT = json::parse(fmt::format(
            R"({{
                "nft_id":"{}",
                "ledger_index":"{}"
            }})",
            kNFT_ID,
            kMAX_SEQ - 1
        ));
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCNFTHistoryHandlerTest, SpecificLedgerHash)
{
    // adjust the order for forward->false
    auto const transactions = genTransactions(kMAX_SEQ - 1, kMIN_SEQ + 1);
    auto const transCursor = TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchNFTTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchNFTTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{kMAX_SEQ - 1, INT32_MAX})),
            testing::_
        )
    )
        .Times(1);

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ - 1);
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    ON_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillByDefault(Return(ledgerHeader));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NFTHistoryHandler{backend_}};
        auto static const kINPUT = json::parse(fmt::format(
            R"({{
                "nft_id":"{}",
                "ledger_hash":"{}"
            }})",
            kNFT_ID,
            kLEDGER_HASH
        ));
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("nft_id").as_string(), kNFT_ID);
        EXPECT_EQ(output.result->at("ledger_index_min").as_uint64(), kMAX_SEQ - 1);
        EXPECT_EQ(output.result->at("ledger_index_max").as_uint64(), kMAX_SEQ - 1);
        EXPECT_FALSE(output.result->as_object().contains("limit"));
        EXPECT_FALSE(output.result->as_object().contains("marker"));
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 1);
    });
}

TEST_F(RPCNFTHistoryHandlerTest, TxLessThanMinSeq)
{
    auto const transactions = genTransactions(kMAX_SEQ - 1, kMIN_SEQ + 1);
    auto const transCursor = TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchNFTTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchNFTTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{kMAX_SEQ - 1, INT32_MAX})),
            testing::_
        )
    )
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NFTHistoryHandler{backend_}};
        auto static const kINPUT = json::parse(fmt::format(
            R"({{
                "nft_id":"{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "forward": false
            }})",
            kNFT_ID,
            kMIN_SEQ + 2,
            kMAX_SEQ - 1
        ));
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("nft_id").as_string(), kNFT_ID);
        EXPECT_EQ(output.result->at("ledger_index_min").as_uint64(), kMIN_SEQ + 2);
        EXPECT_EQ(output.result->at("ledger_index_max").as_uint64(), kMAX_SEQ - 1);
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 1);
        EXPECT_FALSE(output.result->as_object().contains("limit"));
        EXPECT_FALSE(output.result->as_object().contains("marker"));
    });
}

TEST_F(RPCNFTHistoryHandlerTest, TxLargerThanMaxSeq)
{
    auto const transactions = genTransactions(kMAX_SEQ - 1, kMIN_SEQ + 1);
    auto const transCursor = TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchNFTTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchNFTTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{kMAX_SEQ - 2, INT32_MAX})),
            testing::_
        )
    )
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NFTHistoryHandler{backend_}};
        auto static const kINPUT = json::parse(fmt::format(
            R"({{
                "nft_id":"{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "forward": false
            }})",
            kNFT_ID,
            kMIN_SEQ + 1,
            kMAX_SEQ - 2
        ));
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("nft_id").as_string(), kNFT_ID);
        EXPECT_EQ(output.result->at("ledger_index_min").as_uint64(), kMIN_SEQ + 1);
        EXPECT_EQ(output.result->at("ledger_index_max").as_uint64(), kMAX_SEQ - 2);
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 1);
        EXPECT_FALSE(output.result->as_object().contains("limit"));
        EXPECT_EQ(output.result->at("marker").as_object(), json::parse(R"({"ledger":12,"seq":34})"));
    });
}

TEST_F(RPCNFTHistoryHandlerTest, LimitMoreThanMax)
{
    auto const transactions = genTransactions(kMIN_SEQ + 1, kMAX_SEQ - 1);
    auto const transCursor = TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchNFTTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchNFTTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{kMAX_SEQ - 1, INT32_MAX})),
            testing::_
        )
    )
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NFTHistoryHandler{backend_}};
        auto static const kINPUT = json::parse(fmt::format(
            R"({{
                "nft_id":"{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "forward": false,
                "limit": {}
            }})",
            kNFT_ID,
            kMIN_SEQ + 1,
            kMAX_SEQ - 1,
            NFTHistoryHandler::kLIMIT_MAX + 1
        ));
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("nft_id").as_string(), kNFT_ID);
        EXPECT_EQ(output.result->at("ledger_index_min").as_uint64(), kMIN_SEQ + 1);
        EXPECT_EQ(output.result->at("ledger_index_max").as_uint64(), kMAX_SEQ - 1);
        EXPECT_EQ(output.result->at("marker").as_object(), json::parse(R"({"ledger":12,"seq":34})"));
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 2);
        EXPECT_EQ(output.result->as_object().at("limit").as_uint64(), NFTHistoryHandler::kLIMIT_MAX);
    });
}
