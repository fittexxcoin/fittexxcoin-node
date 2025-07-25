// Copyright (c) 2011-2014 The Fittexxcoin Core developers
// Copyright (c) 2017-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <qt/walletmodel.h>

#include <amount.h>

#include <QObject>

#include <memory>

class SendCoinsRecipient;

namespace interfaces {
class Node;
class PendingWalletTx;
} // namespace interfaces

/** Data model for a walletmodel transaction. */
class WalletModelTransaction {
public:
    explicit WalletModelTransaction(
        const QList<SendCoinsRecipient> &recipients);

    QList<SendCoinsRecipient> getRecipients() const;

    std::unique_ptr<interfaces::PendingWalletTx> &getWtx();
    unsigned int getTransactionSize();

    void setTransactionFee(const Amount newFee);
    Amount getTransactionFee() const;

    Amount getTotalTransactionAmount() const;

    // needed for the subtract-fee-from-amount feature
    void reassignAmounts(int nChangePosRet);

private:
    QList<SendCoinsRecipient> recipients;
    std::unique_ptr<interfaces::PendingWalletTx> wtx;
    Amount fee;
};
