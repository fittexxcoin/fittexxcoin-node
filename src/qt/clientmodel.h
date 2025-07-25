// Copyright (c) 2011-2016 The Fittexxcoin Core developers
// Copyright (c) 2020-2022 The Fittexxcoin developers
// Copyright (c) 2022 The Fittexxcoin Node developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <dsproof/dspid.h>
#include <primitives/txid.h>

#include <QDateTime>
#include <QObject>

#include <atomic>
#include <memory>

class BanTableModel;
class OptionsModel;
class PeerTableModel;

class CBlockIndex;

namespace interfaces {
class Handler;
class Node;
} // namespace interfaces

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

enum class BlockSource { NONE, REINDEX, DISK, NETWORK };

/** Model for Fittexxcoin network client. */
class ClientModel : public QObject {
    Q_OBJECT

public:
    enum NumConnections {
        CONNECTIONS_NONE = 0,
        CONNECTIONS_IN = (1U << 0),
        CONNECTIONS_OUT = (1U << 1),
        CONNECTIONS_ALL = (CONNECTIONS_IN | CONNECTIONS_OUT),
    };

    explicit ClientModel(interfaces::Node &node, OptionsModel *optionsModel,
                         QObject *parent = nullptr);
    ~ClientModel();

    interfaces::Node &node() const { return m_node; }
    OptionsModel *getOptionsModel();
    PeerTableModel *getPeerTableModel();
    BanTableModel *getBanTableModel();

    //! Return number of connections, default is in- and outbound (total)
    int getNumConnections(NumConnections flags = CONNECTIONS_ALL) const;
    int getHeaderTipHeight() const;
    int64_t getHeaderTipTime() const;

    //! Returns enum BlockSource of the current importing/syncing state
    enum BlockSource getBlockSource() const;
    //! Return warnings to be displayed in status bar
    QString getStatusBarWarnings() const;

    QString formatFullVersion() const;
    QString formatSubVersion() const;
    bool isReleaseVersion() const;
    QString formatClientStartupTime() const;
    QString dataDir() const;
    QString blocksDir() const;

    bool getProxyInfo(std::string &ip_port) const;

    // caches for the best header
    mutable std::atomic<int> cachedBestHeaderHeight;
    mutable std::atomic<int64_t> cachedBestHeaderTime;

private:
    interfaces::Node &m_node;
    std::unique_ptr<interfaces::Handler> m_handler_show_progress;
    std::unique_ptr<interfaces::Handler>
        m_handler_notify_num_connections_changed;
    std::unique_ptr<interfaces::Handler>
        m_handler_notify_network_active_changed;
    std::unique_ptr<interfaces::Handler> m_handler_notify_alert_changed;
    std::unique_ptr<interfaces::Handler> m_handler_banned_list_changed;
    std::unique_ptr<interfaces::Handler> m_handler_notify_block_tip;
    std::unique_ptr<interfaces::Handler> m_handler_notify_header_tip;
    std::unique_ptr<interfaces::Handler> m_handler_notify_transaction_double_spent;
    OptionsModel *optionsModel;
    PeerTableModel *peerTableModel;
    BanTableModel *banTableModel;

    QTimer *pollTimer;

    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();

Q_SIGNALS:
    void numConnectionsChanged(int count);
    void numBlocksChanged(int count, const QDateTime &blockDate, const QString &blockHash, double nVerificationProgress, bool header);
    void mempoolSizeChanged(size_t count, size_t totalSize, size_t dynamicUsage);
    void networkActiveChanged(bool networkActive);
    void alertsChanged(const QString &warnings);
    void bytesChanged(quint64 totalBytesIn, quint64 totalBytesOut);

    //! Fired when a message should be reported to the user
    void message(const QString &title, const QString &message,
                 unsigned int style);

    // Show progress dialog e.g. for verifychain
    void showProgress(const QString &title, int nProgress);

    void transactionDoubleSpent(const TxId txId, const DspId dspId);

public Q_SLOTS:
    void updateTimer();
    void updateNumConnections(int numConnections);
    void updateNetworkActive(bool networkActive);
    void updateAlert();
    void updateBanlist();
};
