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

#include "util/config/Config.hpp"

#include "util/Assert.hpp"
#include "util/Constants.hpp"
#include "util/config/impl/Helpers.hpp"
#include "util/log/Logger.hpp"

#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/parse_options.hpp>
#include <boost/json/value.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <ios>
#include <iterator>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace util {

// Note: `store_(store)` MUST use `()` instead of `{}` otherwise gcc
// picks `initializer_list` constructor and anything passed becomes an
// array :-D
Config::Config(boost::json::value store) : store_(std::move(store))
{
}

Config::operator bool() const noexcept
{
    return not store_.is_null();
}

bool
Config::contains(KeyType key) const
{
    return lookup(key).has_value();
}

std::optional<boost::json::value>
Config::lookup(KeyType key) const
{
    if (store_.is_null())
        return std::nullopt;

    std::reference_wrapper<boost::json::value const> cur = std::cref(store_);
    auto hasBrokenPath = false;
    auto tokenized = impl::Tokenizer<KeyType, kSEPARATOR>{key};
    std::string subkey{};

    auto maybeSection = tokenized.next();
    while (maybeSection.has_value()) {
        auto section = maybeSection.value();
        subkey += section;

        if (not hasBrokenPath) {
            if (not cur.get().is_object())
                throw impl::StoreException("Not an object at '" + subkey + "'");
            if (not cur.get().as_object().contains(section)) {
                hasBrokenPath = true;
            } else {
                cur = std::cref(cur.get().as_object().at(section));
            }
        }

        subkey += kSEPARATOR;
        maybeSection = tokenized.next();
    }

    if (hasBrokenPath)
        return std::nullopt;
    return std::make_optional(cur);
}

std::optional<Config::ArrayType>
Config::maybeArray(KeyType key) const
{
    try {
        auto maybeArr = lookup(key);
        if (maybeArr && maybeArr->is_array()) {
            auto& arr = maybeArr->as_array();
            ArrayType out;
            out.reserve(arr.size());

            std::ranges::transform(arr, std::back_inserter(out), [](auto&& element) {
                return Config{std::forward<decltype(element)>(element)};
            });
            return std::make_optional<ArrayType>(std::move(out));
        }
    } catch (impl::StoreException const&) {  // NOLINT(bugprone-empty-catch)
        // ignore store error, but rethrow key errors
    }

    return std::nullopt;
}

Config::ArrayType
Config::array(KeyType key) const
{
    if (auto maybeArr = maybeArray(key); maybeArr)
        return maybeArr.value();
    throw std::logic_error("No array found at '" + key + "'");
}

Config::ArrayType
Config::arrayOr(KeyType key, ArrayType fallback) const
{
    if (auto maybeArr = maybeArray(key); maybeArr)
        return maybeArr.value();
    return fallback;
}

Config::ArrayType
Config::arrayOrThrow(KeyType key, std::string_view err) const
{
    try {
        return maybeArray(key).value();
    } catch (std::exception const&) {
        throw std::runtime_error(std::string{err});
    }
}

Config
Config::section(KeyType key) const
{
    auto maybeElement = lookup(key);
    if (maybeElement && maybeElement->is_object())
        return Config{std::move(*maybeElement)};
    throw std::logic_error("No section found at '" + key + "'");
}

Config
Config::sectionOr(KeyType key, boost::json::object fallback) const
{
    auto maybeElement = lookup(key);
    if (maybeElement && maybeElement->is_object())
        return Config{std::move(*maybeElement)};
    return Config{std::move(fallback)};
}

Config::ArrayType
Config::array() const
{
    if (not store_.is_array())
        throw std::logic_error("_self_ is not an array");

    ArrayType out;
    auto const& arr = store_.as_array();
    out.reserve(arr.size());

    std::ranges::transform(arr, std::back_inserter(out), [](auto const& element) { return Config{element}; });
    return out;
}

std::chrono::milliseconds
Config::toMilliseconds(float value)
{
    ASSERT(value >= 0.0f, "Floating point value of seconds must be non-negative, got: {}", value);
    return std::chrono::milliseconds{std::lroundf(value * static_cast<float>(util::kMILLISECONDS_PER_SECOND))};
}

Config
ConfigReader::open(std::filesystem::path path)
{
    try {
        std::ifstream const in(path, std::ios::in | std::ios::binary);
        if (in) {
            std::stringstream contents;
            contents << in.rdbuf();
            auto opts = boost::json::parse_options{};
            opts.allow_comments = true;
            return Config{boost::json::parse(contents.str(), {}, opts)};
        }
    } catch (std::exception const& e) {
        LOG(util::LogService::error()) << "Could not read configuration file from '" << path.string()
                                       << "': " << e.what();
    }

    return Config{};
}

}  // namespace util
