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

#include "util/BinaryTestObject.hpp"

#include "etlng/Models.hpp"
#include "etlng/impl/Extraction.hpp"
#include "util/StringUtils.hpp"

#include <gtest/gtest.h>
#include <org/xrpl/rpc/v1/ledger.pb.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/proto/org/xrpl/rpc/v1/get_ledger.pb.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/TxFormats.h>
#include <xrpl/protocol/TxMeta.h>

#include <string>
#include <utility>

namespace {

constinit auto const kSEQ = 30;

constinit auto const kTXN_HEX =
    "1200192200000008240011CC9B201B001F71D6202A0000000168400000"
    "000000000C7321ED475D1452031E8F9641AF1631519A58F7B8681E172E"
    "4838AA0E59408ADA1727DD74406960041F34F10E0CBB39444B4D4E577F"
    "C0B7E8D843D091C2917E96E7EE0E08B30C91413EC551A2B8A1D405E8BA"
    "34FE185D8B10C53B40928611F2DE3B746F0303751868747470733A2F2F"
    "677265677765697362726F642E636F6D81146203F49C21D5D6E022CB16"
    "DE3538F248662FC73C";

constinit auto const kTXN_META =
    "201C00000001F8E511005025001F71B3556ED9C9459001E4F4A9121F4E"
    "07AB6D14898A5BBEF13D85C25D743540DB59F3CF566203F49C21D5D6E0"
    "22CB16DE3538F248662FC73CFFFFFFFFFFFFFFFFFFFFFFFFE6FAEC5A00"
    "0800006203F49C21D5D6E022CB16DE3538F248662FC73C8962EFA00000"
    "0006751868747470733A2F2F677265677765697362726F642E636F6DE1"
    "EC5A000800006203F49C21D5D6E022CB16DE3538F248662FC73C93E8B1"
    "C200000028751868747470733A2F2F677265677765697362726F642E63"
    "6F6DE1EC5A000800006203F49C21D5D6E022CB16DE3538F248662FC73C"
    "9808B6B90000001D751868747470733A2F2F677265677765697362726F"
    "642E636F6DE1EC5A000800006203F49C21D5D6E022CB16DE3538F24866"
    "2FC73C9C28BBAC00000012751868747470733A2F2F6772656777656973"
    "62726F642E636F6DE1EC5A000800006203F49C21D5D6E022CB16DE3538"
    "F248662FC73CA048C0A300000007751868747470733A2F2F6772656777"
    "65697362726F642E636F6DE1EC5A000800006203F49C21D5D6E022CB16"
    "DE3538F248662FC73CAACE82C500000029751868747470733A2F2F6772"
    "65677765697362726F642E636F6DE1EC5A000800006203F49C21D5D6E0"
    "22CB16DE3538F248662FC73CAEEE87B80000001E751868747470733A2F"
    "2F677265677765697362726F642E636F6DE1EC5A000800006203F49C21"
    "D5D6E022CB16DE3538F248662FC73CB30E8CAF00000013751868747470"
    "733A2F2F677265677765697362726F642E636F6DE1EC5A000800006203"
    "F49C21D5D6E022CB16DE3538F248662FC73CB72E91A200000008751868"
    "747470733A2F2F677265677765697362726F642E636F6DE1EC5A000800"
    "006203F49C21D5D6E022CB16DE3538F248662FC73CC1B453C40000002A"
    "751868747470733A2F2F677265677765697362726F642E636F6DE1EC5A"
    "000800006203F49C21D5D6E022CB16DE3538F248662FC73CC5D458BB00"
    "00001F751868747470733A2F2F677265677765697362726F642E636F6D"
    "E1EC5A000800006203F49C21D5D6E022CB16DE3538F248662FC73CC9F4"
    "5DAE00000014751868747470733A2F2F677265677765697362726F642E"
    "636F6DE1EC5A000800006203F49C21D5D6E022CB16DE3538F248662FC7"
    "3CCE1462A500000009751868747470733A2F2F67726567776569736272"
    "6F642E636F6DE1EC5A000800006203F49C21D5D6E022CB16DE3538F248"
    "662FC73CD89A24C70000002B751868747470733A2F2F67726567776569"
    "7362726F642E636F6DE1EC5A000800006203F49C21D5D6E022CB16DE35"
    "38F248662FC73CDCBA29BA00000020751868747470733A2F2F67726567"
    "7765697362726F642E636F6DE1EC5A000800006203F49C21D5D6E022CB"
    "16DE3538F248662FC73CE0DA2EB100000015751868747470733A2F2F67"
    "7265677765697362726F642E636F6DE1EC5A000800006203F49C21D5D6"
    "E022CB16DE3538F248662FC73CE4FA33A40000000A751868747470733A"
    "2F2F677265677765697362726F642E636F6DE1EC5A000800006203F49C"
    "21D5D6E022CB16DE3538F248662FC73CF39FFABD000000217518687474"
    "70733A2F2F677265677765697362726F642E636F6DE1EC5A0008000062"
    "03F49C21D5D6E022CB16DE3538F248662FC73CF7BFFFB0000000167518"
    "68747470733A2F2F677265677765697362726F642E636F6DE1EC5A0008"
    "00006203F49C21D5D6E022CB16DE3538F248662FC73CFBE004A7000000"
    "0B751868747470733A2F2F677265677765697362726F642E636F6DE1F1"
    "E1E72200000000501A6203F49C21D5D6E022CB16DE3538F248662FC73C"
    "662FC73C8962EFA000000006FAEC5A000800006203F49C21D5D6E022CB"
    "16DE3538F248662FC73C8962EFA000000006751868747470733A2F2F67"
    "7265677765697362726F642E636F6DE1EC5A000800006203F49C21D5D6"
    "E022CB16DE3538F248662FC73C93E8B1C200000028751868747470733A"
    "2F2F677265677765697362726F642E636F6DE1EC5A000800006203F49C"
    "21D5D6E022CB16DE3538F248662FC73C9808B6B90000001D7518687474"
    "70733A2F2F677265677765697362726F642E636F6DE1EC5A0008000062"
    "03F49C21D5D6E022CB16DE3538F248662FC73C9C28BBAC000000127518"
    "68747470733A2F2F677265677765697362726F642E636F6DE1EC5A0008"
    "00006203F49C21D5D6E022CB16DE3538F248662FC73CA048C0A3000000"
    "07751868747470733A2F2F677265677765697362726F642E636F6DE1EC"
    "5A000800006203F49C21D5D6E022CB16DE3538F248662FC73CAACE82C5"
    "00000029751868747470733A2F2F677265677765697362726F642E636F"
    "6DE1EC5A000800006203F49C21D5D6E022CB16DE3538F248662FC73CAE"
    "EE87B80000001E751868747470733A2F2F677265677765697362726F64"
    "2E636F6DE1EC5A000800006203F49C21D5D6E022CB16DE3538F248662F"
    "C73CB30E8CAF00000013751868747470733A2F2F677265677765697362"
    "726F642E636F6DE1EC5A000800006203F49C21D5D6E022CB16DE3538F2"
    "48662FC73CB72E91A200000008751868747470733A2F2F677265677765"
    "697362726F642E636F6DE1EC5A000800006203F49C21D5D6E022CB16DE"
    "3538F248662FC73CC1B453C40000002A751868747470733A2F2F677265"
    "677765697362726F642E636F6DE1EC5A000800006203F49C21D5D6E022"
    "CB16DE3538F248662FC73CC5D458BB0000001F751868747470733A2F2F"
    "677265677765697362726F642E636F6DE1EC5A000800006203F49C21D5"
    "D6E022CB16DE3538F248662FC73CC9F45DAE0000001475186874747073"
    "3A2F2F677265677765697362726F642E636F6DE1EC5A000800006203F4"
    "9C21D5D6E022CB16DE3538F248662FC73CCE1462A50000000975186874"
    "7470733A2F2F677265677765697362726F642E636F6DE1EC5A00080000"
    "6203F49C21D5D6E022CB16DE3538F248662FC73CD89A24C70000002B75"
    "1868747470733A2F2F677265677765697362726F642E636F6DE1EC5A00"
    "0800006203F49C21D5D6E022CB16DE3538F248662FC73CDCBA29BA0000"
    "0020751868747470733A2F2F677265677765697362726F642E636F6DE1"
    "EC5A000800006203F49C21D5D6E022CB16DE3538F248662FC73CE0DA2E"
    "B100000015751868747470733A2F2F677265677765697362726F642E63"
    "6F6DE1EC5A000800006203F49C21D5D6E022CB16DE3538F248662FC73C"
    "E4FA33A40000000A751868747470733A2F2F677265677765697362726F"
    "642E636F6DE1EC5A000800006203F49C21D5D6E022CB16DE3538F24866"
    "2FC73CEF7FF5C60000002C751868747470733A2F2F6772656777656973"
    "62726F642E636F6DE1EC5A000800006203F49C21D5D6E022CB16DE3538"
    "F248662FC73CF39FFABD00000021751868747470733A2F2F6772656777"
    "65697362726F642E636F6DE1EC5A000800006203F49C21D5D6E022CB16"
    "DE3538F248662FC73CF7BFFFB000000016751868747470733A2F2F6772"
    "65677765697362726F642E636F6DE1EC5A000800006203F49C21D5D6E0"
    "22CB16DE3538F248662FC73CFBE004A70000000B751868747470733A2F"
    "2F677265677765697362726F642E636F6DE1F1E1E1E511006125001F71"
    "B3556ED9C9459001E4F4A9121F4E07AB6D14898A5BBEF13D85C25D7435"
    "40DB59F3CF56BE121B82D5812149D633F605EB07265A80B762A365CE94"
    "883089FEEE4B955701E6240011CC9B202B0000002C6240000002540BE3"
    "ECE1E72200000000240011CC9C2D0000000A202B0000002D202C000000"
    "066240000002540BE3E081146203F49C21D5D6E022CB16DE3538F24866"
    "2FC73CE1E1F1031000";

constinit auto const kRAW_HEADER =
    "03C3141A01633CD656F91B4EBB5EB89B791BD34DBC8A04BB6F407C5335BC54351E"
    "DD733898497E809E04074D14D271E4832D7888754F9230800761563A292FA2315A"
    "6DB6FE30CC5909B285080FCD6773CC883F9FE0EE4D439340AC592AADB973ED3CF5"
    "3E2232B33EF57CECAC2816E3122816E31A0A00F8377CD95DFA484CFAE282656A58"
    "CE5AA29652EFFD80AC59CD91416E4E13DBBE";

}  // namespace

