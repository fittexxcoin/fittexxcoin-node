// Copyright (c) 2018 The Fittexxcoin Core developers
// Copyright (c) 2022 The Fittexxcoin Node developers
// Copyright (c) 2019-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <amount.h>                 // For Amount
#include <dsproof/dsproof.h>        // For DoubleSpendProof
#include <primitives/transaction.h> // For CTxOut
#include <pubkey.h> // For CKeyID and ScriptID (definitions needed in CTxDestination instantiation)
#include <script/ismine.h>             // For isminefilter, isminetype
#include <script/standard.h>           // For CTxDestination
#include <support/allocators/secure.h> // For SecureString
#include <ui_interface.h>              // For ChangeType

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

class CChainParams;
class CCoinControl;
class CKey;
class CMutableTransaction;
class COutPoint;
class CTransaction;
class CWallet;
enum class OutputType;
struct CRecipient;
struct TxId;

namespace interfaces {

class Handler;
class PendingWalletTx;
struct WalletAddress;
struct WalletBalances;
struct WalletTx;
struct WalletTxOut;
struct WalletTxStatus;

using WalletOrderForm = std::vector<std::pair<std::string, std::string>>;
using WalletValueMap = std::map<std::string, std::string>;

//! Interface for accessing a wallet.
class Wallet {
public:
    virtual ~Wallet() {}

    //! Encrypt wallet.
    virtual bool encryptWallet(const SecureString &wallet_passphrase) = 0;

    //! Return whether wallet is encrypted.
    virtual bool isCrypted() = 0;

    //! Lock wallet.
    virtual bool lock() = 0;

    //! Unlock wallet.
    virtual bool unlock(const SecureString &wallet_passphrase) = 0;

    //! Return whether wallet is locked.
    virtual bool isLocked() = 0;

    //! Change wallet passphrase.
    virtual bool
    changeWalletPassphrase(const SecureString &old_wallet_passphrase,
                           const SecureString &new_wallet_passphrase) = 0;

    //! Abort a rescan.
    virtual void abortRescan() = 0;

    //! Back up wallet.
    virtual bool backupWallet(const std::string &filename) = 0;

    //! Get wallet name.
    virtual std::string getWalletName() = 0;

    //! Get chainparams.
    virtual const CChainParams &getChainParams() = 0;

    //! Get set of addresses corresponding to a given label.
    virtual std::set<CTxDestination>
    getLabelAddresses(const std::string &label) = 0;

    //! Get key from pool.
    virtual bool getKeyFromPool(bool internal, CPubKey &pub_key) = 0;

    //! Get public key.
    virtual bool getPubKey(const CKeyID &address, CPubKey &pub_key) = 0;

    //! Get private key.
    virtual bool getPrivKey(const CKeyID &address, CKey &key) = 0;

    //! Return whether wallet has private key.
    virtual bool isSpendable(const CTxDestination &dest) = 0;

    //! Return whether wallet has watch only keys.
    virtual bool haveWatchOnly() = 0;

    //! Add or update address.
    virtual bool setAddressBook(const CTxDestination &dest,
                                const std::string &name,
                                const std::string &purpose) = 0;

    // Remove address.
    virtual bool delAddressBook(const CTxDestination &dest) = 0;

    //! Look up address in wallet, return whether exists.
    virtual bool getAddress(const CTxDestination &dest, std::string *name,
                            isminetype *is_mine, std::string *purpose) = 0;

    //! Get wallet address list.
    virtual std::vector<WalletAddress> getAddresses() = 0;

    //! Add scripts to key store so old so software versions opening the wallet
    //! database can detect payments to newer address types.
    virtual void learnRelatedScripts(const CPubKey &key, OutputType type) = 0;

    //! Add dest data.
    virtual bool addDestData(const CTxDestination &dest, const std::string &key,
                             const std::string &value) = 0;

    //! Erase dest data.
    virtual bool eraseDestData(const CTxDestination &dest,
                               const std::string &key) = 0;

    //! Get dest values with prefix.
    virtual std::vector<std::string>
    getDestValues(const std::string &prefix) = 0;

    //! Lock coin.
    virtual void lockCoin(const COutPoint &output) = 0;

