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

#include "util/LoggerFixtures.hpp"
#include "util/log/Logger.hpp"
#include "util/newconfig/Array.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigFileJson.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"
#include "web/dosguard/WhitelistHandler.hpp"

#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

using namespace util;
using namespace util::config;
using namespace web::dosguard;

struct WhitelistHandlerTest : NoLoggerFixture {};

inline static ClioConfigDefinition
getParseWhitelistHandlerConfig(boost::json::value val)
{
    ConfigFileJson const jsonVal{val.as_object()};
    auto config = ClioConfigDefinition{{"dos_guard.whitelist.[]", Array{ConfigValue{ConfigType::String}}}};
    auto const errors = config.parse(jsonVal);
    [&]() { ASSERT_FALSE(errors.has_value()); }();
    return config;
}

TEST_F(WhitelistHandlerTest, TestWhiteListIPV4)
{
    struct MockResolver {
        MOCK_METHOD(std::vector<std::string>, resolve, (std::string_view, std::string_view));
        MOCK_METHOD(std::vector<std::string>, resolve, (std::string_view));
    };

    testing::StrictMock<MockResolver> mockResolver;

    static constexpr auto kJSON_DATA_IP_V4 = R"JSON(
        {
            "dos_guard": {
                "whitelist": [
                    "127.0.0.1",
                    "192.168.0.1/22",
                    "10.0.0.1"
                ]
            }
        }
    )JSON";

    EXPECT_CALL(mockResolver, resolve(testing::_))
        .Times(3)
        .WillRepeatedly([](auto hostname) -> std::vector<std::string> { return {std::string{hostname}}; });

    ClioConfigDefinition const cfg{getParseWhitelistHandlerConfig(boost::json::parse(kJSON_DATA_IP_V4))};
    WhitelistHandler const whitelistHandler{cfg, mockResolver};

    EXPECT_TRUE(whitelistHandler.isWhiteListed("192.168.1.10"));
    EXPECT_FALSE(whitelistHandler.isWhiteListed("193.168.0.123"));
    EXPECT_TRUE(whitelistHandler.isWhiteListed("10.0.0.1"));
    EXPECT_FALSE(whitelistHandler.isWhiteListed("10.0.0.2"));
}

TEST_F(WhitelistHandlerTest, TestWhiteListResolvesHostname)
{
    static constexpr auto kJSON_DATA_IP_V4 = R"JSON(
        {
            "dos_guard": {
                "whitelist": [
                    "localhost",
                    "10.0.0.1"
                ]
            }
        }
    )JSON";

    ClioConfigDefinition const cfg{getParseWhitelistHandlerConfig(boost::json::parse(kJSON_DATA_IP_V4))};
    WhitelistHandler const whitelistHandler{cfg};

    EXPECT_TRUE(whitelistHandler.isWhiteListed("127.0.0.1"));
    EXPECT_FALSE(whitelistHandler.isWhiteListed("193.168.0.123"));
    EXPECT_TRUE(whitelistHandler.isWhiteListed("10.0.0.1"));
    EXPECT_FALSE(whitelistHandler.isWhiteListed("10.0.0.2"));
}

TEST_F(WhitelistHandlerTest, TestWhiteListIPV6)
{
    static constexpr auto kJSON_DATA_IP_V6 = R"JSON(
        {
            "dos_guard": {
                "whitelist": [
                    "2002:1dd8:85a7:0000:0000:8a6e:0000:1111",
                    "2001:0db8:85a3:0000:0000:8a2e:0000:0000/22"
                ]
            }
        }
    )JSON";

    ClioConfigDefinition const cfg{getParseWhitelistHandlerConfig(boost::json::parse(kJSON_DATA_IP_V6))};
    WhitelistHandler const whitelistHandler{cfg};

    EXPECT_TRUE(whitelistHandler.isWhiteListed("2002:1dd8:85a7:0000:0000:8a6e:0000:1111"));
    EXPECT_FALSE(whitelistHandler.isWhiteListed("2002:1dd8:85a7:1101:0000:8a6e:0000:1111"));
    EXPECT_TRUE(whitelistHandler.isWhiteListed("2001:0db8:85a3:0000:0000:8a2e:0000:0000"));
    EXPECT_TRUE(whitelistHandler.isWhiteListed("2001:0db8:85a3:0000:1111:8a2e:0370:7334"));
}
