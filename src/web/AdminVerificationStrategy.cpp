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

#include "web/AdminVerificationStrategy.hpp"

#include "util/JsonUtils.hpp"
#include "util/newconfig/ConfigDefinition.hpp"

#include <boost/beast/http/field.hpp>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/digest.h>

#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace web {

bool
IPAdminVerificationStrategy::isAdmin(RequestHeader const&, std::string_view ip) const
{
    return ip == "127.0.0.1";
}

PasswordAdminVerificationStrategy::PasswordAdminVerificationStrategy(std::string const& password)
{
    ripple::sha256_hasher hasher;
    hasher(password.data(), password.size());
    auto const d = static_cast<ripple::sha256_hasher::result_type>(hasher);
    ripple::uint256 sha256;
    std::memcpy(sha256.data(), d.data(), d.size());
    passwordSha256_ = ripple::to_string(sha256);
    // make sure it's uppercase
    passwordSha256_ = util::toUpper(std::move(passwordSha256_));
}

bool
PasswordAdminVerificationStrategy::isAdmin(RequestHeader const& request, std::string_view) const
{
    auto it = request.find(boost::beast::http::field::authorization);
    if (it == request.end()) {
        // No Authorization header
        return false;
    }
    auto userAuth = it->value();
    if (!userAuth.starts_with(kPASSWORD_PREFIX)) {
        // Invalid Authorization header
        return false;
    }

    userAuth.remove_prefix(kPASSWORD_PREFIX.size());
    return passwordSha256_ == util::toUpper(userAuth);
}

std::shared_ptr<AdminVerificationStrategy>
makeAdminVerificationStrategy(std::optional<std::string> password)
{
    if (password.has_value()) {
        return std::make_shared<PasswordAdminVerificationStrategy>(std::move(*password));
    }
    return std::make_shared<IPAdminVerificationStrategy>();
}

std::expected<std::shared_ptr<AdminVerificationStrategy>, std::string>
makeAdminVerificationStrategy(util::config::ClioConfigDefinition const& config)
{
    auto adminPassword = config.maybeValue<std::string>("server.admin_password");
    auto const localAdmin = config.maybeValue<bool>("server.local_admin");

    if (adminPassword.has_value() and localAdmin.has_value() and *localAdmin)
        return std::unexpected{"Admin config error: 'local_admin' and admin_password can not be set together."};

    if (localAdmin.has_value() and !*localAdmin and !adminPassword.has_value()) {
        return std::unexpected{
            "Admin config error: either 'local_admin' should be enabled or 'admin_password' must be specified."
        };
    }

    return makeAdminVerificationStrategy(std::move(adminPassword));
}

}  // namespace web
