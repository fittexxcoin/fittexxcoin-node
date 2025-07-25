// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#if defined(HAVE_CONFIG_H)
#include <config/fittexxcoin-config.h>
#endif

#include <amount.h>
#include <blockfileinfo.h>
#include <chain.h>
#include <coins.h>
#include <consensus/consensus.h>
#include <flatfile.h>
#include <fs.h>
#include <policy/policy.h>
#include <protocol.h> // For CMessageHeader::MessageMagic
#include <script/interpreter.h>
#include <script/script_error.h>
#include <script/script_execution_context.h>
#include <script/script_metrics.h>
#include <sync.h>
#include <versionbits.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <exception>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

class arith_uint256;

class CBlockIndex;
class CBlockTreeDB;
class CBlockUndo;
class CChainParams;
class CChain;
class CCoinsViewDB;
class Config;
class CScriptCheck;
class CTxMemPool;
class CTxUndo;
class CValidationState;

struct FlatFilePos;
struct ChainTxData;
struct PrecomputedTransactionData;
struct LockPoints;

namespace Consensus {
struct Params;
}

#define MIN_TRANSACTION_SIZE                                                   \
    (::GetSerializeSize(CTransaction::null, PROTOCOL_VERSION))

/** Default for -minrelaytxfee, minimum relay fee for transactions */
static constexpr Amount DEFAULT_MIN_RELAY_TX_FEE_PER_KB(1000 * SATOSHI);
/** Default for -excessutxocharge for transactions transactions */
static constexpr Amount DEFAULT_UTXO_FEE = Amount::zero();
//! -maxtxfee default
static constexpr Amount DEFAULT_TRANSACTION_MAXFEE(COIN / 10);
//! Discourage users to set fees higher than this amount (in satoshis) per kB
static constexpr Amount HIGH_TX_FEE_PER_KB(COIN / 100);
/**
 * -maxtxfee will warn if called with a higher fee than this amount (in satoshis
 */
static constexpr Amount HIGH_MAX_TX_FEE(100 * HIGH_TX_FEE_PER_KB);
/**
 * Default for -mempoolexpiry, expiration time for mempool transactions in
 * hours.
 */
static constexpr unsigned int DEFAULT_MEMPOOL_EXPIRY = 336;
/**
 *  Default for -mempoolexpiryperiod, execute the mempool transaction expiration
 *  this often (in hours).
 */
static constexpr int64_t DEFAULT_MEMPOOL_EXPIRY_TASK_PERIOD = 24;
/** The maximum size of a blk?????.dat file (since 0.8) */
static constexpr unsigned int MAX_BLOCKFILE_SIZE = 0x8000000; // 128 MiB
/** The pre-allocation chunk size for blk?????.dat files (since 0.8) */
static constexpr unsigned int BLOCKFILE_CHUNK_SIZE = 0x1000000; // 16 MiB
/** The pre-allocation chunk size for rev?????.dat files (since 0.8) */
static constexpr unsigned int UNDOFILE_CHUNK_SIZE = 0x100000; // 1 MiB

/** Maximum number of dedicated script-checking threads allowed */
static constexpr int MAX_SCRIPTCHECK_THREADS = 15;
/** -par default (number of script-checking threads, 0 = auto) */
static constexpr int DEFAULT_SCRIPTCHECK_THREADS = 0;
/**
 * Number of blocks that can be requested at any given time from a single peer.
 */
static constexpr int MAX_BLOCKS_IN_TRANSIT_PER_PEER = 16;
/**
 * Timeout in seconds during which a peer must stall block download progress
 * before being disconnected.
 */
static constexpr unsigned int BLOCK_STALLING_TIMEOUT = 2;
/**
 * Number of headers sent in one getheaders result. We rely on the assumption
 * that if a peer sends less than this number, we reached its tip. Changing this
 * value is a protocol upgrade.
 */
static constexpr unsigned int MAX_HEADERS_RESULTS = 2000;
/**
 * Maximum depth of blocks we're willing to serve as compact blocks to peers
 * when requested. For older blocks, a regular BLOCK response will be sent.
 */
static constexpr int MAX_CMPCTBLOCK_DEPTH = 5;
/**
 * Maximum depth of blocks we're willing to respond to GETBLOCKTXN requests for.
 */
