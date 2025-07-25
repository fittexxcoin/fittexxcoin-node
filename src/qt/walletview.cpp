// Copyright (c) 2011-2016 The Fittexxcoin Core developers
// Copyright (c) 2022 The Fittexxcoin Node developers
// Copyright (c) 2017-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <txmempool.h>

#include <qt/walletview.h>

#include <interfaces/node.h>
#include <qt/addressbookpage.h>
#include <qt/askpassphrasedialog.h>
#include <qt/fittexxcoingui.h>
#include <qt/clientmodel.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/overviewpage.h>
#include <qt/platformstyle.h>
#include <qt/receivecoinsdialog.h>
#include <qt/sendcoinsdialog.h>
#include <qt/signverifymessagedialog.h>
#include <qt/transactiontablemodel.h>
#include <qt/transactionview.h>
#include <qt/walletmodel.h>
#include <ui_interface.h>

#include <QAction>
#include <QActionGroup>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QProgressDialog>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDebug>

extern CTxMemPool g_mempool;

WalletView::WalletView(const PlatformStyle *_platformStyle,
                       WalletModel *_walletModel, QWidget *parent)
    : QStackedWidget(parent), clientModel(nullptr), walletModel(_walletModel),
      platformStyle(_platformStyle) {
    // Create tabs
    overviewPage = new OverviewPage(platformStyle);

    transactionsPage = new QWidget(this);
    QVBoxLayout *vbox = new QVBoxLayout();
    QHBoxLayout *hbox_buttons = new QHBoxLayout();
    transactionView = new TransactionView(platformStyle, this);
    vbox->addWidget(transactionView);
    QPushButton *exportButton = new QPushButton(tr("&Export"), this);
    exportButton->setToolTip(
        tr("Export the data in the current tab to a file"));
    if (platformStyle->getImagesOnButtons()) {
        exportButton->setIcon(platformStyle->SingleColorIcon(":/icons/export"));
    }
    hbox_buttons->addStretch();
    hbox_buttons->addWidget(exportButton);
    vbox->addLayout(hbox_buttons);
    transactionsPage->setLayout(vbox);

    receiveCoinsPage = new ReceiveCoinsDialog(platformStyle);
    sendCoinsPage = new SendCoinsDialog(platformStyle, walletModel);

    usedSendingAddressesPage =
        new AddressBookPage(platformStyle, AddressBookPage::ForEditing,
                            AddressBookPage::SendingTab, this);
    usedReceivingAddressesPage =
        new AddressBookPage(platformStyle, AddressBookPage::ForEditing,
                            AddressBookPage::ReceivingTab, this);

    addWidget(overviewPage);
    addWidget(transactionsPage);
    addWidget(receiveCoinsPage);
    addWidget(sendCoinsPage);

    // Clicking on a transaction on the overview pre-selects the transaction on
    // the transaction history page
    connect(overviewPage, &OverviewPage::transactionClicked, transactionView,
            static_cast<void (TransactionView::*)(const QModelIndex &)>(
                &TransactionView::focusTransaction));

    connect(overviewPage, &OverviewPage::outOfSyncWarningClicked, this,
            &WalletView::requestedSyncWarningInfo);

    // Highlight transaction after send
    connect(sendCoinsPage, &SendCoinsDialog::coinsSent, transactionView,
            static_cast<void (TransactionView::*)(const uint256 &)>(
                &TransactionView::focusTransaction));

    // Clicking on "Export" allows to export the transaction list
    connect(exportButton, &QPushButton::clicked, transactionView,
            &TransactionView::exportClicked);

    // Pass through messages from sendCoinsPage
    connect(sendCoinsPage, &SendCoinsDialog::message, this,
            &WalletView::message);
    // Pass through messages from transactionView
    connect(transactionView, &TransactionView::message, this,
            &WalletView::message);

    // Set the model properly.
    setWalletModel(walletModel);
}

WalletView::~WalletView() {}

void WalletView::setFittexxcoinGUI(FittexxcoinGUI *gui) {
    if (gui) {
        // Clicking on a transaction on the overview page simply sends you to
        // transaction history page
        connect(overviewPage, &OverviewPage::transactionClicked, gui,
                &FittexxcoinGUI::gotoHistoryPage);

        // Navigate to transaction history page after send
        connect(sendCoinsPage, &SendCoinsDialog::coinsSent, gui,
                &FittexxcoinGUI::gotoHistoryPage);

        // Receive and report messages
        connect(
            this, &WalletView::message,
            [gui](const QString &title, const QString &message,
                  unsigned int style) { gui->message(title, message, style); });

        // Pass through encryption status changed signals
        connect(this, &WalletView::encryptionStatusChanged, gui,
                &FittexxcoinGUI::updateWalletStatus);

        // Pass through transaction notifications
        connect(this, &WalletView::incomingTransaction, gui,
                &FittexxcoinGUI::incomingTransaction);

        // Connect HD enabled state signal
        connect(this, &WalletView::hdEnabledStatusChanged, gui,
                &FittexxcoinGUI::updateWalletStatus);
    }
}

