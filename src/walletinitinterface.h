// Copyright (c) 2017 The Fittexxcoin Core developers
// Copyright (c) 2018-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

struct NodeContext;

class WalletInitInterface {
public:
    /** Is the wallet component enabled */
    virtual bool HasWalletSupport() const = 0;
    /** Get wallet help string */
    virtual void AddWalletOptions() const = 0;
    /** Check wallet parameter interaction */
    virtual bool ParameterInteraction() const = 0;
    /** Add wallets that should be opened to list of chain clients. */
    virtual void Construct(NodeContext &node) const = 0;

    virtual ~WalletInitInterface() {}
};

extern const WalletInitInterface &g_wallet_init_interface;
