// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <validationinterface.h>

#include <scheduler.h>
#include <txmempool.h>
#include <util/system.h>
#include <validation.h>

#include <atomic>
#include <future>
#include <list>
#include <tuple>
#include <utility>

#include <boost/signals2/signal.hpp>

struct ValidationInterfaceConnections {
    boost::signals2::scoped_connection UpdatedBlockTip;
    boost::signals2::scoped_connection TransactionAddedToMempool;
    boost::signals2::scoped_connection TransactionDoubleSpent;
    boost::signals2::scoped_connection BadDSProofsDetectedFromNodeIds;
    boost::signals2::scoped_connection BlockConnected;
    boost::signals2::scoped_connection BlockDisconnected;
    boost::signals2::scoped_connection TransactionRemovedFromMempool;
    boost::signals2::scoped_connection ChainStateFlushed;
    boost::signals2::scoped_connection Broadcast;
    boost::signals2::scoped_connection BlockChecked;
    boost::signals2::scoped_connection NewPoWValidBlock;
};

struct MainSignalsInstance {
    boost::signals2::signal<void(const CBlockIndex *, const CBlockIndex *,
                                 bool fInitialDownload)>
        UpdatedBlockTip;
    boost::signals2::signal<void(const CTransactionRef &)>
        TransactionAddedToMempool;
    boost::signals2::signal<void(const CTransactionRef &, const DspId &)>
        TransactionDoubleSpent;
    boost::signals2::signal<void(const std::vector<NodeId> &)>
        BadDSProofsDetectedFromNodeIds;
    boost::signals2::signal<void(const std::shared_ptr<const CBlock> &,
                                 const CBlockIndex *pindex,
                                 const std::vector<CTransactionRef> &)>
        BlockConnected;
    boost::signals2::signal<void(const std::shared_ptr<const CBlock> &)>
        BlockDisconnected;
    boost::signals2::signal<void(const CTransactionRef &)>
        TransactionRemovedFromMempool;
    boost::signals2::signal<void(const CBlockLocator &)> ChainStateFlushed;
    boost::signals2::signal<void(int64_t nBestBlockTime, CConnman *connman)>
        Broadcast;
    boost::signals2::signal<void(const CBlock &, const CValidationState &)>
        BlockChecked;
    boost::signals2::signal<void(const CBlockIndex *,
                                 const std::shared_ptr<const CBlock> &)>
        NewPoWValidBlock;

    // We are not allowed to assume the scheduler only runs in one thread,
    // but must ensure all callbacks happen in-order, so we end up creating
    // our own queue here :(
    SingleThreadedSchedulerClient m_schedulerClient;
    std::unordered_map<CValidationInterface *, ValidationInterfaceConnections>
        m_connMainSignals;

    explicit MainSignalsInstance(CScheduler *pscheduler)
        : m_schedulerClient(pscheduler) {}
};

static CMainSignals g_signals;

// This map has to be a separate global instead of a member of
// MainSignalsInstance, because RegisterWithMempoolSignals is currently called
// before RegisterBackgroundSignalScheduler, so MainSignalsInstance hasn't been
// created yet.
static std::unordered_map<CTxMemPool *, boost::signals2::scoped_connection>
    g_connNotifyEntryRemoved;

void CMainSignals::RegisterBackgroundSignalScheduler(CScheduler &scheduler) {
    assert(!m_internals);
    m_internals.reset(new MainSignalsInstance(&scheduler));
}

void CMainSignals::UnregisterBackgroundSignalScheduler() {
    m_internals.reset(nullptr);
}

void CMainSignals::FlushBackgroundCallbacks() {
    if (m_internals) {
        m_internals->m_schedulerClient.EmptyQueue();
    }
}

size_t CMainSignals::CallbacksPending() {
    if (!m_internals) {
        return 0;
    }
    return m_internals->m_schedulerClient.CallbacksPending();
}

