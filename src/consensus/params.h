// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <primitives/blockhash.h>
#include <uint256.h>

#include <limits>
#include <optional>

namespace Consensus {

/**
 * Parameters that influence chain consensus.
 */
struct Params {
    BlockHash hashGenesisBlock;
    int nSubsidyHalvingInterval;
    /** Block height at which BIP16 becomes active */
    int BIP16Height;
    /** Block height and hash at which BIP34 becomes active */
    int BIP34Height;
    BlockHash BIP34Hash;
    /** Block height at which BIP65 becomes active */
    int BIP65Height;
    /** Block height at which BIP66 becomes active */
    int BIP66Height;
    /** Block height at which CSV (BIP68, BIP112 and BIP113) becomes active */
    int CSVHeight;
    /** Block height at which UAHF kicks in */
    int uahfHeight;
    /** Block height at which the new DAA becomes active */
    int daaHeight;
    /** Block height at which the magnetic anomaly activation becomes active */
    int magneticAnomalyHeight;
    /** Block height at which the graviton activation becomes active */
    int gravitonHeight;
    /** Block height at which the phonon activation becomes active */
    int phononHeight;
    /** Unix time used for MTP activation of 15 Nov 2020 12:00:00 UTC upgrade */
    int axionActivationTime;

    /** Note: Unix time used for MTP activation of the 15 May 2021 12:00:00 UTC upgrade was 1621080000, but since
     *  it was a relay-rules-only upgrade, so we no longer track this time for blockchain consensus. */
    /** Block height at which the May 15, 2022 rules became active (this is one less than the upgrade block itself)  */
    int upgrade8Height;
    /** Unix time used for MTP activation of 15 May 2023 12:00:00 UTC upgrade */
    int64_t upgrade9ActivationTime;
    /** Unix time used for tentative MTP activation of 15 May 2024 12:00:00 UTC upgrade */
    int64_t upgrade10ActivationTime;

    /** Default blocksize limit -- can be overridden with the -excessiveblocksize= command-line switch */
    uint64_t nDefaultExcessiveBlockSize;
    /**
     * Chain-specific default for -blockmaxsize, which controls the maximum size of blocks that the
     * mining code will create.
     */
    uint64_t nDefaultGeneratedBlockSize;


    /** Proof of work parameters */
    uint256 powLimit;
    bool fPowAllowMinDifficultyBlocks;
    bool fPowNoRetargeting;
    int64_t nPowTargetSpacing;
    int64_t nASERTHalfLife;
    int64_t nPowTargetTimespan;
    int64_t DifficultyAdjustmentInterval() const {
        return nPowTargetTimespan / nPowTargetSpacing;
    }
    uint256 nMinimumChainWork;
    BlockHash defaultAssumeValid;

    /** Used by the ASERT DAA activated after Nov. 15, 2020 */
    struct ASERTAnchor {
        int nHeight;
        uint32_t nBits;
        int64_t nPrevBlockTime;
    };

    /** For chains with a checkpoint after the ASERT anchor block, this is always defined */
    std::optional<ASERTAnchor> asertAnchorParams;

};
} // namespace Consensus
