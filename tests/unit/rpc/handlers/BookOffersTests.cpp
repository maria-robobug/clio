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
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/BookOffers.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/parse.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/Blob.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Book.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/UintTypes.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr auto kACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";

constexpr auto kLEDGER_HASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kINDEX1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
constexpr auto kINDEX2 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";

// 20 USD : 10 XRP
constexpr auto kPAYS20_USD_GETS10_XRP_BOOK_DIR = "43B83ADC452B85FCBADA6CAEAC5181C255A213630D58FFD455071AFD498D0000";

// 20 XRP : 10 USD
constexpr auto kPAYS20_XRP_GETS10_USD_BOOK_DIR = "7B1767D41DBCE79D9585CF9D0262A5FEC45E5206FF524F8B55071AFD498D0000";

// transfer rate x2
constexpr auto kTRANSFER_RATE_X2 = 2000000000;

struct ParameterTestBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

}  // namespace

using namespace rpc;
namespace json = boost::json;
using namespace testing;

struct RPCBookOffersHandlerTest : HandlerBaseTest {
    RPCBookOffersHandlerTest()
    {
        backend_->setRange(10, 300);
    }
};

struct RPCBookOffersParameterTest : RPCBookOffersHandlerTest, WithParamInterface<ParameterTestBundle> {};

TEST_P(RPCBookOffersParameterTest, CheckError)
{
    auto bundle = GetParam();
    auto const handler = AnyHandler{BookOffersHandler{backend_}};
    runSpawn([&](boost::asio::yield_context yield) {
        auto const output = handler.process(json::parse(bundle.testJson), Context{.yield = yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), bundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), bundle.expectedErrorMessage);
    });
}