void CMainSignals::RegisterWithMempoolSignals(CTxMemPool &pool) {
    g_connNotifyEntryRemoved.emplace(
        std::piecewise_construct, std::forward_as_tuple(&pool),
        std::forward_as_tuple(pool.NotifyEntryRemoved.connect(
            std::bind(&CMainSignals::MempoolEntryRemoved, this,
                      std::placeholders::_1, std::placeholders::_2))));
}

void CMainSignals::UnregisterWithMempoolSignals(CTxMemPool &pool) {
    g_connNotifyEntryRemoved.erase(&pool);
}

CMainSignals &GetMainSignals() {
    return g_signals;
}

static std::atomic_bool g_reg_unsafe{false};

void SetValidationInterfaceRegistrationsUnsafe(bool unsafe) { g_reg_unsafe = unsafe; }

static void LogIfUnsafe(const char *func) {
    if (g_reg_unsafe) {
        LogPrintf("WARNING: %s called from outside of the init/shutdown code paths (thread: %s)."
                  " This is not supported! FIXME!\n", func, util::ThreadGetInternalName());
    }
}

void RegisterValidationInterface(CValidationInterface *pwalletIn) {
    LogIfUnsafe(__func__);
    ValidationInterfaceConnections &conns =
        g_signals.m_internals->m_connMainSignals[pwalletIn];
    conns.UpdatedBlockTip = g_signals.m_internals->UpdatedBlockTip.connect(
        std::bind(&CValidationInterface::UpdatedBlockTip, pwalletIn,
                  std::placeholders::_1, std::placeholders::_2,
                  std::placeholders::_3));
    conns.TransactionAddedToMempool =
        g_signals.m_internals->TransactionAddedToMempool.connect(
            std::bind(&CValidationInterface::TransactionAddedToMempool,
                      pwalletIn, std::placeholders::_1));
    conns.BlockConnected = g_signals.m_internals->BlockConnected.connect(
        std::bind(&CValidationInterface::BlockConnected, pwalletIn,
                  std::placeholders::_1, std::placeholders::_2,
                  std::placeholders::_3));
    conns.BlockDisconnected = g_signals.m_internals->BlockDisconnected.connect(
        std::bind(&CValidationInterface::BlockDisconnected, pwalletIn,
                  std::placeholders::_1));
    conns.TransactionRemovedFromMempool =
        g_signals.m_internals->TransactionRemovedFromMempool.connect(
            std::bind(&CValidationInterface::TransactionRemovedFromMempool,
                      pwalletIn, std::placeholders::_1));
    conns.ChainStateFlushed = g_signals.m_internals->ChainStateFlushed.connect(
        std::bind(&CValidationInterface::ChainStateFlushed, pwalletIn,
                  std::placeholders::_1));
    conns.Broadcast = g_signals.m_internals->Broadcast.connect(
        std::bind(&CValidationInterface::ResendWalletTransactions, pwalletIn,
                  std::placeholders::_1, std::placeholders::_2));
    conns.BlockChecked = g_signals.m_internals->BlockChecked.connect(
        std::bind(&CValidationInterface::BlockChecked, pwalletIn,
                  std::placeholders::_1, std::placeholders::_2));
    conns.NewPoWValidBlock = g_signals.m_internals->NewPoWValidBlock.connect(
        std::bind(&CValidationInterface::NewPoWValidBlock, pwalletIn,
                  std::placeholders::_1, std::placeholders::_2));
    conns.TransactionDoubleSpent =
        g_signals.m_internals->TransactionDoubleSpent.connect(
            std::bind(&CValidationInterface::TransactionDoubleSpent,
                      pwalletIn, std::placeholders::_1, std::placeholders::_2));
    conns.BadDSProofsDetectedFromNodeIds =
        g_signals.m_internals->BadDSProofsDetectedFromNodeIds.connect(
            std::bind(&CValidationInterface::BadDSProofsDetectedFromNodeIds,
                      pwalletIn, std::placeholders::_1));
}

void UnregisterValidationInterface(CValidationInterface *pwalletIn) {
    if (g_signals.m_internals) {
        LogIfUnsafe(__func__);
        g_signals.m_internals->m_connMainSignals.erase(pwalletIn);
    }
}

