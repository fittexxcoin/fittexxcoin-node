// Copyright (c) 2017 The Fittexxcoin Core developers
// Copyright (c) 2019-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <fs.h>

#include <vector>

//! Get the path of the wallet directory.
fs::path GetWalletDir();

//! Get wallets in wallet directory.
std::vector<fs::path> ListWalletDir();

//! The WalletLocation class provides wallet information.
class WalletLocation final {
    std::string m_name;
    fs::path m_path;

public:
    explicit WalletLocation() {}
    explicit WalletLocation(const std::string &name);

    //! Get wallet name.
    const std::string &GetName() const { return m_name; }

    //! Get wallet absolute path.
    const fs::path &GetPath() const { return m_path; }

    //! Return whether the wallet exists.
    bool Exists() const;
};