void WalletView::setClientModel(ClientModel *_clientModel) {
    this->clientModel = _clientModel;

    if (_clientModel) {
        connect(_clientModel, &ClientModel::transactionDoubleSpent, this,
                &WalletView::transactionDoubleSpent);
    }

    overviewPage->setClientModel(_clientModel);
    sendCoinsPage->setClientModel(_clientModel);
}

void WalletView::transactionDoubleSpent(const TxId txId, const DspId dspId) {
    const CTransactionRef &tx = walletModel->wallet().getTx(txId);
    if (!tx) {
        return;
    }

    const auto &optDspPair = g_mempool.getDoubleSpendProof(dspId, nullptr);
    if (!optDspPair.has_value()) {
        return;
    }

    const DoubleSpendProof &dsProof = (*optDspPair).first;
    QString msg = tr("Outpoint %1:%2 was attempted to be double spent").arg(
        QString::fromStdString(dsProof.prevTxId().ToString().substr(0, 10)),
        QString::number(dsProof.prevOutIndex())
    );
    Q_EMIT message(tr("Double Spend Proof"), msg, CClientUIInterface::MSG_INFORMATION);

    QString hashQStr = QString::fromStdString(txId.ToString());
    walletModel->getTransactionTableModel()->updateTransaction(hashQStr, CT_UPDATED,
                                                               true);
}

void WalletView::setWalletModel(WalletModel *_walletModel) {
    this->walletModel = _walletModel;

    // Put transaction list in tabs
    transactionView->setModel(_walletModel);
    overviewPage->setWalletModel(_walletModel);
    receiveCoinsPage->setModel(_walletModel);
    sendCoinsPage->setModel(_walletModel);
    usedReceivingAddressesPage->setModel(
        _walletModel ? _walletModel->getAddressTableModel() : nullptr);
    usedSendingAddressesPage->setModel(
        _walletModel ? _walletModel->getAddressTableModel() : nullptr);

    if (_walletModel) {
        // Receive and pass through messages from wallet model
        connect(_walletModel, &WalletModel::message, this,
                &WalletView::message);

        // Handle changes in encryption status
        connect(_walletModel, &WalletModel::encryptionStatusChanged, this,
                &WalletView::encryptionStatusChanged);
        updateEncryptionStatus();

        // update HD status
        Q_EMIT hdEnabledStatusChanged();

        // Balloon pop-up for new transaction
        connect(_walletModel->getTransactionTableModel(),
                &TransactionTableModel::rowsInserted, this,
                &WalletView::processNewTransaction);

        // Ask for passphrase if needed
        connect(_walletModel, &WalletModel::requireUnlock, this,
                &WalletView::unlockWallet);

        // Show progress dialog
        connect(_walletModel, &WalletModel::showProgress, this,
                &WalletView::showProgress);
    }
}

void WalletView::processNewTransaction(const QModelIndex &parent, int start,
                                       int end) {
    // Prevent balloon-spam when initial block download is in progress
    if (!walletModel || !clientModel ||
        clientModel->node().isInitialBlockDownload()) {
        return;
    }

    TransactionTableModel *ttm = walletModel->getTransactionTableModel();
    if (!ttm || ttm->processingQueuedTransactions()) {
        return;
    }

    QString date = ttm->index(start, TransactionTableModel::Date, parent)
                       .data()
                       .toString();
    qint64 amount = ttm->index(start, TransactionTableModel::Amount, parent)
                        .data(Qt::EditRole)
                        .toULongLong();
    QString type = ttm->index(start, TransactionTableModel::Type, parent)
                       .data()
                       .toString();
    QModelIndex index = ttm->index(start, 0, parent);
    QString address =
        ttm->data(index, TransactionTableModel::AddressRole).toString();
    QString label =
        ttm->data(index, TransactionTableModel::LabelRole).toString();

    Q_EMIT incomingTransaction(date,
                               walletModel->getOptionsModel()->getDisplayUnit(),
                               int64_t(amount) * SATOSHI, type, address, label,
                               walletModel->getWalletName());
}

void WalletView::gotoOverviewPage() {
    setCurrentWidget(overviewPage);
}

void WalletView::gotoHistoryPage() {
    setCurrentWidget(transactionsPage);
}

