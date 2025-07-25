// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Fittexxcoin Core developers
// Copyright (c) 2018-2022 The Fittexxcoin developers
// Copyright (c) 2022 The Fittexxcoin Node developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <amount.h>
#include <dsproof/dsproof.h>
#include <interfaces/chain.h>
#include <outputtype.h>
#include <primitives/blockhash.h>
#include <script/ismine.h>
#include <script/sign.h>
#include <streams.h>
#include <tinyformat.h>
#include <ui_interface.h>
#include <util/strencodings.h>
#include <util/system.h>
#include <validationinterface.h>
#include <wallet/coinselection.h>
#include <wallet/crypter.h>
#include <wallet/rpcwallet.h>
#include <wallet/walletdb.h>
#include <wallet/walletutil.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

//! Responsible for reading and validating the -wallet arguments and verifying
//! the wallet database.
//  This function will perform salvage on the wallet if requested, as long as
//  only one wallet is being loaded (WalletParameterInteraction forbids
//  -salvagewallet, -zapwallettxes or -upgradewallet with multiwallet).
bool VerifyWallets(const CChainParams &chainParams, interfaces::Chain &chain,
                   const std::vector<std::string> &wallet_files);

//! Load wallet databases.
bool LoadWallets(const CChainParams &chainParams, interfaces::Chain &chain,
                 const std::vector<std::string> &wallet_files);

//! Complete startup of wallets.
void StartWallets(CScheduler &scheduler);

//! Flush all wallets in preparation for shutdown.
void FlushWallets();

//! Stop all wallets. Wallets will be flushed first.
void StopWallets();

//! Close all wallets.
void UnloadWallets();

//! Explicitly unload and delete the wallet.
//! Blocks the current thread after signaling the unload intent so that all
//! wallet clients release the wallet.
//! Note that, when blocking is not required, the wallet is implicitly unloaded
//! by the shared pointer deleter.
void UnloadWallet(std::shared_ptr<CWallet> &&wallet);

bool AddWallet(const std::shared_ptr<CWallet> &wallet);
bool RemoveWallet(const std::shared_ptr<CWallet> &wallet);
bool HasWallets();
std::vector<std::shared_ptr<CWallet>> GetWallets();
std::shared_ptr<CWallet> GetWallet(const std::string &name);

//! Default for -keypool
static constexpr unsigned int DEFAULT_KEYPOOL_SIZE = 1000;
//! -paytxfee default
constexpr Amount DEFAULT_PAY_TX_FEE = Amount::zero();
//! -fallbackfee default
static constexpr Amount DEFAULT_FALLBACK_FEE(20000 * SATOSHI);
//! -mintxfee default
static constexpr Amount DEFAULT_TRANSACTION_MINFEE_PER_KB = 1000 * SATOSHI;
//! minimum recommended increment for BIP 125 replacement txs
static constexpr Amount WALLET_INCREMENTAL_RELAY_FEE(5000 * SATOSHI);
//! Default for -spendzeroconfchange
static constexpr bool DEFAULT_SPEND_ZEROCONF_CHANGE = true;
//! Default for -avoidpartialspends
static constexpr bool DEFAULT_AVOIDPARTIALSPENDS = false;
static constexpr bool DEFAULT_WALLETBROADCAST = true;
static constexpr bool DEFAULT_DISABLE_WALLET = false;
//! Default for -usebip69
static constexpr bool DEFAULT_USE_BIP69 = true;
//! Default for -allowlegacyp2sh
static constexpr bool DEFAULT_ALLOW_LEGACY_P2SH = false;
//! Default for the RPC option "include_unsafe"
static constexpr bool DEFAULT_INCLUDE_UNSAFE_INPUTS = false;
//! Pre-calculated constant for input size estimation
static constexpr size_t DUMMY_P2PKH_INPUT_SIZE = 148;

class CChainParams;
class CCoinControl;
class COutput;
class CReserveKey;
class CScript;
class CTxMemPool;
class CWalletTx;

/** (client) version numbers for particular wallet features */
enum WalletFeature {
    // the earliest version new wallets supports (only useful for
    // getwalletinfo's clientversion output)
    FEATURE_BASE = 10500,

    // wallet encryption
    FEATURE_WALLETCRYPT = 40000,
    // compressed public keys
    FEATURE_COMPRPUBKEY = 60000,

    // Hierarchical key derivation after BIP32 (HD Wallet)
    FEATURE_HD = 130000,

    // Wallet with HD chain split (change outputs will use m/0'/1'/k)
    FEATURE_HD_SPLIT = 160300,

    // Wallet without a default key written
    FEATURE_NO_DEFAULT_KEY = 190700,

    // Upgraded to HD SPLIT and can have a pre-split keypool
    FEATURE_PRE_SPLIT_KEYPOOL = 200300,

    FEATURE_LATEST = FEATURE_PRE_SPLIT_KEYPOOL,
};

//! Default for -addresstype
constexpr OutputType DEFAULT_ADDRESS_TYPE{OutputType::LEGACY};

//! Default for -changetype
constexpr OutputType DEFAULT_CHANGE_TYPE{OutputType::CHANGE_AUTO};

enum WalletFlags : uint64_t {
    // Wallet flags in the upper section (> 1 << 31) will lead to not opening
    // the wallet if flag is unknown.
    // Unknown wallet flags in the lower section <= (1 << 31) will be tolerated.

    // Will enforce the rule that the wallet can't contain any private keys
    // (only watch-only/pubkeys).
    WALLET_FLAG_DISABLE_PRIVATE_KEYS = (1ULL << 32),

    //! Flag set when a wallet contains no HD seed and no private keys, scripts,
    //! addresses, and other watch only things, and is therefore "blank."
    //!
    //! The only function this flag serves is to distinguish a blank wallet from
    //! a newly created wallet when the wallet database is loaded, to avoid
    //! initialization that should only happen on first run.
    //!
    //! This flag is also a mandatory flag to prevent previous versions of
    //! fittexxcoin from opening the wallet, thinking it was newly created, and
    //! then improperly reinitializing it.
    WALLET_FLAG_BLANK_WALLET = (1ULL << 33),
};

static constexpr uint64_t g_known_wallet_flags =
    WALLET_FLAG_DISABLE_PRIVATE_KEYS | WALLET_FLAG_BLANK_WALLET;

/** Return value of CWallet::CreateTransaction */
enum class CreateTransactionResult : uint32_t {
    //! Sucess
    CT_OK,
    //! An error happened
    CT_ERROR,
    //! Error: Invalid input arguments to method
    CT_INVALID_PARAMETER,
    //! Error: The wallet balance is insufficient
    CT_INSUFFICIENT_FUNDS,
    //! Error: The input amount is insufficient for creating the outputs + fee
    CT_INSUFFICIENT_AMOUNT,
};

/** A key pool entry */
class CKeyPool {
public:
    int64_t nTime;
    CPubKey vchPubKey;
    // for change outputs
    bool fInternal;
    // For keys generated before keypool split upgrade
    bool m_pre_split;

    CKeyPool();
    CKeyPool(const CPubKey &vchPubKeyIn, bool internalIn);

