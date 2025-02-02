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

#include "rpc/handlers/Unsubscribe.hpp"

#include "feed/SubscriptionManagerInterface.hpp"
#include "feed/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Checkers.hpp"
#include "rpc/common/Specs.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/common/Validators.hpp"
#include "util/Assert.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <xrpl/protocol/Book.h>
#include <xrpl/protocol/jss.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace rpc {

UnsubscribeHandler::UnsubscribeHandler(std::shared_ptr<feed::SubscriptionManagerInterface> const& subscriptions)
    : subscriptions_(subscriptions)
{
}

RpcSpecConstRef
UnsubscribeHandler::spec([[maybe_unused]] uint32_t apiVersion)
{
    static auto const kBOOKS_VALIDATOR =
        validation::CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
            if (!value.is_array())
                return Error{Status{RippledError::rpcINVALID_PARAMS, std::string(key) + "NotArray"}};

            for (auto const& book : value.as_array()) {
                if (!book.is_object())
                    return Error{Status{RippledError::rpcINVALID_PARAMS, std::string(key) + "ItemNotObject"}};

                if (book.as_object().contains("both") && !book.as_object().at("both").is_bool())
                    return Error{Status{RippledError::rpcINVALID_PARAMS, "bothNotBool"}};

                auto const parsedBook = parseBook(book.as_object());
                if (auto const status = std::get_if<Status>(&parsedBook))
                    return Error(*status);
            }

            return MaybeError{};
        }};

    static auto const kRPC_SPEC = RpcSpec{
        {JS(streams), validation::CustomValidators::subscribeStreamValidator},
        {JS(accounts), validation::CustomValidators::subscribeAccountsValidator},
        {JS(accounts_proposed), validation::CustomValidators::subscribeAccountsValidator},
        {JS(books), kBOOKS_VALIDATOR},
        {JS(url), check::Deprecated{}},
        {JS(rt_accounts), check::Deprecated{}},
        {"rt_transactions", check::Deprecated{}},
    };

    return kRPC_SPEC;
}

UnsubscribeHandler::Result
UnsubscribeHandler::process(Input input, Context const& ctx) const
{
    if (input.streams)
        unsubscribeFromStreams(*(input.streams), ctx.session);

    if (input.accounts)
        unsubscribeFromAccounts(*(input.accounts), ctx.session);

    if (input.accountsProposed)
        unsubscribeFromProposedAccounts(*(input.accountsProposed), ctx.session);

    if (input.books)
        unsubscribeFromBooks(*(input.books), ctx.session);

    return Output{};
}

void
UnsubscribeHandler::unsubscribeFromStreams(
    std::vector<std::string> const& streams,
    feed::SubscriberSharedPtr const& session
) const
{
    for (auto const& stream : streams) {
        if (stream == "ledger") {
            subscriptions_->unsubLedger(session);
        } else if (stream == "transactions") {
            subscriptions_->unsubTransactions(session);
        } else if (stream == "transactions_proposed") {
            subscriptions_->unsubProposedTransactions(session);
        } else if (stream == "validations") {
            subscriptions_->unsubValidation(session);
        } else if (stream == "manifests") {
            subscriptions_->unsubManifest(session);
        } else if (stream == "book_changes") {
            subscriptions_->unsubBookChanges(session);
        } else {
            ASSERT(false, "Unknown stream: {}", stream);
        }
    }
}

void
UnsubscribeHandler::unsubscribeFromAccounts(std::vector<std::string> accounts, feed::SubscriberSharedPtr const& session)
    const
{
    for (auto const& account : accounts) {
        auto const accountID = accountFromStringStrict(account);
        subscriptions_->unsubAccount(*accountID, session);
    }
}

void
UnsubscribeHandler::unsubscribeFromProposedAccounts(
    std::vector<std::string> accountsProposed,
    feed::SubscriberSharedPtr const& session
) const
{
    for (auto const& account : accountsProposed) {
        auto const accountID = accountFromStringStrict(account);
        subscriptions_->unsubProposedAccount(*accountID, session);
    }
}
void
UnsubscribeHandler::unsubscribeFromBooks(std::vector<OrderBook> const& books, feed::SubscriberSharedPtr const& session)
    const
{
    for (auto const& orderBook : books) {
        subscriptions_->unsubBook(orderBook.book, session);

        if (orderBook.both)
            subscriptions_->unsubBook(ripple::reversed(orderBook.book), session);
    }
}

UnsubscribeHandler::Input
tag_invoke(boost::json::value_to_tag<UnsubscribeHandler::Input>, boost::json::value const& jv)
{
    auto input = UnsubscribeHandler::Input{};
    auto const& jsonObject = jv.as_object();

    if (auto const& streams = jsonObject.find(JS(streams)); streams != jsonObject.end()) {
        input.streams = std::vector<std::string>();
        for (auto const& stream : streams->value().as_array())
            input.streams->push_back(boost::json::value_to<std::string>(stream));
    }
    if (auto const& accounts = jsonObject.find(JS(accounts)); accounts != jsonObject.end()) {
        input.accounts = std::vector<std::string>();
        for (auto const& account : accounts->value().as_array())
            input.accounts->push_back(boost::json::value_to<std::string>(account));
    }
    if (auto const& accountsProposed = jsonObject.find(JS(accounts_proposed)); accountsProposed != jsonObject.end()) {
        input.accountsProposed = std::vector<std::string>();
        for (auto const& account : accountsProposed->value().as_array())
            input.accountsProposed->push_back(boost::json::value_to<std::string>(account));
    }
    if (auto const& books = jsonObject.find(JS(books)); books != jsonObject.end()) {
        input.books = std::vector<UnsubscribeHandler::OrderBook>();
        for (auto const& book : books->value().as_array()) {
            auto internalBook = UnsubscribeHandler::OrderBook{};
            auto const& bookObject = book.as_object();

            if (auto const& both = bookObject.find(JS(both)); both != bookObject.end())
                internalBook.both = both->value().as_bool();

            auto const parsedBookMaybe = parseBook(book.as_object());
            internalBook.book = std::get<ripple::Book>(parsedBookMaybe);
            input.books->push_back(internalBook);
        }
    }

    return input;
}
}  // namespace rpc