    //! Unlock coin.
    virtual void unlockCoin(const COutPoint &output) = 0;

    //! Return whether coin is locked.
    virtual bool isLockedCoin(const COutPoint &output) = 0;

    //! List locked coins.
    virtual void listLockedCoins(std::vector<COutPoint> &outputs) = 0;

    //! Create transaction.
    virtual std::unique_ptr<PendingWalletTx>
    createTransaction(const std::vector<CRecipient> &recipients,
                      const CCoinControl &coin_control, bool sign,
                      int &change_pos, Amount &fee,
                      std::string &fail_reason) = 0;

    //! Return whether transaction can be abandoned.
    virtual bool transactionCanBeAbandoned(const TxId &txid) = 0;

    //! Abandon transaction.
    virtual bool abandonTransaction(const TxId &txid) = 0;

    //! Get a transaction.
    virtual CTransactionRef getTx(const TxId &txid) = 0;

    //! Get transaction information.
    virtual WalletTx getWalletTx(const TxId &txid) = 0;

    //! Get list of all wallet transactions.
    virtual std::vector<WalletTx> getWalletTxs() = 0;

    //! Try to get updated status for a particular transaction, if possible
    //! without blocking.
    virtual bool tryGetTxStatus(const TxId &txid, WalletTxStatus &tx_status,
                                int &num_blocks, int64_t &block_time) = 0;

    //! Get transaction details.
    virtual WalletTx getWalletTxDetails(const TxId &txid,
                                        WalletTxStatus &tx_status,
                                        WalletOrderForm &order_form,
                                        bool &in_mempool, int &num_blocks) = 0;

    //! Get balances.
    virtual WalletBalances getBalances() = 0;

    //! Get balances if possible without blocking.
    virtual bool tryGetBalances(WalletBalances &balances, int &num_blocks) = 0;

    //! Get balance.
    virtual Amount getBalance() = 0;

    //! Get available balance.
    virtual Amount getAvailableBalance(const CCoinControl &coin_control) = 0;

    //! Return whether transaction input belongs to wallet.
    virtual isminetype txinIsMine(const CTxIn &txin) = 0;

    //! Return whether transaction output belongs to wallet.
    virtual isminetype txoutIsMine(const CTxOut &txout) = 0;

    //! Return debit amount if transaction input belongs to wallet.
    virtual Amount getDebit(const CTxIn &txin, isminefilter filter) = 0;

    //! Return credit amount if transaction input belongs to wallet.
    virtual Amount getCredit(const CTxOut &txout, isminefilter filter) = 0;

    //! Return AvailableCoins + LockedCoins grouped by wallet address.
    //! (put change in one group with wallet address)
    using CoinsList = std::map<CTxDestination,
                               std::vector<std::tuple<COutPoint, WalletTxOut>>>;
    virtual CoinsList listCoins() = 0;

    //! Return wallet transaction output information.
    virtual std::vector<WalletTxOut>
    getCoins(const std::vector<COutPoint> &outputs) = 0;

    //! Get required fee.
    virtual Amount getRequiredFee(unsigned int tx_bytes) = 0;

    //! Get minimum fee.
    virtual Amount getMinimumFee(unsigned int tx_bytes,
                                 const CCoinControl &coin_control) = 0;

    // Return whether HD enabled.
    virtual bool hdEnabled() = 0;

    // Return whether the wallet is blank.
    virtual bool canGetAddresses() = 0;

    // Check if a certain wallet flag is set.
    virtual bool IsWalletFlagSet(uint64_t flag) = 0;

    // Get default address type.
    virtual OutputType getDefaultAddressType() = 0;

    // Get default change type.
    virtual OutputType getDefaultChangeType() = 0;

    //! Register handler for unload message.
    using UnloadFn = std::function<void()>;
    virtual std::unique_ptr<Handler> handleUnload(UnloadFn fn) = 0;

    //! Register handler for show progress messages.
    using ShowProgressFn =
        std::function<void(const std::string &title, int progress)>;
    virtual std::unique_ptr<Handler> handleShowProgress(ShowProgressFn fn) = 0;