    template <typename Stream>
    void Serialize(Stream &s) const {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH)) {
            s << nVersion;
        }
        s << nTime << vchPubKey << fInternal << m_pre_split;
    }

    template <typename Stream>
    void Unserialize(Stream &s) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH)) {
            s >> nVersion;
        }
        s >> nTime >> vchPubKey;
        try {
            s >> fInternal;
        } catch (std::ios_base::failure &) {
            /* flag as external address if we can't read the internal boolean
               (this will be the case for any wallet before the HD chain split version) */
            fInternal = false;
        }
        try {
            s >> m_pre_split;
        } catch (std::ios_base::failure &) {
            /* flag as postsplit address if we can't read the m_pre_split boolean
               (this will be the case for any wallet that upgrades to HD chain split) */
            m_pre_split = false;
        }
    }
};

/** Address book data */
class CAddressBookData {
public:
    std::string name;
    std::string purpose;

    CAddressBookData() : purpose("unknown") {}

    typedef std::map<std::string, std::string> StringMap;
    StringMap destdata;
};

struct CRecipient {
    CScript scriptPubKey;
    Amount nAmount;
    token::OutputDataPtr tokenDataPtr; ///< usually null unless we are sending a token
    bool fSubtractFeeFromAmount;
};

typedef std::map<std::string, std::string> mapValue_t;

static inline void ReadOrderPos(int64_t &nOrderPos, mapValue_t &mapValue) {
    if (!mapValue.count("n")) {
        // TODO: calculate elsewhere
        nOrderPos = -1;
        return;
    }

    nOrderPos = atoi64(mapValue["n"].c_str());
}

static inline void WriteOrderPos(const int64_t &nOrderPos,
                                 mapValue_t &mapValue) {
    if (nOrderPos == -1) {
        return;
    }
    mapValue["n"] = i64tostr(nOrderPos);
}

struct COutputEntry {
    CTxDestination destination;
    Amount amount;
    int vout;
};

/** A transaction with a merkle branch linking it to the block chain. */
class CMerkleTx {
private:
    /** Constant used in hashBlock to indicate tx has been abandoned */
    static const BlockHash ABANDON_HASH;

public:
    CTransactionRef tx;
    BlockHash hashBlock;

    /**
     * An nIndex == -1 means that hashBlock (in nonzero) refers to the earliest
     * block in the chain we know this or any in-wallet dependency conflicts
     * with. Older clients interpret nIndex == -1 as unconfirmed for backward
     * compatibility.
     */
    int nIndex;

    CMerkleTx() {
        SetTx(MakeTransactionRef());
        Init();
    }

    explicit CMerkleTx(CTransactionRef arg) {
        SetTx(std::move(arg));
        Init();
    }

    void Init() {
        hashBlock = BlockHash();
        nIndex = -1;
    }

    void SetTx(CTransactionRef arg) { tx = std::move(arg); }

    SERIALIZE_METHODS(CMerkleTx, obj) {
        // For compatibility with older versions.
        std::vector<uint256> vMerkleBranch;
        READWRITE(obj.tx);
        READWRITE(obj.hashBlock);
        READWRITE(vMerkleBranch);
        READWRITE(obj.nIndex);
    }

    void SetMerkleBranch(const BlockHash &block_hash, int posInBlock);

    /**
     * Return depth of transaction in blockchain:
     * <0  : conflicts with a transaction this deep in the blockchain
     *  0  : in memory pool, waiting to be included in a block
     * >=1 : this many blocks deep in the main chain
     */
    int GetDepthInMainChain(interfaces::Chain::Lock &locked_chain) const;
    bool IsInMainChain(interfaces::Chain::Lock &locked_chain) const {
        return GetDepthInMainChain(locked_chain) > 0;
    }

    /**
     * @return number of blocks to maturity for this transaction:
     *  0 : is not a coinbase transaction, or is a mature coinbase transaction
     * >0 : is a coinbase transaction which matures in this many blocks
     */
    int GetBlocksToMaturity(interfaces::Chain::Lock &locked_chain) const;
    bool hashUnset() const {
        return (hashBlock.IsNull() || hashBlock == ABANDON_HASH);
    }
    bool isAbandoned() const { return (hashBlock == ABANDON_HASH); }
    void setAbandoned() { hashBlock = ABANDON_HASH; }

    TxId GetId() const { return tx->GetId(); }
    bool IsCoinBase() const { return tx->IsCoinBase(); }
    bool IsImmatureCoinBase(interfaces::Chain::Lock &locked_chain) const;
};

// Get the marginal bytes of spending the specified output
int CalculateMaximumSignedInputSize(const CTxOut &txout, const CWallet *pwallet,
                                    bool use_max_sig = false);

/**
 * A transaction with a bunch of additional info that only the owner cares
 * about. It includes any unrecorded transactions needed to link it back to the
 * block chain.
 */
class CWalletTx : public CMerkleTx {
private:
    const CWallet *pwallet;

public:
    /**
     * Key/value map with information about the transaction.
     *
     * The following keys can be read and written through the map and are
     * serialized in the wallet database:
     *
     *     "comment", "to"   - comment strings provided to sendtoaddress,
     *                         and sendmany wallet RPCs
     *     "replaces_txid"   - txid (as HexStr) of transaction replaced by
     *                         bumpfee on transaction created by bumpfee
     *     "replaced_by_txid" - txid (as HexStr) of transaction created by
     *                         bumpfee on transaction replaced by bumpfee
     *     "from", "message" - obsolete fields that could be set in UI prior to
     *                         2011 (removed in commit 4d9b223)
     *
     * The following keys are serialized in the wallet database, but shouldn't
     * be read or written through the map (they will be temporarily added and
     * removed from the map during serialization):
     *
     *     "fromaccount"     - serialized strFromAccount value
     *     "n"               - serialized nOrderPos value
     *     "timesmart"       - serialized nTimeSmart value
     *     "spent"           - serialized vfSpent value that existed prior to
     *                         2014 (removed in commit 93a18a3)
     */
    mapValue_t mapValue;
    std::vector<std::pair<std::string, std::string>> vOrderForm;
    unsigned int fTimeReceivedIsTxTime;
    //! time received by this node
    unsigned int nTimeReceived;
    /**
     * Stable timestamp that never changes, and reflects the order a transaction
     * was added to the wallet. Timestamp is based on the block time for a
     * transaction added as part of a block, or else the time when the
     * transaction was received if it wasn't part of a block, with the timestamp
     * adjusted in both cases so timestamp order matches the order transactions
     * were added to the wallet. More details can be found in
     * CWallet::ComputeTimeSmart().
     */
    unsigned int nTimeSmart;
    /**
     * From me flag is set to 1 for transactions that were created by the wallet
     * on this fittexxcoin node, and set to 0 for transactions that were created
     * externally and came in through the network or sendrawtransaction RPC.
     */
    char fFromMe;
    //! position in ordered transaction list
    int64_t nOrderPos;
    std::multimap<int64_t, CWalletTx *>::const_iterator m_it_wtxOrdered;

