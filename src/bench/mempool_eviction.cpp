// Copyright (c) 2011-2016 The Fittexxcoin Core developers
// Copyright (c) 2021-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <policy/policy.h>
#include <txmempool.h>

#include <list>
#include <vector>

static void AddTx(const CTransactionRef &tx, const Amount &nFee, CTxMemPool &pool)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main, pool.cs) {
    int64_t nTime = 0;
    bool spendsCoinbase = false;
    unsigned int nSigChecks = 1;
    LockPoints lp;
    pool.addUnchecked(CTxMemPoolEntry(tx, nFee, nTime, spendsCoinbase, nSigChecks, lp));
}

// Right now this is only testing eviction performance in an extremely small
// mempool. Code needs to be written to generate a much wider variety of
// unique transactions for a more meaningful performance measurement.
static void MempoolEviction(benchmark::State &state) {
    Amount const value = 10 * COIN;
    CMutableTransaction tx1 = CMutableTransaction();
    tx1.vin.resize(1);
    tx1.vin[0].scriptSig = CScript() << OP_1;
    tx1.vout.resize(1);
    tx1.vout[0].scriptPubKey = CScript() << OP_1 << OP_EQUAL;
    tx1.vout[0].nValue = value;

    CMutableTransaction tx2 = CMutableTransaction();
    tx2.vin.resize(1);
    tx2.vin[0].scriptSig = CScript() << OP_2;
    tx2.vout.resize(1);
    tx2.vout[0].scriptPubKey = CScript() << OP_2 << OP_EQUAL;
    tx2.vout[0].nValue = value;

    CMutableTransaction tx3 = CMutableTransaction();
    tx3.vin.resize(1);
    tx3.vin[0].prevout = COutPoint(tx2.GetId(), 0);
    tx3.vin[0].scriptSig = CScript() << OP_2;
    tx3.vout.resize(1);
    tx3.vout[0].scriptPubKey = CScript() << OP_3 << OP_EQUAL;
    tx3.vout[0].nValue = value;

    CMutableTransaction tx4 = CMutableTransaction();
    tx4.vin.resize(2);
    tx4.vin[0].prevout = COutPoint();
    tx4.vin[0].scriptSig = CScript() << OP_4;
    tx4.vin[1].prevout = COutPoint();
    tx4.vin[1].scriptSig = CScript() << OP_4;
    tx4.vout.resize(2);
    tx4.vout[0].scriptPubKey = CScript() << OP_4 << OP_EQUAL;
    tx4.vout[0].nValue = value;
    tx4.vout[1].scriptPubKey = CScript() << OP_4 << OP_EQUAL;
    tx4.vout[1].nValue = value;

    CMutableTransaction tx5 = CMutableTransaction();
    tx5.vin.resize(2);
    tx5.vin[0].prevout = COutPoint(tx4.GetId(), 0);
    tx5.vin[0].scriptSig = CScript() << OP_4;
    tx5.vin[1].prevout = COutPoint();
    tx5.vin[1].scriptSig = CScript() << OP_5;
    tx5.vout.resize(2);
    tx5.vout[0].scriptPubKey = CScript() << OP_5 << OP_EQUAL;
    tx5.vout[0].nValue = value;
    tx5.vout[1].scriptPubKey = CScript() << OP_5 << OP_EQUAL;
    tx5.vout[1].nValue = value;

    CMutableTransaction tx6 = CMutableTransaction();
    tx6.vin.resize(2);
    tx6.vin[0].prevout = COutPoint(tx4.GetId(), 1);
    tx6.vin[0].scriptSig = CScript() << OP_4;
    tx6.vin[1].prevout = COutPoint();
    tx6.vin[1].scriptSig = CScript() << OP_6;
    tx6.vout.resize(2);
    tx6.vout[0].scriptPubKey = CScript() << OP_6 << OP_EQUAL;
    tx6.vout[0].nValue = value;
    tx6.vout[1].scriptPubKey = CScript() << OP_6 << OP_EQUAL;
    tx6.vout[1].nValue = value;

    CMutableTransaction tx7 = CMutableTransaction();
    tx7.vin.resize(2);
    tx7.vin[0].prevout = COutPoint(tx5.GetId(), 0);
    tx7.vin[0].scriptSig = CScript() << OP_5;
    tx7.vin[1].prevout = COutPoint(tx6.GetId(), 0);
    tx7.vin[1].scriptSig = CScript() << OP_6;
    tx7.vout.resize(2);
    tx7.vout[0].scriptPubKey = CScript() << OP_7 << OP_EQUAL;
    tx7.vout[0].nValue = value;
    tx7.vout[1].scriptPubKey = CScript() << OP_7 << OP_EQUAL;
    tx7.vout[1].nValue = value;

    CTxMemPool pool;
    LOCK2(cs_main, pool.cs);
    // Create transaction references outside the "hot loop"
    const CTransactionRef tx1_r{MakeTransactionRef(tx1)};
    const CTransactionRef tx2_r{MakeTransactionRef(tx2)};
    const CTransactionRef tx3_r{MakeTransactionRef(tx3)};
    const CTransactionRef tx4_r{MakeTransactionRef(tx4)};
    const CTransactionRef tx5_r{MakeTransactionRef(tx5)};
    const CTransactionRef tx6_r{MakeTransactionRef(tx6)};
    const CTransactionRef tx7_r{MakeTransactionRef(tx7)};

    BENCHMARK_LOOP {
        AddTx(tx1_r, 10000 * SATOSHI, pool);
        AddTx(tx2_r,  5000 * SATOSHI, pool);
        AddTx(tx3_r, 20000 * SATOSHI, pool);
        AddTx(tx4_r,  7000 * SATOSHI, pool);
        AddTx(tx5_r,  1000 * SATOSHI, pool);
        AddTx(tx6_r,  1100 * SATOSHI, pool);
        AddTx(tx7_r,  9000 * SATOSHI, pool);
        pool.TrimToSize(pool.DynamicMemoryUsage() * 3 / 4);
        pool.TrimToSize(GetSerializeSize(*tx1_r, PROTOCOL_VERSION));
    }
}

BENCHMARK(MempoolEviction, 41000);
