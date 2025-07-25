// Copyright (c) 2011-2014 The Fittexxcoin Core developers
// Copyright (c) 2017-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <QObject>
#include <QString>

class TransactionRecord;

namespace interfaces {
class Node;
class Wallet;
struct WalletTx;
struct WalletTxStatus;
} // namespace interfaces

/**
 * Provide a human-readable extended HTML description of a transaction.
 */
class TransactionDesc : public QObject {
    Q_OBJECT

public:
    static QString toHTML(interfaces::Node &node, interfaces::Wallet &wallet,
                          TransactionRecord *rec, int unit);

private:
    TransactionDesc() {}

    static QString FormatTxStatus(const interfaces::WalletTx &wtx,
                                  const interfaces::WalletTxStatus &status,
                                  bool inMempool, int numBlocks);
};
