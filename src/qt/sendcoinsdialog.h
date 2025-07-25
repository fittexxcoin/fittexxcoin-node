// Copyright (c) 2011-2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <qt/walletmodel.h>

#include <QDialog>
#include <QMessageBox>
#include <QString>
#include <QTimer>

class ClientModel;
class PlatformStyle;
class SendCoinsEntry;
class SendCoinsRecipient;

namespace Ui {
class SendCoinsDialog;
}

QT_BEGIN_NAMESPACE
class QUrl;
QT_END_NAMESPACE

/** Dialog for sending fittexxcoins */
class SendCoinsDialog : public QDialog {
    Q_OBJECT

public:
    SendCoinsDialog(const PlatformStyle *platformStyle, WalletModel *model,
                    QWidget *parent = nullptr);
    ~SendCoinsDialog();

    void setClientModel(ClientModel *clientModel);
    void setModel(WalletModel *model);

    /**
     * Set up the tab chain manually, as Qt messes up the tab chain by default
     * in some cases (issue
     * https://bugreports.qt-project.org/browse/QTBUG-10907).
     */
    QWidget *setupTafxxain(QWidget *prev);

    void setAddress(const QString &address);
    void pasteEntry(const SendCoinsRecipient &rv);
    bool handlePaymentRequest(const SendCoinsRecipient &recipient);

public Q_SLOTS:
    void clear();
    void reject() override;
    void accept() override;
    SendCoinsEntry *addEntry();
    void updateTabsAndLabels();
    void setBalance(const interfaces::WalletBalances &balances);

Q_SIGNALS:
    void coinsSent(const uint256 &txid);

private:
    Ui::SendCoinsDialog *ui;
    ClientModel *clientModel;
    WalletModel *model;
    bool fNewRecipientAllowed;
    bool fFeeMinimized;
    const PlatformStyle *platformStyle;

    // Process WalletModel::SendCoinsReturn and generate a pair consisting of a
    // message and message flags for use in Q_EMIT message().
    // Additional parameter msgArg can be used via .arg(msgArg).
    void
    processSendCoinsReturn(const WalletModel::SendCoinsReturn &sendCoinsReturn,
                           const QString &msgArg = QString());
    void minimizeFeeSection(bool fMinimize);
    void updateFeeMinimizedLabel();
    // Update the passed in CCoinControl with state from the GUI
    void updateCoinControlState(CCoinControl &ctrl);

private Q_SLOTS:
    void on_sendButton_clicked();
    void on_buttonChooseFee_clicked();
    void on_buttonMinimizeFee_clicked();
    void removeEntry(SendCoinsEntry *entry);
    void useAvailableBalance(SendCoinsEntry *entry);
    void updateDisplayUnit();
    void coinControlFeatureChanged(bool);
    void coinControlButtonClicked();
    void coinControlChangeChecked(int);
    void coinControlChangeEdited(const QString &);
    void coinControlUpdateLabels();
    void coinControlClipboardQuantity();
    void coinControlClipboardAmount();
    void coinControlClipboardFee();
    void coinControlClipboardAfterFee();
    void coinControlClipboardBytes();
    void coinControlClipboardLowOutput();
    void coinControlClipboardChange();
    void setMinimumFee();
    void updateFeeSectionControls();
    void updateMinFeeLabel();
    void updateSmartFeeLabel();

Q_SIGNALS:
    // Fired when a message should be reported to the user
    void message(const QString &title, const QString &message,
                 unsigned int style);
};

#define SEND_CONFIRM_DELAY 3

class SendConfirmationDialog : public QMessageBox {
    Q_OBJECT

public:
    SendConfirmationDialog(const QString &title, const QString &text,
                           int secDelay = SEND_CONFIRM_DELAY,
                           QWidget *parent = nullptr);
    int exec();

private Q_SLOTS:
    void countDown();
    void updateYesButton();

private:
    QAbstractButton *yesButton;
    QTimer countDownTimer;
    int secDelay;
};
