// Copyright (c) 2011-2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <qt/guiutil.h>

#include <QDialog>
#include <QHeaderView>
#include <QItemSelection>
#include <QKeyEvent>
#include <QMenu>
#include <QPoint>
#include <QVariant>

class PlatformStyle;
class WalletModel;

namespace Ui {
class ReceiveCoinsDialog;
}

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/** Dialog for requesting payment of fittexxcoins */
class ReceiveCoinsDialog : public QDialog {
    Q_OBJECT

public:
    enum ColumnWidths {
        DATE_COLUMN_WIDTH = 130,
        LABEL_COLUMN_WIDTH = 120,
        AMOUNT_MINIMUM_COLUMN_WIDTH = 180,
        MINIMUM_COLUMN_WIDTH = 130
    };

    explicit ReceiveCoinsDialog(const PlatformStyle *platformStyle,
                                QWidget *parent = nullptr);
    ~ReceiveCoinsDialog();

    void setModel(WalletModel *model);

public Q_SLOTS:
    void clear();
    void reject();
    void accept();

protected:
    virtual void keyPressEvent(QKeyEvent *event);

private:
    Ui::ReceiveCoinsDialog *ui;
    GUIUtil::TableViewLastColumnResizingFixer *columnResizingFixer;
    WalletModel *model;
    QMenu *contextMenu;
    const PlatformStyle *platformStyle;

    QModelIndex selectedRow();
    void copyColumnToClipboard(int column);
    virtual void resizeEvent(QResizeEvent *event);

private Q_SLOTS:
    void on_receiveButton_clicked();
    void on_showRequestButton_clicked();
    void on_removeRequestButton_clicked();
    void on_recentRequestsView_doubleClicked(const QModelIndex &index);
    void recentRequestsView_selectionChanged(const QItemSelection &selected,
                                             const QItemSelection &deselected);
    void updateDisplayUnit();
    void showMenu(const QPoint &point);
    void copyURI();
    void copyLabel();
    void copyMessage();
    void copyAmount();
};
