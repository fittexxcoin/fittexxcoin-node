// Copyright (c) 2014 The Fittexxcoin Core developers
// Copyright (c) 2020-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <QIcon>
#include <QPixmap>
#include <QString>

/* Coin network-specific GUI style information */
class NetworkStyle {
public:
    /** Get style associated with provided BIP70 network id, or 0 if not known
     */
    static const NetworkStyle *instantiate(const QString &networkId);

    const QString &getAppName() const { return appName; }
    const QIcon &getSplashIcon() const { return splashIcon; }
    const QIcon &getTrayAndWindowIcon() const { return trayAndWindowIcon; }
    const QString &getTitleAddText() const { return titleAddText; }

private:
    NetworkStyle(const QString &appName, int iconColorHue, const char *titleAddText);

    QString appName;
    QIcon splashIcon;
    QIcon trayAndWindowIcon;
    QString titleAddText;
};
