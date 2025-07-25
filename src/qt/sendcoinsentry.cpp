// Copyright (c) 2011-2016 The Fittexxcoin Core developers
// Copyright (c) 2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/fittexxcoin-config.h>
#endif

#include <qt/forms/ui_sendcoinsentry.h>
#include <qt/sendcoinsentry.h>

#include <config.h>
#include <qt/addressbookpage.h>
#include <qt/addresstablemodel.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/walletmodel.h>

#include <QApplication>

SendCoinsEntry::SendCoinsEntry(const PlatformStyle *_platformStyle,
                               WalletModel *_model, QWidget *parent)
    : QStackedWidget(parent), ui(new Ui::SendCoinsEntry), model(_model),
      platformStyle(_platformStyle) {
    ui->setupUi(this);

    ui->addressBookButton->setIcon(
        platformStyle->SingleColorIcon(":/icons/address-book"));
    ui->pasteButton->setIcon(
        platformStyle->SingleColorIcon(":/icons/editpaste"));
    ui->deleteButton->setIcon(platformStyle->SingleColorIcon(":/icons/remove"));
    ui->deleteButton_is->setIcon(
        platformStyle->SingleColorIcon(":/icons/remove"));
    ui->deleteButton_s->setIcon(
        platformStyle->SingleColorIcon(":/icons/remove"));

    setCurrentWidget(ui->SendCoins);

    if (platformStyle->getUseExtraSpacing()) {
        ui->payToLayout->setSpacing(4);
    }
    ui->addAsLabel->setPlaceholderText(
        tr("Enter a label for this address to add it to your address book"));

    // normal Fittexxcoin address field
    GUIUtil::setupAddressWidget(ui->payTo, this);
    // just a label for displaying Fittexxcoin address(es)
    ui->payTo_is->setFont(GUIUtil::fixedPitchFont());

    // Connect signals
    connect(ui->payAmount, &FittexxcoinAmountField::valueChanged, this,
            &SendCoinsEntry::payAmountChanged);
    connect(ui->checkboxSubtractFeeFromAmount, &QCheckBox::toggled, this,
            &SendCoinsEntry::subtractFeeFromAmountChanged);
    connect(ui->deleteButton, &QPushButton::clicked, this,
            &SendCoinsEntry::deleteClicked);
    connect(ui->deleteButton_is, &QPushButton::clicked, this,
            &SendCoinsEntry::deleteClicked);
    connect(ui->deleteButton_s, &QPushButton::clicked, this,
            &SendCoinsEntry::deleteClicked);
    connect(ui->useAvailableBalanceButton, &QPushButton::clicked, this,
            &SendCoinsEntry::useAvailableBalanceClicked);

    // Set the model properly.
    setModel(model);
}

SendCoinsEntry::~SendCoinsEntry() {
    delete ui;
}

void SendCoinsEntry::on_pasteButton_clicked() {
    // Paste text from clipboard into recipient field
    // The field is focused so that the pasted address is validated/normalized on blur
    ui->payTo->clear();
    ui->payTo->setFocus();
    ui->payTo->paste();
}

void SendCoinsEntry::on_addressBookButton_clicked() {
    if (!model) {
        return;
    }
    AddressBookPage dlg(platformStyle, AddressBookPage::ForSelection,
                        AddressBookPage::SendingTab, this);
    dlg.setModel(model->getAddressTableModel());
    if (dlg.exec()) {
        ui->payTo->setText(dlg.getReturnValue());
        ui->payAmount->setFocus();
    }
}

void SendCoinsEntry::on_payTo_textChanged(const QString &address) {
    updateLabel(address);
}

void SendCoinsEntry::setModel(WalletModel *_model) {
    this->model = _model;

    if (_model) {
        ui->messageTextLabel->setToolTip(
            tr("A message that was attached to the %1 URI which will be stored "
               "with the transaction for your reference. Note: This message "
               "will not be sent over the Fittexxcoin network.")
                .arg(QString::fromStdString(
                    _model->getChainParams().CashAddrPrefix())));
    }

    if (_model && _model->getOptionsModel()) {
        connect(_model->getOptionsModel(), &OptionsModel::displayUnitChanged,
                this, &SendCoinsEntry::updateDisplayUnit);
    }

    clear();
}

void SendCoinsEntry::clear() {
    // clear UI elements for normal payment
    ui->payTo->clear();
    ui->addAsLabel->clear();
    ui->payAmount->clear();
    ui->checkboxSubtractFeeFromAmount->setCheckState(Qt::Unchecked);
    ui->messageTextLabel->clear();
    ui->messageTextLabel->hide();
    ui->messageLabel->hide();
    // clear UI elements for unauthenticated payment request
    ui->payTo_is->clear();
    ui->memoTextLabel_is->clear();
    ui->payAmount_is->clear();
    // clear UI elements for authenticated payment request
    ui->payTo_s->clear();
    ui->memoTextLabel_s->clear();
    ui->payAmount_s->clear();

    // update the display unit, to not use the default ("fxx")
    updateDisplayUnit();
}

void SendCoinsEntry::checkSubtractFeeFromAmount() {
    ui->checkboxSubtractFeeFromAmount->setChecked(true);
}

void SendCoinsEntry::deleteClicked() {
    Q_EMIT removeEntry(this);
}

