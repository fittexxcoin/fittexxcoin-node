// Copyright (c) 2012-2015 The Fittexxcoin Core developers
// Copyright (c) 2017-2020 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <compressor.h>

#include <util/system.h>

#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <cstdint>

// amounts 0.00000001 .. 0.00100000
#define NUM_MULTIPLES_UNIT 100000

// amounts 0.01 .. 100.00
#define NUM_MULTIPLES_CENT 10000

// amounts 1 .. 10000
#define NUM_MULTIPLES_1fxx 10000

// amounts 50 .. 21000000
#define NUM_MULTIPLES_50fxx 420000

BOOST_FIXTURE_TEST_SUITE(compress_tests, BasicTestingSetup)

static bool TestEncode(Amount in) {
    return in == DecompressAmount(CompressAmount(in));
}

static bool TestDecode(uint64_t in) {
    return in == CompressAmount(DecompressAmount(in));
}

static bool TestPair(Amount dec, uint64_t enc) {
    return CompressAmount(dec) == enc && DecompressAmount(enc) == dec;
}

BOOST_AUTO_TEST_CASE(compress_amounts) {
    BOOST_CHECK(TestPair(Amount::zero(), 0x0));
    BOOST_CHECK(TestPair(SATOSHI, 0x1));
    BOOST_CHECK(TestPair(CENT, 0x7));
    BOOST_CHECK(TestPair(COIN, 0x9));
    BOOST_CHECK(TestPair(50 * COIN, 0x32));
    BOOST_CHECK(TestPair(21000000 * COIN, 0x1406f40));

    for (int64_t i = 1; i <= NUM_MULTIPLES_UNIT; i++) {
        BOOST_CHECK(TestEncode(i * SATOSHI));
    }

    for (int64_t i = 1; i <= NUM_MULTIPLES_CENT; i++) {
        BOOST_CHECK(TestEncode(i * CENT));
    }

    for (int64_t i = 1; i <= NUM_MULTIPLES_1fxx; i++) {
        BOOST_CHECK(TestEncode(i * COIN));
    }

    for (int64_t i = 1; i <= NUM_MULTIPLES_50fxx; i++) {
        BOOST_CHECK(TestEncode(i * 50 * COIN));
    }

    for (int64_t i = 0; i < 100000; i++) {
        BOOST_CHECK(TestDecode(i));
    }
}

BOOST_AUTO_TEST_SUITE_END()