static constexpr int MAX_BLOCKTXN_DEPTH = 10;
/**
 * Size of the "block download window": how far ahead of our current height do
 * we fetch ? Larger windows tolerate larger download speed differences between
 * peer, but increase the potential degree of disordering of blocks on disk
 * (which make reindexing and in the future perhaps pruning harder). We'll
 * probably want to make this a per-peer adaptive value at some point.
 */
static constexpr unsigned int BLOCK_DOWNLOAD_WINDOW = 1024;
/** Time to wait (in seconds) between writing blocks/block index to disk. */
static constexpr unsigned int DATABASE_WRITE_INTERVAL = 60 * 60;
/** Time to wait (in seconds) between flushing chainstate to disk. */
static constexpr unsigned int DATABASE_FLUSH_INTERVAL = 24 * 60 * 60;
/** Maximum length of reject messages. */
static constexpr unsigned int MAX_REJECT_MESSAGE_LENGTH = 111;
/** Block download timeout base, expressed in millionths of the block interval
 * (i.e. 10 min) */
static constexpr int64_t BLOCK_DOWNLOAD_TIMEOUT_BASE = 1000000;
/**
 * Additional block download timeout per parallel downloading peer (i.e. 5 min)
 */
static constexpr int64_t BLOCK_DOWNLOAD_TIMEOUT_PER_PEER = 500000;

static constexpr int64_t DEFAULT_MAX_TIP_AGE = 24 * 60 * 60;
/**
 * Maximum age of our tip in seconds for us to be considered current for fee
 * estimation.
 */
static constexpr int64_t MAX_FEE_ESTIMATION_TIP_AGE = 3 * 60 * 60;

/** Default for -permitbaremultisig */
static constexpr bool DEFAULT_PERMIT_BAREMULTISIG = true;
static constexpr bool DEFAULT_CHECKPOINTS_ENABLED = true;
static constexpr bool DEFAULT_TXINDEX = false;
static constexpr unsigned int DEFAULT_BANSCORE_THRESHOLD = 100;

/** Default for -persistmempool */
static constexpr bool DEFAULT_PERSIST_MEMPOOL = true;
/** Default for using fee filter */
static constexpr bool DEFAULT_FEEFILTER = true;

/**
 * Maximum number of headers to announce when relaying blocks with headers
 * message.
 */
static constexpr unsigned int MAX_BLOCKS_TO_ANNOUNCE = 8;

/** Maximum number of unconnecting headers announcements before DoS score */
static constexpr int MAX_UNCONNECTING_HEADERS = 10;

static constexpr bool DEFAULT_PEERBLOOMFILTERS = true;

/** Default for -stopatheight */
static constexpr int DEFAULT_STOPATHEIGHT = 0;
/** Default for -maxreorgdepth */
static constexpr int DEFAULT_MAX_REORG_DEPTH = 10;
/** Default for -finalizeheaders */
static constexpr bool DEFAULT_FINALIZE_HEADERS = true;
/** Default DoS score for finalized header violation - range 0..100 */
static constexpr unsigned int DEFAULT_FINALIZE_HEADERS_PENALTY = 100;
/**
 * Default for -finalizationdelay
 * This is the minimum time between a block header reception and the block
 * finalization.
 * This value should be >> block propagation and validation time
 */
static constexpr int64_t DEFAULT_MIN_FINALIZATION_DELAY = 2 * 60 * 60;
/** Default for -parkdeepreorg */
static constexpr bool DEFAULT_PARK_DEEP_REORG = true;
/** Default for -automaticunparking */
static constexpr bool DEFAULT_AUTOMATIC_UNPARKING = true;

extern CScript COINBASE_FLAGS;
extern RecursiveMutex cs_main;
extern CTxMemPool g_mempool;
extern uint64_t nLastBlockTx;
extern uint64_t nLastBlockSize;
extern const std::string strMessageMagic;
extern Mutex g_best_block_mutex;
extern std::condition_variable g_best_block_cv;
extern uint256 g_best_block;
extern std::atomic_bool fImporting;
extern std::atomic_bool fReindex;
extern bool fIsBareMultisigStd;
extern bool fRequireStandard;
extern bool fCheckBlockIndex;
extern bool fCheckBlockReads;
extern bool fCheckpointsEnabled;
extern size_t nCoinCacheUsage;

