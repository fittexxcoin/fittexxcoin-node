// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Fittexxcoin Core developers
// Copyright (c) 2019 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <shutdown.h>

#include <atomic>

static std::atomic<bool> fRequestShutdown(false);

void StartShutdown() {
    fRequestShutdown = true;
}
void AbortShutdown() {
    fRequestShutdown = false;
}
bool ShutdownRequested() {
    return fRequestShutdown;
}
