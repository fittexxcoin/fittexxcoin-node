// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>

#if defined(BUILD_FITTEXXCOIN_INTERNAL) && defined(HAVE_CONFIG_H)
#include <config/fittexxcoin-config.h>
#if defined(_WIN32)
#if defined(DLL_EXPORT)
#if defined(HAVE_FUNC_ATTRIBUTE_DLLEXPORT)
#define EXPORT_SYMBOL __declspec(dllexport)
#else
#define EXPORT_SYMBOL
#endif
#endif
#elif defined(HAVE_FUNC_ATTRIBUTE_VISIBILITY)
#define EXPORT_SYMBOL __attribute__((visibility("default")))
#endif
#elif defined(MSC_VER) && !defined(STATIC_LIBFITTEXXCOINCONSENSUS)
#define EXPORT_SYMBOL __declspec(dllimport)
#endif

#ifndef EXPORT_SYMBOL
#define EXPORT_SYMBOL
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define FITTEXXCOINCONSENSUS_API_VER 1

typedef enum fittexxcoinconsensus_error_t {
    fittexxcoinconsensus_ERR_OK = 0,
    fittexxcoinconsensus_ERR_TX_INDEX,
    fittexxcoinconsensus_ERR_TX_SIZE_MISMATCH,
    fittexxcoinconsensus_ERR_TX_DESERIALIZE,
    fittexxcoinconsensus_ERR_AMOUNT_REQUIRED,
    fittexxcoinconsensus_ERR_INVALID_FLAGS,
} fittexxcoinconsensus_error;

/** Script verification flags */
enum {
    fittexxcoinconsensus_SCRIPT_FLAGS_VERIFY_NONE = 0,
    // evaluate P2SH (BIP16) subscripts
    fittexxcoinconsensus_SCRIPT_FLAGS_VERIFY_P2SH = (1U << 0),
    // enforce strict DER (BIP66) compliance
    fittexxcoinconsensus_SCRIPT_FLAGS_VERIFY_DERSIG = (1U << 2),
    // enable CHECKLOCKTIMEVERIFY (BIP65)
    fittexxcoinconsensus_SCRIPT_FLAGS_VERIFY_CHECKLOCKTIMEVERIFY = (1U << 9),
    // enable CHECKSEQUENCEVERIFY (BIP112)
    fittexxcoinconsensus_SCRIPT_FLAGS_VERIFY_CHECKSEQUENCEVERIFY = (1U << 10),
    // enable WITNESS (BIP141)
    fittexxcoinconsensus_SCRIPT_FLAGS_VERIFY_WITNESS_DEPRECATED = (1U << 11),
    // enable SIGHASH_FORKID replay protection
    fittexxcoinconsensus_SCRIPT_ENABLE_SIGHASH_FORKID = (1U << 16),
    fittexxcoinconsensus_SCRIPT_FLAGS_VERIFY_ALL =
        fittexxcoinconsensus_SCRIPT_FLAGS_VERIFY_P2SH |
        fittexxcoinconsensus_SCRIPT_FLAGS_VERIFY_DERSIG |
        fittexxcoinconsensus_SCRIPT_FLAGS_VERIFY_CHECKLOCKTIMEVERIFY |
        fittexxcoinconsensus_SCRIPT_FLAGS_VERIFY_CHECKSEQUENCEVERIFY,
};

/// Returns 1 if the input nIn of the serialized transaction pointed to by txTo
/// correctly spends the scriptPubKey pointed to by scriptPubKey under the
/// additional constraints specified by flags.
/// If not nullptr, err will contain an error/success code for the operation
EXPORT_SYMBOL int fittexxcoinconsensus_verify_script(
    const uint8_t *scriptPubKey, unsigned int scriptPubKeyLen,
    const uint8_t *txTo, unsigned int txToLen, unsigned int nIn,
    unsigned int flags, fittexxcoinconsensus_error *err);

EXPORT_SYMBOL int fittexxcoinconsensus_verify_script_with_amount(
    const uint8_t *scriptPubKey, unsigned int scriptPubKeyLen, int64_t amount,
    const uint8_t *txTo, unsigned int txToLen, unsigned int nIn,
    unsigned int flags, fittexxcoinconsensus_error *err);

EXPORT_SYMBOL unsigned int fittexxcoinconsensus_version();

#ifdef __cplusplus
} // extern "C"
#endif

#undef EXPORT_SYMBOL
