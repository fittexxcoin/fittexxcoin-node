// Copyright (c) 2012-2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2020 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bloom.h>

#include <clientversion.h>
#include <consensus/merkle.h>
#include <key.h>
#include <key_io.h>
#include <merkleblock.h>
#include <primitives/block.h>
#include <random.h>
#include <serialize.h>
#include <streams.h>
#include <uint256.h>
#include <util/strencodings.h>
#include <util/system.h>

#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(bloom_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(bloom_create_insert_serialize) {
    CBloomFilter filter(3, 0.01, 0, BLOOM_UPDATE_ALL);

    filter.insert(ParseHex("99108ad8ed9bb6274d3980bab5a85c048f0950c8"));
    BOOST_CHECK_MESSAGE(
        filter.contains(ParseHex("99108ad8ed9bb6274d3980bab5a85c048f0950c8")),
        "Bloom filter doesn't contain just-inserted object!");
    // One bit different in first byte
    BOOST_CHECK_MESSAGE(
        !filter.contains(ParseHex("19108ad8ed9bb6274d3980bab5a85c048f0950c8")),
        "Bloom filter contains something it shouldn't!");

    filter.insert(ParseHex("b5a2c786d9ef4658287ced5914b37a1b4aa32eee"));
    BOOST_CHECK_MESSAGE(
        filter.contains(ParseHex("b5a2c786d9ef4658287ced5914b37a1b4aa32eee")),
        "Bloom filter doesn't contain just-inserted object (2)!");

    filter.insert(ParseHex("b9300670b4c5366e95b2699e8b18bc75e5f729c5"));
    BOOST_CHECK_MESSAGE(
        filter.contains(ParseHex("b9300670b4c5366e95b2699e8b18bc75e5f729c5")),
        "Bloom filter doesn't contain just-inserted object (3)!");

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << filter;

    std::vector<uint8_t> vch = ParseHex("03614e9b050000000000000001");
    std::vector<char> expected(vch.size());

    for (size_t i = 0; i < vch.size(); i++) {
        expected[i] = (char)vch[i];
    }

    BOOST_CHECK_EQUAL_COLLECTIONS(stream.begin(), stream.end(),
                                  expected.begin(), expected.end());

    BOOST_CHECK_MESSAGE(
        filter.contains(ParseHex("99108ad8ed9bb6274d3980bab5a85c048f0950c8")),
        "Bloom filter doesn't contain just-inserted object!");
    filter.clear();
    BOOST_CHECK_MESSAGE(
        !filter.contains(ParseHex("99108ad8ed9bb6274d3980bab5a85c048f0950c8")),
        "Bloom filter should be empty!");
}

BOOST_AUTO_TEST_CASE(bloom_create_insert_serialize_with_tweak) {
    // Same test as bloom_create_insert_serialize, but we add a nTweak of 100
    CBloomFilter filter(3, 0.01, 2147483649UL, BLOOM_UPDATE_ALL);

    filter.insert(ParseHex("99108ad8ed9bb6274d3980bab5a85c048f0950c8"));
    BOOST_CHECK_MESSAGE(
        filter.contains(ParseHex("99108ad8ed9bb6274d3980bab5a85c048f0950c8")),
        "Bloom filter doesn't contain just-inserted object!");
    // One bit different in first byte
    BOOST_CHECK_MESSAGE(
        !filter.contains(ParseHex("19108ad8ed9bb6274d3980bab5a85c048f0950c8")),
        "Bloom filter contains something it shouldn't!");

    filter.insert(ParseHex("b5a2c786d9ef4658287ced5914b37a1b4aa32eee"));
    BOOST_CHECK_MESSAGE(
        filter.contains(ParseHex("b5a2c786d9ef4658287ced5914b37a1b4aa32eee")),
        "Bloom filter doesn't contain just-inserted object (2)!");

    filter.insert(ParseHex("b9300670b4c5366e95b2699e8b18bc75e5f729c5"));
    BOOST_CHECK_MESSAGE(
        filter.contains(ParseHex("b9300670b4c5366e95b2699e8b18bc75e5f729c5")),
        "Bloom filter doesn't contain just-inserted object (3)!");

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << filter;

    std::vector<uint8_t> vch = ParseHex("03ce4299050000000100008001");
    std::vector<char> expected(vch.size());

    for (size_t i = 0; i < vch.size(); i++) {
        expected[i] = (char)vch[i];
    }

    BOOST_CHECK_EQUAL_COLLECTIONS(stream.begin(), stream.end(),
                                  expected.begin(), expected.end());
}

BOOST_AUTO_TEST_CASE(bloom_create_insert_key) {
    std::string strSecret =
        std::string("5Kg1gnAjaLfKiwhhPpGS3QfRg2m6awQvaj98JCZBZQ5SuS2F15C");
    CKey key = DecodeSecret(strSecret);
    CPubKey pubkey = key.GetPubKey();
    std::vector<uint8_t> vchPubKey(pubkey.begin(), pubkey.end());

    CBloomFilter filter(2, 0.001, 0, BLOOM_UPDATE_ALL);
    filter.insert(vchPubKey);
    uint160 hash = pubkey.GetID();
    filter.insert(std::vector<uint8_t>(hash.begin(), hash.end()));

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << filter;

    std::vector<uint8_t> vch = ParseHex("038fc16b080000000000000001");
    std::vector<char> expected(vch.size());

    for (size_t i = 0; i < vch.size(); i++) {
        expected[i] = (char)vch[i];
    }

    BOOST_CHECK_EQUAL_COLLECTIONS(stream.begin(), stream.end(),
                                  expected.begin(), expected.end());
}

