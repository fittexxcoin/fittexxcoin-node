// Copyright (c) 2017-2018 The Fittexxcoin Core developers
// Copyright (c) 2019-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <dbwrapper.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <threadinterrupt.h>
#include <uint256.h>
#include <validationinterface.h>

class CBlockIndex;

/**
 * Base class for indices of blockchain data. This implements
 * CValidationInterface and ensures blocks are indexed sequentially according
 * to their position in the active chain.
 */
class BaseIndex : public CValidationInterface {
protected:
    class DB : public CDBWrapper {
    public:
        DB(const fs::path &path, size_t n_cache_size, bool f_memory = false,
           bool f_wipe = false, bool f_obfuscate = false);

        /// Read block locator of the chain that the txindex is in sync with.
        bool ReadBestBlock(CBlockLocator &locator) const;

        /// Write block locator of the chain that the txindex is in sync with.
        void WriteBestBlock(CDBBatch &batch, const CBlockLocator &locator);
    };

private:
    /// Whether the index is in sync with the main chain. The flag is flipped
    /// from false to true once, after which point this starts processing
    /// ValidationInterface notifications to stay in sync.
    std::atomic<bool> m_synced{false};

    /// The last block in the chain that the index is in sync with.
    std::atomic<const CBlockIndex *> m_best_block_index{nullptr};

    std::thread m_thread_sync;
    CThreadInterrupt m_interrupt;

    /// Sync the index with the block index starting from the current best
    /// block. Intended to be run in its own thread, m_thread_sync, and can be
    /// interrupted with m_interrupt. Once the index gets in sync, the m_synced
    /// flag is set and the BlockConnected ValidationInterface callback takes
    /// over and the sync thread exits.
    void ThreadSync();

    /// Write the current index state (eg. chain block locator and
    /// subclass-specific items) to disk.
    ///
    /// Recommendations for error handling:
    /// If called on a successor of the previous committed best block in the
    /// index, the index can continue processing without risk of corruption,
    /// though the index state will need to catch up from further behind on
    /// reboot. If the new state is not a successor of the previous state (due
    /// to a chain reorganization), the index must halt until Commit succeeds or
    /// else it could end up getting corrupted.
    bool Commit();

protected:
    void
    BlockConnected(const std::shared_ptr<const CBlock> &block,
                   const CBlockIndex *pindex,
                   const std::vector<CTransactionRef> &txn_conflicted) override;

    void ChainStateFlushed(const CBlockLocator &locator) override;

    /// Initialize internal state from the database and block index.
    virtual bool Init();

    /// Write update index entries for a newly connected block.
    virtual bool WriteBlock(const CBlock &block, const CBlockIndex *pindex) {
        return true;
    }

    /// Virtual method called internally by Commit that can be overridden to
    /// atomically commit more index state.
    virtual bool CommitInternal(CDBBatch &batch);

    virtual DB &GetDB() const = 0;

    /// Get the name of the index for display in logs.
    virtual const char *GetName() const = 0;

public:
    /// Destructor interrupts sync thread if running and blocks until it exits.
    virtual ~BaseIndex();

    /// Blocks the current thread until the index is caught up to the current
    /// state of the block chain. This only blocks if the index has gotten in
    /// sync once and only needs to process blocks in the ValidationInterface
    /// queue. If the index is catching up from far behind, this method does
    /// not block and immediately returns false.
    bool BlockUntilSyncedToCurrentChain();

    void Interrupt();

    /// Start initializes the sync state and registers the instance as a
    /// ValidationInterface so that it stays in sync with blockchain updates.
    void Start();

    /// Stops the instance from staying in sync with blockchain updates.
    void Stop();
};