    // memory only
    mutable bool fDebitCached;
    mutable bool fCreditCached;
    mutable bool fImmatureCreditCached;
    mutable bool fAvailableCreditCached;
    mutable bool fWatchDebitCached;
    mutable bool fWatchCreditCached;
    mutable bool fImmatureWatchCreditCached;
    mutable bool fAvailableWatchCreditCached;
    mutable bool fChangeCached;
    mutable bool fInMempool;
    mutable bool fDsProofCached;
    mutable Amount nDebitCached;
    mutable Amount nCreditCached;
    mutable Amount nImmatureCreditCached;
    mutable Amount nAvailableCreditCached;
    mutable Amount nWatchDebitCached;
    mutable Amount nWatchCreditCached;
    mutable Amount nImmatureWatchCreditCached;
    mutable Amount nAvailableWatchCreditCached;
    mutable Amount nChangeCached;
    mutable DoubleSpendProof dsProofCached;

    CWalletTx(const CWallet *pwalletIn, CTransactionRef arg)
        : CMerkleTx(std::move(arg)) {
        Init(pwalletIn);
    }

    void Init(const CWallet *pwalletIn) {
        pwallet = pwalletIn;
        mapValue.clear();
        vOrderForm.clear();
        fTimeReceivedIsTxTime = false;
        nTimeReceived = 0;
        nTimeSmart = 0;
        fFromMe = false;
        fDebitCached = false;
        fCreditCached = false;
        fImmatureCreditCached = false;
        fAvailableCreditCached = false;
        fWatchDebitCached = false;
        fWatchCreditCached = false;
        fImmatureWatchCreditCached = false;
        fAvailableWatchCreditCached = false;
        fChangeCached = false;
        fInMempool = false;
        fDsProofCached = false;
        nDebitCached = Amount::zero();
        nCreditCached = Amount::zero();
        nImmatureCreditCached = Amount::zero();
        nAvailableCreditCached = Amount::zero();
        nWatchDebitCached = Amount::zero();
        nWatchCreditCached = Amount::zero();
        nAvailableWatchCreditCached = Amount::zero();
        nImmatureWatchCreditCached = Amount::zero();
        nChangeCached = Amount::zero();
        nOrderPos = -1;
        dsProofCached = DoubleSpendProof();
    }

    template <typename Stream> void Serialize(Stream &s) const {
        char fSpent = false;
        mapValue_t mapValueCopy = mapValue;

        mapValueCopy["fromaccount"] = "";
        WriteOrderPos(nOrderPos, mapValueCopy);
        if (nTimeSmart) {
            mapValueCopy["timesmart"] = strprintf("%u", nTimeSmart);
        }

        s << static_cast<const CMerkleTx &>(*this);
        //! Used to be vtxPrev
        std::vector<CMerkleTx> vUnused;
        s << vUnused << mapValueCopy << vOrderForm << fTimeReceivedIsTxTime
          << nTimeReceived << fFromMe << fSpent;
    }

    template <typename Stream> void Unserialize(Stream &s) {
        Init(nullptr);
        char fSpent;

        s >> static_cast<CMerkleTx &>(*this);
        //! Used to be vtxPrev
        std::vector<CMerkleTx> vUnused;
        s >> vUnused >> mapValue >> vOrderForm >> fTimeReceivedIsTxTime >>
            nTimeReceived >> fFromMe >> fSpent;

        ReadOrderPos(nOrderPos, mapValue);
        nTimeSmart = mapValue.count("timesmart")
                         ? (unsigned int)atoi64(mapValue["timesmart"])
                         : 0;

        mapValue.erase("fromaccount");
        mapValue.erase("spent");
        mapValue.erase("n");
        mapValue.erase("timesmart");
    }

    //! make sure balances are recalculated
    void MarkDirty() {
        fCreditCached = false;
        fAvailableCreditCached = false;
        fImmatureCreditCached = false;
        fWatchDebitCached = false;
        fWatchCreditCached = false;
        fAvailableWatchCreditCached = false;
        fImmatureWatchCreditCached = false;
        fDebitCached = false;
        fChangeCached = false;
        fDsProofCached = false;
    }

    void BindWallet(CWallet *pwalletIn) {
        pwallet = pwalletIn;
        MarkDirty();
    }

    //! filter decides which addresses will count towards the debit
    Amount GetDebit(const isminefilter &filter) const;
    Amount GetCredit(interfaces::Chain::Lock &locked_chain,
                     const isminefilter &filter) const;
    Amount GetImmatureCredit(interfaces::Chain::Lock &locked_chain,
                             bool fUseCache = true) const;
    // TODO: Remove "NO_THREAD_SAFETY_ANALYSIS" and replace it with the correct
    // annotation "EXCLUSIVE_LOCKS_REQUIRED(cs_main, pwallet->cs_wallet)". The
    // annotation "NO_THREAD_SAFETY_ANALYSIS" was temporarily added to avoid
    // having to resolve the issue of member access into incomplete type
    // CWallet.
    Amount GetAvailableCredit(interfaces::Chain::Lock &locked_chain,
                              bool fUseCache = true,
                              const isminefilter &filter = ISMINE_SPENDABLE)
        const NO_THREAD_SAFETY_ANALYSIS;
    Amount GetImmatureWatchOnlyCredit(interfaces::Chain::Lock &locked_chain,
                                      const bool fUseCache = true) const;
    Amount GetChange() const;
    DoubleSpendProof GetDsProof() const;

    // Get the marginal bytes if spending the specified output from this
    // transaction
    int GetSpendSize(unsigned int out, bool use_max_sig = false) const {
        return CalculateMaximumSignedInputSize(tx->vout[out], pwallet,
                                               use_max_sig);
    }

    void GetAmounts(std::list<COutputEntry> &listReceived,
                    std::list<COutputEntry> &listSent, Amount &nFee,
                    const isminefilter &filter) const;

    bool IsFromMe(const isminefilter &filter) const {
        return GetDebit(filter) > Amount::zero();
    }

    // True if only scriptSigs are different
    bool IsEquivalentTo(const CWalletTx &tx) const;

    bool InMempool() const;
    bool IsTrusted(interfaces::Chain::Lock &locked_chain) const;
    bool IsDoubleSpent() const { return !GetDsProof().isEmpty(); }

    int64_t GetTxTime() const;

    // RelayWalletTransaction may only be called if fBroadcastTransactions!
    bool RelayWalletTransaction(interfaces::Chain::Lock &locked_chain,
                                CConnman *connman);

    /**
     * Pass this transaction to the mempool. Fails if absolute fee exceeds
     * absurd fee.
     */
    bool AcceptToMemoryPool(interfaces::Chain::Lock &locked_chain,
                            const Amount nAbsurdFee, CValidationState &state);

    // TODO: Remove "NO_THREAD_SAFETY_ANALYSIS" and replace it with the correct
    // annotation "EXCLUSIVE_LOCKS_REQUIRED(pwallet->cs_wallet)". The annotation
    // "NO_THREAD_SAFETY_ANALYSIS" was temporarily added to avoid having to
    // resolve the issue of member access into incomplete type CWallet. Note
    // that we still have the runtime check "AssertLockHeld(pwallet->cs_wallet)"
    // in place.
    std::set<TxId> GetConflicts() const NO_THREAD_SAFETY_ANALYSIS;
};

