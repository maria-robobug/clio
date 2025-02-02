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
#include "rpc/common/JsonBool.hpp"
#include "util/NameGenerator.hpp"

#include <boost/json/parse.hpp>
#include <boost/json/value_to.hpp>
#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace rpc;
namespace json = boost::json;
using namespace testing;

struct JsonBoolTestsCaseBundle {
    std::string testName;
    std::string json;
    bool expectedBool;
};

class JsonBoolTests : public TestWithParam<JsonBoolTestsCaseBundle> {
public:
    static auto
    generateTestValuesForParametersTest()
    {
        return std::vector<JsonBoolTestsCaseBundle>{
            {.testName = "NullValue", .json = R"({ "test_bool": null })", .expectedBool = false},
            {.testName = "BoolTrueValue", .json = R"({ "test_bool": true })", .expectedBool = true},
            {.testName = "BoolFalseValue", .json = R"({ "test_bool": false })", .expectedBool = false},
            {.testName = "IntTrueValue", .json = R"({ "test_bool": 1 })", .expectedBool = true},
            {.testName = "IntFalseValue", .json = R"({ "test_bool": 0 })", .expectedBool = false},
            {.testName = "DoubleTrueValue", .json = R"({ "test_bool": 0.1 })", .expectedBool = true},
            {.testName = "DoubleFalseValue", .json = R"({ "test_bool": 0.0 })", .expectedBool = false},
            {.testName = "StringTrueValue", .json = R"({ "test_bool": "true" })", .expectedBool = true},
            {.testName = "StringFalseValue", .json = R"({ "test_bool": "false" })", .expectedBool = true},
            {.testName = "ArrayTrueValue", .json = R"({ "test_bool": [0] })", .expectedBool = true},
            {.testName = "ArrayFalseValue", .json = R"({ "test_bool": [] })", .expectedBool = false},
            {.testName = "ObjectTrueValue", .json = R"({ "test_bool": { "key": null } })", .expectedBool = true},
            {.testName = "ObjectFalseValue", .json = R"({ "test_bool": {} })", .expectedBool = false}
        };
    }
};

INSTANTIATE_TEST_CASE_P(
    JsonBoolCheckGroup,
    JsonBoolTests,
    ValuesIn(JsonBoolTests::generateTestValuesForParametersTest()),
    tests::util::kNAME_GENERATOR
);

TEST_P(JsonBoolTests, Parse)
{
    auto const testBundle = GetParam();
    auto const jv = json::parse(testBundle.json).as_object();
    ASSERT_TRUE(jv.contains("test_bool"));
    EXPECT_EQ(testBundle.expectedBool, value_to<JsonBool>(jv.at("test_bool")).value);
}
