// Copyright (c) 2018 The Fittexxcoin Core developers
// Copyright (c) 2019-2020 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/fittexxcoin.h>

#include <QCoreApplication>

#include <functional>
#include <string>

/** Translate string to current locale using Qt. */
extern const std::function<std::string(const char *)> G_TRANSLATION_FUN =
    [](const char *psz) {
        return QCoreApplication::translate("fittexxcoin-node", psz).toStdString();
    };

int main(int argc, char *argv[]) {
    return GuiMain(argc, argv);
}