BOOST_AUTO_TEST_CASE(bloom_match) {
    // Random real transaction
    // (b4749f017444b051c44dfd2720e88f314ff94f3dd6d56d40ef65854fcd7fff6b)
    CDataStream stream(
        ParseHex("01000000010b26e9b7735eb6aabdf358bab62f9816a21ba9ebdb719d5299e"
                 "88607d722c190000000008b4830450220070aca44506c5cef3a16ed519d7c"
                 "3c39f8aab192c4e1c90d065f37b8a4af6141022100a8e160b856c2d43d27d"
                 "8fba71e5aef6405b8643ac4cb7cb3c462aced7f14711a0141046d11fee51b"
                 "0e60666d5049a9101a72741df480b96ee26488a4d3466b95c9a40ac5eeef8"
                 "7e10a5cd336c19a84565f80fa6c547957b7700ff4dfbdefe76036c339ffff"
                 "ffff021bff3d11000000001976a91404943fdd508053c75000106d3bc6e27"
                 "54dbcff1988ac2f15de00000000001976a914a266436d2965547608b9e15d"
                 "9032a7b9d64fa43188ac00000000"),
        SER_DISK, CLIENT_VERSION);
    CTransaction tx(deserialize, stream);

    // and one which spends it
    // (e2769b09e784f32f62ef849763d4f45b98e07ba658647343b915ff832b110436)
    uint8_t ch[] = {
        0x01, 0x00, 0x00, 0x00, 0x01, 0x6b, 0xff, 0x7f, 0xcd, 0x4f, 0x85, 0x65,
        0xef, 0x40, 0x6d, 0xd5, 0xd6, 0x3d, 0x4f, 0xf9, 0x4f, 0x31, 0x8f, 0xe8,
        0x20, 0x27, 0xfd, 0x4d, 0xc4, 0x51, 0xb0, 0x44, 0x74, 0x01, 0x9f, 0x74,
        0xb4, 0x00, 0x00, 0x00, 0x00, 0x8c, 0x49, 0x30, 0x46, 0x02, 0x21, 0x00,
        0xda, 0x0d, 0xc6, 0xae, 0xce, 0xfe, 0x1e, 0x06, 0xef, 0xdf, 0x05, 0x77,
        0x37, 0x57, 0xde, 0xb1, 0x68, 0x82, 0x09, 0x30, 0xe3, 0xb0, 0xd0, 0x3f,
        0x46, 0xf5, 0xfc, 0xf1, 0x50, 0xbf, 0x99, 0x0c, 0x02, 0x21, 0x00, 0xd2,
        0x5b, 0x5c, 0x87, 0x04, 0x00, 0x76, 0xe4, 0xf2, 0x53, 0xf8, 0x26, 0x2e,
        0x76, 0x3e, 0x2d, 0xd5, 0x1e, 0x7f, 0xf0, 0xbe, 0x15, 0x77, 0x27, 0xc4,
        0xbc, 0x42, 0x80, 0x7f, 0x17, 0xbd, 0x39, 0x01, 0x41, 0x04, 0xe6, 0xc2,
        0x6e, 0xf6, 0x7d, 0xc6, 0x10, 0xd2, 0xcd, 0x19, 0x24, 0x84, 0x78, 0x9a,
        0x6c, 0xf9, 0xae, 0xa9, 0x93, 0x0b, 0x94, 0x4b, 0x7e, 0x2d, 0xb5, 0x34,
        0x2b, 0x9d, 0x9e, 0x5b, 0x9f, 0xf7, 0x9a, 0xff, 0x9a, 0x2e, 0xe1, 0x97,
        0x8d, 0xd7, 0xfd, 0x01, 0xdf, 0xc5, 0x22, 0xee, 0x02, 0x28, 0x3d, 0x3b,
        0x06, 0xa9, 0xd0, 0x3a, 0xcf, 0x80, 0x96, 0x96, 0x8d, 0x7d, 0xbb, 0x0f,
        0x91, 0x78, 0xff, 0xff, 0xff, 0xff, 0x02, 0x8b, 0xa7, 0x94, 0x0e, 0x00,
        0x00, 0x00, 0x00, 0x19, 0x76, 0xa9, 0x14, 0xba, 0xde, 0xec, 0xfd, 0xef,
        0x05, 0x07, 0x24, 0x7f, 0xc8, 0xf7, 0x42, 0x41, 0xd7, 0x3b, 0xc0, 0x39,
        0x97, 0x2d, 0x7b, 0x88, 0xac, 0x40, 0x94, 0xa8, 0x02, 0x00, 0x00, 0x00,
        0x00, 0x19, 0x76, 0xa9, 0x14, 0xc1, 0x09, 0x32, 0x48, 0x3f, 0xec, 0x93,
        0xed, 0x51, 0xf5, 0xfe, 0x95, 0xe7, 0x25, 0x59, 0xf2, 0xcc, 0x70, 0x43,
        0xf9, 0x88, 0xac, 0x00, 0x00, 0x00, 0x00, 0x00};
    std::vector<uint8_t> vch(ch, ch + sizeof(ch) - 1);
    CDataStream spendStream(vch, SER_DISK, CLIENT_VERSION);
    CTransaction spendingTx(deserialize, spendStream);

    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(uint256S(
        "0xb4749f017444b051c44dfd2720e88f314ff94f3dd6d56d40ef65854fcd7fff6b"));
    BOOST_CHECK_MESSAGE(filter.IsRelevantAndUpdate(tx),
                        "Simple Bloom filter didn't match tx hash");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    // byte-reversed tx hash
    filter.insert(ParseHex(
        "6bff7fcd4f8565ef406dd5d63d4ff94f318fe82027fd4dc451b04474019f74b4"));
    BOOST_CHECK_MESSAGE(
        filter.IsRelevantAndUpdate(tx),
        "Simple Bloom filter didn't match manually serialized tx hash");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(ParseHex("30450220070aca44506c5cef3a16ed519d7c3c39f8aab192c4e"
                           "1c90d065f37b8a4af6141022100a8e160b856c2d43d27d8fba7"
                           "1e5aef6405b8643ac4cb7cb3c462aced7f14711a01"));
    BOOST_CHECK_MESSAGE(filter.IsRelevantAndUpdate(tx),
                        "Simple Bloom filter didn't match input signature");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(ParseHex("046d11fee51b0e60666d5049a9101a72741df480b96ee26488a"
                           "4d3466b95c9a40ac5eeef87e10a5cd336c19a84565f80fa6c54"
                           "7957b7700ff4dfbdefe76036c339"));
    BOOST_CHECK_MESSAGE(filter.IsRelevantAndUpdate(tx),
                        "Simple Bloom filter didn't match input pub key");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(ParseHex("04943fdd508053c75000106d3bc6e2754dbcff19"));
    BOOST_CHECK_MESSAGE(filter.IsRelevantAndUpdate(tx),
                        "Simple Bloom filter didn't match output address");
    BOOST_CHECK_MESSAGE(filter.IsRelevantAndUpdate(spendingTx),
                        "Simple Bloom filter didn't add output");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(ParseHex("04943fdd508053c75000106d3bc6e2754dbcff19"));
    BOOST_CHECK_MESSAGE(filter.MatchAndInsertOutputs(tx),
                        "Simple Bloom filter didn't match output address");
    BOOST_CHECK_MESSAGE(!filter.MatchAndInsertOutputs(spendingTx),
                        "Simple Bloom filter matched unrelated output");
    BOOST_CHECK_MESSAGE(filter.MatchInputs(spendingTx),
                        "Simple Bloom filter didn't add output");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(ParseHex("a266436d2965547608b9e15d9032a7b9d64fa431"));
    BOOST_CHECK_MESSAGE(filter.IsRelevantAndUpdate(tx),
                        "Simple Bloom filter didn't match output address");

    const TxId txid(uint256S(
        "0x90c122d70786e899529d71dbeba91ba216982fb6ba58f3bdaab65e73b7e9260b"));

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(COutPoint(txid, 0));
    BOOST_CHECK_MESSAGE(filter.IsRelevantAndUpdate(tx),
                        "Simple Bloom filter didn't match COutPoint");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    COutPoint prevOutPoint(txid, 0);
    {
        std::vector<uint8_t> data(32 + sizeof(uint32_t));
        memcpy(data.data(), prevOutPoint.GetTxId().begin(), 32);
        uint32_t n = prevOutPoint.GetN();
        memcpy(data.data() + 32, &n, sizeof(uint32_t));
        filter.insert(data);
    }
    BOOST_CHECK_MESSAGE(
        filter.IsRelevantAndUpdate(tx),
        "Simple Bloom filter didn't match manually serialized COutPoint");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(uint256S(
        "00000009e784f32f62ef849763d4f45b98e07ba658647343b915ff832b110436"));
    BOOST_CHECK_MESSAGE(!filter.IsRelevantAndUpdate(tx),
                        "Simple Bloom filter matched random tx hash");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(ParseHex("0000006d2965547608b9e15d9032a7b9d64fa431"));
    BOOST_CHECK_MESSAGE(!filter.IsRelevantAndUpdate(tx),
                        "Simple Bloom filter matched random address");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(COutPoint(txid, 1));
    BOOST_CHECK_MESSAGE(!filter.IsRelevantAndUpdate(tx),
                        "Simple Bloom filter matched COutPoint for an output "
                        "we didn't care about");

    const TxId lowtxid(uint256S(
        "0x000000d70786e899529d71dbeba91ba216982fb6ba58f3bdaab65e73b7e9260b"));

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(COutPoint(lowtxid, 0));
    BOOST_CHECK_MESSAGE(!filter.IsRelevantAndUpdate(tx),
                        "Simple Bloom filter matched COutPoint for an output "
                        "we didn't care about");
}

