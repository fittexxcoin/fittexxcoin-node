// Copyright (c) 2015-2018 The Fittexxcoin Core developers
// Copyright (c) 2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#include <streams.h>
#include <util/system.h>
#include <validation.h>
#include <version.h>
#include <zmq/zmqnotificationinterface.h>
#include <zmq/zmqpublishnotifier.h>
#include <zmq/zmqutil.h>

#include <zmq.h>

CZMQNotificationInterface::CZMQNotificationInterface() : pcontext(nullptr) {}

CZMQNotificationInterface::~CZMQNotificationInterface() {
    Shutdown();
}

std::list<const CZMQAbstractNotifier *>
CZMQNotificationInterface::GetActiveNotifiers() const {
    std::list<const CZMQAbstractNotifier *> result;
    for (const auto &n : notifiers) {
        result.push_back(n.get());
    }
    return result;
}

CZMQNotificationInterface *CZMQNotificationInterface::Create() {
    std::map<std::string, CZMQNotifierFactory> factories;

    factories["pubhashblock"] =
        CZMQAbstractNotifier::Create<CZMQPublishHashBlockNotifier>;
    factories["pubhashtx"] =
        CZMQAbstractNotifier::Create<CZMQPublishHashTransactionNotifier>;
    factories["pubrawblock"] =
        CZMQAbstractNotifier::Create<CZMQPublishRawBlockNotifier>;
    factories["pubrawtx"] =
        CZMQAbstractNotifier::Create<CZMQPublishRawTransactionNotifier>;
    factories["pubhashds"] =
        CZMQAbstractNotifier::Create<CZMQPublishHashDoubleSpendNotifier>;
    factories["pubrawds"] =
        CZMQAbstractNotifier::Create<CZMQPublishRawDoubleSpendNotifier>;

    std::list<std::unique_ptr<CZMQAbstractNotifier>> notifiers;
    for (const auto &entry : factories) {
        std::string arg("-zmq" + entry.first);
        if (gArgs.IsArgSet(arg)) {
            const auto& factory = entry.second;
            const std::string address = gArgs.GetArg(arg, "");
            std::unique_ptr<CZMQAbstractNotifier> notifier = factory();
            notifier->SetType(entry.first);
            notifier->SetAddress(address);
            notifiers.push_back(std::move(notifier));
        }
    }

    if (!notifiers.empty()) {
        std::unique_ptr<CZMQNotificationInterface> notificationInterface(new CZMQNotificationInterface);
        notificationInterface->notifiers = std::move(notifiers);

        if (notificationInterface->Initialize()) {
            return notificationInterface.release();
        }
    }

    return nullptr;
}

// Called at startup to conditionally set up ZMQ socket(s)
bool CZMQNotificationInterface::Initialize() {
    int major = 0, minor = 0, patch = 0;
    zmq_version(&major, &minor, &patch);
    LogPrint(BCLog::ZMQ, "zmq: version %d.%d.%d\n", major, minor, patch);

    LogPrint(BCLog::ZMQ, "zmq: Initialize notification interface\n");
    assert(!pcontext);

    pcontext = zmq_ctx_new();

    if (!pcontext) {
        zmqError("Unable to initialize context");
        return false;
    }

    for (auto &notifier : notifiers) {
        if (notifier->Initialize(pcontext)) {
            LogPrint(BCLog::ZMQ, "  Notifier %s ready (address = %s)\n",
                     notifier->GetType(), notifier->GetAddress());
        } else {
            LogPrint(BCLog::ZMQ, "  Notifier %s failed (address = %s)\n",
                     notifier->GetType(), notifier->GetAddress());
            return false;
        }
    }

    return true;
}

// Called during shutdown sequence
void CZMQNotificationInterface::Shutdown() {
    LogPrint(BCLog::ZMQ, "zmq: Shutdown notification interface\n");
    if (pcontext) {
        for (auto &notifier : notifiers) {
            LogPrint(BCLog::ZMQ, "   Shutdown notifier %s at %s\n",
                     notifier->GetType(), notifier->GetAddress());
            notifier->Shutdown();
        }
        zmq_ctx_term(pcontext);

        pcontext = nullptr;
    }
}

namespace {

template <typename Function>
void TryForEachAndRemoveFailed(std::list<std::unique_ptr<CZMQAbstractNotifier>>& notifiers, const Function& func) {
    for (auto i = notifiers.begin(); i != notifiers.end(); ) {
         CZMQAbstractNotifier* notifier = i->get();
         if (func(notifier)) {
             ++i;
         } else {
             notifier->Shutdown();
             i = notifiers.erase(i);
         }
     }
}

} // namespace

void CZMQNotificationInterface::UpdatedBlockTip(const CBlockIndex *pindexNew,
                                                const CBlockIndex *pindexFork,
                                                bool fInitialDownload) {
    // In IBD or blocks were disconnected without any new ones
    if (fInitialDownload || pindexNew == pindexFork) {
        return;
    }

    TryForEachAndRemoveFailed(notifiers, [pindexNew](CZMQAbstractNotifier* notifier) {
        return notifier->NotifyBlock(pindexNew);
    });
}

void CZMQNotificationInterface::TransactionAddedToMempool(
    const CTransactionRef &ptx) {
    // Used by BlockConnected and BlockDisconnected as well, because they're all
    // the same external callback.
    const CTransaction &tx = *ptx;

    TryForEachAndRemoveFailed(notifiers, [&tx](CZMQAbstractNotifier* notifier) {
        return notifier->NotifyTransaction(tx);
    });
}

void CZMQNotificationInterface::BlockConnected(
    const std::shared_ptr<const CBlock> &pblock,
    const CBlockIndex *,
    const std::vector<CTransactionRef> &) {
    for (const CTransactionRef &ptx : pblock->vtx) {
        // Do a normal notify for each transaction added in the block
        TransactionAddedToMempool(ptx);
    }
}

void CZMQNotificationInterface::BlockDisconnected(
    const std::shared_ptr<const CBlock> &pblock) {
    for (const CTransactionRef &ptx : pblock->vtx) {
        // Do a normal notify for each transaction removed in block
        // disconnection
        TransactionAddedToMempool(ptx);
    }
}

void CZMQNotificationInterface::TransactionDoubleSpent(const CTransactionRef &ptx, const DspId &) {
    const CTransaction &tx = *ptx;

    TryForEachAndRemoveFailed(notifiers, [&tx](CZMQAbstractNotifier* notifier) {
        return notifier->NotifyDoubleSpend(tx);
    });
}


CZMQNotificationInterface *g_zmq_notification_interface = nullptr;