static auto
generateParameterBookOffersTestBundles()
{
    return std::vector<ParameterTestBundle>{
        ParameterTestBundle{
            .testName = "MissingTakerGets",
            .testJson = R"({
                "taker_pays" : 
                {
                    "currency" : "USD",
                    "issuer" : "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                }
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'taker_gets' missing"
        },
        ParameterTestBundle{
            .testName = "MissingTakerPays",
            .testJson = R"({
                "taker_gets" : 
                {
                    "currency" : "USD",
                    "issuer" : "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                }
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'taker_pays' missing"
        },
        ParameterTestBundle{
            .testName = "WrongTypeTakerPays",
            .testJson = R"({
                "taker_pays" : "wrong",
                "taker_gets" : 
                {
                    "currency" : "XRP"
                }
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        ParameterTestBundle{
            .testName = "WrongTypeTakerGets",
            .testJson = R"({
                "taker_gets" : "wrong",
                "taker_pays" : 
                {
                    "currency" : "XRP"
                }
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        ParameterTestBundle{
            .testName = "TakerPaysMissingCurrency",
            .testJson = R"({
                "taker_pays" : {},
                "taker_gets" : 
                {
                    "currency" : "XRP"
                }
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'currency' missing"
        },
        ParameterTestBundle{
            .testName = "TakerGetsMissingCurrency",
            .testJson = R"({
                "taker_gets" : {},
                "taker_pays" : 
                {
                    "currency" : "XRP"
                }
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'currency' missing"
        },
        ParameterTestBundle{
            .testName = "TakerGetsWrongCurrency",
            .testJson = R"({
                "taker_gets" : 
                {
                    "currency" : "CNYY",
                    "issuer" : "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                },
                "taker_pays" : 
                {
                    "currency" : "XRP"
                }
            })",
            .expectedError = "dstAmtMalformed",
            .expectedErrorMessage = "Destination amount/currency/issuer is malformed."
        },
        ParameterTestBundle{
            .testName = "TakerPaysWrongCurrency",
            .testJson = R"({
                "taker_pays" : 
                {
                    "currency" : "CNYY",
                    "issuer" : "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                },
                "taker_gets" : 
                {
                    "currency" : "XRP"
                }
            })",
            .expectedError = "srcCurMalformed",
            .expectedErrorMessage = "Source currency is malformed."
        },
        ParameterTestBundle{
            .testName = "TakerGetsCurrencyNotString",
            .testJson = R"({
                "taker_gets" : 
                {
                    "currency" : 123,
                    "issuer" : "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                },
                "taker_pays" : 
                {
                    "currency" : "XRP"
                }
            })",
            .expectedError = "dstAmtMalformed",
            .expectedErrorMessage = "Destination amount/currency/issuer is malformed."
        },
        ParameterTestBundle{
            .testName = "TakerPaysCurrencyNotString",
            .testJson = R"({
                "taker_pays" : 
                {
                    "currency" : 123,
                    "issuer" : "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                },
                "taker_gets" : 
                {
                    "currency" : "XRP"
                }
            })",
            .expectedError = "srcCurMalformed",
            .expectedErrorMessage = "Source currency is malformed."
        },
        ParameterTestBundle{
            .testName = "TakerGetsWrongIssuer",
            .testJson = R"({
                "taker_gets" : 
                {
                    "currency" : "CNY",
                    "issuer" : "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs5"
                },
                "taker_pays" : 
                {
                    "currency" : "XRP"
                }
            })",
            .expectedError = "dstIsrMalformed",
            .expectedErrorMessage = "Destination issuer is malformed."
        },
        ParameterTestBundle{
            .testName = "TakerPaysWrongIssuer",
            .testJson = R"({
                "taker_pays" : 
                {
                    "currency" : "CNY",
                    "issuer" : "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs5"
                },
                "taker_gets" : 
                {
                    "currency" : "XRP"
                }
            })",
            .expectedError = "srcIsrMalformed",
            .expectedErrorMessage = "Source issuer is malformed."
        },
        ParameterTestBundle{
            .testName = "InvalidTaker",
            .testJson = R"({
                "taker_pays" : 
                {
                    "currency" : "CNY",
                    "issuer" : "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                },
                "taker_gets" : 
                {
                    "currency" : "XRP"
                },
                "taker": "123"
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid field 'taker'."
        },
        ParameterTestBundle{
            .testName = "TakerNotString",
            .testJson = R"({
                "taker_pays" : 
                {
                    "currency" : "CNY",
                    "issuer" : "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                },
                "taker_gets" : 
                {
                    "currency" : "XRP"
                },
                "taker": 123
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid field 'taker'."
        },
        ParameterTestBundle{
            .testName = "LimitNotInt",
            .testJson = R"({
                "taker_pays" : 
                {
                    "currency" : "CNY",
                    "issuer" : "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                },
                "taker_gets" : 
                {
                    "currency" : "XRP"
                },
                "limit": "123"
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        ParameterTestBundle{
            .testName = "LimitNagetive",
            .testJson = R"({
                "taker_pays" : 
                {
                    "currency" : "CNY",
                    "issuer" : "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                },
                "taker_gets" : 
                {
                    "currency" : "XRP"
                },
                "limit": -1
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        ParameterTestBundle{
            .testName = "LimitZero",
            .testJson = R"({
                "taker_pays" : 
                {
                    "currency" : "CNY",
                    "issuer" : "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                },
                "taker_gets" : 
                {
                    "currency" : "XRP"
                },
                "limit": 0
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        ParameterTestBundle{
            .testName = "LedgerIndexInvalid",
            .testJson = R"({
                "taker_pays" : 
                {
                    "currency" : "CNY",
                    "issuer" : "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                },
                "taker_gets" : 
                {
                    "currency" : "XRP"
                },
                "ledger_index": "xxx"
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledgerIndexMalformed"
        },
        ParameterTestBundle{
            .testName = "LedgerHashInvalid",
            .testJson = R"({
                "taker_pays" : 
                {
                    "currency" : "CNY",
                    "issuer" : "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                },
                "taker_gets" : 
                {
                    "currency" : "XRP"
                },
                "ledger_hash": "xxx"
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledger_hashMalformed"
        },
        ParameterTestBundle{
            .testName = "LedgerHashNotString",
            .testJson = R"({
                "taker_pays" : 
                {
                    "currency" : "CNY",
                    "issuer" : "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                },
                "taker_gets" : 
                {
                    "currency" : "XRP"
                },
                "ledger_hash": 123
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledger_hashNotString"
        },
        ParameterTestBundle{
            .testName = "GetsPaysXRPWithIssuer",
            .testJson = R"({
                "taker_pays" : 
                {
                    "currency" : "XRP",
                    "issuer" : "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"
                },
                "taker_gets" : 
                {
                    "currency" : "CNY",
                    "issuer" : "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                }
            })",
            .expectedError = "srcIsrMalformed",
            .expectedErrorMessage = "Unneeded field 'taker_pays.issuer' for XRP currency specification."
        },
        ParameterTestBundle{
            .testName = "PaysCurrencyWithXRPIssuer",
            .testJson = R"({
                "taker_pays" : 
                {
                    "currency" : "JPY"                    
                },
                "taker_gets" : 
                {
                    "currency" : "CNY",
                    "issuer" : "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                }
            })",
            .expectedError = "srcIsrMalformed",
            .expectedErrorMessage = "Invalid field 'taker_pays.issuer', expected non-XRP issuer."
        },
        ParameterTestBundle{
            .testName = "GetsCurrencyWithXRPIssuer",
            .testJson = R"({
                "taker_pays" : 
                {
                    "currency" : "XRP"                    
                },
                "taker_gets" : 
                {
                    "currency" : "CNY"                    
                }            
            })",
            .expectedError = "dstIsrMalformed",
            .expectedErrorMessage = "Invalid field 'taker_gets.issuer', expected non-XRP issuer."
        },
        ParameterTestBundle{
            .testName = "GetsXRPWithIssuer",
            .testJson = R"({
                "taker_pays" : 
                {
                    "currency" : "CNY",
                    "issuer" : "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"                    
                },
                "taker_gets" : 
                {
                    "currency" : "XRP",
                    "issuer" : "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"                    
                }            
            })",
            .expectedError = "dstIsrMalformed",
            .expectedErrorMessage = "Unneeded field 'taker_gets.issuer' for XRP currency specification."
        },
        ParameterTestBundle{
            .testName = "BadMarket",
            .testJson = R"({
                "taker_pays" : 
                {
                    "currency" : "CNY",
                    "issuer" : "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"                    
                },
                "taker_gets" : 
                {
                    "currency" : "CNY",
                    "issuer" : "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"                   
                }            
            })",
            .expectedError = "badMarket",
            .expectedErrorMessage = "badMarket"
        }
    };
}

INSTANTIATE_TEST_SUITE_P(
    RPCBookOffersHandler,
    RPCBookOffersParameterTest,
    testing::ValuesIn(generateParameterBookOffersTestBundles()),
    tests::util::kNAME_GENERATOR
);

struct BookOffersNormalTestBundle {
    std::string testName;
    std::string inputJson;
    std::map<ripple::uint256, std::optional<ripple::uint256>> mockedSuccessors;
    std::map<ripple::uint256, Blob> mockedLedgerObjects;
    uint32_t ledgerObjectCalls;
    std::vector<ripple::STObject> mockedOffers;
    std::string expectedJson;
};

struct RPCBookOffersNormalPathTest : public RPCBookOffersHandlerTest,
                                     public WithParamInterface<BookOffersNormalTestBundle> {};

TEST_P(RPCBookOffersNormalPathTest, CheckOutput)
{
    auto const& bundle = GetParam();
    auto const seq = 300;

    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // return valid ledgerHeader
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, seq);
    ON_CALL(*backend_, fetchLedgerBySequence(seq, _)).WillByDefault(Return(ledgerHeader));

    // return valid book dir
    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(bundle.mockedSuccessors.size());
    for (auto const& [key, value] : bundle.mockedSuccessors) {
        ON_CALL(*backend_, doFetchSuccessorKey(key, seq, _)).WillByDefault(Return(value));
    }

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(bundle.ledgerObjectCalls);

    for (auto const& [key, value] : bundle.mockedLedgerObjects) {
        ON_CALL(*backend_, doFetchLedgerObject(key, seq, _)).WillByDefault(Return(value));
    }

    std::vector<Blob> bbs;
    std::ranges::transform(
        bundle.mockedOffers,

        std::back_inserter(bbs),
        [](auto const& obj) { return obj.getSerializer().peekData(); }
    );
    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const handler = AnyHandler{BookOffersHandler{backend_}};
    runSpawn([&](boost::asio::yield_context yield) {
        auto const output = handler.process(json::parse(bundle.inputJson), Context{.yield = yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), json::parse(bundle.expectedJson));
    });
}