void SendCoinsEntry::useAvailableBalanceClicked() {
    Q_EMIT useAvailableBalance(this);
}

bool SendCoinsEntry::validate(interfaces::Node &node) {
    if (!model) {
        return false;
    }

    // Check input validity
    bool retval = true;

#ifdef ENABLE_BIP70
    // Skip checks for payment request
    if (recipient.paymentRequest.IsInitialized()) {
        return retval;
    }
#endif

    ui->payTo->fixup();
    if (!ui->payTo->validate()) {
        retval = false;
    }

    if (!ui->payAmount->validate()) {
        retval = false;
    }

    // Sending a zero amount is invalid
    if (ui->payAmount->value(nullptr) <= Amount::zero()) {
        ui->payAmount->setValid(false);
        retval = false;
    }

    // Reject dust outputs:
    if (retval &&
        GUIUtil::isDust(node, ui->payTo->text(), ui->payAmount->value(),
                        model->getChainParams())) {
        ui->payAmount->setValid(false);
        retval = false;
    }

    return retval;
}

SendCoinsRecipient SendCoinsEntry::getValue() {
#ifdef ENABLE_BIP70
    // Payment request
    if (recipient.paymentRequest.IsInitialized()) {
        return recipient;
    }
#endif

    // Normal payment
    recipient.address = ui->payTo->text();
    recipient.label = ui->addAsLabel->text();
    recipient.amount = ui->payAmount->value();
    recipient.message = ui->messageTextLabel->text();
    recipient.fSubtractFeeFromAmount =
        (ui->checkboxSubtractFeeFromAmount->checkState() == Qt::Checked);

    return recipient;
}

QWidget *SendCoinsEntry::setupTafxxain(QWidget *prev) {
    QWidget::setTabOrder(prev, ui->payTo);
    QWidget::setTabOrder(ui->payTo, ui->addAsLabel);
    QWidget *w = ui->payAmount->setupTafxxain(ui->addAsLabel);
    QWidget::setTabOrder(w, ui->checkboxSubtractFeeFromAmount);
    QWidget::setTabOrder(ui->checkboxSubtractFeeFromAmount,
                         ui->addressBookButton);
    QWidget::setTabOrder(ui->addressBookButton, ui->pasteButton);
    QWidget::setTabOrder(ui->pasteButton, ui->deleteButton);
    return ui->deleteButton;
}

void SendCoinsEntry::setValue(const SendCoinsRecipient &value) {
    recipient = value;

#ifdef ENABLE_BIP70
    // payment request
    if (recipient.paymentRequest.IsInitialized()) {
        // unauthenticated
        if (recipient.authenticatedMerchant.isEmpty()) {
            ui->payTo_is->setText(recipient.address);
            ui->memoTextLabel_is->setText(recipient.message);
            ui->payAmount_is->setValue(recipient.amount);
            ui->payAmount_is->setReadOnly(true);
            setCurrentWidget(ui->SendCoins_UnauthenticatedPaymentRequest);
        }

        // authenticated
        else {
            ui->payTo_s->setText(recipient.authenticatedMerchant);
            ui->memoTextLabel_s->setText(recipient.message);
            ui->payAmount_s->setValue(recipient.amount);
            ui->payAmount_s->setReadOnly(true);
            setCurrentWidget(ui->SendCoins_AuthenticatedPaymentRequest);
        }
    }

    // normal payment
    else
#endif
    {
        // message
        ui->messageTextLabel->setText(recipient.message);
        ui->messageTextLabel->setVisible(!recipient.message.isEmpty());
        ui->messageLabel->setVisible(!recipient.message.isEmpty());

        ui->addAsLabel->clear();
        // this may set a label from addressbook
        ui->payTo->setText(recipient.address);
        // if a label had been set from the addressbook, don't overwrite with an
        // empty label
        if (!recipient.label.isEmpty()) {
            ui->addAsLabel->setText(recipient.label);
        }
        ui->payAmount->setValue(recipient.amount);
    }
}

void SendCoinsEntry::setAddress(const QString &address) {
    ui->payTo->setText(address);
    ui->payAmount->setFocus();
}

void SendCoinsEntry::setAmount(const Amount amount) {
    ui->payAmount->setValue(amount);
}

bool SendCoinsEntry::isClear() {
    return ui->payTo->text().isEmpty() && ui->payTo_is->text().isEmpty() &&
           ui->payTo_s->text().isEmpty();
}

void SendCoinsEntry::setFocus() {
    ui->payTo->setFocus();
}

void SendCoinsEntry::updateDisplayUnit() {
    if (model && model->getOptionsModel()) {
        // Update payAmount with the current unit
        ui->payAmount->setDisplayUnit(
            model->getOptionsModel()->getDisplayUnit());
        ui->payAmount_is->setDisplayUnit(
            model->getOptionsModel()->getDisplayUnit());
        ui->payAmount_s->setDisplayUnit(
            model->getOptionsModel()->getDisplayUnit());
    }
}

bool SendCoinsEntry::updateLabel(const QString &address) {
    if (!model) {
        return false;
    }

    // Fill in label from address book, if address has an associated label
    QString associatedLabel =
        model->getAddressTableModel()->labelForAddress(address);
    if (!associatedLabel.isEmpty()) {
        ui->addAsLabel->setText(associatedLabel);
        return true;
    }

    return false;
}
