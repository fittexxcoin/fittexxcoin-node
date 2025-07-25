// Copyright (c) 2018 The Fittexxcoin Core developers
// Copyright (c) 2020-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <interfaces/chain.h>

#include <test/setup_common.h>

struct InitWalletDirTestingSetup : public BasicTestingSetup {
    explicit InitWalletDirTestingSetup(
        const std::string &chainName = CBaseChainParams::MAIN);
    ~InitWalletDirTestingSetup();
    void SetWalletDir(const fs::path &walletdir_path);

    fs::path m_datadir;
    fs::path m_cwd;
    std::map<std::string, fs::path> m_walletdir_path_cases;
    std::unique_ptr<interfaces::Chain> m_chain = interfaces::MakeChain();
    std::unique_ptr<interfaces::ChainClient> m_chain_client;
};