/**
 * A fee rate smaller than this is considered zero fee (for relaying, mining and
 * transaction creation)
 */
extern CFeeRate minRelayTxFee;
/**
 * Absolute maximum transaction fee (in satoshis) used by wallet and mempool
 * (rejects high fee in sendrawtransaction)
 */
extern Amount maxTxFee;
/**
 * If the tip is older than this (in seconds), the node is considered to be in
 * initial block download.
 */
extern int64_t nMaxTipAge;

/**
 * Block hash whose ancestors we will assume to have valid scripts without
 * checking them.
 */
extern BlockHash hashAssumeValid;

/**
 * Minimum work we will assume exists on some valid chain.
 */
extern arith_uint256 nMinimumChainWork;

/**
 * Best header we've seen so far (used for getheaders queries' starting points).
 */
extern CBlockIndex *pindexBestHeader;

/** Pruning-related variables and constants */
/** True if any block files have ever been pruned. */
extern bool fHavePruned;
/** True if we're running in -prune mode. */
extern bool fPruneMode;
/** Number of MiB of block files that we're trying to stay below. */
extern uint64_t nPruneTarget;
/**
 * Block files containing a block-height within MIN_BLOCKS_TO_KEEP of
 * ::ChainActive().Tip() will not be pruned.
 */
static constexpr unsigned int MIN_BLOCKS_TO_KEEP = 288;
/** Minimum blocks required to signal NODE_NETWORK_LIMITED */
static constexpr unsigned int NODE_NETWORK_LIMITED_MIN_BLOCKS = 288;

static constexpr signed int DEFAULT_CHECKBLOCKS = 6;
static constexpr unsigned int DEFAULT_CHECKLEVEL = 3;

/**
 * Require that user allocate at least 550MB for block & undo files (blk???.dat
 * and rev???.dat)
 * At 1MB per block, 288 blocks = 288MB.
 * Add 15% for Undo data = 331MB
 * Add 20% for Orphan block rate = 397MB
 * We want the low water mark after pruning to be at least 397 MB and since we
 * prune in full block file chunks, we need the high water mark which triggers
 * the prune to be one 128MB block file + added 15% undo data = 147MB greater
 * for a total of 545MB. Setting the target to > than 550MB will make it likely
 * we can respect the target.
 */
static constexpr uint64_t MIN_DISK_SPACE_FOR_BLOCK_FILES = 550 * 1024 * 1024;

class BlockValidationOptions {
private:
    uint64_t excessiveBlockSize;
    bool checkPoW : 1;
    bool checkMerkleRoot : 1;

public:
    // Do full validation by default
    BlockValidationOptions(const Config &config);
    BlockValidationOptions(uint64_t _excessiveBlockSize, bool _checkPow = true,
                           bool _checkMerkleRoot = true)
        : excessiveBlockSize(_excessiveBlockSize), checkPoW(_checkPow),
          checkMerkleRoot(_checkMerkleRoot) {}

    BlockValidationOptions withCheckPoW(bool _checkPoW = true) const {
        BlockValidationOptions ret = *this;
        ret.checkPoW = _checkPoW;
        return ret;
    }

    BlockValidationOptions
    withCheckMerkleRoot(bool _checkMerkleRoot = true) const {
        BlockValidationOptions ret = *this;
        ret.checkMerkleRoot = _checkMerkleRoot;
        return ret;
    }

    bool shouldValidatePoW() const { return checkPoW; }
    bool shouldValidateMerkleRoot() const { return checkMerkleRoot; }
    uint64_t getExcessiveBlockSize() const { return excessiveBlockSize; }
};

/**
 * Process an incoming block. This only returns after the best known valid
 * block is made active. Note that it does not, however, guarantee that the
 * specific block passed to it has been checked for validity!
 *
 * If you want to *possibly* get feedback on whether pblock is valid, you must
 * install a CValidationInterface (see validationinterface.h) - this will have
 * its BlockChecked method called whenever *any* block completes validation.
 *
 * Note that we guarantee that either the proof-of-work is valid on pblock, or
 * (and possibly also) BlockChecked will have been called.
 *
 * May not be called in a validationinterface callback.
 *
 * @param[in]   config  The global config.
 * @param[in]   pblock  The block we want to process.
 * @param[in]   fForceProcessing Process this block even if unrequested; used
 * for non-network block sources and whitelisted peers.
 * @param[out]  fNewBlock A boolean which is set to indicate if the block was
 *                        first received via this call.
 * @return True if the block is accepted as a valid block.
 */
