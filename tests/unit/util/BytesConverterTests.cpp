//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

#include "util/BytesConverter.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

using namespace util;

TEST(MBToBytesTest, SimpleValues)
{
    EXPECT_EQ(mbToBytes(0), 0);
    EXPECT_EQ(mbToBytes(1), 1024 * 1024);
    EXPECT_EQ(mbToBytes(2), 2 * 1024 * 1024);
}

TEST(MBToBytesTest, LimitValues)
{
    auto const maxNum = std::numeric_limits<std::uint32_t>::max();
    EXPECT_NE(mbToBytes(maxNum), maxNum * 1024 * 1024);
    EXPECT_EQ(mbToBytes(maxNum), maxNum * 1024ul * 1024ul);
}
