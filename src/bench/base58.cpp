// Copyright (c) 2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>

#include <base58.h>
#include <validation.h>

#include <array>
#include <string>
#include <vector>

static void Base58Encode(benchmark::State &state) {
    static const std::vector<uint8_t> buffer = {
        17,  79,  8,   99,  150, 189, 208, 162, 22,  23, 203,
        163, 36,  58,  147, 227, 139, 2,   215, 100, 91, 38,
        11,  141, 253, 40,  117, 21,  16,  90,  200, 24};
    BENCHMARK_LOOP {
        EncodeBase58(buffer);
    }
}

static void Base58CheckEncode(benchmark::State &state) {
    static const std::vector<uint8_t> buffer = {
        17,  79,  8,   99,  150, 189, 208, 162, 22,  23, 203,
        163, 36,  58,  147, 227, 139, 2,   215, 100, 91, 38,
        11,  141, 253, 40,  117, 21,  16,  90,  200, 24};
    BENCHMARK_LOOP {
        EncodeBase58Check(buffer);
    }
}

static void Base58Decode(benchmark::State &state) {
    const char *addr = "17VZNX1SN5NtKa8UQFxwQbFeFc3iqRYhem";
    std::vector<uint8_t> vch;
    BENCHMARK_LOOP {
        static_cast<void>(DecodeBase58(addr, vch, 64));
    }
}

BENCHMARK(Base58Encode, 470 * 1000);
BENCHMARK(Base58CheckEncode, 320 * 1000);
BENCHMARK(Base58Decode, 800 * 1000);