bool ProcessNewBlock(const Config &config,
                     const std::shared_ptr<const CBlock> pblock,
                     bool fForceProcessing, bool *fNewBlock)
    LOCKS_EXCLUDED(cs_main);

/**
 * Process incoming block headers.
 *
 * May not be called in a validationinterface callback.
 *
 * @param[in]  config        The config.
 * @param[in]  block         The block headers themselves.
 * @param[out] state         This may be set to an Error state if any error
 *                           occurred processing them.
 * @param[out] ppindex       If set, the pointer will be set to point to the
 *                           last new block index object for the given headers.
 * @param[out] first_invalid First header that fails validation, if one exists.
 * @return True if block headers were accepted as valid.
 */
bool ProcessNewBlockHeaders(const Config &config,
                            const std::vector<CBlockHeader> &block,
                            CValidationState &state,
                            const CBlockIndex **ppindex = nullptr,
                            CBlockHeader *first_invalid = nullptr)
    LOCKS_EXCLUDED(cs_main);

/**
 * Open a block file (blk?????.dat).
 */
FILE *OpenBlockFile(const FlatFilePos &pos, bool fReadOnly = false);

/**
 * Translation to a filesystem path.
 */
fs::path GetBlockPosFilename(const FlatFilePos &pos);

/**
 * Import blocks from an external file.
 */
void LoadExternalBlockFile(const Config &config, FILE *fileIn,
                           FlatFilePos *dbp = nullptr);

/**
 * Ensures we have a genesis block in the block tree, possibly writing one to
 * disk.
 */
bool LoadGenesisBlock(const CChainParams &chainparams);

/**
 * Load the block tree and coins database from disk, initializing state if we're
 * running with -reindex.
 */
bool LoadBlockIndex(const Config &config) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/**
 * Update the chain tip based on database information.
 */
bool LoadChainTip(const Config &config) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/**
 * Unload database information.
 */
void UnloadBlockIndex();

/** Run instances of script checking worker threads */
void StartScriptCheckWorkerThreads(int threads_num);
/** Stop all of the script checking worker threads */
void StopScriptCheckWorkerThreads();

/**
 * Check whether we are doing an initial block download (synchronizing from disk
 * or network)
 */
bool IsInitialBlockDownload();

/**
 * Retrieve a transaction (from memory pool, or from disk, if possible).
 */
bool GetTransaction(const TxId &txid, CTransactionRef &txOut,
                    const Consensus::Params &params, BlockHash &hashBlock,
                    bool fAllowSlow = false,
                    const CBlockIndex *const blockIndex = nullptr);

/**
 * Find the best known block, and make it the tip of the block chain
 *
 * May not be called with cs_main held. May not be called in a
 * validationinterface callback.
 */
bool ActivateBestChain(
    const Config &config, CValidationState &state,
    std::shared_ptr<const CBlock> pblock = std::shared_ptr<const CBlock>());
Amount GetBlockSubsidy(int nHeight, const Consensus::Params &consensusParams);

/**
 * Guess verification progress (as a fraction between 0.0=genesis and
 * 1.0=current tip).
 */
double GuessVerificationProgress(const ChainTxData &data,
                                 const CBlockIndex *pindex);

/**
 * Calculate the amount of disk space the block & undo files currently use.
 */
uint64_t CalculateCurrentUsage();

/**
 * Mark one block file as pruned.
 */
void PruneOneBlockFile(const int fileNumber) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/**
 * Actually unlink the specified files
 */
void UnlinkPrunedFiles(const std::set<int> &setFilesToPrune);

/** Flush all state, indexes and buffers to disk. */
void FlushStateToDisk();
/** Prune block files and flush state to disk. */
void PruneAndFlush();
/** Prune block files up to a given height */
void PruneBlockFilesManual(int nManualPruneHeight);

/**
 * (try to) add transaction to memory pool
 */
