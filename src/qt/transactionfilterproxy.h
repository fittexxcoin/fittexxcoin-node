// Copyright (c) 2011-2014 The Fittexxcoin Core developers
// Copyright (c) 2017-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <amount.h>

#include <QDateTime>
#include <QSortFilterProxyModel>

/**
 * Filter the transaction list according to pre-specified rules.
 */
class TransactionFilterProxy : public QSortFilterProxyModel {
    Q_OBJECT

public:
    explicit TransactionFilterProxy(QObject *parent = nullptr);

    /**
     * Earliest date that can be represented (far in the past).
     */
    static const QDateTime MIN_DATE;
    /**
     * Last date that can be represented (far in the future).
     */
    static const QDateTime MAX_DATE;
    /**
     * Type filter bit field (all types).
     */
    static const quint32 ALL_TYPES = 0xFFFFFFFF;

    static quint32 TYPE(int type) { return 1 << type; }

    enum WatchOnlyFilter {
        WatchOnlyFilter_All,
        WatchOnlyFilter_Yes,
        WatchOnlyFilter_No
    };

    void setDateRange(const QDateTime &from, const QDateTime &to);
    void setSearchString(const QString &);
    /**
     * @note Type filter takes a bit field created with TYPE() or ALL_TYPES
     */
    void setTypeFilter(quint32 modes);
    void setMinAmount(const Amount minimum);
    void setWatchOnlyFilter(WatchOnlyFilter filter);

    /**
     * Set maximum number of rows returned, -1 if unlimited.
     */
    void setLimit(int limit);

    /**
     * Set whether to show conflicted transactions.
     */
    void setShowInactive(bool showInactive);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

protected:
    bool filterAcceptsRow(int source_row,
                          const QModelIndex &source_parent) const override;

private:
    QDateTime dateFrom;
    QDateTime dateTo;
    QString m_search_string;
    quint32 typeFilter;
    WatchOnlyFilter watchOnlyFilter;
    Amount minAmount;
    int limitRows;
    bool showInactive;
};