void WalletView::gotoReceiveCoinsPage() {
    setCurrentWidget(receiveCoinsPage);
}

void WalletView::gotoSendCoinsPage(QString addr) {
    setCurrentWidget(sendCoinsPage);

    if (!addr.isEmpty()) {
        sendCoinsPage->setAddress(addr);
    }
}

void WalletView::gotoSignMessageTab(QString addr) {
    // calls show() in showTab_SM()
    SignVerifyMessageDialog *signVerifyMessageDialog =
        new SignVerifyMessageDialog(platformStyle, this);
    signVerifyMessageDialog->setAttribute(Qt::WA_DeleteOnClose);
    signVerifyMessageDialog->setModel(walletModel);
    signVerifyMessageDialog->showTab_SM(true);

    if (!addr.isEmpty()) {
        signVerifyMessageDialog->setAddress_SM(addr);
    }
}

void WalletView::gotoVerifyMessageTab(QString addr) {
    // calls show() in showTab_VM()
    SignVerifyMessageDialog *signVerifyMessageDialog =
        new SignVerifyMessageDialog(platformStyle, this);
    signVerifyMessageDialog->setAttribute(Qt::WA_DeleteOnClose);
    signVerifyMessageDialog->setModel(walletModel);
    signVerifyMessageDialog->showTab_VM(true);

    if (!addr.isEmpty()) {
        signVerifyMessageDialog->setAddress_VM(addr);
    }
}

bool WalletView::handlePaymentRequest(const SendCoinsRecipient &recipient) {
    return sendCoinsPage->handlePaymentRequest(recipient);
}

void WalletView::showOutOfSyncWarning(bool fShow) {
    overviewPage->showOutOfSyncWarning(fShow);
}

void WalletView::updateEncryptionStatus() {
    Q_EMIT encryptionStatusChanged();
}

void WalletView::encryptWallet(bool status) {
    if (!walletModel) {
        return;
    }

    AskPassphraseDialog dlg(status ? AskPassphraseDialog::Encrypt
                                   : AskPassphraseDialog::Decrypt,
                            this);
    dlg.setModel(walletModel);
    dlg.exec();

    updateEncryptionStatus();
}

void WalletView::backupWallet() {
    QString filename =
        GUIUtil::getSaveFileName(this, tr("Backup Wallet"), QString(),
                                 tr("Wallet Data (*.dat)"), nullptr);

    if (filename.isEmpty()) {
        return;
    }

    if (!walletModel->wallet().backupWallet(filename.toLocal8Bit().data())) {
        Q_EMIT message(
            tr("Backup Failed"),
            tr("There was an error trying to save the wallet data to %1.")
                .arg(filename),
            CClientUIInterface::MSG_ERROR);
    } else {
        Q_EMIT message(
            tr("Backup Successful"),
            tr("The wallet data was successfully saved to %1.").arg(filename),
            CClientUIInterface::MSG_INFORMATION);
    }
}

void WalletView::changePassphrase() {
    AskPassphraseDialog dlg(AskPassphraseDialog::ChangePass, this);
    dlg.setModel(walletModel);
    dlg.exec();
}

void WalletView::unlockWallet() {
    if (!walletModel) {
        return;
    }

    // Unlock wallet when requested by wallet model
    if (walletModel->getEncryptionStatus() == WalletModel::Locked) {
        AskPassphraseDialog dlg(AskPassphraseDialog::Unlock, this);
        dlg.setModel(walletModel);
        dlg.exec();
    }
}

void WalletView::usedSendingAddresses() {
    if (!walletModel) {
        return;
    }

    GUIUtil::bringToFront(usedSendingAddressesPage);
}

void WalletView::usedReceivingAddresses() {
    if (!walletModel) {
        return;
    }

    GUIUtil::bringToFront(usedReceivingAddressesPage);
}

void WalletView::showProgress(const QString &title, int nProgress) {
    if (nProgress == 0) {
        progressDialog = new QProgressDialog(title, "", 0, 100);
        progressDialog->setWindowModality(Qt::ApplicationModal);
        progressDialog->setMinimumDuration(0);
        progressDialog->setAutoClose(false);
        progressDialog->setValue(0);
        progressDialog->setCancelButtonText(tr("Cancel"));
    } else if (nProgress == 100) {
        if (progressDialog) {
            progressDialog->close();
            progressDialog->deleteLater();
        }
    } else if (progressDialog) {
        if (progressDialog->wasCanceled()) {
            getWalletModel()->wallet().abortRescan();
        } else {
            progressDialog->setValue(nProgress);
        }
    }
}

void WalletView::requestedSyncWarningInfo() {
    Q_EMIT outOfSyncWarningClicked();
}
