// Copyright (c) 2018-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

// TODO: Split chain.h apart and only include CBlockIndex
#include <chain.h>

struct CBlockIndexWorkComparator {
    bool operator()(const CBlockIndex *pa, const CBlockIndex *pb) const {
        // First sort by most total work, ...
        if (pa->nChainWork > pb->nChainWork) {
            return false;
        }
        if (pa->nChainWork < pb->nChainWork) {
            return true;
        }

        // ... then by earliest time received, ...
        if (pa->nSequenceId < pb->nSequenceId) {
            return false;
        }
        if (pa->nSequenceId > pb->nSequenceId) {
            return true;
        }

        // Use pointer address as tie breaker (should only happen with blocks
        // loaded from disk, as those all have id 0).
        if (pa < pb) {
            return false;
        }
        if (pa > pb) {
            return true;
        }

        // Identical blocks.
        return false;
    }
};
