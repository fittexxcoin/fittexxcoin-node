// Copyright (c) 2011-2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <QDialog>
#include <QValidator>

class OptionsModel;
class QValidatedLineEdit;

QT_BEGIN_NAMESPACE
class QDataWidgetMapper;
QT_END_NAMESPACE

namespace Ui {
class OptionsDialog;
}

/** Proxy address widget validator, checks for a valid proxy address. */
class ProxyAddressValidator : public QValidator {
    Q_OBJECT

public:
    explicit ProxyAddressValidator(QObject *parent);

    State validate(QString &input, int &pos) const override;
};

/** Preferences dialog. */
class OptionsDialog : public QDialog {
    Q_OBJECT

public:
    explicit OptionsDialog(QWidget *parent, bool enableWallet);
    ~OptionsDialog();

    enum Tab {
        TAB_MAIN,
        TAB_NETWORK,
    };

    void setModel(OptionsModel *model);
    void setMapper();
    void setCurrentTab(OptionsDialog::Tab tab);

private Q_SLOTS:
    /* set OK button state (enabled / disabled) */
    void setOkButtonState(bool fState);
    void on_resetButton_clicked();
    void on_openFittexxcoinConfButton_clicked();
    void on_okButton_clicked();
    void on_cancelButton_clicked();

    void on_hideTrayIcon_stateChanged(int fState);

    void togglePruneWarning(bool enabled);
    void showRestartWarning(bool fPersistent = false);
    void thirdPartyTxWarning(bool fPersistent = false);
    void clearStatusLabel();
    void updateProxyValidationState();
    /* query the networks, for which the default proxy is used */
    void updateDefaultProxyNets();

Q_SIGNALS:
    void proxyIpChecks(QValidatedLineEdit *pUiProxyIp, int nProxyPort);

private:
    Ui::OptionsDialog *ui;
    OptionsModel *model;
    QDataWidgetMapper *mapper;
};
