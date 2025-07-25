// Copyright (c) 2017 The Fittexxcoin Core developers
// Copyright (c) 2019-2020 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/walletutil.h>

#include <util/system.h>

fs::path GetWalletDir() {
    fs::path path;

    if (gArgs.IsArgSet("-walletdir")) {
        path = gArgs.GetArg("-walletdir", "");
        if (!fs::is_directory(path)) {
            // If the path specified doesn't exist, we return the deliberately
            // invalid empty string.
            path = "";
        }
    } else {
        path = GetDataDir();
        // If a wallets directory exists, use that, otherwise default to
        // GetDataDir
        if (fs::is_directory(path / "wallets")) {
            path /= "wallets";
        }
    }

    return path;
}

static bool IsBerkeleyBtree(const fs::path &path) {
    // A Berkeley DB Btree file has at least 4K.
    // This check also prevents opening lock files.
    boost::system::error_code ec;
    if (fs::file_size(path, ec) < 4096) {
        return false;
    }

    fs::ifstream file(path.string(), std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    // Magic bytes start at offset 12
    file.seekg(12, std::ios::beg);
    uint32_t data = 0;
    // Read 4 bytes of file to compare against magic
    file.read((char *)&data, sizeof(data));

    // Berkeley DB Btree magic bytes, from:
    //  https://github.com/file/file/blob/5824af38469ec1ca9ac3ffd251e7afe9dc11e227/magic/Magdir/database#L74-L75
    //  - big endian systems - 00 05 31 62
    //  - little endian systems - 62 31 05 00
    return data == 0x00053162 || data == 0x62310500;
}

std::vector<fs::path> ListWalletDir() {
    const fs::path wallet_dir = GetWalletDir();
    const size_t offset = wallet_dir.string().size() + 1;
    std::vector<fs::path> paths;

    for (auto it = fs::recursive_directory_iterator(wallet_dir);
         it != fs::recursive_directory_iterator(); ++it) {
        // Get wallet path relative to walletdir by removing walletdir from the
        // wallet path. This can be replaced by
        // boost::filesystem::lexically_relative once boost is bumped to 1.60.
        const fs::path path = it->path().string().substr(offset);

        if (it->status().type() == fs::directory_file &&
            IsBerkeleyBtree(it->path() / "wallet.dat")) {
            // Found a directory which contains wallet.dat btree file, add it as
            // a wallet.
            paths.emplace_back(path);
        } else if (it.level() == 0 &&
                   it->symlink_status().type() == fs::regular_file &&
                   IsBerkeleyBtree(it->path())) {
            if (it->path().filename() == "wallet.dat") {
                // Found top-level wallet.dat btree file, add top level
                // directory "" as a wallet.
                paths.emplace_back();
            } else {
                // Found top-level btree file not called wallet.dat. Current
                // fittexxcoin software will never create these files but will allow
                // them to be opened in a shared database environment for
                // backwards compatibility. Add it to the list of available
                // wallets.
                paths.emplace_back(path);
            }
        }
    }

    return paths;
}

WalletLocation::WalletLocation(const std::string &name)
    : m_name(name), m_path(fs::absolute(name, GetWalletDir())) {}

bool WalletLocation::Exists() const {
    return fs::symlink_status(m_path).type() != fs::file_not_found;
}
