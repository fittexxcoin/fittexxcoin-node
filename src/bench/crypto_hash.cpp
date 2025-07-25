// Copyright (c) 2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <bloom.h>
#include <crypto/ripemd160.h>
#include <crypto/sha1.h>
#include <crypto/sha256.h>
#include <crypto/sha3.h>
#include <crypto/sha512.h>
#include <crypto/siphash.h>
#include <hash.h>
#include <random.h>
#include <uint256.h>
#include <util/time.h>

#include <iostream>
#include <string>

/* Number of bytes to hash per iteration */
static const uint64_t BUFFER_SIZE = 1000 * 1000;

static void RIPEMD160(benchmark::State &state) {
    uint8_t hash[CRIPEMD160::OUTPUT_SIZE];
    std::vector<uint8_t> in(BUFFER_SIZE, 0);
    BENCHMARK_LOOP {
        CRIPEMD160().Write(in.data(), in.size()).Finalize(hash);
    }
}

static void SHA1(benchmark::State &state) {
    uint8_t hash[CSHA1::OUTPUT_SIZE];
    std::vector<uint8_t> in(BUFFER_SIZE, 0);
    BENCHMARK_LOOP {
        CSHA1().Write(in.data(), in.size()).Finalize(hash);
    }
}

static void SHA256(benchmark::State &state) {
    uint8_t hash[CSHA256::OUTPUT_SIZE];
    std::vector<uint8_t> in(BUFFER_SIZE, 0);
    BENCHMARK_LOOP {
        CSHA256().Write(in.data(), in.size()).Finalize(hash);
    }
}

static void SHA3_256_1M(benchmark::State &state) {
    uint8_t hash[SHA3_256::OUTPUT_SIZE];
    std::vector<uint8_t> in(BUFFER_SIZE, 0);
    BENCHMARK_LOOP {
        SHA3_256().Write(in).Finalize(hash);
    }
}

static void SHA256_32b(benchmark::State &state) {
    std::vector<uint8_t> in(32, 0);
    BENCHMARK_LOOP {
        CSHA256().Write(in.data(), in.size()).Finalize(in.data());
    }
}

static void SHA256D64_1024(benchmark::State &state) {
    std::vector<uint8_t> in(64 * 1024, 0);
    BENCHMARK_LOOP {
        SHA256D64(in.data(), in.data(), 1024);
    }
}

static void SHA512(benchmark::State &state) {
    uint8_t hash[CSHA512::OUTPUT_SIZE];
    std::vector<uint8_t> in(BUFFER_SIZE, 0);
    BENCHMARK_LOOP {
        CSHA512().Write(in.data(), in.size()).Finalize(hash);
    }
}

static void SipHash_32b(benchmark::State &state) {
    uint256 x;
    uint64_t k1 = 0;
    BENCHMARK_LOOP {
        uint64_t hash64 = SipHashUint256(0, ++k1, x);
        std::memcpy(x.begin(), &hash64, sizeof(hash64));
    }
}

static void FastRandom_32bit(benchmark::State &state) {
    FastRandomContext rng(true);
    BENCHMARK_LOOP {
        rng.rand32();
    }
}

static void FastRandom_1bit(benchmark::State &state) {
    FastRandomContext rng(true);
    BENCHMARK_LOOP {
        rng.randbool();
    }
}

BENCHMARK(RIPEMD160, 440);
BENCHMARK(SHA1, 570);
BENCHMARK(SHA256, 340);
BENCHMARK(SHA512, 330);
BENCHMARK(SHA3_256_1M, 250);

BENCHMARK(SHA256_32b, 4700 * 1000);
BENCHMARK(SipHash_32b, 40 * 1000 * 1000);
BENCHMARK(SHA256D64_1024, 7400);
BENCHMARK(FastRandom_32bit, 110 * 1000 * 1000);
BENCHMARK(FastRandom_1bit, 440 * 1000 * 1000);