BOOST_AUTO_TEST_CASE(merkle_block_1) {
    CBlock block = getBlock13b8a();
    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    // Match the last transaction
    filter.insert(uint256S(
        "0x74d681e0e03bafa802c8aa084379aa98d9fcd632ddc2ed9782b586ec87451f20"));

    CMerkleBlock merkleBlock(block, filter);
    BOOST_CHECK_EQUAL(merkleBlock.header.GetHash().GetHex(),
                      block.GetHash().GetHex());

    BOOST_CHECK_EQUAL(merkleBlock.vMatchedTxn.size(), 1U);
    std::pair<size_t, uint256> pair = merkleBlock.vMatchedTxn[0];

    BOOST_CHECK(merkleBlock.vMatchedTxn[0].second ==
                uint256S("0x74d681e0e03bafa802c8aa084379aa98d9fcd632ddc2ed9782b"
                         "586ec87451f20"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[0].first == 8);

    std::vector<uint256> vMatched;
    std::vector<size_t> vIndex;
    BOOST_CHECK(merkleBlock.txn.ExtractMatches(vMatched, vIndex) ==
                block.hashMerkleRoot);
    BOOST_CHECK(vMatched.size() == merkleBlock.vMatchedTxn.size());
    for (size_t i = 0; i < vMatched.size(); i++) {
        BOOST_CHECK(vMatched[i] == merkleBlock.vMatchedTxn[i].second);
    }

    // Also match the 8th transaction
    filter.insert(uint256S(
        "0xdd1fd2a6fc16404faf339881a90adbde7f4f728691ac62e8f168809cdfae1053"));
    merkleBlock = CMerkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    BOOST_CHECK(merkleBlock.vMatchedTxn.size() == 2);

    BOOST_CHECK(merkleBlock.vMatchedTxn[1] == pair);

    BOOST_CHECK(merkleBlock.vMatchedTxn[0].second ==
                uint256S("0xdd1fd2a6fc16404faf339881a90adbde7f4f728691ac62e8f16"
                         "8809cdfae1053"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[0].first == 7);

    BOOST_CHECK(merkleBlock.txn.ExtractMatches(vMatched, vIndex) ==
                block.hashMerkleRoot);
    BOOST_CHECK(vMatched.size() == merkleBlock.vMatchedTxn.size());
    for (size_t i = 0; i < vMatched.size(); i++) {
        BOOST_CHECK(vMatched[i] == merkleBlock.vMatchedTxn[i].second);
    }
}

BOOST_AUTO_TEST_CASE(merkle_block_2) {
    // Random real block
    // (000000005a4ded781e667e06ceefafb71410b511fe0d5adc3e5a27ecbec34ae6)
    // With 4 txes
    CBlock block;
    CDataStream stream(
        ParseHex(
            "0100000075616236cc2126035fadb38deb65b9102cc2c41c09cdf29fc051906800"
            "000000fe7d5e12ef0ff901f6050211249919b1c0653771832b3a80c66cea42847f"
            "0ae1d4d26e49ffff001d00f0a44104010000000100000000000000000000000000"
            "00000000000000000000000000000000000000ffffffff0804ffff001d029105ff"
            "ffffff0100f2052a010000004341046d8709a041d34357697dfcb30a9d05900a62"
            "94078012bf3bb09c6f9b525f1d16d5503d7905db1ada9501446ea00728668fc571"
            "9aa80be2fdfc8a858a4dbdd4fbac00000000010000000255605dc6f5c3dc148b6d"
            "a58442b0b2cd422be385eab2ebea4119ee9c268d28350000000049483045022100"
            "aa46504baa86df8a33b1192b1b9367b4d729dc41e389f2c04f3e5c7f0559aae702"
            "205e82253a54bf5c4f65b7428551554b2045167d6d206dfe6a2e198127d3f7df15"
            "01ffffffff55605dc6f5c3dc148b6da58442b0b2cd422be385eab2ebea4119ee9c"
            "268d2835010000004847304402202329484c35fa9d6bb32a55a70c0982f606ce0e"
            "3634b69006138683bcd12cbb6602200c28feb1e2555c3210f1dddb299738b4ff8b"
            "be9667b68cb8764b5ac17b7adf0001ffffffff0200e1f505000000004341046a07"
            "65b5865641ce08dd39690aade26dfbf5511430ca428a3089261361cef170e3929a"
            "68aee3d8d4848b0c5111b0a37b82b86ad559fd2a745b44d8e8d9dfdc0cac00180d"
            "8f000000004341044a656f065871a353f216ca26cef8dde2f03e8c16202d2e8ad7"
            "69f02032cb86a5eb5e56842e92e19141d60a01928f8dd2c875a390f67c1f6c94cf"
            "c617c0ea45afac0000000001000000025f9a06d3acdceb56be1bfeaa3e8a25e62d"
            "182fa24fefe899d1c17f1dad4c2028000000004847304402205d6058484157235b"
            "06028c30736c15613a28bdb768ee628094ca8b0030d4d6eb0220328789c9a2ec27"
            "ddaec0ad5ef58efded42e6ea17c2e1ce838f3d6913f5e95db601ffffffff5f9a06"
            "d3acdceb56be1bfeaa3e8a25e62d182fa24fefe899d1c17f1dad4c202801000000"
            "4a493046022100c45af050d3cea806cedd0ab22520c53ebe63b987b8954146cdca"
            "42487b84bdd6022100b9b027716a6b59e640da50a864d6dd8a0ef24c76ce62391f"
            "a3eabaf4d2886d2d01ffffffff0200e1f505000000004341046a0765b5865641ce"
            "08dd39690aade26dfbf5511430ca428a3089261361cef170e3929a68aee3d8d484"
            "8b0c5111b0a37b82b86ad559fd2a745b44d8e8d9dfdc0cac00180d8f0000000043"
            "41046a0765b5865641ce08dd39690aade26dfbf5511430ca428a3089261361cef1"
            "70e3929a68aee3d8d4848b0c5111b0a37b82b86ad559fd2a745b44d8e8d9dfdc0c"
            "ac000000000100000002e2274e5fea1bf29d963914bd301aa63b64daaf8a3e88f1"
            "19b5046ca5738a0f6b0000000048473044022016e7a727a061ea2254a6c358376a"
            "aa617ac537eb836c77d646ebda4c748aac8b0220192ce28bf9f2c06a6467e6531e"
            "27648d2b3e2e2bae85159c9242939840295ba501ffffffffe2274e5fea1bf29d96"
            "3914bd301aa63b64daaf8a3e88f119b5046ca5738a0f6b010000004a4930460221"
            "00b7a1a755588d4190118936e15cd217d133b0e4a53c3c15924010d5648d8925c9"
            "022100aaef031874db2114f2d869ac2de4ae53908fbfea5b2b1862e181626bb900"
            "5c9f01ffffffff0200e1f505000000004341044a656f065871a353f216ca26cef8"
            "dde2f03e8c16202d2e8ad769f02032cb86a5eb5e56842e92e19141d60a01928f8d"
            "d2c875a390f67c1f6c94cfc617c0ea45afac00180d8f000000004341046a0765b5"
            "865641ce08dd39690aade26dfbf5511430ca428a3089261361cef170e3929a68ae"
            "e3d8d4848b0c5111b0a37b82b86ad559fd2a745b44d8e8d9dfdc0cac00000000"),
        SER_NETWORK, PROTOCOL_VERSION);
    stream >> block;

    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    // Match the first transaction
    filter.insert(uint256S(
        "0xe980fe9f792d014e73b95203dc1335c5f9ce19ac537a419e6df5b47aecb93b70"));

    CMerkleBlock merkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    BOOST_CHECK(merkleBlock.vMatchedTxn.size() == 1);
    std::pair<size_t, uint256> pair = merkleBlock.vMatchedTxn[0];

    BOOST_CHECK(merkleBlock.vMatchedTxn[0].second ==
                uint256S("0xe980fe9f792d014e73b95203dc1335c5f9ce19ac537a419e6df"
                         "5b47aecb93b70"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[0].first == 0);

    std::vector<uint256> vMatched;
    std::vector<size_t> vIndex;
    BOOST_CHECK(merkleBlock.txn.ExtractMatches(vMatched, vIndex) ==
                block.hashMerkleRoot);
    BOOST_CHECK(vMatched.size() == merkleBlock.vMatchedTxn.size());
    for (size_t i = 0; i < vMatched.size(); i++) {
        BOOST_CHECK(vMatched[i] == merkleBlock.vMatchedTxn[i].second);
    }

    // Match an output from the second transaction (the pubkey for address
    // 1DZTzaBHUDM7T3QvUKBz4qXMRpkg8jsfB5)
    // This should match the third transaction because it spends the output
    // matched
    // It also matches the fourth transaction, which spends to the pubkey again
    filter.insert(ParseHex("044a656f065871a353f216ca26cef8dde2f03e8c16202d2e8ad"
                           "769f02032cb86a5eb5e56842e92e19141d60a01928f8dd2c875"
                           "a390f67c1f6c94cfc617c0ea45af"));

    merkleBlock = CMerkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    BOOST_CHECK(merkleBlock.vMatchedTxn.size() == 4);

    BOOST_CHECK(pair == merkleBlock.vMatchedTxn[0]);

    BOOST_CHECK(merkleBlock.vMatchedTxn[1].second ==
                uint256S("0x28204cad1d7fc1d199e8ef4fa22f182de6258a3eaafe1bbe56e"
                         "bdcacd3069a5f"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[1].first == 1);

    BOOST_CHECK(merkleBlock.vMatchedTxn[2].second ==
                uint256S("0x6b0f8a73a56c04b519f1883e8aafda643ba61a30bd1439969df"
                         "21bea5f4e27e2"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[2].first == 2);

    BOOST_CHECK(merkleBlock.vMatchedTxn[3].second ==
                uint256S("0x3c1d7e82342158e4109df2e0b6348b6e84e403d8b4046d70076"
                         "63ace63cddb23"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[3].first == 3);

    BOOST_CHECK(merkleBlock.txn.ExtractMatches(vMatched, vIndex) ==
                block.hashMerkleRoot);
    BOOST_CHECK(vMatched.size() == merkleBlock.vMatchedTxn.size());
    for (size_t i = 0; i < vMatched.size(); i++) {
        BOOST_CHECK(vMatched[i] == merkleBlock.vMatchedTxn[i].second);
    }
}

BOOST_AUTO_TEST_CASE(merkle_block_2_reversed) {
    // Like merkle_block_2 except this block gets its transactions reversed in
    // order to check non-topological processing.
    // Random real block
    // (000000005a4ded781e667e06ceefafb71410b511fe0d5adc3e5a27ecbec34ae6)
    // With 4 txes
    CBlock block;
    CDataStream stream(
        ParseHex(
            "0100000075616236cc2126035fadb38deb65b9102cc2c41c09cdf29fc051906800"
            "000000fe7d5e12ef0ff901f6050211249919b1c0653771832b3a80c66cea42847f"
            "0ae1d4d26e49ffff001d00f0a44104010000000100000000000000000000000000"
            "00000000000000000000000000000000000000ffffffff0804ffff001d029105ff"
            "ffffff0100f2052a010000004341046d8709a041d34357697dfcb30a9d05900a62"
            "94078012bf3bb09c6f9b525f1d16d5503d7905db1ada9501446ea00728668fc571"
            "9aa80be2fdfc8a858a4dbdd4fbac00000000010000000255605dc6f5c3dc148b6d"
            "a58442b0b2cd422be385eab2ebea4119ee9c268d28350000000049483045022100"
            "aa46504baa86df8a33b1192b1b9367b4d729dc41e389f2c04f3e5c7f0559aae702"
            "205e82253a54bf5c4f65b7428551554b2045167d6d206dfe6a2e198127d3f7df15"
            "01ffffffff55605dc6f5c3dc148b6da58442b0b2cd422be385eab2ebea4119ee9c"
            "268d2835010000004847304402202329484c35fa9d6bb32a55a70c0982f606ce0e"
            "3634b69006138683bcd12cbb6602200c28feb1e2555c3210f1dddb299738b4ff8b"
            "be9667b68cb8764b5ac17b7adf0001ffffffff0200e1f505000000004341046a07"
            "65b5865641ce08dd39690aade26dfbf5511430ca428a3089261361cef170e3929a"
            "68aee3d8d4848b0c5111b0a37b82b86ad559fd2a745b44d8e8d9dfdc0cac00180d"
            "8f000000004341044a656f065871a353f216ca26cef8dde2f03e8c16202d2e8ad7"
            "69f02032cb86a5eb5e56842e92e19141d60a01928f8dd2c875a390f67c1f6c94cf"
            "c617c0ea45afac0000000001000000025f9a06d3acdceb56be1bfeaa3e8a25e62d"
            "182fa24fefe899d1c17f1dad4c2028000000004847304402205d6058484157235b"
            "06028c30736c15613a28bdb768ee628094ca8b0030d4d6eb0220328789c9a2ec27"
            "ddaec0ad5ef58efded42e6ea17c2e1ce838f3d6913f5e95db601ffffffff5f9a06"
            "d3acdceb56be1bfeaa3e8a25e62d182fa24fefe899d1c17f1dad4c202801000000"
            "4a493046022100c45af050d3cea806cedd0ab22520c53ebe63b987b8954146cdca"
            "42487b84bdd6022100b9b027716a6b59e640da50a864d6dd8a0ef24c76ce62391f"
            "a3eabaf4d2886d2d01ffffffff0200e1f505000000004341046a0765b5865641ce"
            "08dd39690aade26dfbf5511430ca428a3089261361cef170e3929a68aee3d8d484"
            "8b0c5111b0a37b82b86ad559fd2a745b44d8e8d9dfdc0cac00180d8f0000000043"
            "41046a0765b5865641ce08dd39690aade26dfbf5511430ca428a3089261361cef1"
            "70e3929a68aee3d8d4848b0c5111b0a37b82b86ad559fd2a745b44d8e8d9dfdc0c"
            "ac000000000100000002e2274e5fea1bf29d963914bd301aa63b64daaf8a3e88f1"
            "19b5046ca5738a0f6b0000000048473044022016e7a727a061ea2254a6c358376a"
            "aa617ac537eb836c77d646ebda4c748aac8b0220192ce28bf9f2c06a6467e6531e"
            "27648d2b3e2e2bae85159c9242939840295ba501ffffffffe2274e5fea1bf29d96"
            "3914bd301aa63b64daaf8a3e88f119b5046ca5738a0f6b010000004a4930460221"
            "00b7a1a755588d4190118936e15cd217d133b0e4a53c3c15924010d5648d8925c9"
            "022100aaef031874db2114f2d869ac2de4ae53908fbfea5b2b1862e181626bb900"
            "5c9f01ffffffff0200e1f505000000004341044a656f065871a353f216ca26cef8"
            "dde2f03e8c16202d2e8ad769f02032cb86a5eb5e56842e92e19141d60a01928f8d"
            "d2c875a390f67c1f6c94cfc617c0ea45afac00180d8f000000004341046a0765b5"
            "865641ce08dd39690aade26dfbf5511430ca428a3089261361cef170e3929a68ae"
            "e3d8d4848b0c5111b0a37b82b86ad559fd2a745b44d8e8d9dfdc0cac00000000"),
        SER_NETWORK, PROTOCOL_VERSION);
    stream >> block;

    // Reverse the transactions and recalculate merkle root. The remainder of
    // this test is the same as merkle_block_2 above except the transaction
    // indices get reversed too.
    std::reverse(block.vtx.begin(), block.vtx.end());
    block.hashMerkleRoot = BlockMerkleRoot(block);

    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    // Match the fourth (was first) transaction
    filter.insert(uint256S(
        "0xe980fe9f792d014e73b95203dc1335c5f9ce19ac537a419e6df5b47aecb93b70"));

    CMerkleBlock merkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    BOOST_CHECK(merkleBlock.vMatchedTxn.size() == 1);
    std::pair<size_t, uint256> pair = merkleBlock.vMatchedTxn[0];

    BOOST_CHECK(merkleBlock.vMatchedTxn[0].second ==
                uint256S("0xe980fe9f792d014e73b95203dc1335c5f9ce19ac537a419e6df"
                         "5b47aecb93b70"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[0].first == 3);

    std::vector<uint256> vMatched;
    std::vector<size_t> vIndex;
    BOOST_CHECK(merkleBlock.txn.ExtractMatches(vMatched, vIndex) ==
                block.hashMerkleRoot);
    BOOST_CHECK(vMatched.size() == merkleBlock.vMatchedTxn.size());
    for (size_t i = 0; i < vMatched.size(); i++) {
        BOOST_CHECK(vMatched[i] == merkleBlock.vMatchedTxn[i].second);
    }

    // Match an output from the third (was second) transaction (the pubkey for
    // address 1DZTzaBHUDM7T3QvUKBz4qXMRpkg8jsfB5) This should match the second
    // (was third) transaction because it spends the output matched
    // It also matches the first (was fourth) transaction, which spends to the
    // pubkey again
    filter.insert(ParseHex("044a656f065871a353f216ca26cef8dde2f03e8c16202d2e8ad"
                           "769f02032cb86a5eb5e56842e92e19141d60a01928f8dd2c875"
                           "a390f67c1f6c94cfc617c0ea45af"));

    merkleBlock = CMerkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    BOOST_CHECK(merkleBlock.vMatchedTxn.size() == 4);

    BOOST_CHECK(merkleBlock.vMatchedTxn[0].second ==
                uint256S("0x3c1d7e82342158e4109df2e0b6348b6e84e403d8b4046d70076"
                         "63ace63cddb23"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[0].first == 0);

    BOOST_CHECK(merkleBlock.vMatchedTxn[1].second ==
                uint256S("0x6b0f8a73a56c04b519f1883e8aafda643ba61a30bd1439969df"
                         "21bea5f4e27e2"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[1].first == 1);

    BOOST_CHECK(merkleBlock.vMatchedTxn[2].second ==
                uint256S("0x28204cad1d7fc1d199e8ef4fa22f182de6258a3eaafe1bbe56e"
                         "bdcacd3069a5f"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[2].first == 2);

    BOOST_CHECK(pair == merkleBlock.vMatchedTxn[3]);

    BOOST_CHECK(merkleBlock.txn.ExtractMatches(vMatched, vIndex) ==
                block.hashMerkleRoot);
    BOOST_CHECK(vMatched.size() == merkleBlock.vMatchedTxn.size());
    for (size_t i = 0; i < vMatched.size(); i++) {
        BOOST_CHECK(vMatched[i] == merkleBlock.vMatchedTxn[i].second);
    }
}

BOOST_AUTO_TEST_CASE(merkle_block_2_with_update_none) {
    // Random real block
    // (000000005a4ded781e667e06ceefafb71410b511fe0d5adc3e5a27ecbec34ae6)
    // With 4 txes
    CBlock block;
    CDataStream stream(
        ParseHex(
            "0100000075616236cc2126035fadb38deb65b9102cc2c41c09cdf29fc051906800"
            "000000fe7d5e12ef0ff901f6050211249919b1c0653771832b3a80c66cea42847f"
            "0ae1d4d26e49ffff001d00f0a44104010000000100000000000000000000000000"
            "00000000000000000000000000000000000000ffffffff0804ffff001d029105ff"
            "ffffff0100f2052a010000004341046d8709a041d34357697dfcb30a9d05900a62"
            "94078012bf3bb09c6f9b525f1d16d5503d7905db1ada9501446ea00728668fc571"
            "9aa80be2fdfc8a858a4dbdd4fbac00000000010000000255605dc6f5c3dc148b6d"
            "a58442b0b2cd422be385eab2ebea4119ee9c268d28350000000049483045022100"
            "aa46504baa86df8a33b1192b1b9367b4d729dc41e389f2c04f3e5c7f0559aae702"
            "205e82253a54bf5c4f65b7428551554b2045167d6d206dfe6a2e198127d3f7df15"
            "01ffffffff55605dc6f5c3dc148b6da58442b0b2cd422be385eab2ebea4119ee9c"
            "268d2835010000004847304402202329484c35fa9d6bb32a55a70c0982f606ce0e"
            "3634b69006138683bcd12cbb6602200c28feb1e2555c3210f1dddb299738b4ff8b"
            "be9667b68cb8764b5ac17b7adf0001ffffffff0200e1f505000000004341046a07"
            "65b5865641ce08dd39690aade26dfbf5511430ca428a3089261361cef170e3929a"
            "68aee3d8d4848b0c5111b0a37b82b86ad559fd2a745b44d8e8d9dfdc0cac00180d"
            "8f000000004341044a656f065871a353f216ca26cef8dde2f03e8c16202d2e8ad7"
            "69f02032cb86a5eb5e56842e92e19141d60a01928f8dd2c875a390f67c1f6c94cf"
            "c617c0ea45afac0000000001000000025f9a06d3acdceb56be1bfeaa3e8a25e62d"
            "182fa24fefe899d1c17f1dad4c2028000000004847304402205d6058484157235b"
            "06028c30736c15613a28bdb768ee628094ca8b0030d4d6eb0220328789c9a2ec27"
            "ddaec0ad5ef58efded42e6ea17c2e1ce838f3d6913f5e95db601ffffffff5f9a06"
            "d3acdceb56be1bfeaa3e8a25e62d182fa24fefe899d1c17f1dad4c202801000000"
            "4a493046022100c45af050d3cea806cedd0ab22520c53ebe63b987b8954146cdca"
            "42487b84bdd6022100b9b027716a6b59e640da50a864d6dd8a0ef24c76ce62391f"
            "a3eabaf4d2886d2d01ffffffff0200e1f505000000004341046a0765b5865641ce"
            "08dd39690aade26dfbf5511430ca428a3089261361cef170e3929a68aee3d8d484"
            "8b0c5111b0a37b82b86ad559fd2a745b44d8e8d9dfdc0cac00180d8f0000000043"
            "41046a0765b5865641ce08dd39690aade26dfbf5511430ca428a3089261361cef1"
            "70e3929a68aee3d8d4848b0c5111b0a37b82b86ad559fd2a745b44d8e8d9dfdc0c"
            "ac000000000100000002e2274e5fea1bf29d963914bd301aa63b64daaf8a3e88f1"
            "19b5046ca5738a0f6b0000000048473044022016e7a727a061ea2254a6c358376a"
            "aa617ac537eb836c77d646ebda4c748aac8b0220192ce28bf9f2c06a6467e6531e"
            "27648d2b3e2e2bae85159c9242939840295ba501ffffffffe2274e5fea1bf29d96"
            "3914bd301aa63b64daaf8a3e88f119b5046ca5738a0f6b010000004a4930460221"
            "00b7a1a755588d4190118936e15cd217d133b0e4a53c3c15924010d5648d8925c9"
            "022100aaef031874db2114f2d869ac2de4ae53908fbfea5b2b1862e181626bb900"
            "5c9f01ffffffff0200e1f505000000004341044a656f065871a353f216ca26cef8"
            "dde2f03e8c16202d2e8ad769f02032cb86a5eb5e56842e92e19141d60a01928f8d"
            "d2c875a390f67c1f6c94cfc617c0ea45afac00180d8f000000004341046a0765b5"
            "865641ce08dd39690aade26dfbf5511430ca428a3089261361cef170e3929a68ae"
            "e3d8d4848b0c5111b0a37b82b86ad559fd2a745b44d8e8d9dfdc0cac00000000"),
        SER_NETWORK, PROTOCOL_VERSION);
    stream >> block;

    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_NONE);
    // Match the first transaction
    filter.insert(uint256S(
        "0xe980fe9f792d014e73b95203dc1335c5f9ce19ac537a419e6df5b47aecb93b70"));

    CMerkleBlock merkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    BOOST_CHECK(merkleBlock.vMatchedTxn.size() == 1);
    std::pair<size_t, uint256> pair = merkleBlock.vMatchedTxn[0];

    BOOST_CHECK(merkleBlock.vMatchedTxn[0].second ==
                uint256S("0xe980fe9f792d014e73b95203dc1335c5f9ce19ac537a419e6df"
                         "5b47aecb93b70"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[0].first == 0);

    std::vector<uint256> vMatched;
    std::vector<size_t> vIndex;
    BOOST_CHECK(merkleBlock.txn.ExtractMatches(vMatched, vIndex) ==
                block.hashMerkleRoot);
    BOOST_CHECK(vMatched.size() == merkleBlock.vMatchedTxn.size());
    for (size_t i = 0; i < vMatched.size(); i++) {
        BOOST_CHECK(vMatched[i] == merkleBlock.vMatchedTxn[i].second);
    }

    // Match an output from the second transaction (the pubkey for address
    // 1DZTzaBHUDM7T3QvUKBz4qXMRpkg8jsfB5)
    // This should not match the third transaction though it spends the output
    // matched
    // It will match the fourth transaction, which has another pay-to-pubkey
    // output to the same address
    filter.insert(ParseHex("044a656f065871a353f216ca26cef8dde2f03e8c16202d2e8ad"
                           "769f02032cb86a5eb5e56842e92e19141d60a01928f8dd2c875"
                           "a390f67c1f6c94cfc617c0ea45af"));

    merkleBlock = CMerkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    BOOST_CHECK(merkleBlock.vMatchedTxn.size() == 3);

    BOOST_CHECK(pair == merkleBlock.vMatchedTxn[0]);

    BOOST_CHECK(merkleBlock.vMatchedTxn[1].second ==
                uint256S("0x28204cad1d7fc1d199e8ef4fa22f182de6258a3eaafe1bbe56e"
                         "bdcacd3069a5f"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[1].first == 1);

    BOOST_CHECK(merkleBlock.vMatchedTxn[2].second ==
                uint256S("0x3c1d7e82342158e4109df2e0b6348b6e84e403d8b4046d70076"
                         "63ace63cddb23"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[2].first == 3);

    BOOST_CHECK(merkleBlock.txn.ExtractMatches(vMatched, vIndex) ==
                block.hashMerkleRoot);
    BOOST_CHECK(vMatched.size() == merkleBlock.vMatchedTxn.size());
    for (size_t i = 0; i < vMatched.size(); i++) {
        BOOST_CHECK(vMatched[i] == merkleBlock.vMatchedTxn[i].second);
    }
}

BOOST_AUTO_TEST_CASE(merkle_block_3_and_serialize) {
    // Random real block
    // (000000000000dab0130bbcc991d3d7ae6b81aa6f50a798888dfe62337458dc45)
    // With one tx
    CBlock block;
    CDataStream stream(
        ParseHex("0100000079cda856b143d9db2c1caff01d1aecc8630d30625d10e8b4b8b00"
                 "00000000000b50cc069d6a3e33e3ff84a5c41d9d3febe7c770fdcc96b2c3f"
                 "f60abe184f196367291b4d4c86041b8fa45d6301010000000100000000000"
                 "00000000000000000000000000000000000000000000000000000ffffffff"
                 "08044c86041b020a02ffffffff0100f2052a01000000434104ecd3229b057"
                 "1c3be876feaac0442a9f13c5a572742927af1dc623353ecf8c202225f6486"
                 "8137a18cdd85cbbb4c74fbccfd4f49639cf1bdc94a5672bb15ad5d4cac000"
                 "00000"),
        SER_NETWORK, PROTOCOL_VERSION);
    stream >> block;

    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    // Match the only transaction
    filter.insert(uint256S(
        "0x63194f18be0af63f2c6bc9dc0f777cbefed3d9415c4af83f3ee3a3d669c00cb5"));

    CMerkleBlock merkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    BOOST_CHECK(merkleBlock.vMatchedTxn.size() == 1);

    BOOST_CHECK(merkleBlock.vMatchedTxn[0].second ==
                uint256S("0x63194f18be0af63f2c6bc9dc0f777cbefed3d9415c4af83f3ee"
                         "3a3d669c00cb5"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[0].first == 0);

    std::vector<uint256> vMatched;
    std::vector<size_t> vIndex;
    BOOST_CHECK(merkleBlock.txn.ExtractMatches(vMatched, vIndex) ==
                block.hashMerkleRoot);
    BOOST_CHECK(vMatched.size() == merkleBlock.vMatchedTxn.size());
    for (size_t i = 0; i < vMatched.size(); i++) {
        BOOST_CHECK(vMatched[i] == merkleBlock.vMatchedTxn[i].second);
    }

    CDataStream merkleStream(SER_NETWORK, PROTOCOL_VERSION);
    merkleStream << merkleBlock;

    std::vector<uint8_t> vch =
        ParseHex("0100000079cda856b143d9db2c1caff01d1aecc8630d30625d10e8b4b8b00"
                 "00000000000b50cc069d6a3e33e3ff84a5c41d9d3febe7c770fdcc96b2c3f"
                 "f60abe184f196367291b4d4c86041b8fa45d630100000001b50cc069d6a3e"
                 "33e3ff84a5c41d9d3febe7c770fdcc96b2c3ff60abe184f19630101");
    std::vector<char> expected(vch.size());

    for (size_t i = 0; i < vch.size(); i++) {
        expected[i] = (char)vch[i];
    }

    BOOST_CHECK_EQUAL_COLLECTIONS(expected.begin(), expected.end(),
                                  merkleStream.begin(), merkleStream.end());
}

BOOST_AUTO_TEST_CASE(merkle_block_4) {
    // Random real block
    // (000000000000b731f2eef9e8c63173adfb07e41bd53eb0ef0a6b720d6cb6dea4)
    // With 7 txes
    CBlock block;
    CDataStream stream(
        ParseHex(
            "0100000082bb869cf3a793432a66e826e05a6fc37469f8efb7421dc88067010000"
            "0000007f16c5962e8bd963659c793ce370d95f093bc7e367117b3c30c1f8fdd0d9"
            "728776381b4d4c86041b554b852907010000000100000000000000000000000000"
            "00000000000000000000000000000000000000ffffffff07044c86041b0136ffff"
            "ffff0100f2052a01000000434104eaafc2314def4ca98ac970241bcab022b9c1e1"
            "f4ea423a20f134c876f2c01ec0f0dd5b2e86e7168cefe0d81113c3807420ce13ad"
            "1357231a2252247d97a46a91ac000000000100000001bcad20a6a29827d1424f08"
            "989255120bf7f3e9e3cdaaa6bb31b0737fe048724300000000494830450220356e"
            "834b046cadc0f8ebb5a8a017b02de59c86305403dad52cd77b55af062ea1022100"
            "9253cd6c119d4729b77c978e1e2aa19f5ea6e0e52b3f16e32fa608cd5bab753901"
            "ffffffff02008d380c010000001976a9142b4b8072ecbba129b6453c63e129e643"
            "207249ca88ac0065cd1d000000001976a9141b8dd13b994bcfc787b32aeadf58cc"
            "b3615cbd5488ac000000000100000003fdacf9b3eb077412e7a968d2e4f11b9a9d"
            "ee312d666187ed77ee7d26af16cb0b000000008c493046022100ea1608e70911ca"
            "0de5af51ba57ad23b9a51db8d28f82c53563c56a05c20f5a87022100a8bdc8b4a8"
            "acc8634c6b420410150775eb7f2474f5615f7fccd65af30f310fbf01410465fdf4"
            "9e29b06b9a1582287b6279014f834edc317695d125ef623c1cc3aaece245bd69fc"
            "ad7508666e9c74a49dc9056d5fc14338ef38118dc4afae5fe2c585caffffffff30"
            "9e1913634ecb50f3c4f83e96e70b2df071b497b8973a3e75429df397b5af830000"
            "00004948304502202bdb79c596a9ffc24e96f4386199aba386e9bc7b6071516e2b"
            "51dda942b3a1ed022100c53a857e76b724fc14d45311eac5019650d415c3abb542"
            "8f3aae16d8e69bec2301ffffffff2089e33491695080c9edc18a428f7d834db5b6"
            "d372df13ce2b1b0e0cbcb1e6c10000000049483045022100d4ce67c5896ee251c8"
            "10ac1ff9ceccd328b497c8f553ab6e08431e7d40bad6b5022033119c0c2b7d792d"
            "31f1187779c7bd95aefd93d90a715586d73801d9b47471c601ffffffff01007144"
            "60030000001976a914c7b55141d097ea5df7a0ed330cf794376e53ec8d88ac0000"
            "000001000000045bf0e214aa4069a3e792ecee1e1bf0c1d397cde8dd08138f4b72"
            "a00681743447000000008b48304502200c45de8c4f3e2c1821f2fc878cba97b1e6"
            "f8807d94930713aa1c86a67b9bf1e40221008581abfef2e30f957815fc89978423"
            "746b2086375ca8ecf359c85c2a5b7c88ad01410462bb73f76ca0994fcb8b4271e6"
            "fb7561f5c0f9ca0cf6485261c4a0dc894f4ab844c6cdfb97cd0b60ffb5018ffd62"
            "38f4d87270efb1d3ae37079b794a92d7ec95ffffffffd669f7d7958d40fc59d225"
            "3d88e0f248e29b599c80bbcec344a83dda5f9aa72c000000008a47304402207812"
            "4c8beeaa825f9e0b30bff96e564dd859432f2d0cb3b72d3d5d93d38d7e93022069"
            "1d233b6c0f995be5acb03d70a7f7a65b6bc9bdd426260f38a1346669507a360141"
            "0462bb73f76ca0994fcb8b4271e6fb7561f5c0f9ca0cf6485261c4a0dc894f4ab8"
            "44c6cdfb97cd0b60ffb5018ffd6238f4d87270efb1d3ae37079b794a92d7ec95ff"
            "fffffff878af0d93f5229a68166cf051fd372bb7a537232946e0a46f53636b4daf"
            "daa4000000008c493046022100c717d1714551663f69c3c5759bdbb3a0fcd3fab0"
            "23abc0e522fe6440de35d8290221008d9cbe25bffc44af2b18e81c58eb37293fd7"
            "fe1c2e7b46fc37ee8c96c50ab1e201410462bb73f76ca0994fcb8b4271e6fb7561"
            "f5c0f9ca0cf6485261c4a0dc894f4ab844c6cdfb97cd0b60ffb5018ffd6238f4d8"
            "7270efb1d3ae37079b794a92d7ec95ffffffff27f2b668859cd7f2f894aa0fd2d9"
            "e60963bcd07c88973f425f999b8cbfd7a1e2000000008c493046022100e0084714"
            "7cbf517bcc2f502f3ddc6d284358d102ed20d47a8aa788a62f0db780022100d17b"
            "2d6fa84dcaf1c95d88d7e7c30385aecf415588d749afd3ec81f6022cecd7014104"
            "62bb73f76ca0994fcb8b4271e6fb7561f5c0f9ca0cf6485261c4a0dc894f4ab844"
            "c6cdfb97cd0b60ffb5018ffd6238f4d87270efb1d3ae37079b794a92d7ec95ffff"
            "ffff0100c817a8040000001976a914b6efd80d99179f4f4ff6f4dd0a007d018c38"
            "5d2188ac000000000100000001834537b2f1ce8ef9373a258e10545ce5a50b758d"
            "f616cd4356e0032554ebd3c4000000008b483045022100e68f422dd7c34fdce11e"
            "eb4509ddae38201773dd62f284e8aa9d96f85099d0b002202243bd399ff96b649a"
            "0fad05fa759d6a882f0af8c90cf7632c2840c29070aec20141045e58067e815c2f"
            "464c6a2a15f987758374203895710c2d452442e28496ff38ba8f5fd901dc20e29e"
            "88477167fe4fc299bf818fd0d9e1632d467b2a3d9503b1aaffffffff0280d7e636"
            "030000001976a914f34c3e10eb387efe872acb614c89e78bfca7815d88ac404b4c"
            "00000000001976a914a84e272933aaf87e1715d7786c51dfaeb5b65a6f88ac0000"
            "0000010000000143ac81c8e6f6ef307dfe17f3d906d999e23e0189fda838c5510d"
            "850927e03ae7000000008c4930460221009c87c344760a64cb8ae6685a3eec2c1a"
            "c1bed5b88c87de51acd0e124f266c16602210082d07c037359c3a257b5c63ebd90"
            "f5a5edf97b2ac1c434b08ca998839f346dd40141040ba7e521fa7946d12edbb1d1"
            "e95a15c34bd4398195e86433c92b431cd315f455fe30032ede69cad9d1e1ed6c3c"
            "4ec0dbfced53438c625462afb792dcb098544bffffffff0240420f000000000019"
            "76a9144676d1b820d63ec272f1900d59d43bc6463d96f888ac40420f0000000000"
            "1976a914648d04341d00d7968b3405c034adc38d4d8fb9bd88ac00000000010000"
            "000248cc917501ea5c55f4a8d2009c0567c40cfe037c2e71af017d0a452ff705e3"
            "f1000000008b483045022100bf5fdc86dc5f08a5d5c8e43a8c9d5b1ed8c65562e2"
            "80007b52b133021acd9acc02205e325d613e555f772802bf413d36ba807892ed1a"
            "690a77811d3033b3de226e0a01410429fa713b124484cb2bd7b5557b2c0b9df7b2"
            "b1fee61825eadc5ae6c37a9920d38bfccdc7dc3cb0c47d7b173dbc9db8d37db0a3"
            "3ae487982c59c6f8606e9d1791ffffffff41ed70551dd7e841883ab8f0b16bf041"
            "76b7d1480e4f0af9f3d4c3595768d068000000008b4830450221008513ad65187b"
            "903aed1102d1d0c47688127658c51106753fed0151ce9c16b80902201432b9ebcb"
            "87bd04ceb2de66035fbbaf4bf8b00d1cfe41f1a1f7338f9ad79d210141049d4cf8"
            "0125bf50be1709f718c07ad15d0fc612b7da1f5570dddc35f2a352f0f27c978b06"
            "820edca9ef982c35fda2d255afba340068c5035552368bc7200c1488ffffffff01"
            "00093d00000000001976a9148edb68822f1ad580b043c7b3df2e400f8699eb4888"
            "ac00000000"),
        SER_NETWORK, PROTOCOL_VERSION);
    stream >> block;

    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    // Match the last transaction
    filter.insert(uint256S(
        "0x0a2a92f0bda4727d0a13eaddf4dd9ac6b5c61a1429e6b2b818f19b15df0ac154"));

    CMerkleBlock merkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    BOOST_CHECK(merkleBlock.vMatchedTxn.size() == 1);
    std::pair<size_t, uint256> pair = merkleBlock.vMatchedTxn[0];

    BOOST_CHECK(merkleBlock.vMatchedTxn[0].second ==
                uint256S("0x0a2a92f0bda4727d0a13eaddf4dd9ac6b5c61a1429e6b2b818f"
                         "19b15df0ac154"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[0].first == 6);

    std::vector<uint256> vMatched;
    std::vector<size_t> vIndex;
    BOOST_CHECK(merkleBlock.txn.ExtractMatches(vMatched, vIndex) ==
                block.hashMerkleRoot);
    BOOST_CHECK(vMatched.size() == merkleBlock.vMatchedTxn.size());
    for (size_t i = 0; i < vMatched.size(); i++) {
        BOOST_CHECK(vMatched[i] == merkleBlock.vMatchedTxn[i].second);
    }

    // Also match the 4th transaction
    filter.insert(uint256S(
        "0x02981fa052f0481dbc5868f4fc2166035a10f27a03cfd2de67326471df5bc041"));
    merkleBlock = CMerkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    BOOST_CHECK(merkleBlock.vMatchedTxn.size() == 2);

    BOOST_CHECK(merkleBlock.vMatchedTxn[0].second ==
                uint256S("0x02981fa052f0481dbc5868f4fc2166035a10f27a03cfd2de673"
                         "26471df5bc041"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[0].first == 3);

    BOOST_CHECK(merkleBlock.vMatchedTxn[1] == pair);

    BOOST_CHECK(merkleBlock.txn.ExtractMatches(vMatched, vIndex) ==
                block.hashMerkleRoot);
    BOOST_CHECK(vMatched.size() == merkleBlock.vMatchedTxn.size());
    for (size_t i = 0; i < vMatched.size(); i++) {
        BOOST_CHECK(vMatched[i] == merkleBlock.vMatchedTxn[i].second);
    }
}

BOOST_AUTO_TEST_CASE(merkle_block_4_test_p2pubkey_only) {
    // Random real block
    // (000000000000b731f2eef9e8c63173adfb07e41bd53eb0ef0a6b720d6cb6dea4)
    // With 7 txes
    CBlock block;
    CDataStream stream(
        ParseHex(
            "0100000082bb869cf3a793432a66e826e05a6fc37469f8efb7421dc88067010000"
            "0000007f16c5962e8bd963659c793ce370d95f093bc7e367117b3c30c1f8fdd0d9"
            "728776381b4d4c86041b554b852907010000000100000000000000000000000000"
            "00000000000000000000000000000000000000ffffffff07044c86041b0136ffff"
            "ffff0100f2052a01000000434104eaafc2314def4ca98ac970241bcab022b9c1e1"
            "f4ea423a20f134c876f2c01ec0f0dd5b2e86e7168cefe0d81113c3807420ce13ad"
            "1357231a2252247d97a46a91ac000000000100000001bcad20a6a29827d1424f08"
            "989255120bf7f3e9e3cdaaa6bb31b0737fe048724300000000494830450220356e"
            "834b046cadc0f8ebb5a8a017b02de59c86305403dad52cd77b55af062ea1022100"
            "9253cd6c119d4729b77c978e1e2aa19f5ea6e0e52b3f16e32fa608cd5bab753901"
            "ffffffff02008d380c010000001976a9142b4b8072ecbba129b6453c63e129e643"
            "207249ca88ac0065cd1d000000001976a9141b8dd13b994bcfc787b32aeadf58cc"
            "b3615cbd5488ac000000000100000003fdacf9b3eb077412e7a968d2e4f11b9a9d"
            "ee312d666187ed77ee7d26af16cb0b000000008c493046022100ea1608e70911ca"
            "0de5af51ba57ad23b9a51db8d28f82c53563c56a05c20f5a87022100a8bdc8b4a8"
            "acc8634c6b420410150775eb7f2474f5615f7fccd65af30f310fbf01410465fdf4"
            "9e29b06b9a1582287b6279014f834edc317695d125ef623c1cc3aaece245bd69fc"
            "ad7508666e9c74a49dc9056d5fc14338ef38118dc4afae5fe2c585caffffffff30"
            "9e1913634ecb50f3c4f83e96e70b2df071b497b8973a3e75429df397b5af830000"
            "00004948304502202bdb79c596a9ffc24e96f4386199aba386e9bc7b6071516e2b"
            "51dda942b3a1ed022100c53a857e76b724fc14d45311eac5019650d415c3abb542"
            "8f3aae16d8e69bec2301ffffffff2089e33491695080c9edc18a428f7d834db5b6"
            "d372df13ce2b1b0e0cbcb1e6c10000000049483045022100d4ce67c5896ee251c8"
            "10ac1ff9ceccd328b497c8f553ab6e08431e7d40bad6b5022033119c0c2b7d792d"
            "31f1187779c7bd95aefd93d90a715586d73801d9b47471c601ffffffff01007144"
            "60030000001976a914c7b55141d097ea5df7a0ed330cf794376e53ec8d88ac0000"
            "000001000000045bf0e214aa4069a3e792ecee1e1bf0c1d397cde8dd08138f4b72"
            "a00681743447000000008b48304502200c45de8c4f3e2c1821f2fc878cba97b1e6"
            "f8807d94930713aa1c86a67b9bf1e40221008581abfef2e30f957815fc89978423"
            "746b2086375ca8ecf359c85c2a5b7c88ad01410462bb73f76ca0994fcb8b4271e6"
            "fb7561f5c0f9ca0cf6485261c4a0dc894f4ab844c6cdfb97cd0b60ffb5018ffd62"
            "38f4d87270efb1d3ae37079b794a92d7ec95ffffffffd669f7d7958d40fc59d225"
            "3d88e0f248e29b599c80bbcec344a83dda5f9aa72c000000008a47304402207812"
            "4c8beeaa825f9e0b30bff96e564dd859432f2d0cb3b72d3d5d93d38d7e93022069"
            "1d233b6c0f995be5acb03d70a7f7a65b6bc9bdd426260f38a1346669507a360141"
            "0462bb73f76ca0994fcb8b4271e6fb7561f5c0f9ca0cf6485261c4a0dc894f4ab8"
            "44c6cdfb97cd0b60ffb5018ffd6238f4d87270efb1d3ae37079b794a92d7ec95ff"
            "fffffff878af0d93f5229a68166cf051fd372bb7a537232946e0a46f53636b4daf"
            "daa4000000008c493046022100c717d1714551663f69c3c5759bdbb3a0fcd3fab0"
            "23abc0e522fe6440de35d8290221008d9cbe25bffc44af2b18e81c58eb37293fd7"
            "fe1c2e7b46fc37ee8c96c50ab1e201410462bb73f76ca0994fcb8b4271e6fb7561"
            "f5c0f9ca0cf6485261c4a0dc894f4ab844c6cdfb97cd0b60ffb5018ffd6238f4d8"
            "7270efb1d3ae37079b794a92d7ec95ffffffff27f2b668859cd7f2f894aa0fd2d9"
            "e60963bcd07c88973f425f999b8cbfd7a1e2000000008c493046022100e0084714"
            "7cbf517bcc2f502f3ddc6d284358d102ed20d47a8aa788a62f0db780022100d17b"
            "2d6fa84dcaf1c95d88d7e7c30385aecf415588d749afd3ec81f6022cecd7014104"
            "62bb73f76ca0994fcb8b4271e6fb7561f5c0f9ca0cf6485261c4a0dc894f4ab844"
            "c6cdfb97cd0b60ffb5018ffd6238f4d87270efb1d3ae37079b794a92d7ec95ffff"
            "ffff0100c817a8040000001976a914b6efd80d99179f4f4ff6f4dd0a007d018c38"
            "5d2188ac000000000100000001834537b2f1ce8ef9373a258e10545ce5a50b758d"
            "f616cd4356e0032554ebd3c4000000008b483045022100e68f422dd7c34fdce11e"
            "eb4509ddae38201773dd62f284e8aa9d96f85099d0b002202243bd399ff96b649a"
            "0fad05fa759d6a882f0af8c90cf7632c2840c29070aec20141045e58067e815c2f"
            "464c6a2a15f987758374203895710c2d452442e28496ff38ba8f5fd901dc20e29e"
            "88477167fe4fc299bf818fd0d9e1632d467b2a3d9503b1aaffffffff0280d7e636"
            "030000001976a914f34c3e10eb387efe872acb614c89e78bfca7815d88ac404b4c"
            "00000000001976a914a84e272933aaf87e1715d7786c51dfaeb5b65a6f88ac0000"
            "0000010000000143ac81c8e6f6ef307dfe17f3d906d999e23e0189fda838c5510d"
            "850927e03ae7000000008c4930460221009c87c344760a64cb8ae6685a3eec2c1a"
            "c1bed5b88c87de51acd0e124f266c16602210082d07c037359c3a257b5c63ebd90"
            "f5a5edf97b2ac1c434b08ca998839f346dd40141040ba7e521fa7946d12edbb1d1"
            "e95a15c34bd4398195e86433c92b431cd315f455fe30032ede69cad9d1e1ed6c3c"
            "4ec0dbfced53438c625462afb792dcb098544bffffffff0240420f000000000019"
            "76a9144676d1b820d63ec272f1900d59d43bc6463d96f888ac40420f0000000000"
            "1976a914648d04341d00d7968b3405c034adc38d4d8fb9bd88ac00000000010000"
            "000248cc917501ea5c55f4a8d2009c0567c40cfe037c2e71af017d0a452ff705e3"
            "f1000000008b483045022100bf5fdc86dc5f08a5d5c8e43a8c9d5b1ed8c65562e2"
            "80007b52b133021acd9acc02205e325d613e555f772802bf413d36ba807892ed1a"
            "690a77811d3033b3de226e0a01410429fa713b124484cb2bd7b5557b2c0b9df7b2"
            "b1fee61825eadc5ae6c37a9920d38bfccdc7dc3cb0c47d7b173dbc9db8d37db0a3"
            "3ae487982c59c6f8606e9d1791ffffffff41ed70551dd7e841883ab8f0b16bf041"
            "76b7d1480e4f0af9f3d4c3595768d068000000008b4830450221008513ad65187b"
            "903aed1102d1d0c47688127658c51106753fed0151ce9c16b80902201432b9ebcb"
            "87bd04ceb2de66035fbbaf4bf8b00d1cfe41f1a1f7338f9ad79d210141049d4cf8"
            "0125bf50be1709f718c07ad15d0fc612b7da1f5570dddc35f2a352f0f27c978b06"
            "820edca9ef982c35fda2d255afba340068c5035552368bc7200c1488ffffffff01"
            "00093d00000000001976a9148edb68822f1ad580b043c7b3df2e400f8699eb4888"
            "ac00000000"),
        SER_NETWORK, PROTOCOL_VERSION);
    stream >> block;

    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_P2PUBKEY_ONLY);
    // Match the generation pubkey
    filter.insert(ParseHex("04eaafc2314def4ca98ac970241bcab022b9c1e1f4ea423a20f"
                           "134c876f2c01ec0f0dd5b2e86e7168cefe0d81113c3807420ce"
                           "13ad1357231a2252247d97a46a91"));
    // ...and the output address of the 4th transaction
    filter.insert(ParseHex("b6efd80d99179f4f4ff6f4dd0a007d018c385d21"));

    CMerkleBlock merkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    // We should match the generation outpoint
    const TxId txid1(uint256S(
        "0x147caa76786596590baa4e98f5d9f48b86c7765e489f7a6ff3360fe5c674360b"));
    BOOST_CHECK(filter.contains(COutPoint(txid1, 0)));
    // ... but not the 4th transaction's output (its not pay-2-pubkey)
    const TxId txid2(uint256S(
        "0x02981fa052f0481dbc5868f4fc2166035a10f27a03cfd2de67326471df5bc041"));
    BOOST_CHECK(!filter.contains(COutPoint(txid2, 0)));
}

BOOST_AUTO_TEST_CASE(merkle_block_4_test_update_none) {
    // Random real block
    // (000000000000b731f2eef9e8c63173adfb07e41bd53eb0ef0a6b720d6cb6dea4)
    // With 7 txes
    CBlock block;
    CDataStream stream(
        ParseHex(
            "0100000082bb869cf3a793432a66e826e05a6fc37469f8efb7421dc88067010000"
            "0000007f16c5962e8bd963659c793ce370d95f093bc7e367117b3c30c1f8fdd0d9"
            "728776381b4d4c86041b554b852907010000000100000000000000000000000000"
            "00000000000000000000000000000000000000ffffffff07044c86041b0136ffff"
            "ffff0100f2052a01000000434104eaafc2314def4ca98ac970241bcab022b9c1e1"
            "f4ea423a20f134c876f2c01ec0f0dd5b2e86e7168cefe0d81113c3807420ce13ad"
            "1357231a2252247d97a46a91ac000000000100000001bcad20a6a29827d1424f08"
            "989255120bf7f3e9e3cdaaa6bb31b0737fe048724300000000494830450220356e"
            "834b046cadc0f8ebb5a8a017b02de59c86305403dad52cd77b55af062ea1022100"
            "9253cd6c119d4729b77c978e1e2aa19f5ea6e0e52b3f16e32fa608cd5bab753901"
            "ffffffff02008d380c010000001976a9142b4b8072ecbba129b6453c63e129e643"
            "207249ca88ac0065cd1d000000001976a9141b8dd13b994bcfc787b32aeadf58cc"
            "b3615cbd5488ac000000000100000003fdacf9b3eb077412e7a968d2e4f11b9a9d"
            "ee312d666187ed77ee7d26af16cb0b000000008c493046022100ea1608e70911ca"
            "0de5af51ba57ad23b9a51db8d28f82c53563c56a05c20f5a87022100a8bdc8b4a8"
            "acc8634c6b420410150775eb7f2474f5615f7fccd65af30f310fbf01410465fdf4"
            "9e29b06b9a1582287b6279014f834edc317695d125ef623c1cc3aaece245bd69fc"
            "ad7508666e9c74a49dc9056d5fc14338ef38118dc4afae5fe2c585caffffffff30"
            "9e1913634ecb50f3c4f83e96e70b2df071b497b8973a3e75429df397b5af830000"
            "00004948304502202bdb79c596a9ffc24e96f4386199aba386e9bc7b6071516e2b"
            "51dda942b3a1ed022100c53a857e76b724fc14d45311eac5019650d415c3abb542"
            "8f3aae16d8e69bec2301ffffffff2089e33491695080c9edc18a428f7d834db5b6"
            "d372df13ce2b1b0e0cbcb1e6c10000000049483045022100d4ce67c5896ee251c8"
            "10ac1ff9ceccd328b497c8f553ab6e08431e7d40bad6b5022033119c0c2b7d792d"
            "31f1187779c7bd95aefd93d90a715586d73801d9b47471c601ffffffff01007144"
            "60030000001976a914c7b55141d097ea5df7a0ed330cf794376e53ec8d88ac0000"
            "000001000000045bf0e214aa4069a3e792ecee1e1bf0c1d397cde8dd08138f4b72"
            "a00681743447000000008b48304502200c45de8c4f3e2c1821f2fc878cba97b1e6"
            "f8807d94930713aa1c86a67b9bf1e40221008581abfef2e30f957815fc89978423"
            "746b2086375ca8ecf359c85c2a5b7c88ad01410462bb73f76ca0994fcb8b4271e6"
            "fb7561f5c0f9ca0cf6485261c4a0dc894f4ab844c6cdfb97cd0b60ffb5018ffd62"
            "38f4d87270efb1d3ae37079b794a92d7ec95ffffffffd669f7d7958d40fc59d225"
            "3d88e0f248e29b599c80bbcec344a83dda5f9aa72c000000008a47304402207812"
            "4c8beeaa825f9e0b30bff96e564dd859432f2d0cb3b72d3d5d93d38d7e93022069"
            "1d233b6c0f995be5acb03d70a7f7a65b6bc9bdd426260f38a1346669507a360141"
            "0462bb73f76ca0994fcb8b4271e6fb7561f5c0f9ca0cf6485261c4a0dc894f4ab8"
            "44c6cdfb97cd0b60ffb5018ffd6238f4d87270efb1d3ae37079b794a92d7ec95ff"
            "fffffff878af0d93f5229a68166cf051fd372bb7a537232946e0a46f53636b4daf"
            "daa4000000008c493046022100c717d1714551663f69c3c5759bdbb3a0fcd3fab0"
            "23abc0e522fe6440de35d8290221008d9cbe25bffc44af2b18e81c58eb37293fd7"
            "fe1c2e7b46fc37ee8c96c50ab1e201410462bb73f76ca0994fcb8b4271e6fb7561"
            "f5c0f9ca0cf6485261c4a0dc894f4ab844c6cdfb97cd0b60ffb5018ffd6238f4d8"
            "7270efb1d3ae37079b794a92d7ec95ffffffff27f2b668859cd7f2f894aa0fd2d9"
            "e60963bcd07c88973f425f999b8cbfd7a1e2000000008c493046022100e0084714"
            "7cbf517bcc2f502f3ddc6d284358d102ed20d47a8aa788a62f0db780022100d17b"
            "2d6fa84dcaf1c95d88d7e7c30385aecf415588d749afd3ec81f6022cecd7014104"
            "62bb73f76ca0994fcb8b4271e6fb7561f5c0f9ca0cf6485261c4a0dc894f4ab844"
            "c6cdfb97cd0b60ffb5018ffd6238f4d87270efb1d3ae37079b794a92d7ec95ffff"
            "ffff0100c817a8040000001976a914b6efd80d99179f4f4ff6f4dd0a007d018c38"
            "5d2188ac000000000100000001834537b2f1ce8ef9373a258e10545ce5a50b758d"
            "f616cd4356e0032554ebd3c4000000008b483045022100e68f422dd7c34fdce11e"
            "eb4509ddae38201773dd62f284e8aa9d96f85099d0b002202243bd399ff96b649a"
            "0fad05fa759d6a882f0af8c90cf7632c2840c29070aec20141045e58067e815c2f"
            "464c6a2a15f987758374203895710c2d452442e28496ff38ba8f5fd901dc20e29e"
            "88477167fe4fc299bf818fd0d9e1632d467b2a3d9503b1aaffffffff0280d7e636"
            "030000001976a914f34c3e10eb387efe872acb614c89e78bfca7815d88ac404b4c"
            "00000000001976a914a84e272933aaf87e1715d7786c51dfaeb5b65a6f88ac0000"
            "0000010000000143ac81c8e6f6ef307dfe17f3d906d999e23e0189fda838c5510d"
            "850927e03ae7000000008c4930460221009c87c344760a64cb8ae6685a3eec2c1a"
            "c1bed5b88c87de51acd0e124f266c16602210082d07c037359c3a257b5c63ebd90"
            "f5a5edf97b2ac1c434b08ca998839f346dd40141040ba7e521fa7946d12edbb1d1"
            "e95a15c34bd4398195e86433c92b431cd315f455fe30032ede69cad9d1e1ed6c3c"
            "4ec0dbfced53438c625462afb792dcb098544bffffffff0240420f000000000019"
            "76a9144676d1b820d63ec272f1900d59d43bc6463d96f888ac40420f0000000000"
            "1976a914648d04341d00d7968b3405c034adc38d4d8fb9bd88ac00000000010000"
            "000248cc917501ea5c55f4a8d2009c0567c40cfe037c2e71af017d0a452ff705e3"
            "f1000000008b483045022100bf5fdc86dc5f08a5d5c8e43a8c9d5b1ed8c65562e2"
            "80007b52b133021acd9acc02205e325d613e555f772802bf413d36ba807892ed1a"
            "690a77811d3033b3de226e0a01410429fa713b124484cb2bd7b5557b2c0b9df7b2"
            "b1fee61825eadc5ae6c37a9920d38bfccdc7dc3cb0c47d7b173dbc9db8d37db0a3"
            "3ae487982c59c6f8606e9d1791ffffffff41ed70551dd7e841883ab8f0b16bf041"
            "76b7d1480e4f0af9f3d4c3595768d068000000008b4830450221008513ad65187b"
            "903aed1102d1d0c47688127658c51106753fed0151ce9c16b80902201432b9ebcb"
            "87bd04ceb2de66035fbbaf4bf8b00d1cfe41f1a1f7338f9ad79d210141049d4cf8"
            "0125bf50be1709f718c07ad15d0fc612b7da1f5570dddc35f2a352f0f27c978b06"
            "820edca9ef982c35fda2d255afba340068c5035552368bc7200c1488ffffffff01"
            "00093d00000000001976a9148edb68822f1ad580b043c7b3df2e400f8699eb4888"
            "ac00000000"),
        SER_NETWORK, PROTOCOL_VERSION);
    stream >> block;

    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_NONE);
    // Match the generation pubkey
    filter.insert(ParseHex("04eaafc2314def4ca98ac970241bcab022b9c1e1f4ea423a20f"
                           "134c876f2c01ec0f0dd5b2e86e7168cefe0d81113c3807420ce"
                           "13ad1357231a2252247d97a46a91"));
    // ...and the output address of the 4th transaction
    filter.insert(ParseHex("b6efd80d99179f4f4ff6f4dd0a007d018c385d21"));

    CMerkleBlock merkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    // We shouldn't match any outpoints (UPDATE_NONE)
    const TxId txid1(uint256S(
        "0x147caa76786596590baa4e98f5d9f48b86c7765e489f7a6ff3360fe5c674360b"));
    BOOST_CHECK(!filter.contains(COutPoint(txid1, 0)));
    const TxId txid2(uint256S(
        "0x02981fa052f0481dbc5868f4fc2166035a10f27a03cfd2de67326471df5bc041"));
    BOOST_CHECK(!filter.contains(COutPoint(txid2, 0)));
}

static std::vector<uint8_t> RandomData() {
    uint256 r = InsecureRand256();
    return std::vector<uint8_t>(r.begin(), r.end());
}

BOOST_AUTO_TEST_CASE(rolling_bloom) {
    SeedInsecureRand(/* deterministic */ true);
    g_mock_deterministic_tests = true;

    // last-100-entry, 1% false positive:
    constexpr int nElements = 100;
    CRollingBloomFilter rb1(nElements, 0.01);

    // Overfill:
    static const int DATASIZE = 399;
    std::vector<uint8_t> data[DATASIZE];
    for (int i = 0; i < DATASIZE; i++) {
        data[i] = RandomData();
        if (i % 2 == 0)  // alternate between vector API and raw pointer API
            rb1.insert(data[i]);
        else
            rb1.insert(data[i].data(), data[i].size());
    }
    // Last 100 guaranteed to be remembered:
    for (int i = DATASIZE - nElements; i < DATASIZE; i++) {
        if (i % 2 == 1)  // alternate between vector API and raw pointer API
            BOOST_CHECK(rb1.contains(data[i]));
        else
            BOOST_CHECK(rb1.contains(data[i].data(), data[i].size()));
    }

    // false positive rate is 1%, so we should get about 100 hits if
    // testing 10,000 random keys. We get worst-case false positive
    // behavior when the filter is as full as possible, which is
    // when we've inserted one minus an integer multiple of nElement*2.
    unsigned int nHits = 0;
    for (int i = 0; i < 10000; i++) {
        if (rb1.contains(RandomData())) {
            ++nHits;
        }
    }
    // Expect about 100 hits
    BOOST_CHECK_EQUAL(nHits, 75);

    BOOST_CHECK(rb1.contains(data[DATASIZE - 1]));
    rb1.reset();
    BOOST_CHECK(!rb1.contains(data[DATASIZE - 1]));

    // Now roll through data, make sure last 100 entries
    // are always remembered:
    for (int i = 0; i < DATASIZE; i++) {
        if (i >= 100) {
            BOOST_CHECK(rb1.contains(data[i - 100]));
        }
        rb1.insert(data[i]);
        BOOST_CHECK(rb1.contains(data[i]));
    }

    // Insert 999 more random entries:
    for (int i = 0; i < 999; i++) {
        std::vector<uint8_t> d = RandomData();
        rb1.insert(d);
        BOOST_CHECK(rb1.contains(d));
    }
    // Sanity check to make sure the filter isn't just filling up:
    nHits = 0;
    for (int i = 0; i < DATASIZE; i++) {
        if (rb1.contains(data[i])) {
            ++nHits;
        }
    }
    // Expect about 5 false positives
    BOOST_CHECK_EQUAL(nHits, 6);

    // last-1000-entry, 0.01% false positive:
    CRollingBloomFilter rb2(1000, 0.001);
    for (int i = 0; i < DATASIZE; i++) {
        rb2.insert(data[i]);
    }
    // ... room for all of them:
    for (int i = 0; i < DATASIZE; i++) {
        BOOST_CHECK(rb2.contains(data[i]));
    }

    rb2.reset();
    // check hash256 & hash160 APIs
    const uint256 ahash{data[0]};
    auto h160vec = data[1];
    h160vec.resize(uint160::size());
    const uint160 ahash160{h160vec};
    BOOST_CHECK(!rb2.contains(ahash));
    BOOST_CHECK(!rb2.contains(ahash160));
    rb2.insert(ahash);
    rb2.insert(ahash160);
    BOOST_CHECK(rb2.contains(ahash));
    BOOST_CHECK(rb2.contains(ahash160));
    // check raw data ptr APIs
    rb2.insert(data[2].data(), data[2].size());
    BOOST_CHECK(rb2.contains(data[2].data(), data[2].size()));

    g_mock_deterministic_tests = false;
}

BOOST_AUTO_TEST_SUITE_END()
