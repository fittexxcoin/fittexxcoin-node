// Copyright (c) 2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2019 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/fittexxcoin-config.h>
#endif

#include <qt/test/compattests.h>

#if defined(ENABLE_WALLET) && defined(ENABLE_BIP70)
// this includes protobuf's port.h which defines its own bswap macos
#include <qt/paymentrequestplus.h>
#endif

#include <compat/byteswap.h>

void CompatTests::bswapTests() {
    // Sibling in fittexxcoin/src/test/bswap_tests.cpp
    uint16_t u1 = 0x1234;
    uint32_t u2 = 0x56789abc;
    uint64_t u3 = 0xdef0123456789abc;
    uint16_t e1 = 0x3412;
    uint32_t e2 = 0xbc9a7856;
    uint64_t e3 = 0xbc9a78563412f0de;
    QVERIFY(bswap_16(u1) == e1);
    QVERIFY(bswap_32(u2) == e2);
    QVERIFY(bswap_64(u3) == e3);
}