bool AcceptToMemoryPool(const Config &config, CTxMemPool &pool,
                        CValidationState &state, const CTransactionRef &tx,
                        bool *pfMissingInputs, bool bypass_limits,
                        const Amount nAbsurdFee, bool test_accept = false)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/**
 * (try to) add transaction to memory pool with a specified acceptance time,
 * and an optional height override.
 */
bool
AcceptToMemoryPoolWithTime(const Config &config, CTxMemPool &pool,
                           CValidationState &state, const CTransactionRef &tx,
                           bool *pfMissingInputs, int64_t nAcceptTime,
                           bool bypass_limits, const Amount nAbsurdFee,
                           bool test_accept = false)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/** Convert CValidationState to a human-readable message for logging */
std::string FormatStateMessage(const CValidationState &state);

/**
 * Simple class for regulating resource usage during CheckInputs (and
 * CScriptCheck), atomic so as to be compatible with parallel validation.
 */
class CheckInputsLimiter {
protected:
    std::atomic<int64_t> remaining;

public:
    CheckInputsLimiter(int64_t limit) : remaining(limit) {}

    bool consume_and_check(int consumed) {
        auto newvalue = (remaining -= consumed);
        return newvalue >= 0;
    }

    bool check() { return remaining >= 0; }
};

class TxSigCheckLimiter : public CheckInputsLimiter {
public:
    TxSigCheckLimiter() : CheckInputsLimiter(MAX_TX_SIGCHECKS) {}

    // Let's make this bad boy copiable.
    TxSigCheckLimiter(const TxSigCheckLimiter &rhs)
        : CheckInputsLimiter(rhs.remaining.load()) {}

    TxSigCheckLimiter &operator=(const TxSigCheckLimiter &rhs) {
        remaining = rhs.remaining.load();
        return *this;
    }

    static TxSigCheckLimiter getDisabled() {
        TxSigCheckLimiter txLimiter;
        // Historically, there has not been a transaction with more than 20k sig
        // checks on testnet or mainnet, so this effectively disable sigchecks.
        txLimiter.remaining = 20000;
        return txLimiter;
    }
};

/**
 * Check whether all inputs of this transaction are valid (no double spends,
 * scripts & sigs, amounts). This does not modify the UTXO set.
 *
 * If pvChecks is not nullptr, script checks are pushed onto it instead of being
 * performed inline. Any script checks which are not necessary (eg due to script
 * execution cache hits) are, obviously, not pushed onto pvChecks/run.
 *
 * Upon success nSigChecksOut will be filled in with either:
 * - correct total for all inputs, or,
 * - 0, in the case when checks were pushed onto pvChecks (i.e., a cache miss
 * with pvChecks non-null), in which case the total can be found by executing
 * pvChecks and adding the results.
 *
 * Setting sigCacheStore/scriptCacheStore to false will remove elements from the
 * corresponding cache which are matched. This is useful for checking blocks
 * where we will likely never need the cache entry again.
 *
 * pLimitSigChecks can be passed to limit the sigchecks count either in parallel
 * or serial validation. With pvChecks null (serial validation), breaking the
 * pLimitSigChecks limit will abort evaluation early and return false. With
 * pvChecks not-null (parallel validation): the cached nSigChecks may itself
 * break the limit in which case false is returned, OR, each entry in the
 * returned pvChecks must be executed exactly once in order to probe the limit
 * accurately.
 */
bool CheckInputs(const CTransaction &tx, CValidationState &state,
                 const CCoinsViewCache &view, bool fScriptChecks,
                 const uint32_t flags, bool sigCacheStore, bool scriptCacheStore,
                 PrecomputedTransactionData &txdata /* in/out param */, int &nSigChecksOut,
                 TxSigCheckLimiter &txLimitSigChecks,
                 CheckInputsLimiter *pBlockLimitSigChecks,
                 std::vector<CScriptCheck> *pvChecks)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/**
 * Handy shortcut to full fledged CheckInputs call.
 */
inline bool
CheckInputs(const CTransaction &tx, CValidationState &state, const CCoinsViewCache &view, bool fScriptChecks,
            const uint32_t flags, bool sigCacheStore, bool scriptCacheStore, PrecomputedTransactionData &txdata /* in/out param */,
            int &nSigChecksOut)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main) {
    TxSigCheckLimiter nSigChecksTxLimiter;
    return CheckInputs(tx, state, view, fScriptChecks, flags, sigCacheStore,
                       scriptCacheStore, txdata, nSigChecksOut, nSigChecksTxLimiter, nullptr, nullptr);
}