static auto
generateNormalPathBookOffersTestBundles()
{
    auto const account = getAccountIdWithString(kACCOUNT);
    auto const account2 = getAccountIdWithString(kACCOUNT2);

    auto const frozenTrustLine = createRippleStateLedgerObject(
        "USD", kACCOUNT, -8, kACCOUNT2, 1000, kACCOUNT, 2000, kINDEX1, 2, ripple::lsfLowFreeze
    );

    auto const gets10USDPays20XRPOffer = createOfferLedgerObject(
        kACCOUNT2,
        10,
        20,
        ripple::to_string(ripple::to_currency("USD")),
        ripple::to_string(ripple::xrpCurrency()),
        kACCOUNT,
        toBase58(ripple::xrpAccount()),
        kPAYS20_XRP_GETS10_USD_BOOK_DIR
    );

    auto const gets10USDPays20XRPOwnerOffer = createOfferLedgerObject(
        kACCOUNT,
        10,
        20,
        ripple::to_string(ripple::to_currency("USD")),
        ripple::to_string(ripple::xrpCurrency()),
        kACCOUNT,
        toBase58(ripple::xrpAccount()),
        kPAYS20_XRP_GETS10_USD_BOOK_DIR
    );

    auto const gets10XRPPays20USDOffer = createOfferLedgerObject(
        kACCOUNT2,
        10,
        20,
        ripple::to_string(ripple::xrpCurrency()),
        ripple::to_string(ripple::to_currency("USD")),
        toBase58(ripple::xrpAccount()),
        kACCOUNT,
        kPAYS20_USD_GETS10_XRP_BOOK_DIR
    );

    auto const getsXRPPaysUSDBook = getBookBase(std::get<ripple::Book>(
        rpc::parseBook(ripple::to_currency("USD"), account, ripple::xrpCurrency(), ripple::xrpAccount())
    ));
    auto const getsUSDPaysXRPBook = getBookBase(std::get<ripple::Book>(
        rpc::parseBook(ripple::xrpCurrency(), ripple::xrpAccount(), ripple::to_currency("USD"), account)
    ));

    auto const getsXRPPaysUSDInputJson = fmt::format(
        R"({{
            "taker_gets": 
            {{
                "currency": "XRP"
            }},
            "taker_pays": 
            {{
                "currency": "USD",
                "issuer": "{}"
            }}
        }})",
        kACCOUNT
    );

    auto const paysXRPGetsUSDInputJson = fmt::format(
        R"({{
            "taker_pays": 
            {{
                "currency": "XRP"
            }},
            "taker_gets": 
            {{
                "currency": "USD",
                "issuer": "{}"
            }}
        }})",
        kACCOUNT
    );

    auto const feeLedgerObject = createLegacyFeeSettingBlob(1, 2, 3, 4, 0);

    auto const trustline30Balance =
        createRippleStateLedgerObject("USD", kACCOUNT, -30, kACCOUNT2, 1000, kACCOUNT, 2000, kINDEX1, 2, 0);

    auto const trustline8Balance =
        createRippleStateLedgerObject("USD", kACCOUNT, -8, kACCOUNT2, 1000, kACCOUNT, 2000, kINDEX1, 2, 0);

    return std::vector<BookOffersNormalTestBundle>{
        BookOffersNormalTestBundle{
            .testName = "PaysUSDGetsXRPNoFrozenOwnerFundEnough",
            .inputJson = getsXRPPaysUSDInputJson,
            // prepare offer dir index
            .mockedSuccessors =
                std::map<ripple::uint256, std::optional<ripple::uint256>>{
                    {getsXRPPaysUSDBook, ripple::uint256{kPAYS20_USD_GETS10_XRP_BOOK_DIR}},
                    {ripple::uint256{kPAYS20_USD_GETS10_XRP_BOOK_DIR}, std::optional<ripple::uint256>{}}
                },
            .mockedLedgerObjects =
                std::map<ripple::uint256, ripple::Blob>{
                    // book dir object
                    {ripple::uint256{kPAYS20_USD_GETS10_XRP_BOOK_DIR},
                     createOwnerDirLedgerObject({ripple::uint256{kINDEX2}}, kINDEX1).getSerializer().peekData()},
                    // pays issuer account object
                    {ripple::keylet::account(account).key,
                     createAccountRootObject(kACCOUNT, 0, 2, 200, 2, kINDEX1, 2).getSerializer().peekData()},
                    // owner account object
                    {ripple::keylet::account(account2).key,
                     createAccountRootObject(kACCOUNT2, 0, 2, 200, 2, kINDEX1, 2).getSerializer().peekData()},
                    // fee settings: base ->3 inc->2, account2 has 2 objects ,total
                    // reserve ->7
                    // owner_funds should be 193
                    {ripple::keylet::fees().key, feeLedgerObject}
                },
            .ledgerObjectCalls = 5,
            .mockedOffers = std::vector<ripple::STObject>{gets10XRPPays20USDOffer},
            .expectedJson = fmt::format(
                R"({{
                    "ledger_hash":"{}",
                    "ledger_index":300,
                    "offers":[
                        {{
                            "Account":"{}",
                            "BookDirectory":"43B83ADC452B85FCBADA6CAEAC5181C255A213630D58FFD455071AFD498D0000",
                            "BookNode":"0",
                            "Flags":0,
                            "LedgerEntryType":"Offer",
                            "OwnerNode":"0",
                            "PreviousTxnID":"0000000000000000000000000000000000000000000000000000000000000000",
                            "PreviousTxnLgrSeq":0,
                            "Sequence":0,
                            "TakerGets":"10",
                            "TakerPays":{{
                                "currency":"USD",
                                "issuer":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value":"20"
                            }},
                            "index":"E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321",
                            "owner_funds":"{}",
                            "quality":"{}"
                        }}
                    ]
                }})",
                kLEDGER_HASH,
                kACCOUNT2,
                193,
                2
            )
        },
        BookOffersNormalTestBundle{
            .testName = "PaysUSDGetsXRPNoFrozenOwnerFundNotEnough",
            .inputJson = getsXRPPaysUSDInputJson,
            // prepare offer dir index
            .mockedSuccessors =
                std::map<ripple::uint256, std::optional<ripple::uint256>>{
                    {getsXRPPaysUSDBook, ripple::uint256{kPAYS20_USD_GETS10_XRP_BOOK_DIR}},
                    {ripple::uint256{kPAYS20_USD_GETS10_XRP_BOOK_DIR}, std::optional<ripple::uint256>{}}
                },
            .mockedLedgerObjects =
                std::map<ripple::uint256, ripple::Blob>{
                    // book dir object
                    {ripple::uint256{kPAYS20_USD_GETS10_XRP_BOOK_DIR},
                     createOwnerDirLedgerObject({ripple::uint256{kINDEX2}}, kINDEX1).getSerializer().peekData()},
                    // pays issuer account object
                    {ripple::keylet::account(account).key,
                     createAccountRootObject(kACCOUNT, 0, 2, 200, 2, kINDEX1, 2).getSerializer().peekData()},
                    // owner account object, hold
                    {ripple::keylet::account(account2).key,
                     createAccountRootObject(kACCOUNT2, 0, 2, 5 + 7, 2, kINDEX1, 2).getSerializer().peekData()},
                    // fee settings: base ->3 inc->2, account2 has 2 objects
                    // ,total
                    // reserve ->7
                    {ripple::keylet::fees().key, feeLedgerObject}
                },
            .ledgerObjectCalls = 5,
            .mockedOffers = std::vector<ripple::STObject>{gets10XRPPays20USDOffer},
            .expectedJson = fmt::format(
                R"({{
                    "ledger_hash":"{}",
                    "ledger_index":300,
                    "offers":
                    [
                        {{
                            "Account":"{}",
                            "BookDirectory":"43B83ADC452B85FCBADA6CAEAC5181C255A213630D58FFD455071AFD498D0000",
                            "BookNode":"0",
                            "Flags":0,
                            "LedgerEntryType":"Offer",
                            "OwnerNode":"0",
                            "PreviousTxnID":"0000000000000000000000000000000000000000000000000000000000000000",
                            "PreviousTxnLgrSeq":0,
                            "Sequence":0,
                            "TakerGets":"10",
                            "TakerPays":{{
                                "currency":"USD",
                                "issuer":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value":"20"
                            }},
                            "index":"E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321",
                            "owner_funds":"{}",
                            "quality":"{}",
                            "taker_gets_funded":"5",
                            "taker_pays_funded":{{
                                "currency":"USD",
                                "issuer":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value":"10"
                            }}
                        }}
                    ]
                }})",
                kLEDGER_HASH,
                kACCOUNT2,
                5,
                2
            )
        },
        BookOffersNormalTestBundle{
            .testName = "PaysUSDGetsXRPFrozen",
            .inputJson = getsXRPPaysUSDInputJson,
            // prepare offer dir index
            .mockedSuccessors =
                std::map<ripple::uint256, std::optional<ripple::uint256>>{
                    {getsXRPPaysUSDBook, ripple::uint256{kPAYS20_USD_GETS10_XRP_BOOK_DIR}},
                    {ripple::uint256{kPAYS20_USD_GETS10_XRP_BOOK_DIR}, std::optional<ripple::uint256>{}}
                },
            .mockedLedgerObjects =
                std::map<ripple::uint256, ripple::Blob>{
                    // book dir object
                    {ripple::uint256{kPAYS20_USD_GETS10_XRP_BOOK_DIR},
                     createOwnerDirLedgerObject({ripple::uint256{kINDEX2}}, kINDEX1).getSerializer().peekData()},
                    // pays issuer account object
                    {ripple::keylet::account(account).key,
                     createAccountRootObject(kACCOUNT, ripple::lsfGlobalFreeze, 2, 200, 2, kINDEX1, 2)
                         .getSerializer()
                         .peekData()}
                },
            .ledgerObjectCalls = 3,
            .mockedOffers = std::vector<ripple::STObject>{gets10XRPPays20USDOffer},
            .expectedJson = fmt::format(
                R"({{
                    "ledger_hash":"{}",
                    "ledger_index":300,
                    "offers":
                    [
                        {{
                            "Account":"{}",
                            "BookDirectory":"43B83ADC452B85FCBADA6CAEAC5181C255A213630D58FFD455071AFD498D0000",
                            "BookNode":"0",
                            "Flags":0,
                            "LedgerEntryType":"Offer",
                            "OwnerNode":"0",
                            "PreviousTxnID":"0000000000000000000000000000000000000000000000000000000000000000",
                            "PreviousTxnLgrSeq":0,
                            "Sequence":0,
                            "TakerGets":"10",
                            "TakerPays":{{
                                "currency":"USD",
                                "issuer":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value":"20"
                            }},
                            "index":"E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321",
                            "owner_funds":"{}",
                            "quality":"{}",
                            "taker_gets_funded":"0",
                            "taker_pays_funded":{{
                                "currency":"USD",
                                "issuer":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value":"0"
                            }}
                        }}
                    ]
                }})",
                kLEDGER_HASH,
                kACCOUNT2,
                0,
                2
            )
        },
        BookOffersNormalTestBundle{
            .testName = "GetsUSDPaysXRPFrozen",
            .inputJson = paysXRPGetsUSDInputJson,
            // prepare offer dir index
            .mockedSuccessors =
                std::map<ripple::uint256, std::optional<ripple::uint256>>{
                    {getsUSDPaysXRPBook, ripple::uint256{kPAYS20_XRP_GETS10_USD_BOOK_DIR}},
                    {ripple::uint256{kPAYS20_XRP_GETS10_USD_BOOK_DIR}, std::optional<ripple::uint256>{}}
                },
            .mockedLedgerObjects =
                std::map<ripple::uint256, ripple::Blob>{
                    // book dir object
                    {ripple::uint256{kPAYS20_XRP_GETS10_USD_BOOK_DIR},
                     createOwnerDirLedgerObject({ripple::uint256{kINDEX2}}, kINDEX1).getSerializer().peekData()},
                    // gets issuer account object
                    {ripple::keylet::account(account).key,
                     createAccountRootObject(
                         kACCOUNT, ripple::lsfGlobalFreeze, 2, 200, 2, kINDEX1, 2, kTRANSFER_RATE_X2
                     )
                         .getSerializer()
                         .peekData()}
                },
            .ledgerObjectCalls = 3,
            .mockedOffers = std::vector<ripple::STObject>{gets10USDPays20XRPOffer},
            .expectedJson = fmt::format(
                R"({{
                    "ledger_hash":"{}",
                    "ledger_index":300,
                    "offers":
                    [
                        {{
                            "Account":"{}",
                            "BookDirectory":"{}",
                            "BookNode":"0",
                            "Flags":0,
                            "LedgerEntryType":"Offer",
                            "OwnerNode":"0",
                            "PreviousTxnID":"0000000000000000000000000000000000000000000000000000000000000000",
                            "PreviousTxnLgrSeq":0,
                            "Sequence":0,
                            "TakerPays":"20",
                            "TakerGets":{{
                                "currency":"USD",
                                "issuer":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value":"10"
                            }},
                            "index":"E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321",
                            "owner_funds":"{}",
                            "quality":"{}",
                            "taker_pays_funded":"0",
                            "taker_gets_funded":{{
                                "currency":"USD",
                                "issuer":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value":"0"
                            }}
                        }}
                    ]
                }})",
                kLEDGER_HASH,
                kACCOUNT2,
                kPAYS20_XRP_GETS10_USD_BOOK_DIR,
                0,
                2
            )
        },
        BookOffersNormalTestBundle{
            .testName = "PaysXRPGetsUSDWithTransferFee",
            .inputJson = paysXRPGetsUSDInputJson,
            // prepare offer dir index
            .mockedSuccessors =
                std::map<ripple::uint256, std::optional<ripple::uint256>>{
                    {getsUSDPaysXRPBook, ripple::uint256{kPAYS20_XRP_GETS10_USD_BOOK_DIR}},
                    {ripple::uint256{kPAYS20_XRP_GETS10_USD_BOOK_DIR}, std::optional<ripple::uint256>{}}
                },
            .mockedLedgerObjects =
                std::map<ripple::uint256, ripple::Blob>{
                    // book dir object
                    {ripple::uint256{kPAYS20_XRP_GETS10_USD_BOOK_DIR},
                     createOwnerDirLedgerObject({ripple::uint256{kINDEX2}}, kINDEX1).getSerializer().peekData()},
                    // gets issuer account object, rate is 1/2
                    {ripple::keylet::account(account).key,
                     createAccountRootObject(kACCOUNT, 0, 2, 200, 2, kINDEX1, 2, kTRANSFER_RATE_X2)
                         .getSerializer()
                         .peekData()},
                    // trust line between gets issuer and owner,owner has 8 USD
                    {ripple::keylet::line(account2, account, ripple::to_currency("USD")).key,
                     trustline8Balance.getSerializer().peekData()},
                },
            .ledgerObjectCalls = 6,
            .mockedOffers = std::vector<ripple::STObject>{gets10USDPays20XRPOffer},
            .expectedJson = fmt::format(
                R"({{
                    "ledger_hash":"{}",
                    "ledger_index":300,
                    "offers":
                    [
                        {{
                            "Account":"{}",
                            "BookDirectory":"{}",
                            "BookNode":"0",
                            "Flags":0,
                            "LedgerEntryType":"Offer",
                            "OwnerNode":"0",
                            "PreviousTxnID":"0000000000000000000000000000000000000000000000000000000000000000",
                            "PreviousTxnLgrSeq":0,
                            "Sequence":0,
                            "TakerPays":"20",
                            "TakerGets":{{
                                "currency":"USD",
                                "issuer":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value":"10"
                            }},
                            "index":"E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321",
                            "owner_funds":"{}",
                            "quality":"{}",
                            "taker_gets_funded":{{
                                "currency":"USD",
                                "issuer":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value":"4"
                            }},
                            "taker_pays_funded":"8"
                        }}
                    ]
                }})",
                kLEDGER_HASH,
                kACCOUNT2,
                kPAYS20_XRP_GETS10_USD_BOOK_DIR,
                8,
                2
            )
        },
        BookOffersNormalTestBundle{
            .testName = "PaysXRPGetsUSDWithMultipleOffers",
            .inputJson = paysXRPGetsUSDInputJson,
            // prepare offer dir index
            .mockedSuccessors =
                std::map<ripple::uint256, std::optional<ripple::uint256>>{
                    {getsUSDPaysXRPBook, ripple::uint256{kPAYS20_XRP_GETS10_USD_BOOK_DIR}},
                    {ripple::uint256{kPAYS20_XRP_GETS10_USD_BOOK_DIR}, std::optional<ripple::uint256>{}}
                },
            .mockedLedgerObjects =
                std::map<ripple::uint256, ripple::Blob>{
                    // book dir object
                    {ripple::uint256{kPAYS20_XRP_GETS10_USD_BOOK_DIR},
                     createOwnerDirLedgerObject({ripple::uint256{kINDEX2}, ripple::uint256{kINDEX2}}, kINDEX1)
                         .getSerializer()
                         .peekData()},
                    // gets issuer account object
                    {ripple::keylet::account(account).key,
                     createAccountRootObject(kACCOUNT, 0, 2, 200, 2, kINDEX1, 2, kTRANSFER_RATE_X2)
                         .getSerializer()
                         .peekData()},
                    // trust line between gets issuer and owner,owner has 30 USD
                    {ripple::keylet::line(account2, account, ripple::to_currency("USD")).key,
                     trustline30Balance.getSerializer().peekData()},
                },
            .ledgerObjectCalls = 6,
            .mockedOffers =
                std::vector<ripple::STObject>{// After offer1, balance is 30 - 2*10 = 10
                                              gets10USDPays20XRPOffer,
                                              // offer2 not fully funded, balance is 10, rate is 2, so only
                                              // gets 5
                                              gets10USDPays20XRPOffer
                },
            .expectedJson = fmt::format(
                R"({{
                    "ledger_hash":"{}",
                    "ledger_index":300,
                    "offers":
                    [
                        {{
                            "Account":"{}",
                            "BookDirectory":"{}",
                            "BookNode":"0",
                            "Flags":0,
                            "LedgerEntryType":"Offer",
                            "OwnerNode":"0",
                            "PreviousTxnID":"0000000000000000000000000000000000000000000000000000000000000000",
                            "PreviousTxnLgrSeq":0,
                            "Sequence":0,
                            "TakerPays":"20",
                            "TakerGets":{{
                                "currency":"USD",
                                "issuer":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value":"10"
                            }},
                            "index":"E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321",
                            "owner_funds":"{}",
                            "quality":"{}"
                        }},
                        {{
                            "Account":"{}",
                            "BookDirectory":"{}",
                            "BookNode":"0",
                            "Flags":0,
                            "LedgerEntryType":"Offer",
                            "OwnerNode":"0",
                            "PreviousTxnID":"0000000000000000000000000000000000000000000000000000000000000000",
                            "PreviousTxnLgrSeq":0,
                            "Sequence":0,
                            "TakerPays":"20",
                            "TakerGets":{{
                                "currency":"USD",
                                "issuer":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value":"10"
                            }},
                            "index":"E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321",
                            "taker_gets_funded":{{
                                "currency":"USD",
                                "issuer":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value":"5"
                            }},
                            "taker_pays_funded":"10",
                            "quality":"{}"
                        }}
                    ]
                }})",
                kLEDGER_HASH,
                kACCOUNT2,
                kPAYS20_XRP_GETS10_USD_BOOK_DIR,
                30,
                2,
                kACCOUNT2,
                kPAYS20_XRP_GETS10_USD_BOOK_DIR,
                2
            )
        },
        BookOffersNormalTestBundle{
            .testName = "PaysXRPGetsUSDSellingOwnCurrency",
            .inputJson = paysXRPGetsUSDInputJson,
            // prepare offer dir index
            .mockedSuccessors =
                std::map<ripple::uint256, std::optional<ripple::uint256>>{
                    {getsUSDPaysXRPBook, ripple::uint256{kPAYS20_XRP_GETS10_USD_BOOK_DIR}},
                    {ripple::uint256{kPAYS20_XRP_GETS10_USD_BOOK_DIR}, std::optional<ripple::uint256>{}}
                },
            .mockedLedgerObjects =
                std::map<ripple::uint256, ripple::Blob>{
                    // book dir object
                    {ripple::uint256{kPAYS20_XRP_GETS10_USD_BOOK_DIR},
                     createOwnerDirLedgerObject({ripple::uint256{kINDEX2}}, kINDEX1).getSerializer().peekData()},
                    // gets issuer account object, rate is 1/2
                    {ripple::keylet::account(account).key,
                     createAccountRootObject(kACCOUNT, 0, 2, 200, 2, kINDEX1, 2, kTRANSFER_RATE_X2)
                         .getSerializer()
                         .peekData()},
                },
            .ledgerObjectCalls = 3,
            .mockedOffers = std::vector<ripple::STObject>{gets10USDPays20XRPOwnerOffer},
            .expectedJson = fmt::format(
                R"({{
                    "ledger_hash":"{}",
                    "ledger_index":300,
                    "offers":
                    [
                        {{
                            "Account":"{}",
                            "BookDirectory":"{}",
                            "BookNode":"0",
                            "Flags":0,
                            "LedgerEntryType":"Offer",
                            "OwnerNode":"0",
                            "PreviousTxnID":"0000000000000000000000000000000000000000000000000000000000000000",
                            "PreviousTxnLgrSeq":0,
                            "Sequence":0,
                            "TakerPays":"20",
                            "TakerGets":{{
                                "currency":"USD",
                                "issuer":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value":"10"
                            }},
                            "index":"E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321",
                            "owner_funds":"{}",
                            "quality":"{}"
                        }}
                    ]
                }})",
                kLEDGER_HASH,
                kACCOUNT,
                kPAYS20_XRP_GETS10_USD_BOOK_DIR,
                10,
                2
            )
        },
        BookOffersNormalTestBundle{
            .testName = "PaysXRPGetsUSDTrustLineFrozen",
            .inputJson = paysXRPGetsUSDInputJson,
            // prepare offer dir index
            .mockedSuccessors =
                std::map<ripple::uint256, std::optional<ripple::uint256>>{
                    {getsUSDPaysXRPBook, ripple::uint256{kPAYS20_XRP_GETS10_USD_BOOK_DIR}},
                    {ripple::uint256{kPAYS20_XRP_GETS10_USD_BOOK_DIR}, std::optional<ripple::uint256>{}}
                },
            .mockedLedgerObjects =
                std::map<ripple::uint256, ripple::Blob>{
                    // book dir object
                    {ripple::uint256{kPAYS20_XRP_GETS10_USD_BOOK_DIR},
                     createOwnerDirLedgerObject({ripple::uint256{kINDEX2}}, kINDEX1).getSerializer().peekData()},
                    // gets issuer account object, rate is 1/2
                    {ripple::keylet::account(account).key,
                     createAccountRootObject(kACCOUNT, 0, 2, 200, 2, kINDEX1, 2, kTRANSFER_RATE_X2)
                         .getSerializer()
                         .peekData()},
                    // trust line between gets issuer and owner,owner has 8 USD
                    {ripple::keylet::line(account2, account, ripple::to_currency("USD")).key,
                     frozenTrustLine.getSerializer().peekData()},
                },
            .ledgerObjectCalls = 6,
            .mockedOffers = std::vector<ripple::STObject>{gets10USDPays20XRPOffer},
            .expectedJson = fmt::format(
                R"({{
                    "ledger_hash":"{}",
                    "ledger_index":300,
                    "offers":
                    [
                        {{
                            "Account":"{}",
                            "BookDirectory":"{}",
                            "BookNode":"0",
                            "Flags":0,
                            "LedgerEntryType":"Offer",
                            "OwnerNode":"0",
                            "PreviousTxnID":"0000000000000000000000000000000000000000000000000000000000000000",
                            "PreviousTxnLgrSeq":0,
                            "Sequence":0,
                            "TakerPays":"20",
                            "TakerGets":{{
                                "currency":"USD",
                                "issuer":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value":"10"
                            }},
                            "index":"E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321",
                            "owner_funds":"{}",
                            "quality":"{}",
                            "taker_gets_funded":{{
                                "currency":"USD",
                                "issuer":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "value":"0"
                            }},
                            "taker_pays_funded":"0"
                        }}
                    ]
                }})",
                kLEDGER_HASH,
                kACCOUNT2,
                kPAYS20_XRP_GETS10_USD_BOOK_DIR,
                0,
                2
            )
        },
    };
}

