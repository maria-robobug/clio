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

#include "migration/impl/MigrationManagerFactory.hpp"

#include "data/cassandra/SettingsProvider.hpp"
#include "migration/MigrationManagerInterface.hpp"
#include "migration/cassandra/CassandraMigrationBackend.hpp"
#include "migration/cassandra/CassandraMigrationManager.hpp"
#include "util/log/Logger.hpp"
#include "util/newconfig/ConfigDefinition.hpp"

#include <boost/algorithm/string/predicate.hpp>

#include <memory>
#include <string>
#include <utility>

namespace migration::impl {

std::expected<std::shared_ptr<MigrationManagerInterface>, std::string>
makeMigrationManager(util::config::ClioConfigDefinition const& config)
{
    static util::Logger const log{"Migration"};  // NOLINT(readability-identifier-naming)
    LOG(log.info()) << "Constructing MigrationManager";

    auto const type = config.get<std::string>("database.type");

    if (not boost::iequals(type, "cassandra")) {
        LOG(log.error()) << "Unknown database type to migrate: " << type;
        return std::unexpected(std::string("Invalid database type"));
    }

    auto const cfg = config.getObject("database." + type);

    auto migrationCfg = config.getObject("migration");

    return std::make_shared<cassandra::CassandraMigrationManager>(
        std::make_shared<cassandra::CassandraMigrationBackend>(data::cassandra::SettingsProvider{cfg}),
        std::move(migrationCfg)
    );
}

}  // namespace migration::impl