/**
 * Mark all the coins corresponding to a given transaction inputs as spent.
 */
void SpendCoins(CCoinsViewCache &view, const CTransaction &tx, CTxUndo &txundo,
                int nHeight);

/**
 * Apply the effects of this transaction on the UTXO set represented by view.
 */
void UpdateCoins(CCoinsViewCache &view, const CTransaction &tx, int nHeight);
void UpdateCoins(CCoinsViewCache &view, const CTransaction &tx, CTxUndo &txundo,
                 int nHeight);

/**
 * Test whether the LockPoints height and time are still valid on the current
 * chain.
 */
bool TestLockPointValidity(const LockPoints *lp)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/**
 * Check if transaction will be BIP 68 final in the next block to be created.
 *
 * Simulates calling SequenceLocks() with data from the tip of the current
 * active chain. Optionally stores in LockPoints the resulting height and time
 * calculated and the hash of the block needed for calculation or skips the
 * calculation and uses the LockPoints passed in for evaluation. The LockPoints
 * should not be considered valid if CheckSequenceLocks returns false.
 *
 * See consensus/consensus.h for flag definitions.
 */
bool CheckSequenceLocks(const CTxMemPool &pool, const CTransaction &tx,
                        int flags, LockPoints *lp = nullptr,
                        bool useExistingLockPoints = false)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/**
 * Closure representing one script verification.
 * Note that this stores references to the spending transaction.
 *
 * Note that if pLimitSigChecks is passed, then failure does not imply that
 * scripts have failed.
 */
class CScriptCheck {
private:
    ScriptExecutionContextOpt context;
    uint32_t nFlags{};
    bool cacheStore{};
    ScriptError error{ScriptError::UNKNOWN};
    ScriptExecutionMetrics metrics{};
    PrecomputedTransactionData txdata{};
    TxSigCheckLimiter *pTxLimitSigChecks{};
    CheckInputsLimiter *pBlockLimitSigChecks{};

public:
    CScriptCheck() = default;

    CScriptCheck(const ScriptExecutionContext &contextIn,
                 uint32_t nFlagsIn, bool cacheIn,
                 const PrecomputedTransactionData &txdataIn,
                 TxSigCheckLimiter *pTxLimitSigChecksIn = nullptr,
                 CheckInputsLimiter *pBlockLimitSigChecksIn = nullptr)
        : context(contextIn), nFlags(nFlagsIn), cacheStore(cacheIn),
          error(ScriptError::UNKNOWN), txdata(txdataIn),
          pTxLimitSigChecks(pTxLimitSigChecksIn),
          pBlockLimitSigChecks(pBlockLimitSigChecksIn) {}

    bool operator()();

    void swap(CScriptCheck &check) {
        context.swap(check.context);
        std::swap(nFlags, check.nFlags);
        std::swap(cacheStore, check.cacheStore);
        std::swap(error, check.error);
        std::swap(metrics, check.metrics);
        std::swap(txdata, check.txdata);
        std::swap(pTxLimitSigChecks, check.pTxLimitSigChecks);
        std::swap(pBlockLimitSigChecks, check.pBlockLimitSigChecks);
    }

    ScriptError GetScriptError() const { return error; }

    ScriptExecutionMetrics GetScriptExecutionMetrics() const { return metrics; }
};

/** Functions for disk access for blocks */
bool ReadBlockFromDisk(CBlock &block, const FlatFilePos &pos,
                       const Consensus::Params &params);
bool ReadBlockFromDisk(CBlock &block, const CBlockIndex *pindex,
                       const Consensus::Params &params);
/**
 * Read raw block bytes from disk. Faster than the above, because this function just returns the raw block data without
 * any unserialization. Intended to be used by the net code for low-overhead serving of block data.
 * `nType` and `nVersion` parameters are used for `-checkblockreads` sanity checking of the serialized data. */
bool ReadRawBlockFromDisk(std::vector<uint8_t> &rawBlock, const CBlockIndex *pindex, const CChainParams &chainParams,
                          int nType, int nVersion);

