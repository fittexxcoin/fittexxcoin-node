// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <blockfileinfo.h>
#include <coins.h>
#include <dbwrapper.h>
#include <flatfile.h>
#include <primitives/block.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

struct BlockHash;
class CBlockIndex;
class CCoinsViewDBCursor;

namespace Consensus {
struct Params;
}

//! No need to periodic flush if at least this much space still available.
static constexpr int MAX_BLOCK_COINSDB_USAGE = 10;
//! -dbcache default (MiB)
static const int64_t nDefaultDbCache = 450;
//! -dbbatchsize default (bytes)
static const int64_t nDefaultDbBatchSize = 16 << 20;
//! max. -dbcache (MiB)
static const int64_t nMaxDbCache = sizeof(void *) > 4 ? 16384 : 1024;
//! min. -dbcache (MiB)
static const int64_t nMinDbCache = 4;
//! Max memory allocated to block tree DB specific cache, if no -txindex (MiB)
static const int64_t nMaxBlockDBCache = 2;
//! Max memory allocated to block tree DB specific cache, if -txindex (MiB)
// Unlike for the UTXO database, for the txindex scenario the leveldb cache make
// a meaningful difference:
// https://github.com/fittexxcoin/fittexxcoin/pull/8273#issuecomment-229601991
static const int64_t nMaxTxIndexCache = 1024;
//! Max memory allocated to coin DB specific cache (MiB)
static const int64_t nMaxCoinsDBCache = 8;

/** CCoinsView backed by the coin database (chainstate/) */
class CCoinsViewDB final : public CCoinsView {
protected:
    CDBWrapper db;

public:
    explicit CCoinsViewDB(size_t nCacheSize, bool fMemory = false,
                          bool fWipe = false);

    bool GetCoin(const COutPoint &outpoint, Coin &coin) const override;
    bool HaveCoin(const COutPoint &outpoint) const override;
    BlockHash GetBestBlock() const override;
    std::vector<BlockHash> GetHeadBlocks() const override;
    bool BatchWrite(CCoinsMap &mapCoins, const BlockHash &hashBlock) override;
    CCoinsViewCursor *Cursor(bool snapshot = false) const override;

    //! Attempt to update from an older database format.
    //! Returns whether an error occurred.
    bool Upgrade();
    size_t EstimateSize() const override;
};

/** Specialization of CCoinsViewCursor to iterate over a CCoinsViewDB */
class CCoinsViewDBCursor : public CCoinsViewCursor {
public:
    ~CCoinsViewDBCursor() {}

    bool GetKey(COutPoint &key) const override;
    bool GetValue(Coin &coin) const override;
    unsigned int GetValueSize() const override;

    bool Valid() const override;
    void Next() override;

private:
    CCoinsViewDBCursor(CDBIterator *pcursorIn, const BlockHash &hashBlockIn)
        : CCoinsViewCursor(hashBlockIn), pcursor(pcursorIn) {}
    std::unique_ptr<CDBIterator> pcursor;
    std::pair<char, COutPoint> keyTmp;

    friend class CCoinsViewDB;
};

/** Access to the block database (blocks/index/) */
class CBlockTreeDB : public CDBWrapper {
public:
    explicit CBlockTreeDB(size_t nCacheSize, bool fMemory = false,
                          bool fWipe = false);

    bool WriteBatchSync(
        const std::vector<std::pair<int, const CBlockFileInfo *>> &fileInfo,
        int nLastFile, const std::vector<const CBlockIndex *> &blockinfo);
    bool ReadBlockFileInfo(int nFile, CBlockFileInfo &info);
    bool ReadLastBlockFile(int &nFile);
    bool WriteReindexing(bool fReindexing);
    bool IsReindexing() const;
    bool WriteFlag(const std::string &name, bool fValue);
    bool ReadFlag(const std::string &name, bool &fValue);
    bool LoadBlockIndexGuts(
        const Consensus::Params &params,
        std::function<CBlockIndex *(const BlockHash &)> insertBlockIndex);
};
