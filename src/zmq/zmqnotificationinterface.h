// Copyright (c) 2015-2018 The Fittexxcoin Core developers
// Copyright (c) 2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <validationinterface.h>

#include <list>
#include <map>
#include <memory>

class CBlockIndex;
class CZMQAbstractNotifier;

class CZMQNotificationInterface final : public CValidationInterface {
public:
    virtual ~CZMQNotificationInterface();

    std::list<const CZMQAbstractNotifier *> GetActiveNotifiers() const;

    static CZMQNotificationInterface *Create();

protected:
    bool Initialize();
    void Shutdown();

    // CValidationInterface
    void TransactionAddedToMempool(const CTransactionRef &tx) override;
    void BlockConnected(const std::shared_ptr<const CBlock> &pblock,
                        const CBlockIndex *pindexConnected,
                        const std::vector<CTransactionRef> &vtxConflicted) override;
    void BlockDisconnected(const std::shared_ptr<const CBlock> &pblock) override;
    void UpdatedBlockTip(const CBlockIndex *pindexNew,
                         const CBlockIndex *pindexFork,
                         bool fInitialDownload) override;
    void TransactionDoubleSpent(const CTransactionRef &ptxn,
                                const DspId &dspId) override;

private:
    CZMQNotificationInterface();

    void *pcontext;
    std::list<std::unique_ptr<CZMQAbstractNotifier>> notifiers;
};

extern CZMQNotificationInterface *g_zmq_notification_interface;
