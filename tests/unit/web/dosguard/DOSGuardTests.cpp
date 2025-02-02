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

#include "util/LoggerFixtures.hpp"
#include "util/newconfig/Array.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"
#include "web/dosguard/DOSGuard.hpp"
#include "web/dosguard/WhitelistHandlerInterface.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string_view>

using namespace testing;
using namespace util;
using namespace std;
using namespace util::config;
using namespace web::dosguard;

struct DOSGuardTest : NoLoggerFixture {
    static constexpr auto kJSON_DATA = R"JSON({
        "dos_guard": {
            "max_fetches": 100,
            "max_connections": 2,
            "max_requests": 3,
            "whitelist": [
                "127.0.0.1"
            ]
        }
    })JSON";

    static constexpr auto kIP = "127.0.0.2";

    struct MockWhitelistHandler : WhitelistHandlerInterface {
        MOCK_METHOD(bool, isWhiteListed, (std::string_view ip), (const));
    };

    ClioConfigDefinition cfg{
        {{"dos_guard.max_fetches", ConfigValue{ConfigType::Integer}.defaultValue(100)},
         {"dos_guard.max_connections", ConfigValue{ConfigType::Integer}.defaultValue(2)},
         {"dos_guard.max_requests", ConfigValue{ConfigType::Integer}.defaultValue(3)},
         {"dos_guard.whitelist", Array{ConfigValue{ConfigType::String}}}}
    };
    NiceMock<MockWhitelistHandler> whitelistHandler;
    DOSGuard guard{cfg, whitelistHandler};
};

TEST_F(DOSGuardTest, Whitelisting)
{
    EXPECT_CALL(whitelistHandler, isWhiteListed("127.0.0.1")).Times(1).WillOnce(Return(false));
    EXPECT_FALSE(guard.isWhiteListed("127.0.0.1"));
    EXPECT_CALL(whitelistHandler, isWhiteListed("127.0.0.1")).Times(1).WillOnce(Return(true));
    EXPECT_TRUE(guard.isWhiteListed("127.0.0.1"));
}

TEST_F(DOSGuardTest, ConnectionCount)
{
    EXPECT_TRUE(guard.isOk(kIP));
    guard.increment(kIP);  // one connection
    EXPECT_TRUE(guard.isOk(kIP));
    guard.increment(kIP);  // two connections
    EXPECT_TRUE(guard.isOk(kIP));
    guard.increment(kIP);  // > two connections, can't connect more
    EXPECT_FALSE(guard.isOk(kIP));

    guard.decrement(kIP);
    EXPECT_TRUE(guard.isOk(kIP));  // can connect again
}

TEST_F(DOSGuardTest, FetchCount)
{
    EXPECT_TRUE(guard.add(kIP, 50));  // half of allowence
    EXPECT_TRUE(guard.add(kIP, 50));  // now fully charged
    EXPECT_FALSE(guard.add(kIP, 1));  // can't add even 1 anymore
    EXPECT_FALSE(guard.isOk(kIP));

    guard.clear();                 // force clear the above fetch count
    EXPECT_TRUE(guard.isOk(kIP));  // can fetch again
}

TEST_F(DOSGuardTest, ClearFetchCountOnTimer)
{
    EXPECT_TRUE(guard.add(kIP, 50));  // half of allowence
    EXPECT_TRUE(guard.add(kIP, 50));  // now fully charged
    EXPECT_FALSE(guard.add(kIP, 1));  // can't add even 1 anymore
    EXPECT_FALSE(guard.isOk(kIP));

    guard.clear();                 // pretend sweep called from timer
    EXPECT_TRUE(guard.isOk(kIP));  // can fetch again
}

TEST_F(DOSGuardTest, RequestLimit)
{
    EXPECT_TRUE(guard.request(kIP));
    EXPECT_TRUE(guard.request(kIP));
    EXPECT_TRUE(guard.request(kIP));
    EXPECT_TRUE(guard.isOk(kIP));
    EXPECT_FALSE(guard.request(kIP));
    EXPECT_FALSE(guard.isOk(kIP));
    guard.clear();
    EXPECT_TRUE(guard.isOk(kIP));  // can request again
}

TEST_F(DOSGuardTest, RequestLimitOnTimer)
{
    EXPECT_TRUE(guard.request(kIP));
    EXPECT_TRUE(guard.request(kIP));
    EXPECT_TRUE(guard.request(kIP));
    EXPECT_TRUE(guard.isOk(kIP));
    EXPECT_FALSE(guard.request(kIP));
    EXPECT_FALSE(guard.isOk(kIP));
    guard.clear();
    EXPECT_TRUE(guard.isOk(kIP));  // can request again
}
