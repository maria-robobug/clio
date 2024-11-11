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

#include "rpc/JS.hpp"
#include "util/LedgerUtils.hpp"

#include <gtest/gtest.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/jss.h>

#include <algorithm>
#include <iterator>
#include <string_view>

TEST(LedgerUtilsTests, LedgerObjectTypeList)
{
    auto constexpr types = util::LedgerTypes::GetLedgerEntryTypeStrList();
    static constexpr char const* typesList[] = {
        JS(account),
        JS(amendments),
        JS(check),
        JS(deposit_preauth),
        JS(directory),
        JS(escrow),
        JS(fee),
        JS(hashes),
        JS(offer),
        JS(payment_channel),
        JS(signer_list),
        JS(state),
        JS(ticket),
        JS(nft_offer),
        JS(nft_page),
        JS(amm),
        JS(bridge),
        JS(xchain_owned_claim_id),
        JS(xchain_owned_create_account_claim_id),
        JS(did),
        JS(mpt_issuance),
        JS(mptoken),
        JS(oracle),
        JS(nunl)
    };

    static_assert(std::size(typesList) == types.size());

    static_assert(std::all_of(std::cbegin(typesList), std::cend(typesList), [&types](std::string_view type) {
        return std::find(std::cbegin(types), std::cend(types), type) != std::cend(types);
    }));
}

TEST(LedgerUtilsTests, AccountOwnedTypeList)
{
    auto constexpr accountOwned = util::LedgerTypes::GetAccountOwnedLedgerTypeStrList();
    static constexpr char const* correctTypes[] = {
        JS(account),
        JS(check),
        JS(deposit_preauth),
        JS(escrow),
        JS(offer),
        JS(payment_channel),
        JS(signer_list),
        JS(state),
        JS(ticket),
        JS(nft_offer),
        JS(nft_page),
        JS(amm),
        JS(bridge),
        JS(xchain_owned_claim_id),
        JS(xchain_owned_create_account_claim_id),
        JS(did),
        JS(oracle),
        JS(mpt_issuance),
        JS(mptoken)
    };

    static_assert(std::size(correctTypes) == accountOwned.size());

    static_assert(std::all_of(
        std::cbegin(correctTypes),
        std::cend(correctTypes),
        [&accountOwned](std::string_view type) {
            return std::find(std::cbegin(accountOwned), std::cend(accountOwned), type) != std::cend(accountOwned);
        }
    ));
}

TEST(LedgerUtilsTests, StrToType)
{
    EXPECT_EQ(util::LedgerTypes::GetLedgerEntryTypeFromStr("mess"), ripple::ltANY);
    EXPECT_EQ(util::LedgerTypes::GetLedgerEntryTypeFromStr("tomato"), ripple::ltANY);
    EXPECT_EQ(util::LedgerTypes::GetLedgerEntryTypeFromStr("account"), ripple::ltACCOUNT_ROOT);

    auto constexpr types = util::LedgerTypes::GetLedgerEntryTypeStrList();
    std::for_each(types.cbegin(), types.cend(), [](auto const& typeStr) {
        EXPECT_NE(util::LedgerTypes::GetLedgerEntryTypeFromStr(typeStr), ripple::ltANY);
    });
}

TEST(LedgerUtilsTests, DeletionBlockerTypes)
{
    auto constexpr testedTypes = util::LedgerTypes::GetDeletionBlockerLedgerTypes();

    static ripple::LedgerEntryType constexpr deletionBlockers[] = {
        ripple::ltCHECK,
        ripple::ltESCROW,
        ripple::ltNFTOKEN_PAGE,
        ripple::ltPAYCHAN,
        ripple::ltRIPPLE_STATE,
        ripple::ltXCHAIN_OWNED_CLAIM_ID,
        ripple::ltXCHAIN_OWNED_CREATE_ACCOUNT_CLAIM_ID,
        ripple::ltBRIDGE,
        ripple::ltMPTOKEN_ISSUANCE,
        ripple::ltMPTOKEN
    };

    static_assert(std::size(deletionBlockers) == testedTypes.size());
    static_assert(std::any_of(testedTypes.cbegin(), testedTypes.cend(), [](auto const& type) {
        return std::find(std::cbegin(deletionBlockers), std::cend(deletionBlockers), type) !=
            std::cend(deletionBlockers);
    }));
}
