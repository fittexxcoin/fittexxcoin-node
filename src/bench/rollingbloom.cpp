// Copyright (c) 2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <iostream>

#include <bench/bench.h>
#include <bloom.h>

static void RollingBloom(benchmark::State &state) {
    CRollingBloomFilter filter(120000, 0.000001);
    std::vector<uint8_t> data(32);
    uint32_t count = 0;
    BENCHMARK_LOOP {
        count++;
        data[0] = count;
        data[1] = count >> 8;
        data[2] = count >> 16;
        data[3] = count >> 24;
        filter.insert(data);

        data[0] = count >> 24;
        data[1] = count >> 16;
        data[2] = count >> 8;
        data[3] = count;
        filter.contains(data);
    }
}

static void RollingBloomReset(benchmark::State &state) {
    CRollingBloomFilter filter(120000, 0.000001);
    BENCHMARK_LOOP {
        filter.reset();
    }
}

BENCHMARK(RollingBloom, 1500 * 1000);
BENCHMARK(RollingBloomReset, 20000);
