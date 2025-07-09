// Copyright (c) 2009-2015 The Fittexxcoin Core developers
// Copyright (c) 2017-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <QObject>
#include <QTest>

class URITests : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void uriTestsCashAddr();
    void uriTestFormatURI();
};
