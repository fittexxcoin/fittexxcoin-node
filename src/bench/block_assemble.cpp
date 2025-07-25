// Copyright (c) 2011-2017 The Fittexxcoin Core developers
// Copyright (c) 2020-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <config.h>
#include <consensus/validation.h>
#include <script/standard.h>
#include <test/util.h>
#include <txmempool.h>
#include <validation.h>

#include <vector>

static void AssembleBlock(benchmark::State &state) {
    const Config &config = GetConfig();

    const CScript redeemScript = CScript() << OP_DROP << OP_TRUE;
    const CScript SCRIPT_PUB = GetScriptForDestination(ScriptID(redeemScript, false /* p2sh_32 = false */));

    const CScript scriptSig = CScript() << std::vector<uint8_t>(100, 0xff)
                                        << ToByteVector(redeemScript);

    // Collect some loose transactions that spend the coinbases of our mined
    // blocks
    constexpr size_t NUM_BLOCKS{200};
    std::array<CTransactionRef, NUM_BLOCKS - COINBASE_MATURITY + 1> txs;
    for (size_t b = 0; b < NUM_BLOCKS; ++b) {
        CMutableTransaction tx;
        tx.vin.push_back(MineBlock(config, SCRIPT_PUB));
        tx.vin.back().scriptSig = scriptSig;
        tx.vout.emplace_back(1337 * SATOSHI, SCRIPT_PUB);
        if (NUM_BLOCKS - b >= COINBASE_MATURITY) {
            txs.at(b) = MakeTransactionRef(tx);
        }
    }

    {
        // Required for ::AcceptToMemoryPool.
        LOCK(::cs_main);

        for (const auto &txr : txs) {
            CValidationState vstate;
            bool ret{::AcceptToMemoryPool(config, ::g_mempool, vstate, txr,
                                          nullptr /* pfMissingInputs */,
                                          false /* bypass_limits */,
                                          /* nAbsurdFee */ Amount::zero())};
            assert(ret);
        }
    }

    BENCHMARK_LOOP {
        PrepareBlock(config, SCRIPT_PUB);
    }
}

BENCHMARK(AssembleBlock, 700);
