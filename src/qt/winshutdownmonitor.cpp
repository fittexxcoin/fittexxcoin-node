// Copyright (c) 2014-2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2020 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/winshutdownmonitor.h>

#if defined(Q_OS_WIN)
#include <shutdown.h>

#include <QDebug>

#include <windows.h>

// If we don't want a message to be processed by Qt, return true and set result
// to the value that the window procedure should return. Otherwise return false.
bool WinShutdownMonitor::nativeEventFilter(const QByteArray &eventType,
                                           void *pMessage, long *pnResult) {
    Q_UNUSED(eventType);

    MSG *pMsg = static_cast<MSG *>(pMessage);

    switch (pMsg->message) {
        case WM_QUERYENDSESSION: {
            // Initiate a client shutdown after receiving a WM_QUERYENDSESSION
            // and block
            // Windows session end until we have finished client shutdown.
            StartShutdown();
            *pnResult = FALSE;
            return true;
        }

        case WM_ENDSESSION: {
            *pnResult = FALSE;
            return true;
        }
    }

    return false;
}

void WinShutdownMonitor::registerShutdownBlockReason(const QString &strReason,
                                                     const HWND &mainWinId) {
    typedef BOOL(WINAPI * PSHUTDOWNBRCREATE)(HWND, LPCWSTR);
    PSHUTDOWNBRCREATE shutdownBRCreate = (PSHUTDOWNBRCREATE)GetProcAddress(
        GetModuleHandleA("User32.dll"), "ShutdownBlockReasonCreate");
    if (shutdownBRCreate == nullptr) {
        qWarning() << "registerShutdownBlockReason: GetProcAddress for "
                      "ShutdownBlockReasonCreate failed";
        return;
    }

    if (shutdownBRCreate(mainWinId, strReason.toStdWString().c_str()))
        qWarning() << "registerShutdownBlockReason: Successfully registered: " +
                          strReason;
    else
        qWarning() << "registerShutdownBlockReason: Failed to register: " +
                          strReason;
}
#endif
