// Copyright (c) 2014-2016 The Fittexxcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <span.h>

#include <cassert>
#include <cstdint>
#include <cstdlib>

/** A hasher class for RIPEMD-160. */
class CRIPEMD160 {
    uint32_t s[5];
    uint8_t buf[64];
    uint64_t bytes;

public:
    static const size_t OUTPUT_SIZE = 20;

    CRIPEMD160();
    CRIPEMD160 &Write(const uint8_t *data, size_t len);
    void Finalize(uint8_t hash[OUTPUT_SIZE]);
    CRIPEMD160 &Reset();

    // Support Span-style API
    CRIPEMD160 &Write(Span<const uint8_t> data) { return Write(data.data(), data.size()); }
    void Finalize(Span<uint8_t> hash) { assert(hash.size() == OUTPUT_SIZE); Finalize(hash.data()); }
};
