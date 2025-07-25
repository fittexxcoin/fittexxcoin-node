// Copyright (c) 2009-2018 The Fittexxcoin Core developers
// Copyright (c) 2020-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <primitives/transaction.h>
#include <psbt.h>
#include <wallet/wallet.h>

bool FillPSBT(const CWallet *pwallet, PartiallySignedTransaction &psbtx,
              uint32_t scriptFlags,
              SigHashType sighash_type = SigHashType(), bool sign = true,
              bool bip32derivs = false);
