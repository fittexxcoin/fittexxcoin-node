// Copyright (c) 2015-2016 The Fittexxcoin Core developers
// Copyright (c) 2019-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <util/time.h>
#include <validation.h>

// Sanity test: this should loop ten times, and
// min/max/average should be close to 100ms.
static void Sleep100ms(benchmark::State &state) {
    BENCHMARK_LOOP {
        MilliSleep(100);
    }
}

BENCHMARK(Sleep100ms, 10);

// Extremely fast-running benchmark:
#include <cmath>

volatile double sum = 0.0; // volatile, global so not optimized away

static void Trig(benchmark::State &state) {
    double d = 0.01;
    BENCHMARK_LOOP {
        sum += sin(d);
        d += 0.000001;
    }
}

BENCHMARK(Trig, 12 * 1000 * 1000);
