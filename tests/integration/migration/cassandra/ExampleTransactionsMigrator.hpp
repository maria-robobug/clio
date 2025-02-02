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

#include "migration/cassandra/CassandraMigrationTestBackend.hpp"
#include "util/newconfig/ObjectView.hpp"

#include <cstdint>
#include <memory>

/**
 * @brief Example migrator for the transactions table. In this example, we show how to traverse the transactions table
 * and migrate the data to index table. We create an index table for transaction hash to transaction type string.
 */
struct ExampleTransactionsMigrator {
    static constexpr char const* kNAME = "ExampleTransactionsMigrator";
    static constexpr char const* kDESCRIPTION = "The migrator for transactions table";

    using Backend = CassandraMigrationTestBackend;
    static std::uint64_t count;

    static void
    runMigration(std::shared_ptr<Backend> const& backend, util::config::ObjectView const& config);
};
