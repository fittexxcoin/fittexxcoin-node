// Copyright (c) 2009-2018 The Fittexxcoin Core developers
// Copyright (c) 2020-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <addrman.h>
#include <blockencodings.h>
#include <chain.h>
#include <coins.h>
#include <compressor.h>
#include <consensus/merkle.h>
#include <dsproof/dsproof.h>
#include <net.h>
#include <primitives/block.h>
#include <protocol.h>
#include <pubkey.h>
#include <script/script.h>
#include <streams.h>
#include <undo.h>
#include <version.h>

#include <test/fuzz/fuzz.h>

#include <algorithm>
#include <cstdint>
#include <unistd.h>
#include <vector>

void test_one_input(Span<const uint8_t> buffer) {
    GenericVectorReader ds(SER_NETWORK, INIT_PROTO_VERSION, buffer, 0);
    try {
        int nVersion;
        ds >> nVersion;
        ds.SetVersion(nVersion);
    } catch (const std::ios_base::failure &e) {
        return;
    }

#if BLOCK_DESERIALIZE
    try {
        CBlock block;
        ds >> block;
    } catch (const std::ios_base::failure &e) {
        return;
    }
#elif TRANSACTION_DESERIALIZE
    try {
        CTransaction tx(deserialize, ds);
    } catch (const std::ios_base::failure &e) {
        return;
    }
#elif BLOCKLOCATOR_DESERIALIZE
    try {
        CBlockLocator bl;
        ds >> bl;
    } catch (const std::ios_base::failure &e) {
        return;
    }
#elif BLOCKMERKLEROOT
    try {
        CBlock block;
        ds >> block;
        bool mutated;
        BlockMerkleRoot(block, &mutated);
    } catch (const std::ios_base::failure &e) {
        return;
    }
#elif ADDRMAN_DESERIALIZE
    try {
        CAddrMan am;
        ds >> am;
    } catch (const std::ios_base::failure &e) {
        return;
    }
#elif BLOCKHEADER_DESERIALIZE
    try {
        CBlockHeader bh;
        ds >> bh;
    } catch (const std::ios_base::failure &e) {
        return;
    }
#elif BANENTRY_DESERIALIZE
    try {
        CBanEntry be;
        ds >> be;
    } catch (const std::ios_base::failure &e) {
        return;
    }
#elif TXUNDO_DESERIALIZE
    try {
        CTxUndo tu;
        ds >> tu;
    } catch (const std::ios_base::failure &e) {
        return;
    }
#elif BLOCKUNDO_DESERIALIZE
    try {
        CBlockUndo bu;
        ds >> bu;
    } catch (const std::ios_base::failure &e) {
        return;
    }
#elif COINS_DESERIALIZE
    try {
        Coin coin;
        ds >> coin;
    } catch (const std::ios_base::failure &e) {
        return;
    }
#elif NETADDR_DESERIALIZE
    try {
        CNetAddr na;
        ds >> na;
    } catch (const std::ios_base::failure &e) {
        return;
    }
#elif SERVICE_DESERIALIZE
    try {
        CService s;
        ds >> s;
    } catch (const std::ios_base::failure &e) {
        return;
    }
#elif MESSAGEHEADER_DESERIALIZE
    CMessageHeader::MessageMagic pchMessageStart = {{0x00, 0x00, 0x00, 0x00}};
    try {
        CMessageHeader mh(pchMessageStart);
        ds >> mh;
        if (!mh.IsValidWithoutConfig(pchMessageStart)) {
            return;
        }
    } catch (const std::ios_base::failure &e) {
        return;
    }
#elif ADDRESS_DESERIALIZE
    try {
        CAddress a;
        ds >> a;
    } catch (const std::ios_base::failure &e) {
        return;
    }
#elif INV_DESERIALIZE
    try {
        CInv i;
        ds >> i;
    } catch (const std::ios_base::failure &e) {
        return;
    }
#elif BLOOMFILTER_DESERIALIZE
    try {
        CBloomFilter bf;
        ds >> bf;
    } catch (const std::ios_base::failure &e) {
        return;
    }
#elif DISKBLOCKINDEX_DESERIALIZE
    try {
        CDiskBlockIndex dbi;
        ds >> dbi;
    } catch (const std::ios_base::failure &e) {
        return;
    }
#elif TXOUTCOMPRESSOR_DESERIALIZE
    CTxOut to;
    auto toc = Using<TxOutCompression>(to);
    try {
        ds >> toc;
    } catch (const std::ios_base::failure &e) {
        return;
    }
#elif BLOCKTRANSACTIONS_DESERIALIZE
    try {
        BlockTransactions bt;
        ds >> bt;
    } catch (const std::ios_base::failure &e) {
        return;
    }
#elif BLOCKTRANSACTIONSREQUEST_DESERIALIZE
    try {
        BlockTransactionsRequest btr;
        ds >> btr;
    } catch (const std::ios_base::failure &e) {
        return;
    }
#elif DSPROOF_DESERIALIZE
    try {
        DoubleSpendProof dsp;
        ds >> dsp;
    } catch (const std::ios_base::failure &e) {
        return;
    }
#else
#error Need at least one fuzz target to compile
#endif
}