class COutput {
public:
    const CWalletTx *tx;
    int i;
    int nDepth;

    /**
     * Pre-computed estimated size of this output as a fully-signed input in a
     * transaction. Can be -1 if it could not be calculated.
     */
    int nInputBytes;

    /** Whether we have the private keys to spend this output */
    bool fSpendable;

    /** Whether we know how to spend this output, ignoring the lack of keys */
    bool fSolvable;

    /**
     * Whether to use the maximum sized, 72 byte signature when calculating the
     * size of the input spend. This should only be set when watch-only outputs
     * are allowed.
     */
    bool use_max_sig;

    /**
     * Whether this output is considered safe to spend. Unconfirmed transactions
     * from outside keys are considered unsafe and will not be used to fund new
     * spending transactions.
     */
    bool fSafe;

    COutput(const CWalletTx *txIn, int iIn, int nDepthIn, bool fSpendableIn,
            bool fSolvableIn, bool fSafeIn, bool use_max_sig_in = false) {
        tx = txIn;
        i = iIn;
        nDepth = nDepthIn;
        fSpendable = fSpendableIn;
        fSolvable = fSolvableIn;
        fSafe = fSafeIn;
        nInputBytes = -1;
        use_max_sig = use_max_sig_in;
        // If known and signable by the given wallet, compute nInputBytes
        // Failure will keep this value -1
        if (fSpendable && tx) {
            nInputBytes = tx->GetSpendSize(i, use_max_sig);
        }
    }

    std::string ToString() const;

    inline CInputCoin GetInputCoin() const {
        return CInputCoin(tx->tx, i, nInputBytes);
    }
};

/** Private key that includes an expiration date in case it never gets used. */
class CWalletKey {
public:
    CPrivKey vchPrivKey;
    int64_t nTimeCreated;
    int64_t nTimeExpires;
    std::string strComment;
    // todo: add something to note what created it (user, getnewaddress,
    // change) maybe should have a map<string, string> property map

    explicit CWalletKey(int64_t nExpires = 0);

    SERIALIZE_METHODS(CWalletKey, obj) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(nVersion);
        }
        READWRITE(obj.vchPrivKey);
        READWRITE(obj.nTimeCreated);
        READWRITE(obj.nTimeExpires);
        READWRITE(LIMITED_STRING(obj.strComment, 65536));
    }
};

struct CoinSelectionParams {
    bool use_bnb = true;
    size_t change_output_size = 0;
    size_t change_spend_size = 0;
    CFeeRate effective_fee = CFeeRate(Amount::zero());
    size_t tx_noinputs_size = 0;

    CoinSelectionParams(bool use_bnb_, size_t change_output_size_,
                        size_t change_spend_size_, CFeeRate effective_fee_,
                        size_t tx_noinputs_size_)
        : use_bnb(use_bnb_), change_output_size(change_output_size_),
          change_spend_size(change_spend_size_), effective_fee(effective_fee_),
          tx_noinputs_size(tx_noinputs_size_) {}
    CoinSelectionParams() {}
};

// forward declarations for ScanForWalletTransactions/RescanFromTime
class WalletRescanReserver;

/**
 * A CWallet is an extension of a keystore, which also maintains a set of
 * transactions and balances, and provides the ability to create new
 * transactions.
 */
class CWallet final : public CCryptoKeyStore, public CValidationInterface {
private:
    static std::atomic<bool> fFlushScheduled;
    std::atomic<bool> fAbortRescan{false};
    // controlled by WalletRescanReserver
    std::atomic<bool> fScanningWallet{false};
    std::mutex mutexScanning;
    friend class WalletRescanReserver;

    WalletBatch *encrypted_batch GUARDED_BY(cs_wallet) = nullptr;

    //! the current wallet version: clients below this version are not able to
    //! load the wallet
    int nWalletVersion = FEATURE_BASE;

    //! the maximum wallet format version: memory-only variable that specifies
    //! to what version this wallet may be upgraded
    int nWalletMaxVersion GUARDED_BY(cs_wallet) = FEATURE_BASE;

    int64_t nNextResend = 0;
    int64_t nLastResend = 0;
    bool fBroadcastTransactions = false;

    /**
     * Used to keep track of spent outpoints, and detect and report conflicts
     * (double-spends or mutated transactions where the mutant gets mined).
     */
    typedef std::multimap<COutPoint, TxId> TxSpends;
    TxSpends mapTxSpends GUARDED_BY(cs_wallet);
    void AddToSpends(const COutPoint &outpoint, const TxId &wtxid)
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    void AddToSpends(const TxId &wtxid) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    /**
     * Add a transaction to the wallet, or update it. pIndex and posInBlock
     * should be set when the transaction was known to be included in a
     * block. When *pIndex == nullptr, then wallet state is not updated in
     * AddToWallet, but notifications happen and cached balances are marked
     * dirty.
     *
     * If fUpdate is true, existing transactions will be updated.
     * TODO: One exception to this is that the abandoned state is cleared under
     * the assumption that any further notification of a transaction that was
     * considered abandoned is an indication that it is not safe to be
     * considered abandoned. Abandoned state should probably be more carefully
     * tracked via different posInBlock signals or by checking mempool presence
     * when necessary.
     */
    bool AddToWalletIfInvolvingMe(const CTransactionRef &tx,
                                  const BlockHash &block_hash, int posInBlock,
                                  bool fUpdate)
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    /**
     * Mark a transaction (and its in-wallet descendants) as conflicting with a
     * particular block.
     */
    void MarkConflicted(const BlockHash &hashBlock, const TxId &txid);

    /**
     * Mark a transaction's inputs dirty, thus forcing the outputs to be
     * recomputed
     */
    void MarkInputsDirty(const CTransactionRef &tx)
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    void SyncMetaData(std::pair<TxSpends::iterator, TxSpends::iterator>)
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    /**
     * Used by
     * TransactionAddedToMemorypool/BlockConnected/Disconnected/ScanForWalletTransactions.
     * Should be called with non-zero block_hash and posInBlock if this is for a
     * transaction that is included in a block.
     */
    void SyncTransaction(const CTransactionRef &tx, const BlockHash &block_hash,
                         int posInBlock = 0, bool update_tx = true)
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    /* the HD chain data model (external chain counters) */
    CHDChain hdChain;

    /* HD derive new child key (on internal or external chain) */
    void DeriveNewChildKey(WalletBatch &batch, CKeyMetadata &metadata,
                           CKey &secret, bool internal = false)
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    std::set<int64_t> setInternalKeyPool;
    std::set<int64_t> setExternalKeyPool GUARDED_BY(cs_wallet);
    std::set<int64_t> set_pre_split_keypool;
    int64_t m_max_keypool_index GUARDED_BY(cs_wallet) = 0;
    std::map<CKeyID, int64_t> m_pool_key_to_index;
    std::atomic<uint64_t> m_wallet_flags{0};

    int64_t nTimeFirstKey GUARDED_BY(cs_wallet) = 0;

