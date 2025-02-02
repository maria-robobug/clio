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

#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/VersionHandler.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/log/Logger.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"

#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <fmt/core.h>
#include <gtest/gtest.h>

namespace {

constexpr auto kDEFAULT_API_VERSION = 3u;
constexpr auto kMIN_API_VERSION = 2u;
constexpr auto kMAX_API_VERSION = 10u;

}  // namespace

using namespace rpc;
using namespace util::config;

class RPCVersionHandlerTest : public HandlerBaseTest {};

TEST_F(RPCVersionHandlerTest, Default)
{
    ClioConfigDefinition cfg{
        {"api_version.min", ConfigValue{ConfigType::Integer}.defaultValue(kMIN_API_VERSION)},
        {"api_version.max", ConfigValue{ConfigType::Integer}.defaultValue(kMAX_API_VERSION)},
        {"api_version.default", ConfigValue{ConfigType::Integer}.defaultValue(kDEFAULT_API_VERSION)}
    };

    boost::json::value jsonData = boost::json::parse(fmt::format(
        R"({{
            "api_version.min": {},
            "api_version.max": {},
            "api_version.default": {}
        }})",
        kMIN_API_VERSION,
        kMAX_API_VERSION,
        kDEFAULT_API_VERSION
    ));

    runSpawn([&](auto yield) {
        auto const handler = AnyHandler{VersionHandler{cfg}};
        auto const output = handler.process(jsonData, Context{yield});
        ASSERT_TRUE(output);

        // check all against all the correct values
        auto const& result = output.result.value().as_object();
        EXPECT_TRUE(result.contains("version"));
        auto const& info = result.at("version").as_object();
        EXPECT_TRUE(info.contains("first"));
        EXPECT_TRUE(info.contains("last"));
        EXPECT_TRUE(info.contains("good"));
        EXPECT_EQ(info.at("first"), 2u);
        EXPECT_EQ(info.at("last"), 10u);
        EXPECT_EQ(info.at("good"), 3u);
    });
}
