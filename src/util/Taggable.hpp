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

#pragma once

#include "util/Assert.hpp"
#include "util/newconfig/ConfigDefinition.hpp"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/json.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

namespace util {
namespace impl {

/**
 * @brief A `null` tag generator - does nothing.
 */
struct NullTagGenerator final {};

/**
 * @brief This strategy uses an `atomic_uint64_t` to remain lock free.
 */
struct UIntTagGenerator final {
    using TagType = std::atomic_uint64_t;

    static TagType
    next();
};

/**
 * @brief This strategy uses `boost::uuids::uuid` with a static random generator and a mutex.
 */
struct UUIDTagGenerator final {
    using TagType = boost::uuids::uuid;

    static TagType
    next();
};

}  // namespace impl

/**
 * @brief Represents any tag decorator.
 */
class BaseTagDecorator {
public:
    virtual ~BaseTagDecorator() = default;

    /**
     * @brief Decorates a std::ostream.
     *
     * @param os The stream to decorate
     */
    virtual void
    decorate(std::ostream& os) const = 0;

    /**
     * @brief Support for decorating streams (boost log, cout, etc.).
     *
     * @param os The stream
     * @param decorator The decorator
     * @return The same stream that we were given
     */
    friend std::ostream&
    operator<<(std::ostream& os, BaseTagDecorator const& decorator)
    {
        decorator.decorate(os);
        return os;
    }
};

/**
 * @brief A decorator that decorates a string (log line) with a unique tag.
 *
 * @tparam Generator The strategy used to generate the tag.
 */
template <typename Generator>
class TagDecorator final : public BaseTagDecorator {
    using ParentType = std::optional<std::reference_wrapper<BaseTagDecorator const>>;
    using TagType = typename Generator::TagType;

    ParentType parent_ = std::nullopt;
    TagType tag_ = Generator::next();

public:
    /**
     * @brief Create a new tag decorator with an optional parent.
     *
     * If the `parent` is specified it will be streamed out as a chain when this decorator will decorate an ostream.
     *
     * Note that if `parent` is specified it is your responsibility that the decorator referred to by `parent` outlives
     * this decorator.
     *
     * @param parent An optional parent tag decorator
     */
    explicit TagDecorator(ParentType parent = std::nullopt) : parent_{parent}
    {
    }

    /**
     * @brief Implementation of the decoration. Chaining tags when parent is available.
     *
     * @param os The stream to output into
     */
    void
    decorate(std::ostream& os) const override
    {
        os << "[";

        if (parent_.has_value())
            (*parent_).get().decorate(os);

        os << tag_ << "] ";
    }
};

/**
 * @brief Specialization for a nop/null decorator.
 *
 * This generates a pass-thru decorate member function which can be optimized away by the compiler.
 */
template <>
class TagDecorator<impl::NullTagGenerator> final : public BaseTagDecorator {
public:
    /**
     * @brief Nop implementation for the decorator.
     *
     * @param os The stream
     */
    void
    decorate([[maybe_unused]] std::ostream& os) const override
    {
        // nop
    }
};

/**
 * @brief A factory for TagDecorator instantiation.
 */
class TagDecoratorFactory final {
    using ParentType = std::optional<std::reference_wrapper<BaseTagDecorator const>>;

    /**
     * @brief Represents the type of tag decorator.
     */
    enum class Type {
        NONE, /**< No decoration and no tag */
        UUID, /**< Tag based on `boost::uuids::uuid`, thread-safe via mutex */
        UINT  /**< atomic_uint64_t tag, thread-safe, lock-free */
    };

    Type type_;                        /*< The type of TagDecorator this factory produces */
    ParentType parent_ = std::nullopt; /*< The parent tag decorator to bind */

    static Type
    getLogTagType(std::string_view style)
    {
        if (boost::iequals(style, "int") || boost::iequals(style, "uint"))
            return TagDecoratorFactory::Type::UINT;

        if (boost::iequals(style, "null") || boost::iequals(style, "none"))
            return TagDecoratorFactory::Type::NONE;

        if (boost::iequals(style, "uuid"))
            return TagDecoratorFactory::Type::UUID;

        ASSERT(false, "log_tag_style does not have valid value");
        std::unreachable();
    }

public:
    ~TagDecoratorFactory() = default;

    /**
     * @brief Instantiates a tag decorator factory from `clio` configuration.
     *
     * @param config The configuration as a json object
     */
    explicit TagDecoratorFactory(util::config::ClioConfigDefinition const& config)
        : type_{getLogTagType(config.get<std::string>("log_tag_style"))}
    {
    }

private:
    TagDecoratorFactory(Type type, ParentType parent) noexcept : type_{type}, parent_{parent}
    {
    }

public:
    /**
     * @brief Instantiates the TagDecorator specified by `type_` with parent bound from `parent_`.
     *
     * @return An instance of the requested decorator
     */
    std::unique_ptr<BaseTagDecorator>
    make() const;

    /**
     * @brief Creates a new tag decorator factory with a bound parent tag decorator.
     *
     * @param parent The parent tag decorator to use
     * @return A new instance of the tag decorator factory
     */
    TagDecoratorFactory
    with(ParentType parent) const noexcept;
};

/**
 * @brief A base class that allows attaching a tag decorator to a subclass.
 */
class Taggable {
    using DecoratorType = std::unique_ptr<BaseTagDecorator>;
    DecoratorType tagDecorator_;

protected:
    /**
     * @brief New Taggable from a specified factory.
     *
     * @param tagFactory The factory to use
     */
    explicit Taggable(util::TagDecoratorFactory const& tagFactory) : tagDecorator_{tagFactory.make()}
    {
    }

public:
    virtual ~Taggable() = default;
    Taggable(Taggable&&) = default;

    Taggable&
    operator=(Taggable&&) = default;

    /**
     * @brief Getter for tag decorator.
     *
     * @return Reference to the tag decorator
     */
    BaseTagDecorator const&
    tag() const
    {
        return *tagDecorator_;
    }
};

}  // namespace util