    /**
     * Private version of AddWatchOnly method which does not accept a timestamp,
     * and which will reset the wallet's nTimeFirstKey value to 1 if the watch
     * key did not previously have a timestamp associated with it. Because this
     * is an inherited virtual method, it is accessible despite being marked
     * private, but it is marked private anyway to encourage use of the other
     * AddWatchOnly which accepts a timestamp and sets nTimeFirstKey more
     * intelligently for more efficient rescans.
     */
    bool AddWatchOnly(const CScript &dest) override
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    /** Interface for accessing chain state. */
    interfaces::Chain &m_chain;

    /**
     * Wallet location which includes wallet name (see WalletLocation).
     */
    WalletLocation m_location;

    /** Internal database handle. */
    std::unique_ptr<WalletDatabase> database;

    /**
     * The following is used to keep track of how far behind the wallet is
     * from the chain sync, and to allow clients to block on us being caught up.
     *
     * Note that this is *not* how far we've processed, we may need some rescan
     * to have seen all transactions in the chain, but is only used to track
     * live BlockConnected callbacks.
     */
    BlockHash m_last_block_processed;

public:
    const CChainParams &chainParams;
    /*
     * Main wallet lock.
     * This lock protects all the fields added by CWallet.
     */
    mutable RecursiveMutex cs_wallet;

    /**
     * Get database handle used by this wallet. Ideally this function would not
     * be necessary.
     */
    WalletDatabase &GetDBHandle() { return *database; }

    /**
     * Select a set of coins such that nValueRet >= nTargetValue and at least
     * all coins from coinControl are selected; Never select unconfirmed coins
     * if they are not ours.
     */
    bool SelectCoins(const std::vector<COutput> &vAvailableCoins,
                     const Amount nTargetValue,
                     std::set<CInputCoin> &setCoinsRet, Amount &nValueRet,
                     const CCoinControl &coin_control,
                     CoinSelectionParams &coin_selection_params,
                     bool &bnb_used) const EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    const WalletLocation &GetLocation() const { return m_location; }

    /**
     * Get a name for this wallet for logging/debugging purposes.
     */
    const std::string &GetName() const { return m_location.GetName(); }

    void LoadKeyPool(int64_t nIndex, const CKeyPool &keypool)
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    void MarkPreSplitKeys() EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    // Map from Key ID to key metadata.
    std::map<CKeyID, CKeyMetadata> mapKeyMetadata GUARDED_BY(cs_wallet);

    // Map from Script ID to key metadata (for watch-only keys).
    std::map<ScriptID, CKeyMetadata> m_script_metadata GUARDED_BY(cs_wallet);

    typedef std::map<unsigned int, CMasterKey> MasterKeyMap;
    MasterKeyMap mapMasterKeys;
    unsigned int nMasterKeyMaxID = 0;

    /** Construct wallet with specified name and database implementation. */
    CWallet(const CChainParams &chainParamsIn, interfaces::Chain &chain,
            const WalletLocation &location,
            std::unique_ptr<WalletDatabase> databaseIn)
        : m_chain(chain), m_location(location), database(std::move(databaseIn)),
          chainParams(chainParamsIn) {}

    ~CWallet() {
        // Should not have slots connected at this point.
        assert(NotifyUnload.empty());
        delete encrypted_batch;
        encrypted_batch = nullptr;
    }

    std::map<TxId, CWalletTx> mapWallet GUARDED_BY(cs_wallet);

    typedef std::multimap<int64_t, CWalletTx *> TxItems;
    TxItems wtxOrdered;

    int64_t nOrderPosNext GUARDED_BY(cs_wallet) = 0;
    uint64_t nAccountingEntryNumber = 0;

    std::map<CTxDestination, CAddressBookData> mapAddressBook;

    std::set<COutPoint> setLockedCoins GUARDED_BY(cs_wallet);

    /** Interface for accessing chain state. */
    interfaces::Chain &chain() const { return m_chain; }

    const CWalletTx *GetWalletTx(const TxId &txid) const;