namespace util {

std::pair<std::string, std::string>
createNftTxAndMetaBlobs()
{
    return {hexStringToBinaryString(kTXN_META), hexStringToBinaryString(kTXN_HEX)};
}

std::pair<ripple::STTx, ripple::TxMeta>
createNftTxAndMeta()
{
    ripple::uint256 hash;
    EXPECT_TRUE(hash.parseHex("6C7F69A6D25A13AC4A2E9145999F45D4674F939900017A96885FDC2757E9284E"));

    auto const [metaBlob, txnBlob] = createNftTxAndMetaBlobs();

    ripple::SerialIter it{txnBlob.data(), txnBlob.size()};
    return {ripple::STTx{it}, ripple::TxMeta{hash, kSEQ, metaBlob}};
}

etlng::model::Transaction
createTransaction(ripple::TxType type)
{
    auto const [sttx, meta] = createNftTxAndMeta();
    return {
        .raw = "",
        .metaRaw = "",
        .sttx = sttx,
        .meta = meta,
        .id = ripple::uint256{"0000000000000000000000000000000000000000000000000000000000000001"},
        .key = "0000000000000000000000000000000000000000000000000000000000000001",
        .type = type
    };
}

etlng::model::Object
createObject()
{
    // random object taken from initial ledger load
    static constinit auto const kOBJ_KEY = "B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960D";
    static constinit auto const kOBJ_PRED = "B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960A";
    static constinit auto const kOBJ_SUCC = "B00AA769C00726371689ED66A7CF57C2502F1BF4BDFF2ACADF67A2A7B5E8960F";
    static constinit auto const kOBJ_BLOB =
        "11007222002200002504270918370000000000000C4538000000000000000A554D94799200CC37EFAF45DA76704ED3CBEDBB4B4FCD"
        "56E9CBA5399EB40A7B3BEC629546DD24CDB4C0004C4A50590000000000000000000000000000000000000000000000000000000000"
        "000000000000016680000000000000004C4A505900000000000000000000000000000000368480B7780E3DCF5D062A7BB54129F42F"
        "8BB63367D6C38D7EA4C680004C4A505900000000000000000000000000000000C8056BA4E36038A8A0D2C0A86963153E95A84D56";

    return {
        .key = {},
        .keyRaw = hexStringToBinaryString(kOBJ_KEY),
        .data = {},
        .dataRaw = hexStringToBinaryString(kOBJ_BLOB),
        .successor = hexStringToBinaryString(kOBJ_SUCC),
        .predecessor = hexStringToBinaryString(kOBJ_PRED),
        .type = {},
    };
}

etlng::model::BookSuccessor
createSuccessor()
{
    return {
        .firstBook = "A000000000000000000000000000000000000000000000000000000000000000",
        .bookBase = "A000000000000000000000000000000000000000000000000000000000000001",
    };
}

etlng::impl::PBLedgerResponseType
createDataAndDiff()
{
    auto const rawHeaderBlob = hexStringToBinaryString(kRAW_HEADER);

    auto res = etlng::impl::PBLedgerResponseType();
    res.set_ledger_header(rawHeaderBlob);
    res.set_objects_included(true);
    res.set_object_neighbors_included(true);

    {
        auto original = org::xrpl::rpc::v1::TransactionAndMetadata();
        auto const [metaRaw, txRaw] = createNftTxAndMetaBlobs();
        original.set_transaction_blob(txRaw);
        original.set_metadata_blob(metaRaw);
        for (int i = 0; i < 10; ++i) {
            auto* p = res.mutable_transactions_list()->add_transactions();
            *p = original;
        }
    }
    {
        auto expected = createObject();
        auto original = org::xrpl::rpc::v1::RawLedgerObject();
        original.set_data(expected.dataRaw);
        original.set_key(expected.keyRaw);
        for (auto i = 0; i < 10; ++i) {
            auto* p = res.mutable_ledger_objects()->add_objects();
            *p = original;
        }
    }
    {
        auto expected = createSuccessor();
        auto original = org::xrpl::rpc::v1::BookSuccessor();
        original.set_first_book(expected.firstBook);
        original.set_book_base(expected.bookBase);

        res.set_object_neighbors_included(true);
        for (auto i = 0; i < 10; ++i) {
            auto* p = res.mutable_book_successors()->Add();
            *p = original;
        }
    }

    return res;
}

etlng::impl::PBLedgerResponseType
createData()
{
    auto const rawHeaderBlob = hexStringToBinaryString(kRAW_HEADER);

    auto res = etlng::impl::PBLedgerResponseType();
    res.set_ledger_header(rawHeaderBlob);
    res.set_objects_included(false);
    res.set_object_neighbors_included(false);

    {
        auto original = org::xrpl::rpc::v1::TransactionAndMetadata();
        auto const [metaRaw, txRaw] = createNftTxAndMetaBlobs();
        original.set_transaction_blob(txRaw);
        original.set_metadata_blob(metaRaw);
        for (int i = 0; i < 10; ++i) {
            auto* p = res.mutable_transactions_list()->add_transactions();
            *p = original;
        }
    }

    return res;
}

}  // namespace util