void UnregisterAllValidationInterfaces() {
    if (!g_signals.m_internals) {
        return;
    }
    LogIfUnsafe(__func__);
    g_signals.m_internals->m_connMainSignals.clear();
}

void CallFunctionInValidationInterfaceQueue(std::function<void()> func) {
    g_signals.m_internals->m_schedulerClient.AddToProcessQueue(std::move(func));
}

void SyncWithValidationInterfaceQueue() {
    AssertLockNotHeld(cs_main);
    // Block until the validation queue drains
    std::promise<void> promise;
    CallFunctionInValidationInterfaceQueue([&promise] { promise.set_value(); });
    promise.get_future().wait();
}

void CMainSignals::MempoolEntryRemoved(CTransactionRef ptx,
                                       MemPoolRemovalReason reason) {
    if (reason != MemPoolRemovalReason::BLOCK &&
        reason != MemPoolRemovalReason::CONFLICT) {
        m_internals->m_schedulerClient.AddToProcessQueue(
            [ptx, this] { m_internals->TransactionRemovedFromMempool(ptx); });
    }
}

void CMainSignals::UpdatedBlockTip(const CBlockIndex *pindexNew,
                                   const CBlockIndex *pindexFork,
                                   bool fInitialDownload) {
    // Dependencies exist that require UpdatedBlockTip events to be delivered in
    // the order in which the chain actually updates. One way to ensure this is
    // for the caller to invoke this signal in the same critical section where
    // the chain is updated

    m_internals->m_schedulerClient.AddToProcessQueue([pindexNew, pindexFork,
                                                      fInitialDownload, this] {
        m_internals->UpdatedBlockTip(pindexNew, pindexFork, fInitialDownload);
    });
}

void CMainSignals::TransactionAddedToMempool(const CTransactionRef &ptx) {
    m_internals->m_schedulerClient.AddToProcessQueue(
        [ptx, this] { m_internals->TransactionAddedToMempool(ptx); });
}

void CMainSignals::TransactionDoubleSpent(const CTransactionRef &ptx, const DspId &dspId) {
    m_internals->m_schedulerClient.AddToProcessQueue(
        [ptx, dspId, this] { m_internals->TransactionDoubleSpent(ptx, dspId); });
}

void CMainSignals::BadDSProofsDetectedFromNodeIds(const std::vector<NodeId> &nodeIds) {
    m_internals->m_schedulerClient.AddToProcessQueue(
        [nodeIds, this] { m_internals->BadDSProofsDetectedFromNodeIds(nodeIds); });
}

void CMainSignals::BlockConnected(
    const std::shared_ptr<const CBlock> &pblock, const CBlockIndex *pindex,
    const std::shared_ptr<const std::vector<CTransactionRef>> &pvtxConflicted) {
    m_internals->m_schedulerClient.AddToProcessQueue(
        [pblock, pindex, pvtxConflicted, this] {
            m_internals->BlockConnected(pblock, pindex, *pvtxConflicted);
        });
}

void CMainSignals::BlockDisconnected(
    const std::shared_ptr<const CBlock> &pblock) {
    m_internals->m_schedulerClient.AddToProcessQueue(
        [pblock, this] { m_internals->BlockDisconnected(pblock); });
}

void CMainSignals::ChainStateFlushed(const CBlockLocator &locator) {
    m_internals->m_schedulerClient.AddToProcessQueue(
        [locator, this] { m_internals->ChainStateFlushed(locator); });
}

void CMainSignals::Broadcast(int64_t nBestBlockTime, CConnman *connman) {
    m_internals->Broadcast(nBestBlockTime, connman);
}

void CMainSignals::BlockChecked(const CBlock &block,
                                const CValidationState &state) {
    m_internals->BlockChecked(block, state);
}

void CMainSignals::NewPoWValidBlock(
    const CBlockIndex *pindex, const std::shared_ptr<const CBlock> &block) {
    m_internals->NewPoWValidBlock(pindex, block);
}