    //! check whether we are allowed to upgrade (or already support) to the
    //! named feature
    bool CanSupportFeature(enum WalletFeature wf) const
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet) {
        AssertLockHeld(cs_wallet);
        return nWalletMaxVersion >= wf;
    }

    /**
     * populate vCoins with vector of available COutputs.
     */
    void AvailableCoins(interfaces::Chain::Lock &locked_chain,
                        std::vector<COutput> &vCoins, bool fOnlySafe = true,
                        const CCoinControl *coinControl = nullptr,
                        const Amount nMinimumAmount = SATOSHI,
                        const Amount nMaximumAmount = MAX_MONEY,
                        const Amount nMinimumSumAmount = MAX_MONEY,
                        const uint64_t nMaximumCount = 0,
                        const int nMinDepth = 0,
                        const int nMaxDepth = 9999999,
                        const CFeeRate = CFeeRate(Amount::zero())) const
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    /**
     * Return list of available coins and locked coins grouped by non-change
     * output address.
     */
    std::map<CTxDestination, std::vector<COutput>>
    ListCoins(interfaces::Chain::Lock &locked_chain) const
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    /**
     * Find non-change parent output.
     */
    const CTxOut &FindNonChangeParentOutput(const CTransaction &tx,
                                            int output) const
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    /**
     * Shuffle and select coins until nTargetValue is reached while avoiding
     * small change; This method is stochastic for some inputs and upon
     * completion the coin set and corresponding actual target value is
     * assembled.
     */
    bool SelectCoinsMinConf(const Amount nTargetValue,
                            const CoinEligibilityFilter &eligibility_filter,
                            std::vector<OutputGroup> groups,
                            std::set<CInputCoin> &setCoinsRet,
                            Amount &nValueRet,
                            const CoinSelectionParams &coin_selection_params,
                            bool &bnb_used) const;

    bool IsSpent(interfaces::Chain::Lock &locked_chain,
                 const COutPoint &outpoint) const
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    std::vector<OutputGroup> GroupOutputs(const std::vector<COutput> &outputs,
                                          bool single_coin) const;

    bool IsLockedCoin(const COutPoint &outpoint) const
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    void LockCoin(const COutPoint &output) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    void UnlockCoin(const COutPoint &output)
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    void UnlockAllCoins() EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    void ListLockedCoins(std::vector<COutPoint> &vOutpts) const
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    /*
     * Rescan abort properties
     */
    void AbortRescan() { fAbortRescan = true; }
    bool IsAbortingRescan() { return fAbortRescan; }
    bool IsScanning() { return fScanningWallet; }

    /**
     * keystore implementation
     * Generate a new key
     */
    CPubKey GenerateNewKey(WalletBatch &batch, bool internal = false)
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    //! Adds a key to the store, and saves it to disk.
    bool AddKeyPubKey(const CKey &key, const CPubKey &pubkey) override
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    bool AddKeyPubKeyWithDB(WalletBatch &batch, const CKey &key,
                            const CPubKey &pubkey)
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    //! Adds a key to the store, without saving it to disk (used by LoadWallet)
    bool LoadKey(const CKey &key, const CPubKey &pubkey) {
        return CCryptoKeyStore::AddKeyPubKey(key, pubkey);
    }

    //! Load metadata (used by LoadWallet)
    void LoadKeyMetadata(const CKeyID &keyID, const CKeyMetadata &metadata)
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    void LoadScriptMetadata(const ScriptID &script_id,
                            const CKeyMetadata &metadata)
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    bool LoadMinVersion(int nVersion) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet) {
        AssertLockHeld(cs_wallet);
        nWalletVersion = nVersion;
        nWalletMaxVersion = std::max(nWalletMaxVersion, nVersion);
        return true;
    }
    void UpdateTimeFirstKey(int64_t nCreateTime)
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    //! Adds an encrypted key to the store, and saves it to disk.
    bool AddCryptedKey(const CPubKey &vchPubKey,
                       const std::vector<uint8_t> &vchCryptedSecret) override;
    //! Adds an encrypted key to the store, without saving it to disk (used by
    //! LoadWallet)
    bool LoadCryptedKey(const CPubKey &vchPubKey,
                        const std::vector<uint8_t> &vchCryptedSecret);
    bool AddCScript(const CScript &redeemScript, bool is_p2sh_32) override;
    bool LoadCScript(const CScript &redeemScript);

    //! Adds a destination data tuple to the store, and saves it to disk
    bool AddDestData(const CTxDestination &dest, const std::string &key,
                     const std::string &value);
    //! Erases a destination data tuple in the store and on disk
    bool EraseDestData(const CTxDestination &dest, const std::string &key);
    //! Adds a destination data tuple to the store, without saving it to disk
    void LoadDestData(const CTxDestination &dest, const std::string &key,
                      const std::string &value);
    //! Look up a destination data tuple in the store, return true if found
    //! false otherwise
    bool GetDestData(const CTxDestination &dest, const std::string &key,
                     std::string *value) const;
    //! Get all destination values matching a prefix.
    std::vector<std::string> GetDestValues(const std::string &prefix) const;

    //! Adds a watch-only address to the store, and saves it to disk.
    bool AddWatchOnly(const CScript &dest, int64_t nCreateTime)
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    bool RemoveWatchOnly(const CScript &dest) override
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    //! Adds a watch-only address to the store, without saving it to disk (used
    //! by LoadWallet)
    bool LoadWatchOnly(const CScript &dest);

    //! Holds a timestamp at which point the wallet is scheduled (externally) to
    //! be relocked. Caller must arrange for actual relocking to occur via
    //! Lock().
    int64_t nRelockTime GUARDED_BY(cs_wallet){0};

    // Used to prevent concurrent calls to walletpassphrase RPC.
    // Locking order: Should be taken *before* cs_main!
    Mutex m_unlock_mutex;
    bool Unlock(const SecureString &strWalletPassphrase,
                bool accept_no_keys = false);
    bool ChangeWalletPassphrase(const SecureString &strOldWalletPassphrase,
                                const SecureString &strNewWalletPassphrase);
    bool EncryptWallet(const SecureString &strWalletPassphrase);

    void GetKeyBirthTimes(interfaces::Chain::Lock &locked_chain,
                          std::map<CTxDestination, int64_t> &mapKeyBirth) const
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    unsigned int ComputeTimeSmart(const CWalletTx &wtx) const;

    /**
     * Increment the next transaction order id
     * @return next transaction order id
     */
    int64_t IncOrderPosNext(WalletBatch *batch = nullptr)
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    DBErrors ReorderTransactions();

    void MarkDirty();
    bool AddToWallet(const CWalletTx &wtxIn, bool fFlushOnClose = true);
    void LoadToWallet(const CWalletTx &wtxIn)
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    void TransactionAddedToMempool(const CTransactionRef &tx) override;
    void
    BlockConnected(const std::shared_ptr<const CBlock> &pblock,
                   const CBlockIndex *pindex,
                   const std::vector<CTransactionRef> &vtxConflicted) override;
    void
    BlockDisconnected(const std::shared_ptr<const CBlock> &pblock) override;
    void TransactionDoubleSpent(const CTransactionRef &ptxn,
                                const DspId &dspId) override;
    int64_t RescanFromTime(int64_t startTime,
                           const WalletRescanReserver &reserver, bool update);

    struct ScanResult {
        enum { SUCCESS, FAILURE, USER_ABORT } status = SUCCESS;

        //! Hash and height of most recent block that was successfully scanned.
        //! Unset if no blocks were scanned due to read errors or the chain
        //! being empty.
        BlockHash stop_block;
        std::optional<int> stop_height;

        //! Hash of the most recent block that could not be scanned due to
        //! read errors or pruning. Will be set if status is FAILURE, unset if
        //! status is SUCCESS, and may or may not be set if status is
        //! USER_ABORT.
        BlockHash failed_block;
    };
    ScanResult ScanForWalletTransactions(const BlockHash &first_block,
                                         const BlockHash &last_block,
                                         const WalletRescanReserver &reserver,
                                         bool fUpdate);
    void TransactionRemovedFromMempool(const CTransactionRef &ptx) override;
    void ReacceptWalletTransactions();
    void ResendWalletTransactions(int64_t nBestBlockTime,
                                  CConnman *connman) override
        EXCLUSIVE_LOCKS_REQUIRED(cs_main);
    // ResendWalletTransactionsBefore may only be called if
    // fBroadcastTransactions!
    std::vector<uint256>
    ResendWalletTransactionsBefore(interfaces::Chain::Lock &locked_chain,
                                   int64_t nTime, CConnman *connman);
    Amount GetBalance(const isminefilter &filter = ISMINE_SPENDABLE,
                      const int min_depth = 0) const;
    Amount GetUnconfirmedBalance() const;
    Amount GetImmatureBalance() const;
    Amount GetUnconfirmedWatchOnlyBalance() const;
    Amount GetImmatureWatchOnlyBalance() const;
    Amount GetLegacyBalance(const isminefilter &filter, int minDepth) const;
    Amount GetAvailableBalance(const CCoinControl *coinControl = nullptr) const;

    OutputType TransactionChangeType(OutputType change_type,
                                     const std::vector<CRecipient> &vecSend);

    /**
     * Insert additional inputs into the transaction by calling
     * CreateTransaction();
     */
    bool FundTransaction(CMutableTransaction &tx, Amount &nFeeRet,
                         int &nChangePosInOut, std::string &strFailReason,
                         bool lockUnspents,
                         const std::set<int> &setSubtractFeeFromOutputs,
                         CCoinControl coinControl);
    /**
     * Create a new transaction paying the recipients with a set of coins
     * selected by SelectCoins(); Also create the change output, when needed
     * @note passing nChangePosInOut as -1 will result in setting a random
     * position
     */
    CreateTransactionResult
    CreateTransaction(interfaces::Chain::Lock &locked_chain,
                      const std::vector<CRecipient> &vecSend,
                      CTransactionRef &tx, CReserveKey &reservekey,
                      Amount &nFeeRet, int &nChangePosInOut,
                      std::string &strFailReason,
                      const CCoinControl &coin_control, bool sign = true,
                      CoinSelectionHint coinsel = CoinSelectionHint::Default);
    bool CommitTransaction(
        CTransactionRef tx, mapValue_t mapValue,
        std::vector<std::pair<std::string, std::string>> orderForm,
        CReserveKey &reservekey, CConnman *connman, CValidationState &state);

    bool DummySignTx(CMutableTransaction &txNew, const std::set<CTxOut> &txouts,
                     bool use_max_sig = false) const {
        std::vector<CTxOut> v_txouts(txouts.size());
        std::copy(txouts.begin(), txouts.end(), v_txouts.begin());
        return DummySignTx(txNew, v_txouts, use_max_sig);
    }
    bool DummySignTx(CMutableTransaction &txNew,
                     const std::vector<CTxOut> &txouts,
                     bool use_max_sig = false) const;
    bool DummySignInput(CTxIn &tx_in, const CTxOut &txout,
                        bool use_max_sig = false) const;

    CFeeRate m_pay_tx_fee{DEFAULT_PAY_TX_FEE};
    bool m_spend_zero_conf_change{DEFAULT_SPEND_ZEROCONF_CHANGE};
    // will be defined via chainparams
    bool m_allow_fallback_fee{true};
    // Override with -mintxfee
    CFeeRate m_min_fee{DEFAULT_TRANSACTION_MINFEE_PER_KB};
    /**
     * If fee estimation does not have enough data to provide estimates, use
     * this fee instead. Has no effect if not using fee estimation Override with
     * -fallbackfee
     */
    CFeeRate m_fallback_fee{DEFAULT_FALLBACK_FEE};
    OutputType m_default_address_type{DEFAULT_ADDRESS_TYPE};
    OutputType m_default_change_type{DEFAULT_CHANGE_TYPE};

    bool NewKeyPool();
    size_t KeypoolCountExternalKeys() EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    bool TopUpKeyPool(unsigned int kpSize = 0);

    /**
     * Reserves a key from the keypool and sets nIndex to its index
     *
     * @param[out] nIndex the index of the key in keypool
     * @param[out] keypool the keypool the key was drawn from, which could be
     * the the pre-split pool if present, or the internal or external pool
     * @param fRequestedInternal true if the caller would like the key drawn
     *     from the internal keypool, false if external is preferred
     *
     * @return true if succeeded, false if failed due to empty keypool
     * @throws std::runtime_error if keypool read failed, key was invalid,
     *     was not found in the wallet, or was misclassified in the internal
     *     or external keypool
     */
    bool ReserveKeyFromKeyPool(int64_t &nIndex, CKeyPool &keypool,
                               bool fRequestedInternal);
    void KeepKey(int64_t nIndex);
    void ReturnKey(int64_t nIndex, bool fInternal, const CPubKey &pubkey);
    bool GetKeyFromPool(CPubKey &key, bool internal = false);
    int64_t GetOldestKeyPoolTime();
    /**
     * Marks all keys in the keypool up to and including reserve_key as used.
     */
    void MarkReserveKeysAsUsed(int64_t keypool_id)
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    const std::map<CKeyID, int64_t> &GetAllReserveKeys() const {
        return m_pool_key_to_index;
    }

    std::set<std::set<CTxDestination>> GetAddressGroupings()
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    std::map<CTxDestination, Amount>
    GetAddressBalances(interfaces::Chain::Lock &locked_chain);

    std::set<CTxDestination> GetLabelAddresses(const std::string &label) const;

    isminetype IsMine(const CTxIn &txin) const;
    /**
     * Returns amount of debit if the input matches the filter, otherwise
     * returns 0
     */
    Amount GetDebit(const CTxIn &txin, const isminefilter &filter) const;
    isminetype IsMine(const CTxOut &txout) const;
    Amount GetCredit(const CTxOut &txout, const isminefilter &filter) const;
    bool IsChange(const CTxOut &txout) const;
    bool IsChange(const CScript &script) const;
    Amount GetChange(const CTxOut &txout) const;
    bool IsMine(const CTransaction &tx) const;
    /** should probably be renamed to IsRelevantToMe */
    bool IsFromMe(const CTransaction &tx) const;
    Amount GetDebit(const CTransaction &tx, const isminefilter &filter) const;
    /** Returns whether all of the inputs match the filter */
    bool IsAllFromMe(const CTransaction &tx, const isminefilter &filter) const;
    Amount GetCredit(const CTransaction &tx, const isminefilter &filter) const;
    Amount GetChange(const CTransaction &tx) const;
    void ChainStateFlushed(const CBlockLocator &loc) override;

    DBErrors LoadWallet(bool &fFirstRunRet);
    DBErrors ZapWalletTx(std::vector<CWalletTx> &vWtx);
    DBErrors ZapSelectTx(std::vector<TxId> &txIdsIn,
                         std::vector<TxId> &txIdsOut)
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    bool SetAddressBook(const CTxDestination &address,
                        const std::string &strName, const std::string &purpose);

    bool DelAddressBook(const CTxDestination &address);

    const std::string &GetLabelName(const CScript &scriptPubKey) const;

    void GetScriptForMining(std::shared_ptr<CReserveScript> &script);

    unsigned int GetKeyPoolSize() EXCLUSIVE_LOCKS_REQUIRED(cs_wallet) {
        // set{Ex,In}ternalKeyPool
        AssertLockHeld(cs_wallet);
        return setInternalKeyPool.size() + setExternalKeyPool.size();
    }

    //! signify that a particular wallet feature is now used. this may change
    //! nWalletVersion and nWalletMaxVersion if those are lower
    void SetMinVersion(enum WalletFeature, WalletBatch *batch_in = nullptr,
                       bool fExplicit = false);

    //! change which version we're allowed to upgrade to (note that this does
    //! not immediately imply upgrading to that format)
    bool SetMaxVersion(int nVersion);

    //! get the current wallet format (the oldest client version guaranteed to
    //! understand this wallet)
    int GetVersion() {
        LOCK(cs_wallet);
        return nWalletVersion;
    }

    //! Get wallet transactions that conflict with given transaction (spend same
    //! outputs)
    std::set<TxId> GetConflicts(const TxId &txid) const
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    //! Check if a given transaction has any of its outputs spent by another
    //! transaction in the wallet
    bool HasWalletSpend(const TxId &txid) const
        EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    //! Flush wallet (bitdb flush)
    void Flush(bool shutdown = false);

    /** Wallet is about to be unloaded */
    boost::signals2::signal<void()> NotifyUnload;

    /**
     * Address book entry changed.
     * @note called with lock cs_wallet held.
     */
    boost::signals2::signal<void(CWallet *wallet, const CTxDestination &address,
                                 const std::string &label, bool isMine,
                                 const std::string &purpose, ChangeType status)>
        NotifyAddressBookChanged;

    /**
     * Wallet transaction added, removed or updated.
     * @note called with lock cs_wallet held.
     */
    boost::signals2::signal<void(CWallet *wallet, const TxId &txid,
                                 ChangeType status)>
        NotifyTransactionChanged;

    /** Show progress e.g. for rescan */
    boost::signals2::signal<void(const std::string &title, int nProgress)>
        ShowProgress;

    /** Watch-only address added */
    boost::signals2::signal<void(bool fHaveWatchOnly)> NotifyWatchonlyChanged;

    /** Keypool has new keys */
    boost::signals2::signal<void()> NotifyCanGetAddressesChanged;

    /** Inquire whether this wallet broadcasts transactions. */
    bool GetBroadcastTransactions() const { return fBroadcastTransactions; }
    /** Set whether this wallet broadcasts transactions. */
    void SetBroadcastTransactions(bool broadcast) {
        fBroadcastTransactions = broadcast;
    }

    /** Return whether transaction can be abandoned */
    bool TransactionCanBeAbandoned(const TxId &txid) const;

    /**
     * Mark a transaction (and it in-wallet descendants) as abandoned so its
     * inputs may be respent.
     */
    bool AbandonTransaction(interfaces::Chain::Lock &locked_chain,
                            const TxId &txid);

    //! Verify wallet naming and perform salvage on the wallet if required
    static bool Verify(const CChainParams &chainParams,
                       interfaces::Chain &chain, const WalletLocation &location,
                       bool salvage_wallet, std::string &error_string,
                       std::string &warning_string);

    /**
     * Initializes the wallet, returns a new CWallet instance or a null pointer
     * in case of an error.
     */
    static std::shared_ptr<CWallet> CreateWalletFromFile(
        const CChainParams &chainParams, interfaces::Chain &chain,
        const WalletLocation &location, uint64_t wallet_creation_flags = 0);

    /**
     * Wallet post-init setup
     * Gives the wallet a chance to register repetitive tasks and complete
     * post-init tasks
     */
    void postInitProcess();

    bool BackupWallet(const std::string &strDest);

    /* Set the HD chain model (chain child index counters) */
    void SetHDChain(const CHDChain &chain, bool memonly);
    const CHDChain &GetHDChain() const { return hdChain; }

    /* Returns true if HD is enabled */
    bool IsHDEnabled() const;

    /* Returns true if the wallet can generate new keys */
    bool CanGenerateKeys();

    /**
     * Returns true if the wallet can give out new addresses. This means it has
     * keys in the keypool or can generate new keys.
     */
    bool CanGetAddresses(bool internal = false);

    /* Generates a new HD seed (will not be activated) */
    CPubKey GenerateNewSeed();

    /**
     * Derives a new HD seed (will not be activated)
     */
    CPubKey DeriveNewSeed(const CKey &key);

    /**
     * Set the current HD seed (will reset the chain child index counters)
     * Sets the seed's version based on the current wallet version (so the
     * caller must ensure the current wallet version is correct before calling
     * this function).
     */
    void SetHDSeed(const CPubKey &key);

    /**
     * Blocks until the wallet state is up-to-date to /at least/ the current
     * chain at the time this function is entered.
     * Obviously holding cs_main/cs_wallet when going into this call may cause
     * deadlock
     */
    void BlockUntilSyncedToCurrentChain() LOCKS_EXCLUDED(cs_main, cs_wallet);

    /**
     * Explicitly make the wallet learn the related scripts for outputs to the
     * given key. This is purely to make the wallet file compatible with older
     * software, as CBasicKeyStore automatically does this implicitly for all
     * keys now.
     */
    void LearnRelatedScripts(const CPubKey &key, OutputType);

    /**
     * Same as LearnRelatedScripts, but when the OutputType is not known (and
     * could be anything).
     */
    void LearnAllRelatedScripts(const CPubKey &key);

    /**
     * Set a single wallet flag.
     */
    void SetWalletFlag(uint64_t flags);

    /**
     * Unsets a single wallet flag.
     */
    void UnsetWalletFlag(uint64_t flag);

    /**
     * Check if a certain wallet flag is set.
     */
    bool IsWalletFlagSet(uint64_t flag);

    /**
     * Overwrite all flags by the given uint64_t.
     * Returns false if unknown, non-tolerable flags are present.
     */
    bool SetWalletFlags(uint64_t overwriteFlags, bool memOnly);

    /**
     * Returns a bracketed wallet name for displaying in logs, will return
     * [default wallet] if the wallet has no name.
     */
    const std::string GetDisplayName() const {
        std::string wallet_name =
            GetName().length() == 0 ? "default wallet" : GetName();
        return strprintf("[%s]", wallet_name);
    };

    /**
     * Prepends the wallet name in logging output to ease debugging in
     * multi-wallet use cases.
     */
    template <typename... Params>
    void WalletLogPrintf(const std::string &fmt, Params... parameters) const {
        LogPrintf(("%s " + fmt).c_str(), GetDisplayName(), parameters...);
    };

    template <typename... Params>
    void WalletLogPrintfToBeContinued(const std::string &fmt,
                                      Params... parameters) const {
        LogPrintfToBeContinued(("%s " + fmt).c_str(), GetDisplayName(),
                               parameters...);
    };

    /**
     * Implement lookup of key origin information through wallet key metadata.
     */
    bool GetKeyOrigin(const CKeyID &keyid, KeyOriginInfo &info) const override;
};