INSTANTIATE_TEST_SUITE_P(
    RPCBookOffersHandler,
    RPCBookOffersNormalPathTest,
    testing::ValuesIn(generateNormalPathBookOffersTestBundles()),
    tests::util::kNAME_GENERATOR
);

// ledger not exist
TEST_F(RPCBookOffersHandlerTest, LedgerNonExistViaIntSequence)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend_, fetchLedgerBySequence(30, _)).WillByDefault(Return(std::optional<ripple::LedgerHeader>{}));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "ledger_index": 30,
            "taker_gets": 
            {{
                "currency": "XRP"
            }},
            "taker_pays": 
            {{
                "currency": "USD",
                "issuer": "{}"
            }}
        }})",
        kACCOUNT
    ));
    auto const handler = AnyHandler{BookOffersHandler{backend_}};
    runSpawn([&](boost::asio::yield_context yield) {
        auto const output = handler.process(kINPUT, Context{.yield = yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCBookOffersHandlerTest, LedgerNonExistViaSequence)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend_, fetchLedgerBySequence(30, _)).WillByDefault(Return(std::optional<ripple::LedgerHeader>{}));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "ledger_index": "30",
            "taker_gets": 
            {{
                "currency": "XRP"
            }},
            "taker_pays": 
            {{
                "currency": "USD",
                "issuer": "{}"
            }}
        }})",
        kACCOUNT
    ));
    auto const handler = AnyHandler{BookOffersHandler{backend_}};
    runSpawn([&](boost::asio::yield_context yield) {
        auto const output = handler.process(kINPUT, Context{.yield = yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCBookOffersHandlerTest, LedgerNonExistViaHash)
{
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    // return empty ledgerHeader
    ON_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _))
        .WillByDefault(Return(std::optional<ripple::LedgerHeader>{}));

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "ledger_hash": "{}",
            "taker_gets": 
            {{
                "currency": "XRP"
            }},
            "taker_pays": 
            {{
                "currency": "USD",
                "issuer": "{}"
            }}
        }})",
        kLEDGER_HASH,
        kACCOUNT
    ));
    auto const handler = AnyHandler{BookOffersHandler{backend_}};
    runSpawn([&](boost::asio::yield_context yield) {
        auto const output = handler.process(kINPUT, Context{.yield = yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCBookOffersHandlerTest, Limit)
{
    auto const seq = 300;

    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // return valid ledgerHeader
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, seq);
    ON_CALL(*backend_, fetchLedgerBySequence(seq, _)).WillByDefault(Return(ledgerHeader));

    auto const issuer = getAccountIdWithString(kACCOUNT);
    // return valid book dir
    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(1);

    auto const getsXRPPaysUSDBook = getBookBase(std::get<ripple::Book>(
        rpc::parseBook(ripple::to_currency("USD"), issuer, ripple::xrpCurrency(), ripple::xrpAccount())
    ));
    ON_CALL(*backend_, doFetchSuccessorKey(getsXRPPaysUSDBook, seq, _))
        .WillByDefault(Return(ripple::uint256{kPAYS20_USD_GETS10_XRP_BOOK_DIR}));

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(5);
    auto const indexes = std::vector<ripple::uint256>(10, ripple::uint256{kINDEX2});

    ON_CALL(*backend_, doFetchLedgerObject(ripple::uint256{kPAYS20_USD_GETS10_XRP_BOOK_DIR}, seq, _))
        .WillByDefault(Return(createOwnerDirLedgerObject(indexes, kINDEX1).getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(getAccountIdWithString(kACCOUNT2)).key, seq, _))
        .WillByDefault(Return(createAccountRootObject(kACCOUNT2, 0, 2, 200, 2, kINDEX1, 2).getSerializer().peekData()));

    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::fees().key, seq, _))
        .WillByDefault(Return(createLegacyFeeSettingBlob(1, 2, 3, 4, 0)));

    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(issuer).key, seq, _))
        .WillByDefault(Return(
            createAccountRootObject(kACCOUNT, 0, 2, 200, 2, kINDEX1, 2, kTRANSFER_RATE_X2).getSerializer().peekData()
        ));

    auto const gets10XRPPays20USDOffer = createOfferLedgerObject(
        kACCOUNT2,
        10,
        20,
        ripple::to_string(ripple::xrpCurrency()),
        ripple::to_string(ripple::to_currency("USD")),
        toBase58(ripple::xrpAccount()),
        kACCOUNT,
        kPAYS20_USD_GETS10_XRP_BOOK_DIR
    );

    std::vector<Blob> const bbs(10, gets10XRPPays20USDOffer.getSerializer().peekData());
    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "taker_gets": 
            {{
                "currency": "XRP"
            }},
            "taker_pays": 
            {{
                "currency": "USD",
                "issuer": "{}"
            }},
            "limit": 5
        }})",
        kACCOUNT
    ));
    auto const handler = AnyHandler{BookOffersHandler{backend_}};
    runSpawn([&](boost::asio::yield_context yield) {
        auto const output = handler.process(kINPUT, Context{.yield = yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value().as_object().at("offers").as_array().size(), 5);
    });
}

