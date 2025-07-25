// Copyright (c) 2019 The Fittexxcoin Core developers
// Copyright (c) 2020-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <memory>

class CBlock;
class Config;
class CScript;
class CTxIn;
class CWallet;

// Lower-level utils //

/** Returns the generated coin */
CTxIn MineBlock(const Config &config, const CScript &coinbase_scriptPubKey);
/** Prepare a block to be mined */
std::shared_ptr<CBlock> PrepareBlock(const Config &config,
                                     const CScript &coinbase_scriptPubKey);
