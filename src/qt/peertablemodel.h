// Copyright (c) 2011-2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <net.h>
#include <net_processing.h> // For CNodeStateStats

#include <QAbstractTableModel>
#include <QStringList>

#include <memory>

class ClientModel;
class PeerTablePriv;

namespace interfaces {
class Node;
}

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

struct CNodeCombinedStats {
    CNodeStats nodeStats;
    CNodeStateStats nodeStateStats;
    bool fNodeStateStatsAvailable;
};

class NodeLessThan {
public:
    NodeLessThan(int nColumn, Qt::SortOrder fOrder)
        : column(nColumn), order(fOrder) {}
    bool operator()(const CNodeCombinedStats &left,
                    const CNodeCombinedStats &right) const;

private:
    int column;
    Qt::SortOrder order;
};

/**
 * Qt model providing information about connected peers, similar to the
 * "getpeerinfo" RPC call. Used by the rpc console UI.
 */
class PeerTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit PeerTableModel(interfaces::Node &node,
                            ClientModel *parent = nullptr);
    ~PeerTableModel();
    const CNodeCombinedStats *getNodeStats(int idx);
    int getRowByNodeId(NodeId nodeid);
    void startAutoRefresh();
    void stopAutoRefresh();

    enum ColumnIndex {
        NetNodeId = 0,
        Address = 1,
        Ping = 2,
        Sent = 3,
        Received = 4,
        Subversion = 5,
    };

    /** @name Methods overridden from QAbstractTableModel
        @{*/
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role) const override;
    QModelIndex index(int row, int column,
                      const QModelIndex &parent) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    void sort(int column, Qt::SortOrder order) override;
    /*@}*/

public Q_SLOTS:
    void refresh();

private:
    interfaces::Node &m_node;
    ClientModel *clientModel;
    QStringList columns;
    std::unique_ptr<PeerTablePriv> priv;
    QTimer *timer;
};
