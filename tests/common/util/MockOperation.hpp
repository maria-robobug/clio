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

#pragma once

#include <gmock/gmock.h>

template <typename ValueType>
struct MockOperation {
    MOCK_METHOD(void, wait, (), (const));
    MOCK_METHOD(ValueType, get, (), (const));
};

template <typename ValueType>
struct MockStoppableOperation {
    MOCK_METHOD(void, requestStop, (), (const));
    MOCK_METHOD(void, wait, (), (const));
    MOCK_METHOD(ValueType, get, (), (const));
};

template <typename ValueType>
struct MockScheduledOperation {
    MOCK_METHOD(void, cancel, (), (const));
    MOCK_METHOD(void, requestStop, (), (const));
    MOCK_METHOD(void, wait, (), (const));
    MOCK_METHOD(ValueType, get, (), (const));
    MOCK_METHOD(void, getToken, (), (const));
};

template <typename ValueType>
struct MockRepeatingOperation {
    MOCK_METHOD(void, requestStop, (), (const));
    MOCK_METHOD(void, wait, (), (const));
    MOCK_METHOD(ValueType, get, (), (const));
};