/** A key allocated from the key pool. */
class CReserveKey final : public CReserveScript {
protected:
    CWallet *pwallet;
    int64_t nIndex{-1};
    CPubKey vchPubKey;
    bool fInternal{false};

public:
    explicit CReserveKey(CWallet *pwalletIn) { pwallet = pwalletIn; }

    CReserveKey(const CReserveKey &) = delete;
    CReserveKey &operator=(const CReserveKey &) = delete;

    ~CReserveKey() { ReturnKey(); }

    void ReturnKey();
    bool GetReservedKey(CPubKey &pubkey, bool internal = false);
    void KeepKey();
    void KeepScript() override { KeepKey(); }
};

/** RAII object to check and reserve a wallet rescan */
class WalletRescanReserver {
private:
    CWallet *m_wallet;
    bool m_could_reserve;

public:
    explicit WalletRescanReserver(CWallet *w)
        : m_wallet(w), m_could_reserve(false) {}

    bool reserve() {
        assert(!m_could_reserve);
        std::lock_guard<std::mutex> lock(m_wallet->mutexScanning);
        if (m_wallet->fScanningWallet) {
            return false;
        }
        m_wallet->fScanningWallet = true;
        m_could_reserve = true;
        return true;
    }

    bool isReserved() const {
        return (m_could_reserve && m_wallet->fScanningWallet);
    }

    ~WalletRescanReserver() {
        std::lock_guard<std::mutex> lock(m_wallet->mutexScanning);
        if (m_could_reserve) {
            m_wallet->fScanningWallet = false;
        }
    }
};

// Calculate the size of the transaction assuming all signatures are max size
// Use DummySignatureCreator, which inserts 71 byte signatures everywhere.
// NOTE: this requires that all inputs must be in mapWallet (eg the tx should
// be IsAllFromMe).
int64_t CalculateMaximumSignedTxSize(const CTransaction &tx,
                                     const CWallet *wallet,
                                     bool use_max_sig = false)
    EXCLUSIVE_LOCKS_REQUIRED(wallet->cs_wallet);
int64_t CalculateMaximumSignedTxSize(const CTransaction &tx,
                                     const CWallet *wallet,
                                     const std::vector<CTxOut> &txouts,
                                     bool use_max_sig = false);
