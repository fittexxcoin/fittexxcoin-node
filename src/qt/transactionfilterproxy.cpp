// Copyright (c) 2011-2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/transactionfilterproxy.h>

#include <qt/guiutil.h>
#include <qt/transactionrecord.h>
#include <qt/transactiontablemodel.h>

#include <QDateTime>

#include <cstdlib>

// Earliest date that can be represented (far in the past)
const QDateTime TransactionFilterProxy::MIN_DATE = GUIUtil::dateTimeFromTime(0);
// Last date that can be represented (far in the future)
const QDateTime TransactionFilterProxy::MAX_DATE = GUIUtil::dateTimeFromTime(0xFF'FF'FF'FF);

TransactionFilterProxy::TransactionFilterProxy(QObject *parent)
    : QSortFilterProxyModel(parent), dateFrom(MIN_DATE), dateTo(MAX_DATE),
      m_search_string(), typeFilter(ALL_TYPES),
      watchOnlyFilter(WatchOnlyFilter_All), minAmount(), limitRows(-1),
      showInactive(true) {}

bool TransactionFilterProxy::filterAcceptsRow(
    int sourceRow, const QModelIndex &sourceParent) const {
    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);

    int status = index.data(TransactionTableModel::StatusRole).toInt();
    if (!showInactive && status == TransactionStatus::Conflicted) {
        return false;
    }

    int type = index.data(TransactionTableModel::TypeRole).toInt();
    if (!(TYPE(type) & typeFilter)) {
        return false;
    }

    bool involvesWatchAddress =
        index.data(TransactionTableModel::WatchonlyRole).toBool();
    if (involvesWatchAddress && watchOnlyFilter == WatchOnlyFilter_No) {
        return false;
    }
    if (!involvesWatchAddress && watchOnlyFilter == WatchOnlyFilter_Yes) {
        return false;
    }

    QDateTime datetime =
        index.data(TransactionTableModel::DateRole).toDateTime();
    if (datetime < dateFrom || datetime > dateTo) {
        return false;
    }

    QString address = index.data(TransactionTableModel::AddressRole).toString();
    QString label = index.data(TransactionTableModel::LabelRole).toString();
    QString txid = index.data(TransactionTableModel::TxHashRole).toString();
    if (!address.contains(m_search_string, Qt::CaseInsensitive) &&
        !label.contains(m_search_string, Qt::CaseInsensitive) &&
        !txid.contains(m_search_string, Qt::CaseInsensitive)) {
        return false;
    }

    Amount amount(
        int64_t(
            llabs(index.data(TransactionTableModel::AmountRole).toLongLong())) *
        SATOSHI);
    if (amount < minAmount) {
        return false;
    }

    return true;
}

void TransactionFilterProxy::setDateRange(const QDateTime &from,
                                          const QDateTime &to) {
    this->dateFrom = from;
    this->dateTo = to;
    invalidateFilter();
}

void TransactionFilterProxy::setSearchString(const QString &search_string) {
    if (m_search_string == search_string) {
        return;
    }
    m_search_string = search_string;
    invalidateFilter();
}

void TransactionFilterProxy::setTypeFilter(quint32 modes) {
    this->typeFilter = modes;
    invalidateFilter();
}

void TransactionFilterProxy::setMinAmount(const Amount minimum) {
    this->minAmount = minimum;
    invalidateFilter();
}

void TransactionFilterProxy::setWatchOnlyFilter(WatchOnlyFilter filter) {
    this->watchOnlyFilter = filter;
    invalidateFilter();
}

void TransactionFilterProxy::setLimit(int limit) {
    this->limitRows = limit;
}

void TransactionFilterProxy::setShowInactive(bool _showInactive) {
    this->showInactive = _showInactive;
    invalidateFilter();
}

int TransactionFilterProxy::rowCount(const QModelIndex &parent) const {
    if (limitRows != -1) {
        return std::min(QSortFilterProxyModel::rowCount(parent), limitRows);
    } else {
        return QSortFilterProxyModel::rowCount(parent);
    }
}