    //! Register handler for status changed messages.
    using StatusChangedFn = std::function<void()>;
    virtual std::unique_ptr<Handler>
    handleStatusChanged(StatusChangedFn fn) = 0;

    //! Register handler for address book changed messages.
    using AddressBookChangedFn = std::function<void(
        const CTxDestination &address, const std::string &label, bool is_mine,
        const std::string &purpose, ChangeType status)>;
    virtual std::unique_ptr<Handler>
    handleAddressBookChanged(AddressBookChangedFn fn) = 0;

    //! Register handler for transaction changed messages.
    using TransactionChangedFn =
        std::function<void(const TxId &txid, ChangeType status)>;
    virtual std::unique_ptr<Handler>
    handleTransactionChanged(TransactionChangedFn fn) = 0;

    //! Register handler for watchonly changed messages.
    using WatchOnlyChangedFn = std::function<void(bool have_watch_only)>;
    virtual std::unique_ptr<Handler>
    handleWatchOnlyChanged(WatchOnlyChangedFn fn) = 0;

    //! Register handler for keypool changed messages.
    using CanGetAddressesChangedFn = std::function<void()>;
    virtual std::unique_ptr<Handler>
    handleCanGetAddressesChanged(CanGetAddressesChangedFn fn) = 0;
};

//! Tracking object returned by CreateTransaction and passed to
//! CommitTransaction.
class PendingWalletTx {
public:
    virtual ~PendingWalletTx() {}

    //! Get transaction data.
    virtual const CTransaction &get() = 0;

    //! Send pending transaction and commit to wallet.
    virtual bool commit(WalletValueMap value_map, WalletOrderForm order_form,
                        std::string &reject_reason) = 0;
};

//! Information about one wallet address.
struct WalletAddress {
    CTxDestination dest;
    isminetype is_mine;
    std::string name;
    std::string purpose;

    WalletAddress(CTxDestination destIn, isminetype isMineIn,
                  std::string nameIn, std::string purposeIn)
        : dest(std::move(destIn)), is_mine(isMineIn), name(std::move(nameIn)),
          purpose(std::move(purposeIn)) {}
};

//! Collection of wallet balances.
struct WalletBalances {
    Amount balance = Amount::zero();
    Amount unconfirmed_balance = Amount::zero();
    Amount immature_balance = Amount::zero();
    bool have_watch_only = false;
    Amount watch_only_balance = Amount::zero();
    Amount unconfirmed_watch_only_balance = Amount::zero();
    Amount immature_watch_only_balance = Amount::zero();

    bool balanceChanged(const WalletBalances &prev) const {
        return balance != prev.balance ||
               unconfirmed_balance != prev.unconfirmed_balance ||
               immature_balance != prev.immature_balance ||
               watch_only_balance != prev.watch_only_balance ||
               unconfirmed_watch_only_balance !=
                   prev.unconfirmed_watch_only_balance ||
               immature_watch_only_balance != prev.immature_watch_only_balance;
    }
};

// Wallet transaction information.
struct WalletTx {
    CTransactionRef tx;
    std::vector<isminetype> txin_is_mine;
    std::vector<isminetype> txout_is_mine;
    std::vector<CTxDestination> txout_address;
    std::vector<isminetype> txout_address_is_mine;
    Amount credit;
    Amount debit;
    Amount change;
    int64_t time;
    std::map<std::string, std::string> value_map;
    bool is_coinbase;
    DoubleSpendProof dsProof;
};

//! Updated transaction status.
struct WalletTxStatus {
    int block_height;
    int blocks_to_maturity;
    int depth_in_main_chain;
    unsigned int time_received;
    uint32_t lock_time;
    bool is_final;
    bool is_trusted;
    bool is_abandoned;
    bool is_coinbase;
    bool is_in_main_chain;
    bool is_double_spent;
};

//! Wallet transaction output.
struct WalletTxOut {
    CTxOut txout;
    int64_t time;
    int depth_in_main_chain = -1;
    bool is_spent = false;
};

//! Return implementation of Wallet interface. This function is defined in
//! dummywallet.cpp and throws if the wallet component is not compiled.
std::unique_ptr<Wallet> MakeWallet(const std::shared_ptr<CWallet> &wallet);

} // namespace interfaces
