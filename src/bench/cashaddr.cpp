// Copyright (c) 2018-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <cashaddr.h>
#include <cashaddrenc.h>

#include <string>
#include <vector>

static void CashAddrEncode(benchmark::State &state) {
    CashAddrContent content;
    content.type = CashAddrType::PUBKEY_TYPE;
    content.hash = {17,  79, 8,   99,  150, 189, 208, 162,
                    22,  23, 203, 163, 36,  58,  147, 227,
                    139, 2,  215, 100, 91,  38,  11,  141,
                    253, 40, 117, 21,  16,  90,  200, 24};
    const std::vector<uint8_t> buffer = PackCashAddrContent(content);

    BENCHMARK_LOOP {
        cashaddr::Encode("fittexxcoin", buffer);
    }
}

static void CashAddrDecode(benchmark::State &state) {
    const char *addrWithPrefix =
        "fittexxcoin:qprnwmr02d7ky9m693qufj5mgkpf4wvssv0w86tkjd";
    const char *addrNoPrefix = "qprnwmr02d7ky9m693qufj5mgkpf4wvssv0w86tkjd";

    BENCHMARK_LOOP {
        cashaddr::Decode(addrWithPrefix, "fittexxcoin");
        cashaddr::Decode(addrNoPrefix, "fittexxcoin");
    }
}

BENCHMARK(CashAddrEncode, 800 * 1000);
BENCHMARK(CashAddrDecode, 800 * 1000);