TEST_F(RPCBookOffersHandlerTest, LimitMoreThanMax)
{
    auto const seq = 300;

    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    // return valid ledgerHeader
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, seq);
    ON_CALL(*backend_, fetchLedgerBySequence(seq, _)).WillByDefault(Return(ledgerHeader));

    auto const issuer = getAccountIdWithString(kACCOUNT);
    // return valid book dir
    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(1);

    auto const getsXRPPaysUSDBook = getBookBase(std::get<ripple::Book>(
        rpc::parseBook(ripple::to_currency("USD"), issuer, ripple::xrpCurrency(), ripple::xrpAccount())
    ));
    ON_CALL(*backend_, doFetchSuccessorKey(getsXRPPaysUSDBook, seq, _))
        .WillByDefault(Return(ripple::uint256{kPAYS20_USD_GETS10_XRP_BOOK_DIR}));

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(5);
    auto const indexes = std::vector<ripple::uint256>(BookOffersHandler::kLIMIT_MAX + 1, ripple::uint256{kINDEX2});

    ON_CALL(*backend_, doFetchLedgerObject(ripple::uint256{kPAYS20_USD_GETS10_XRP_BOOK_DIR}, seq, _))
        .WillByDefault(Return(createOwnerDirLedgerObject(indexes, kINDEX1).getSerializer().peekData()));
    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(getAccountIdWithString(kACCOUNT2)).key, seq, _))
        .WillByDefault(Return(createAccountRootObject(kACCOUNT2, 0, 2, 200, 2, kINDEX1, 2).getSerializer().peekData()));

    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::fees().key, seq, _))
        .WillByDefault(Return(createLegacyFeeSettingBlob(1, 2, 3, 4, 0)));

    ON_CALL(*backend_, doFetchLedgerObject(ripple::keylet::account(issuer).key, seq, _))
        .WillByDefault(Return(
            createAccountRootObject(kACCOUNT, 0, 2, 200, 2, kINDEX1, 2, kTRANSFER_RATE_X2).getSerializer().peekData()
        ));

    auto const gets10XRPPays20USDOffer = createOfferLedgerObject(
        kACCOUNT2,
        10,
        20,
        ripple::to_string(ripple::xrpCurrency()),
        ripple::to_string(ripple::to_currency("USD")),
        toBase58(ripple::xrpAccount()),
        kACCOUNT,
        kPAYS20_USD_GETS10_XRP_BOOK_DIR
    );

    std::vector<Blob> const bbs(BookOffersHandler::kLIMIT_MAX + 1, gets10XRPPays20USDOffer.getSerializer().peekData());
    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto static const kINPUT = json::parse(fmt::format(
        R"({{
            "taker_gets": 
            {{
                "currency": "XRP"
            }},
            "taker_pays": 
            {{
                "currency": "USD",
                "issuer": "{}"
            }},
            "limit": {}
        }})",
        kACCOUNT,
        BookOffersHandler::kLIMIT_MAX + 1
    ));
    auto const handler = AnyHandler{BookOffersHandler{backend_}};
    runSpawn([&](boost::asio::yield_context yield) {
        auto const output = handler.process(kINPUT, Context{.yield = yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value().as_object().at("offers").as_array().size(), BookOffersHandler::kLIMIT_MAX);
    });
}