bool UndoReadFromDisk(CBlockUndo &blockundo, const CBlockIndex *pindex);

/** Functions for validating blocks and updating the block tree */

/**
 * Context-independent validity checks.
 *
 * Returns true if the provided block is valid (has valid header,
 * transactions are valid, block is a valid size, etc.)
 */
bool CheckBlock(const CBlock &block, CValidationState &state,
                const Consensus::Params &params,
                BlockValidationOptions validationOptions);

/**
 * This is a variant of ContextualCheckTransaction which computes the contextual
 * check for a transaction based on the chain tip.
 *
 * See consensus/consensus.h for flag definitions.
 */
bool ContextualCheckTransactionForCurrentBlock(const Consensus::Params &params,
                                               const CTransaction &tx,
                                               CValidationState &state,
                                               int flags = -1);

/**
 * Check a block is completely valid from start to finish (only works on top of
 * our current best block)
 */
bool TestBlockValidity(CValidationState &state, const CChainParams &params,
                       const CBlock &block, CBlockIndex *pindexPrev,
                       BlockValidationOptions validationOptions)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/**
 * RAII wrapper for VerifyDB: Verify consistency of the block and coin
 * databases.
 */
class CVerifyDB {
public:
    CVerifyDB();
    ~CVerifyDB();
    bool VerifyDB(const Config &config, CCoinsView *coinsview, int nCheckLevel,
                  int nCheckDepth);
};

/** Replay blocks that aren't fully applied to the database. */
bool ReplayBlocks(const Consensus::Params &params, CCoinsView *view);

/** Find the last common block between the parameter chain and a locator. */
CBlockIndex *FindForkInGlobalIndex(const CChain &chain,
                                   const CBlockLocator &locator)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/**
 * Mark a block as precious and reorganize.
 *
 * May not be called in a validationinterface callback.
 */
bool PreciousBlock(const Config &config, CValidationState &state,
                   CBlockIndex *pindex) LOCKS_EXCLUDED(cs_main);

/**
 * Mark a block as finalized.
 * A finalized block can not be reorged in any way.
 */
bool FinalizeBlockAndInvalidate(const Config &config, CValidationState &state,
                                CBlockIndex *pindex)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/** Mark a block as invalid. */
bool InvalidateBlock(const Config &config, CValidationState &state,
                     CBlockIndex *pindex);

/** Park a block. */
bool ParkBlock(const Config &config, CValidationState &state,
               CBlockIndex *pindex);

/** Remove invalidity status from a block and its descendants. */
void ResetBlockFailureFlags(CBlockIndex *pindex)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/** Remove parked status from a block and its descendants. */
void UnparkBlockAndChildren(CBlockIndex *pindex)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/** Remove parked status from a block. */
void UnparkBlock(CBlockIndex *pindex) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/**
 * Retrieve the topmost finalized block.
 */
const CBlockIndex *GetFinalizedBlock() EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/**
 * Checks if a block is finalized.
 */
bool IsBlockFinalized(const CBlockIndex *pindex)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/**
 * Get the most-work chain. Caution: `cs_main` must be held to call methods on or otherwise operate on this object!
 * @returns the most-work chain.
 */
CChain &ChainActive();

/**
 * Global variable that points to the coins database (protected by cs_main)
 */
extern std::unique_ptr<CCoinsViewDB> pcoinsdbview;

/**
 * Global variable that points to the active CCoinsView (protected by cs_main)
 */
extern std::unique_ptr<CCoinsViewCache> pcoinsTip;

/**
 * Global variable that points to the active block tree (protected by cs_main)
 */
extern std::unique_ptr<CBlockTreeDB> pblocktree;

/**
 * Return the spend height, which is one more than the inputs.GetBestBlock().
 * While checking, GetBestBlock() refers to the parent block. (protected by
 * cs_main)
 * This is also true for mempool checks.
 */
int GetSpendHeight(const CCoinsViewCache &inputs);

/**
 * Determine what nVersion a new block should use.
 */
int32_t ComputeBlockVersion(const CBlockIndex *pindexPrev,
                            const Consensus::Params &params);

