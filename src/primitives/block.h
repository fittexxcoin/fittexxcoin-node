// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <primitives/blockhash.h>
#include <primitives/transaction.h>
#include <serialize.h>
#include <uint256.h>

#include <utility>

/**
 * Nodes collect new transactions into a block, hash them into a hash tree, and
 * scan through nonce values to make the block's hash satisfy proof-of-work
 * requirements. When they solve the proof-of-work, they broadcast the block to
 * everyone and the block is added to the block chain. The first transaction in
 * the block is a special one that creates a new coin owned by the creator of
 * the block.
 */
class CBlockHeader {
public:
    // header
    int32_t nVersion{};
    BlockHash hashPrevBlock{};
    uint256 hashMerkleRoot{};
    uint32_t nTime{};
    uint32_t nBits{};
    uint32_t nNonce{};

    constexpr CBlockHeader() noexcept = default;

    SERIALIZE_METHODS(CBlockHeader, obj) {
        READWRITE(obj.nVersion, obj.hashPrevBlock, obj.hashMerkleRoot, obj.nTime, obj.nBits, obj.nNonce);
    }

    void SetNull() { *this = CBlockHeader{}; }

    bool IsNull() const { return nBits == 0u; }

    BlockHash GetHash() const;

    int64_t GetBlockTime() const { return nTime; }
};

/// Block headers are 80 bytes. Some code depends on this constant (see: validation.cpp).
inline constexpr size_t BLOCK_HEADER_SIZE = 80u;

class CBlock : public CBlockHeader {
public:
    // network and disk
    std::vector<CTransactionRef> vtx;

    // memory only
    mutable bool fChecked = false;

    CBlock() noexcept = default;

    CBlock(const CBlockHeader &header) : CBlockHeader(header) {}

    SERIALIZE_METHODS(CBlock, obj) {
        READWRITEAS(CBlockHeader, obj);
        READWRITE(obj.vtx);
    }

    void SetNull() {
        CBlockHeader::SetNull();
        vtx.clear();
        fChecked = false;
    }

    CBlockHeader GetBlockHeader() const { return *this; }

    std::string ToString() const;
};

/**
 * Describes a place in the block chain to another node such that if the other
 * node doesn't have the same branch, it can find a recent common trunk.  The
 * further back it is, the further before the fork it may be.
 */
struct CBlockLocator {
    std::vector<BlockHash> vHave;

    CBlockLocator() noexcept = default;

    explicit CBlockLocator(std::vector<BlockHash> &&vHaveIn) noexcept : vHave(std::move(vHaveIn)) {}

    SERIALIZE_METHODS(CBlockLocator, obj) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(nVersion);
        }
        READWRITE(obj.vHave);
    }

    void SetNull() { vHave.clear(); }

    bool IsNull() const { return vHave.empty(); }
};
