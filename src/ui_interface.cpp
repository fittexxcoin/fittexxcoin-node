// Copyright (c) 2010-2016 The Fittexxcoin Core developers
// Copyright (c) 2022 The Fittexxcoin Node developers
// Copyright (c) 2017-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <ui_interface.h>

#include <util/system.h>

#include <boost/signals2/last_value.hpp>
#include <boost/signals2/signal.hpp>

CClientUIInterface uiInterface;

struct UISignals {
    boost::signals2::signal<CClientUIInterface::ThreadSafeMessageBoxSig,
                            boost::signals2::last_value<bool>>
        ThreadSafeMessageBox;
    boost::signals2::signal<CClientUIInterface::ThreadSafeQuestionSig,
                            boost::signals2::last_value<bool>>
        ThreadSafeQuestion;
    boost::signals2::signal<CClientUIInterface::InitMessageSig> InitMessage;
    boost::signals2::signal<CClientUIInterface::NotifyNumConnectionsChangedSig>
        NotifyNumConnectionsChanged;
    boost::signals2::signal<CClientUIInterface::NotifyNetworkActiveChangedSig>
        NotifyNetworkActiveChanged;
    boost::signals2::signal<CClientUIInterface::NotifyAlertChangedSig>
        NotifyAlertChanged;
    boost::signals2::signal<CClientUIInterface::LoadWalletSig> LoadWallet;
    boost::signals2::signal<CClientUIInterface::ShowProgressSig> ShowProgress;
    boost::signals2::signal<CClientUIInterface::NotifyBlockTipSig>
        NotifyBlockTip;
    boost::signals2::signal<CClientUIInterface::NotifyHeaderTipSig>
        NotifyHeaderTip;
    boost::signals2::signal<CClientUIInterface::BannedListChangedSig>
        BannedListChanged;
    boost::signals2::signal<CClientUIInterface::NotifyTransactionDoubleSpentSig>
        NotifyTransactionDoubleSpent;
} g_ui_signals;

#define ADD_SIGNALS_IMPL_WRAPPER(signal_name)                                  \
    boost::signals2::connection CClientUIInterface::signal_name##_connect(     \
        std::function<signal_name##Sig> fn) {                                  \
        return g_ui_signals.signal_name.connect(fn);                           \
    }                                                                          \
    void CClientUIInterface::signal_name##_disconnect(                         \
        std::function<signal_name##Sig> fn) {                                  \
        return g_ui_signals.signal_name.disconnect(&fn);                       \
    }

ADD_SIGNALS_IMPL_WRAPPER(ThreadSafeMessageBox);
ADD_SIGNALS_IMPL_WRAPPER(ThreadSafeQuestion);
ADD_SIGNALS_IMPL_WRAPPER(InitMessage);
ADD_SIGNALS_IMPL_WRAPPER(NotifyNumConnectionsChanged);
ADD_SIGNALS_IMPL_WRAPPER(NotifyNetworkActiveChanged);
ADD_SIGNALS_IMPL_WRAPPER(NotifyAlertChanged);
ADD_SIGNALS_IMPL_WRAPPER(LoadWallet);
ADD_SIGNALS_IMPL_WRAPPER(ShowProgress);
ADD_SIGNALS_IMPL_WRAPPER(NotifyBlockTip);
ADD_SIGNALS_IMPL_WRAPPER(NotifyHeaderTip);
ADD_SIGNALS_IMPL_WRAPPER(BannedListChanged);
ADD_SIGNALS_IMPL_WRAPPER(NotifyTransactionDoubleSpent);

bool CClientUIInterface::ThreadSafeMessageBox(const std::string &message,
                                              const std::string &caption,
                                              unsigned int style) {
    return g_ui_signals.ThreadSafeMessageBox(message, caption, style);
}
bool CClientUIInterface::ThreadSafeQuestion(
    const std::string &message, const std::string &non_interactive_message,
    const std::string &caption, unsigned int style) {
    return g_ui_signals.ThreadSafeQuestion(message, non_interactive_message,
                                           caption, style);
}
void CClientUIInterface::InitMessage(const std::string &message) {
    return g_ui_signals.InitMessage(message);
}
void CClientUIInterface::NotifyNumConnectionsChanged(int newNumConnections) {
    return g_ui_signals.NotifyNumConnectionsChanged(newNumConnections);
}
void CClientUIInterface::NotifyNetworkActiveChanged(bool networkActive) {
    return g_ui_signals.NotifyNetworkActiveChanged(networkActive);
}
void CClientUIInterface::NotifyAlertChanged() {
    return g_ui_signals.NotifyAlertChanged();
}
void CClientUIInterface::LoadWallet(std::shared_ptr<CWallet> wallet) {
    return g_ui_signals.LoadWallet(wallet);
}
void CClientUIInterface::ShowProgress(const std::string &title, int nProgress,
                                      bool resume_possible) {
    return g_ui_signals.ShowProgress(title, nProgress, resume_possible);
}
void CClientUIInterface::NotifyBlockTip(bool b, const CBlockIndex *i) {
    return g_ui_signals.NotifyBlockTip(b, i);
}
void CClientUIInterface::NotifyHeaderTip(bool b, const CBlockIndex *i) {
    return g_ui_signals.NotifyHeaderTip(b, i);
}
void CClientUIInterface::BannedListChanged() {
    return g_ui_signals.BannedListChanged();
}
void CClientUIInterface::NotifyTransactionDoubleSpent(const TxId txId,
                                                      const DspId dspId) {
    return g_ui_signals.NotifyTransactionDoubleSpent(txId, dspId);
}

bool InitError(const std::string &str) {
    uiInterface.ThreadSafeMessageBox(str, "", CClientUIInterface::MSG_ERROR);
    return false;
}

void InitWarning(const std::string &str) {
    uiInterface.ThreadSafeMessageBox(str, "", CClientUIInterface::MSG_WARNING);
}

std::string AmountHighWarn(const std::string &optname) {
    return strprintf(_("%s is set very high!"), optname);
}

std::string AmountErrMsg(const char *const optname,
                         const std::string &strValue) {
    return strprintf(_("Invalid amount for -%s=<amount>: '%s'"), optname,
                     strValue);
}