/**
 * Reject codes greater or equal to this can be returned by AcceptToMemPool or
 * AcceptBlock for blocks/transactions, to signal internal conditions. They
 * cannot and should not be sent over the P2P network.
 */
static const unsigned int REJECT_INTERNAL = 0x100;
/** Too high fee. Can not be triggered by P2P transactions */
static const unsigned int REJECT_HIGHFEE = 0x100;
/** Block conflicts with a transaction already known */
static const unsigned int REJECT_AGAINST_FINALIZED = 0x103;

/** Get block file info entry for one block file */
CBlockFileInfo *GetBlockFileInfo(size_t n);

/** Dump the mempool to disk. */
bool DumpMempool(const CTxMemPool &pool);

/** Load the mempool from disk. */
bool LoadMempool(const Config &config, CTxMemPool &pool);

/** Dump all dsproofs to disk. */
bool DumpDSProofs(const CTxMemPool &pool);

/** Load dsproofs from disk. */
bool LoadDSProofs(CTxMemPool &pool);

//! Check whether the block associated with this index entry is pruned or not.
bool IsBlockPruned(const CBlockIndex *pblockindex);

/// This class manages tracking exactly at what block a particular upgrade activated, relative to a block index it is
/// given.  Works correcly even if there is a reorg and/or if the active chain is not being considered.  It was written
/// originally for Upgrade9 activation height tracking, but it is generic enough in that it can be re-used for any
/// future upgrade, if needed.
struct ActivationBlockTracker {
    /// Typedef for a function pointer to one of the Is*Enabled() functions in consensus/activation.h
    /// e.g.: IsUpgrade9Enabled
    using Predicate = bool (*)(const Consensus::Params &, const CBlockIndex *);

    ActivationBlockTracker(Predicate isUpgradeXEnabledFunc) : predicate(isUpgradeXEnabledFunc) {}

    /**
     * @brief GetActivationBlock - Given a block index for which the upgrade in question is already activated, returns
     *                             the activation block for the upgrade. (The activation block is the first block which
     *                             is an ancestor of `pindex` for which `predicate()` returns `true`.
     * @pre pindex **must** have the upgrade activated for itself (e.g. it must be a block index that returns `true` for
     *             `predicate(params, pindex)`. For efficiency, this precondition is not checked!
     * @param params - Consensus params for the global chain, e.g. config.GetChainParams().GetConsensus()
     * @param pindex - Usually the current tip, but not necessarily. pindex need not live on the active chain.
     * @return The block that the upgrade activated. The activation block is the last block mined under the OLD rules,
     *         and the first block for which `predicate()` returns `true`.  The block after this one would be really
     *         the first block where e.g. tokens are enabled if we are considering upgrade9, for example.  May return
     *         pindex itself.  If this function's precondition is met (`pindex` has the upgrade activated), will never
     *         return nullptr.  Otherwise if the precondition is not satisfied, this function's behavior is undefined.
     */
    const CBlockIndex *GetActivationBlock(const CBlockIndex *pindex, const Consensus::Params &params)
        EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    /**
     * For testing purposes.  We cache the activation block index for efficiency. If block indices are freed then this
     * needs to be called to ensure no dangling pointer when a new block tree is created.
     */
    void ResetActivationBlockCache() noexcept EXCLUSIVE_LOCKS_REQUIRED(cs_main) {
        cachedActivationBlock = nullptr;
    }

    /**
     * For testing purposes.  Get the current cached activation block.
     */
    const CBlockIndex *GetActivationBlockCache() const noexcept EXCLUSIVE_LOCKS_REQUIRED(cs_main) {
        return cachedActivationBlock;
    }

    Predicate GetPredicate() const { return predicate; }

private:
    const CBlockIndex *cachedActivationBlock GUARDED_BY(cs_main) = nullptr;
    const Predicate predicate;
};

/// Global object to track the exact height when Upgrade9 activated (needed by Token consensus rules).
extern ActivationBlockTracker g_upgrade9_block_tracker;

/// Returns the script flags which are basically nextBlockScriptFlags | STANDARD_SCRIPT_VERIFY_FLAGS
uint32_t GetMemPoolScriptFlags(const Consensus::Params &params, const CBlockIndex *pindex,
                               uint32_t *nextBlockFlags = nullptr /* out param: block flags without standard */);
