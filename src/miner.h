// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <primitives/block.h>
#include <txmempool.h>

#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include <cstdint>
#include <memory>

class CBlockIndex;
class CChainParams;
class Config;
class CScript;

namespace Consensus {
struct Params;
}

static const bool DEFAULT_PRINTPRIORITY = false;

struct CBlockTemplateEntry {
    CTransactionRef tx;
    Amount fees;
    int64_t sigChecks;

    CBlockTemplateEntry(CTransactionRef _tx, Amount _fees, int64_t _sigChecks)
        : tx(_tx), fees(_fees), sigChecks(_sigChecks){};
};

struct CBlockTemplate {
    CBlock block;

    std::vector<CBlockTemplateEntry> entries;
};



/** Generate a new block, without valid proof-of-work */
class BlockAssembler {
private:
    // The constructed block template
    std::unique_ptr<CBlockTemplate> pblocktemplate;
    // A convenience pointer that always refers to the CBlock in pblocktemplate
    CBlock *pblock;

    // Configuration parameters for the block size
    uint64_t nMaxGeneratedBlockSize;
    uint64_t nMaxGeneratedBlockSigChecks;
    CFeeRate blockMinFeeRate;

    // Information on the current status of the block
    uint64_t nBlockSize;
    uint64_t nBlockTx;
    uint64_t nBlockSigChecks;
    Amount nFees;

    // Chain context for the block
    int nHeight;
    int64_t nLockTimeCutoff;
    int64_t nMedianTimePast;
    const CChainParams &chainparams;

    const CTxMemPool *mempool;

    const bool fPrintPriority;

public:
    struct Options {
        Options();
        uint64_t nExcessiveBlockSize;
        uint64_t nMaxGeneratedBlockSize;
        CFeeRate blockMinFeeRate;
    };

    BlockAssembler(const Config &config, const CTxMemPool &_mempool);
    BlockAssembler(const CChainParams &params, const CTxMemPool &_mempool,
                   const Options &options);

    /**
     *  Construct a new block template with coinbase to scriptPubKeyIn
     *  @param scriptPubKeyIn  Script to which to send mining reward
     *  @param timeLimitSecs   If >0, limit the amount of time spent
     *                         assembling the block to this time limit,
     *                         in seconds. If <= 0, no time limit.
     *  @param checkValidity   If false, we do not call TestBlockValidity
     *                         and instead assume the block we create
     *                         is valid. This option is offered for
     *                         performance.
     */
    std::unique_ptr<CBlockTemplate>
    CreateNewBlock(const CScript &scriptPubKeyIn, double timeLimitSecs = 0., bool checkValidity = true);

    uint64_t GetMaxGeneratedBlockSize() const { return nMaxGeneratedBlockSize; }

private:
    // utility functions
    /** Clear the block's state and prepare for assembling a new block */
    void resetBlock();
    /** Add a tx to the block */
    void AddToBlock(CTxMemPool::txiter iter);

    // Methods for how to add transactions to a block.
    /**
     * Add transactions from the mempool based on individual tx feerate.
     */
    void addTxs(int64_t nLimitTimePoint)
        EXCLUSIVE_LOCKS_REQUIRED(mempool->cs);

    // helper functions for addTxs()
    /** Test if a new Tx would "fit" in the block */
    bool TestTx(uint64_t txSize, int64_t txSigChecks) const;

    /// Check the transaction for finality, etc before adding to block
    bool CheckTx(const CTransaction &tx) const;
};

/** Modify the extranonce in a block */
void IncrementExtraNonce(CBlock *pblock, const CBlockIndex *pindexPrev, const Config &config,
                         unsigned int &nExtraNonce);
int64_t UpdateTime(CBlockHeader *pblock, const Consensus::Params &params,
                   const CBlockIndex *pindexPrev);
