// Copyright (c) 2017-2018 The Fittexxcoin Core developers
// Copyright (c) 2020-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <primitives/transaction.h>

class Config;
struct TxId;

/** Broadcast a transaction */
TxId BroadcastTransaction(const Config &config, CTransactionRef tx,
                          bool allowhighfees = false);
