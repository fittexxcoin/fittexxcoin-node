// Copyright (c) 2017-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
// Copyright (c) 2015 The Fittexxcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <chainparams.h>
#include <config.h>
#include <consensus/activation.h>
#include <pow.h>
#include <random.h>
#include <tinyformat.h>
#include <util/system.h>

#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <cmath>
#include <memory>
#include <string>

BOOST_FIXTURE_TEST_SUITE(pow_tests, TestingSetup)

/* Test calculation of next difficulty target with no constraints applying */
BOOST_AUTO_TEST_CASE(get_next_work) {
    DummyConfig config(CBaseChainParams::MAIN);

    int64_t nLastRetargetTime = 1261130161; // Block #30240
    CBlockIndex pindexLast;
    pindexLast.nHeight = 32255;
    pindexLast.nTime = 1262152739; // Block #32255
    pindexLast.nBits = 0x1d00ffff;
    BOOST_CHECK_EQUAL(
        CalculateNextWorkRequired(&pindexLast, nLastRetargetTime,
                                  config.GetChainParams().GetConsensus()),
        0x1d00d86aU);
}

/* Test the constraint on the upper bound for next work */
BOOST_AUTO_TEST_CASE(get_next_work_pow_limit) {
    DummyConfig config(CBaseChainParams::MAIN);

    int64_t nLastRetargetTime = 1231006505; // Block #0
    CBlockIndex pindexLast;
    pindexLast.nHeight = 2015;
    pindexLast.nTime = 1233061996; // Block #2015
    pindexLast.nBits = 0x1d00ffff;
    BOOST_CHECK_EQUAL(
        CalculateNextWorkRequired(&pindexLast, nLastRetargetTime,
                                  config.GetChainParams().GetConsensus()),
        0x1d00ffffU);
}

/* Test the constraint on the lower bound for actual time taken */
BOOST_AUTO_TEST_CASE(get_next_work_lower_limit_actual) {
    DummyConfig config(CBaseChainParams::MAIN);

    int64_t nLastRetargetTime = 1279008237; // Block #66528
    CBlockIndex pindexLast;
    pindexLast.nHeight = 68543;
    pindexLast.nTime = 1279297671; // Block #68543
    pindexLast.nBits = 0x1c05a3f4;
    BOOST_CHECK_EQUAL(
        CalculateNextWorkRequired(&pindexLast, nLastRetargetTime,
                                  config.GetChainParams().GetConsensus()),
        0x1c0168fdU);
}

/* Test the constraint on the upper bound for actual time taken */
BOOST_AUTO_TEST_CASE(get_next_work_upper_limit_actual) {
    DummyConfig config(CBaseChainParams::MAIN);

    int64_t nLastRetargetTime = 1263163443; // NOTE: Not an actual block time
    CBlockIndex pindexLast;
    pindexLast.nHeight = 46367;
    pindexLast.nTime = 1269211443; // Block #46367
    pindexLast.nBits = 0x1c387f6f;
    BOOST_CHECK_EQUAL(
        CalculateNextWorkRequired(&pindexLast, nLastRetargetTime,
                                  config.GetChainParams().GetConsensus()),
        0x1d00e1fdU);
}

using CBlockIndexPtr = std::unique_ptr<CBlockIndex>;
const auto MkCBlockIndexPtr = &std::make_unique<CBlockIndex>;

BOOST_AUTO_TEST_CASE(GetBlockProofEquivalentTime_test) {
    DummyConfig config(CBaseChainParams::MAIN);

    std::vector<CBlockIndexPtr> blocks(10000);
    for (int i = 0; i < 10000; i++) {
        blocks[i] = MkCBlockIndexPtr();
        blocks[i]->pprev = i ? blocks[i - 1].get() : nullptr;
        blocks[i]->nHeight = i;
        blocks[i]->nTime =
            1269211443 +
            i * config.GetChainParams().GetConsensus().nPowTargetSpacing;
        blocks[i]->nBits = 0x207fffff; /* target 0x7fffff000... */
        blocks[i]->nChainWork =
            i ? blocks[i - 1]->nChainWork + GetBlockProof(*blocks[i])
              : arith_uint256(0);
    }

    for (int j = 0; j < 1000; j++) {
        CBlockIndexPtr& p1 = blocks[InsecureRandRange(10000)];
        CBlockIndexPtr& p2 = blocks[InsecureRandRange(10000)];
        CBlockIndexPtr& p3 = blocks[InsecureRandRange(10000)];

        int64_t tdiff = GetBlockProofEquivalentTime(
            *p1, *p2, *p3, config.GetChainParams().GetConsensus());
        BOOST_CHECK_EQUAL(tdiff, p1->GetBlockTime() - p2->GetBlockTime());
    }
}

static CBlockIndexPtr GetBlockIndex(CBlockIndex *pindexPrev, int64_t nTimeInterval,
                                    uint32_t nBits) {
    CBlockIndexPtr block = MkCBlockIndexPtr();
    block->pprev = pindexPrev;
    block->nHeight = pindexPrev->nHeight + 1;
    block->nTime = pindexPrev->nTime + nTimeInterval;
    block->nBits = nBits;

    block->BuildSkip();
    block->nChainWork = pindexPrev->nChainWork + GetBlockProof(*block);
    return block;
}

BOOST_AUTO_TEST_CASE(retargeting_test) {
    DummyConfig config(CBaseChainParams::MAIN);

    std::vector<CBlockIndexPtr> blocks(1);

    const Consensus::Params &params = config.GetChainParams().GetConsensus();
    const arith_uint256 powLimit = UintToArith256(params.powLimit);
    arith_uint256 currentPow = powLimit >> 1;
    uint32_t initialBits = currentPow.GetCompact();

    // Genesis block.
    blocks[0] = MkCBlockIndexPtr();
    blocks[0]->nHeight = 0;
    blocks[0]->nTime = 1269211443;
    blocks[0]->nBits = initialBits;

    blocks[0]->nChainWork = GetBlockProof(*blocks[0]);

    // Pile up some blocks.
    for (size_t i = 1; i < 100; i++) {
        blocks.push_back(GetBlockIndex(blocks[i - 1].get(), params.nPowTargetSpacing, initialBits));
    }

    CBlockHeader blkHeaderDummy;

    // We start getting 2h blocks time. For the first 5 blocks, it doesn't
    // matter as the MTP is not affected. For the next 5 block, MTP difference
    // increases but stays below 12h.
    for (size_t i = 100; i < 110; i++) {
        blocks.push_back(GetBlockIndex(blocks[i - 1].get(), 2 * 3600, initialBits));
        BOOST_CHECK_EQUAL(
            GetNextWorkRequired(blocks[i].get(), &blkHeaderDummy, params),
            initialBits);
    }

    // Now we expect the difficulty to decrease. (Block #110)
    blocks.push_back(GetBlockIndex(blocks[109].get(), 2 * 3600, initialBits));
    currentPow.SetCompact(currentPow.GetCompact());
    currentPow += (currentPow >> 2);
    BOOST_CHECK_EQUAL(
        GetNextWorkRequired(blocks[110].get(), &blkHeaderDummy, params),
        currentPow.GetCompact());

    // As we continue with 2h blocks, difficulty continue to decrease. (Block #111)
    blocks.push_back(
        GetBlockIndex(blocks[110].get(), 2 * 3600, currentPow.GetCompact()));
    currentPow.SetCompact(currentPow.GetCompact());
    currentPow += (currentPow >> 2);
    BOOST_CHECK_EQUAL(
        GetNextWorkRequired(blocks[111].get(), &blkHeaderDummy, params),
        currentPow.GetCompact());

    // We decrease again. (Block #112)
    blocks.push_back(
        GetBlockIndex(blocks[111].get(), 2 * 3600, currentPow.GetCompact()));
    currentPow.SetCompact(currentPow.GetCompact());
    currentPow += (currentPow >> 2);
    BOOST_CHECK_EQUAL(
        GetNextWorkRequired(blocks[112].get(), &blkHeaderDummy, params),
        currentPow.GetCompact());

    // We check that we do not go below the minimal difficulty. (Block #113)
    blocks.push_back(
        GetBlockIndex(blocks[112].get(), 2 * 3600, currentPow.GetCompact()));
    currentPow.SetCompact(currentPow.GetCompact());
    currentPow += (currentPow >> 2);
    BOOST_CHECK(powLimit.GetCompact() != currentPow.GetCompact());
    BOOST_CHECK_EQUAL(
        GetNextWorkRequired(blocks[113].get(), &blkHeaderDummy, params),
        powLimit.GetCompact());

    // Once we reached the minimal difficulty, we stick with it. (Block #114)
    blocks.push_back(GetBlockIndex(blocks[113].get(), 2 * 3600, powLimit.GetCompact()));
    BOOST_CHECK(powLimit.GetCompact() != currentPow.GetCompact());
    BOOST_CHECK_EQUAL(
        GetNextWorkRequired(blocks[114].get(), &blkHeaderDummy, params),
        powLimit.GetCompact());
}

BOOST_AUTO_TEST_CASE(cash_difficulty_test) {
    DummyConfig config(CBaseChainParams::MAIN);

    std::vector<CBlockIndexPtr> blocks(3000);

    const Consensus::Params &params = config.GetChainParams().GetConsensus();
    const arith_uint256 powLimit = UintToArith256(params.powLimit);
    uint32_t powLimitBits = powLimit.GetCompact();
    arith_uint256 currentPow = powLimit >> 4;
    uint32_t initialBits = currentPow.GetCompact();

    // Genesis block.
    blocks[0] = MkCBlockIndexPtr();
    blocks[0]->nHeight = 0;
    blocks[0]->nTime = 1269211443;
    blocks[0]->nBits = initialBits;

    blocks[0]->nChainWork = GetBlockProof(*blocks[0]);

    // Block counter.
    size_t i;

    // Pile up some blocks every 10 mins to establish some history.
    for (i = 1; i < 2050; i++) {
        blocks[i] = GetBlockIndex(blocks[i - 1].get(), 600, initialBits);
    }

    CBlockHeader blkHeaderDummy;
    uint32_t nBits =
        GetNextCashWorkRequired(blocks[2049].get(), &blkHeaderDummy, params);

    // Difficulty stays the same as long as we produce a block every 10 mins.
    for (size_t j = 0; j < 10; i++, j++) {
        blocks[i] = GetBlockIndex(blocks[i - 1].get(), 600, nBits);
        BOOST_CHECK_EQUAL(
            GetNextCashWorkRequired(blocks[i].get(), &blkHeaderDummy, params),
            nBits);
    }

    // Make sure we skip over blocks that are out of wack. To do so, we produce
    // a block that is far in the future, and then produce a block with the
    // expected timestamp.
    blocks[i] = GetBlockIndex(blocks[i - 1].get(), 6000, nBits);
    BOOST_CHECK_EQUAL(
        GetNextCashWorkRequired(blocks[i++].get(), &blkHeaderDummy, params), nBits);
    blocks[i] = GetBlockIndex(blocks[i - 1].get(), 2 * 600 - 6000, nBits);
    BOOST_CHECK_EQUAL(
        GetNextCashWorkRequired(blocks[i++].get(), &blkHeaderDummy, params), nBits);

    // The system should continue unaffected by the block with a bogous
    // timestamps.
    for (size_t j = 0; j < 20; i++, j++) {
        blocks[i] = GetBlockIndex(blocks[i - 1].get(), 600, nBits);
        BOOST_CHECK_EQUAL(
            GetNextCashWorkRequired(blocks[i].get(), &blkHeaderDummy, params),
            nBits);
    }

    // We start emitting blocks slightly faster. The first block has no impact.
    blocks[i] = GetBlockIndex(blocks[i - 1].get(), 550, nBits);
    BOOST_CHECK_EQUAL(
        GetNextCashWorkRequired(blocks[i++].get(), &blkHeaderDummy, params), nBits);

    // Now we should see difficulty increase slowly.
    for (size_t j = 0; j < 10; i++, j++) {
        blocks[i] = GetBlockIndex(blocks[i - 1].get(), 550, nBits);
        const uint32_t nextBits =
            GetNextCashWorkRequired(blocks[i].get(), &blkHeaderDummy, params);

        arith_uint256 currentTarget;
        currentTarget.SetCompact(nBits);
        arith_uint256 nextTarget;
        nextTarget.SetCompact(nextBits);

        // Make sure that difficulty increases very slowly.
        BOOST_CHECK(nextTarget < currentTarget);
        BOOST_CHECK((currentTarget - nextTarget) < (currentTarget >> 10));

        nBits = nextBits;
    }

    // Check the actual value.
    BOOST_CHECK_EQUAL(nBits, 0x1c0fe7b1);

    // If we dramatically shorten block production, difficulty increases faster.
    for (size_t j = 0; j < 20; i++, j++) {
        blocks[i] = GetBlockIndex(blocks[i - 1].get(), 10, nBits);
        const uint32_t nextBits =
            GetNextCashWorkRequired(blocks[i].get(), &blkHeaderDummy, params);

        arith_uint256 currentTarget;
        currentTarget.SetCompact(nBits);
        arith_uint256 nextTarget;
        nextTarget.SetCompact(nextBits);

        // Make sure that difficulty increases faster.
        BOOST_CHECK(nextTarget < currentTarget);
        BOOST_CHECK((currentTarget - nextTarget) < (currentTarget >> 4));

        nBits = nextBits;
    }

    // Check the actual value.
    BOOST_CHECK_EQUAL(nBits, 0x1c0db19f);

    // We start to emit blocks significantly slower. The first block has no
    // impact.
    blocks[i] = GetBlockIndex(blocks[i - 1].get(), 6000, nBits);
    nBits = GetNextCashWorkRequired(blocks[i++].get(), &blkHeaderDummy, params);

    // Check the actual value.
    BOOST_CHECK_EQUAL(nBits, 0x1c0d9222);

    // If we dramatically slow down block production, difficulty decreases.
    for (size_t j = 0; j < 93; i++, j++) {
        blocks[i] = GetBlockIndex(blocks[i - 1].get(), 6000, nBits);
        const uint32_t nextBits =
            GetNextCashWorkRequired(blocks[i].get(), &blkHeaderDummy, params);

        arith_uint256 currentTarget;
        currentTarget.SetCompact(nBits);
        arith_uint256 nextTarget;
        nextTarget.SetCompact(nextBits);

        // Check the difficulty decreases.
        BOOST_CHECK(nextTarget <= powLimit);
        BOOST_CHECK(nextTarget > currentTarget);
        BOOST_CHECK((nextTarget - currentTarget) < (currentTarget >> 3));

        nBits = nextBits;
    }

    // Check the actual value.
    BOOST_CHECK_EQUAL(nBits, 0x1c2f13b9);

    // Due to the window of time being bounded, next block's difficulty actually
    // gets harder.
    blocks[i] = GetBlockIndex(blocks[i - 1].get(), 6000, nBits);
    nBits = GetNextCashWorkRequired(blocks[i++].get(), &blkHeaderDummy, params);
    BOOST_CHECK_EQUAL(nBits, 0x1c2ee9bf);

    // And goes down again. It takes a while due to the window being bounded and
    // the skewed block causes 2 blocks to get out of the window.
    for (size_t j = 0; j < 192; i++, j++) {
        blocks[i] = GetBlockIndex(blocks[i - 1].get(), 6000, nBits);
        const uint32_t nextBits =
            GetNextCashWorkRequired(blocks[i].get(), &blkHeaderDummy, params);

        arith_uint256 currentTarget;
        currentTarget.SetCompact(nBits);
        arith_uint256 nextTarget;
        nextTarget.SetCompact(nextBits);

        // Check the difficulty decreases.
        BOOST_CHECK(nextTarget <= powLimit);
        BOOST_CHECK(nextTarget > currentTarget);
        BOOST_CHECK((nextTarget - currentTarget) < (currentTarget >> 3));

        nBits = nextBits;
    }

    // Check the actual value.
    BOOST_CHECK_EQUAL(nBits, 0x1d00ffff);

    // Once the difficulty reached the minimum allowed level, it doesn't get any
    // easier.
    for (size_t j = 0; j < 5; i++, j++) {
        blocks[i] = GetBlockIndex(blocks[i - 1].get(), 6000, nBits);
        const uint32_t nextBits =
            GetNextCashWorkRequired(blocks[i].get(), &blkHeaderDummy, params);

        // Check the difficulty stays constant.
        BOOST_CHECK_EQUAL(nextBits, powLimitBits);
        nBits = nextBits;
    }
}

double TargetFromBits(const uint32_t nBits) {
    return (nBits & 0xff'ff'ff) * pow(256, (nBits >> 24)-3);
}

double GetASERTApproximationError(const CBlockIndex *pindexPrev,
                                  const uint32_t finalBits,
                                  const CBlockIndex *pindexAnchorBlock) {
    const int64_t nHeightDiff = pindexPrev->nHeight - pindexAnchorBlock->nHeight;
    const int64_t nTimeDiff   = pindexPrev->GetBlockTime()   - pindexAnchorBlock->pprev->GetBlockTime();
    const uint32_t initialBits = pindexAnchorBlock->nBits;

    BOOST_CHECK(nHeightDiff >= 0);
    double dInitialPow = TargetFromBits(initialBits);
    double dFinalPow   = TargetFromBits(finalBits);

    double dExponent = double(nTimeDiff - (nHeightDiff+1) * 600) / double(2*24*3600);
    double dTarget = dInitialPow * pow(2, dExponent);

    return (dFinalPow - dTarget) / dTarget;
}

BOOST_AUTO_TEST_CASE(asert_difficulty_test) {
    DummyConfig config(CBaseChainParams::MAIN);

    std::vector<CBlockIndexPtr> blocks(3000 + 2*24*3600);

    Consensus::Params mutableParams = config.GetChainParams().GetConsensus(); // copy params
    mutableParams.asertAnchorParams.reset();  // clear hard-coded anchor block so that we may perform these below tests
    const Consensus::Params &params = mutableParams; // take a const reference
    const arith_uint256 powLimit = UintToArith256(params.powLimit);
    arith_uint256 currentPow = powLimit >> 3;
    uint32_t initialBits = currentPow.GetCompact();
    double dMaxErr = 0.0001166792656486;

    // Genesis block, and parent of ASERT anchor block in this test case.
    blocks[0] = MkCBlockIndexPtr();
    blocks[0]->nHeight = 0;
    blocks[0]->nTime = 1269211443;
    // The pre-anchor block's nBits should never be used, so we set it to a nonsense value in order to
    // trigger an error if it is ever accessed
    blocks[0]->nBits = 0x0dedbeef;

    blocks[0]->nChainWork = GetBlockProof(*blocks[0]);

    // Block counter.
    size_t i = 1;

    // ASERT anchor block. We give this one a solvetime of 150 seconds to ensure that
    // the solvetime between the pre-anchor and the anchor blocks is actually used.
    blocks[1] = GetBlockIndex(blocks[0].get(), 150, initialBits);
    // The nBits for the next block should not be equal to the anchor block's nBits
    CBlockHeader blkHeaderDummy;
    uint32_t nBits = GetNextASERTWorkRequired(blocks[i++].get(), &blkHeaderDummy, params, blocks[1].get());
    BOOST_CHECK(fabs(GetASERTApproximationError(blocks[i-1].get(), nBits, blocks[1].get())) < dMaxErr);
    BOOST_CHECK(nBits != initialBits);

    // If we add another block at 1050 seconds, we should return to the anchor block's nBits
    blocks[i] = GetBlockIndex(blocks[i-1].get(), 1050, nBits);
    nBits = GetNextASERTWorkRequired(blocks[i++].get(), &blkHeaderDummy, params, blocks[1].get());
    BOOST_CHECK(nBits == initialBits);
    BOOST_CHECK(fabs(GetASERTApproximationError(blocks[i-1].get(), nBits, blocks[1].get())) < dMaxErr);

    currentPow = arith_uint256().SetCompact(nBits);
    // Before we do anything else, check that timestamps *before* the anchor block work fine.
    // Jumping 2 days into the past will give a timestamp before the achnor, and should halve the target
    blocks[i] = GetBlockIndex(blocks[i-1].get(), 600-172800, nBits);
    nBits = GetNextASERTWorkRequired(blocks[i++].get(), &blkHeaderDummy, params, blocks[1].get());
    currentPow = arith_uint256().SetCompact(nBits);
    // Because nBits truncates target, we don't end up with exactly 1/2 the target
    BOOST_CHECK(currentPow <= arith_uint256().SetCompact(initialBits  ) / 2);
    BOOST_CHECK(currentPow >= arith_uint256().SetCompact(initialBits-1) / 2);
    BOOST_CHECK(fabs(GetASERTApproximationError(blocks[i-1].get(), nBits, blocks[1].get())) < dMaxErr);

    // Jumping forward 2 days should return the target to the initial value
    blocks[i] = GetBlockIndex(blocks[i-1].get(), 600+172800, nBits);
    nBits = GetNextASERTWorkRequired(blocks[i++].get(), &blkHeaderDummy, params, blocks[1].get());
    currentPow = arith_uint256().SetCompact(nBits);
    BOOST_CHECK(nBits == initialBits);
    BOOST_CHECK(fabs(GetASERTApproximationError(blocks[i-1].get(), nBits, blocks[1].get())) < dMaxErr);

    // Pile up some blocks every 10 mins to establish some history.
    for (; i < 150; i++) {
        blocks[i] = GetBlockIndex(blocks[i - 1].get(), 600, nBits);
        BOOST_CHECK_EQUAL(blocks[i]->nBits, nBits);
    }

    nBits = GetNextASERTWorkRequired(blocks[i - 1].get(), &blkHeaderDummy, params, blocks[1].get());

    BOOST_CHECK_EQUAL(nBits, initialBits);

    // Difficulty stays the same as long as we produce a block every 10 mins.
    for (size_t j = 0; j < 10; i++, j++) {
        blocks[i] = GetBlockIndex(blocks[i - 1].get(), 600, nBits);
        BOOST_CHECK_EQUAL(
            GetNextASERTWorkRequired(blocks[i].get(), &blkHeaderDummy, params, blocks[1].get()),
            nBits);
    }

    // If we add a two blocks whose solvetimes together add up to 1200s,
    // then the next block's target should be the same as the one before these blocks
    // (at this point, equal to initialBits).
    blocks[i] = GetBlockIndex(blocks[i - 1].get(), 300, nBits);
    nBits = GetNextASERTWorkRequired(blocks[i++].get(), &blkHeaderDummy, params, blocks[1].get());
    BOOST_CHECK(fabs(GetASERTApproximationError(blocks[i-1].get(), nBits, blocks[ 1 ].get())) < dMaxErr);
    BOOST_CHECK(fabs(GetASERTApproximationError(blocks[i-1].get(), nBits, blocks[i-2].get())) < dMaxErr); // relative
    blocks[i] = GetBlockIndex(blocks[i - 1].get(), 900, nBits);
    nBits = GetNextASERTWorkRequired(blocks[i++].get(), &blkHeaderDummy, params, blocks[1].get());
    BOOST_CHECK(fabs(GetASERTApproximationError(blocks[i-1].get(), nBits, blocks[ 1 ].get())) < dMaxErr); // absolute
    BOOST_CHECK(fabs(GetASERTApproximationError(blocks[i-1].get(), nBits, blocks[i-2].get())) < dMaxErr); // relative
    BOOST_CHECK_EQUAL(nBits, initialBits);
    BOOST_CHECK(nBits != blocks[i-1]->nBits);

    // Same in reverse - this time slower block first, followed by faster block.
    blocks[i] = GetBlockIndex(blocks[i - 1].get(), 900, nBits);
    nBits = GetNextASERTWorkRequired(blocks[i++].get(), &blkHeaderDummy, params, blocks[1].get());
    BOOST_CHECK(fabs(GetASERTApproximationError(blocks[i-1].get(), nBits, blocks[ 1 ].get())) < dMaxErr); // absolute
    BOOST_CHECK(fabs(GetASERTApproximationError(blocks[i-1].get(), nBits, blocks[i-2].get())) < dMaxErr); // relative
    blocks[i] = GetBlockIndex(blocks[i - 1].get(), 300, nBits);
    nBits = GetNextASERTWorkRequired(blocks[i++].get(), &blkHeaderDummy, params, blocks[1].get());
    BOOST_CHECK(fabs(GetASERTApproximationError(blocks[i-1].get(), nBits, blocks[ 1 ].get())) < dMaxErr); // absolute
    BOOST_CHECK(fabs(GetASERTApproximationError(blocks[i-1].get(), nBits, blocks[i-2].get())) < dMaxErr); // relative
    BOOST_CHECK_EQUAL(nBits, initialBits);
    BOOST_CHECK(nBits != blocks[i-1]->nBits);

    // Jumping forward 2 days should double the target (halve the difficulty)
    blocks[i] = GetBlockIndex(blocks[i - 1].get(), 600 + 2*24*3600, nBits);
    nBits = GetNextASERTWorkRequired(blocks[i++].get(), &blkHeaderDummy, params, blocks[1].get());
    BOOST_CHECK(fabs(GetASERTApproximationError(blocks[i-1].get(), nBits, blocks[ 1 ].get())) < dMaxErr); // absolute
    BOOST_CHECK(fabs(GetASERTApproximationError(blocks[i-1].get(), nBits, blocks[i-2].get())) < dMaxErr); // relative
    currentPow = arith_uint256().SetCompact(nBits) / 2;
    BOOST_CHECK_EQUAL(currentPow.GetCompact(), initialBits);

    // Jumping backward 2 days should bring target back to where we started
    blocks[i] = GetBlockIndex(blocks[i - 1].get(), 600 - 2*24*3600, nBits);
    nBits = GetNextASERTWorkRequired(blocks[i++].get(), &blkHeaderDummy, params, blocks[1].get());
    BOOST_CHECK(fabs(GetASERTApproximationError(blocks[i-1].get(), nBits, blocks[ 1 ].get())) < dMaxErr); // absolute
    BOOST_CHECK(fabs(GetASERTApproximationError(blocks[i-1].get(), nBits, blocks[i-2].get())) < dMaxErr); // relative
    BOOST_CHECK_EQUAL(nBits, initialBits);

    // Jumping backward 2 days should halve the target (double the difficulty)
    blocks[i] = GetBlockIndex(blocks[i - 1].get(), 600 - 2*24*3600, nBits);
    nBits = GetNextASERTWorkRequired(blocks[i++].get(), &blkHeaderDummy, params, blocks[1].get());
    BOOST_CHECK(fabs(GetASERTApproximationError(blocks[i-1].get(), nBits, blocks[ 1 ].get())) < dMaxErr); // absolute
    BOOST_CHECK(fabs(GetASERTApproximationError(blocks[i-1].get(), nBits, blocks[i-2].get())) < dMaxErr); // relative
    currentPow = arith_uint256().SetCompact(nBits);
    // Because nBits truncates target, we don't end up with exactly 1/2 the target
    BOOST_CHECK(currentPow <= arith_uint256().SetCompact(initialBits  ) / 2);
    BOOST_CHECK(currentPow >= arith_uint256().SetCompact(initialBits-1) / 2);

    // And forward again
    blocks[i] = GetBlockIndex(blocks[i - 1].get(), 600 + 2*24*3600, nBits);
    nBits = GetNextASERTWorkRequired(blocks[i++].get(), &blkHeaderDummy, params, blocks[1].get());
    BOOST_CHECK(fabs(GetASERTApproximationError(blocks[i-1].get(), nBits, blocks[ 1 ].get())) < dMaxErr); // absolute
    BOOST_CHECK(fabs(GetASERTApproximationError(blocks[i-1].get(), nBits, blocks[i-2].get())) < dMaxErr); // relative
    BOOST_CHECK_EQUAL(nBits, initialBits);
    blocks[i] = GetBlockIndex(blocks[i - 1].get(), 600 + 2*24*3600, nBits);
    nBits = GetNextASERTWorkRequired(blocks[i++].get(), &blkHeaderDummy, params, blocks[1].get());
    BOOST_CHECK(fabs(GetASERTApproximationError(blocks[i-1].get(), nBits, blocks[ 1 ].get())) < dMaxErr); // absolute
    BOOST_CHECK(fabs(GetASERTApproximationError(blocks[i-1].get(), nBits, blocks[i-2].get())) < dMaxErr); // relative
    currentPow = arith_uint256().SetCompact(nBits) / 2;
    BOOST_CHECK_EQUAL(currentPow.GetCompact(), initialBits);

    // Iterate over the entire -2*24*3600..+2*24*3600 range to check that our integer approximation:
    //   1. Should be monotonic
    //   2. Should change target at least once every 8 seconds (worst-case: 15-bit precision on nBits)
    //   3. Should never change target by more than XXXX per 1-second step
    //   4. Never exceeds dMaxError in absolute error vs a double float calculation
    //   5. Has almost exactly the dMax and dMin errors we expect for the formula
    double dMin = 0;
    double dMax = 0;
    double dErr;
    double dRelMin = 0;
    double dRelMax = 0;
    double dRelErr;
    double dMaxStep = 0;
    uint32_t nBitsRingBuffer[8];
    double dStep = 0;
    blocks[i] = GetBlockIndex(blocks[i - 1].get(), -2*24*3600 - 30, nBits);
    for (size_t j = 0; j < 4*24*3600 + 660; j++) {
        blocks[i]->nTime++;
        nBits = GetNextASERTWorkRequired(blocks[i].get(), &blkHeaderDummy, params, blocks[1].get());

        if (j > 8) {
            // 1: Monotonic
            BOOST_CHECK(arith_uint256().SetCompact(nBits) >= arith_uint256().SetCompact(nBitsRingBuffer[(j-1)%8]));
            // 2: Changes at least once every 8 seconds (worst case: nBits = 1d008000 to 1d008001)
            BOOST_CHECK(arith_uint256().SetCompact(nBits) > arith_uint256().SetCompact(nBitsRingBuffer[j%8]));
            // 3: Check 1-sec step size
            dStep = (TargetFromBits(nBits) - TargetFromBits(nBitsRingBuffer[(j-1)%8])) / TargetFromBits(nBits);
            if (dStep > dMaxStep) dMaxStep = dStep;
            BOOST_CHECK(dStep < 0.0000314812106363); // from nBits = 1d008000 to 1d008001
        }
        nBitsRingBuffer[j%8] = nBits;

        // 4 and 5: check error vs double precision float calculation
        dErr    = GetASERTApproximationError(blocks[i].get(), nBits, blocks[1].get());
        dRelErr = GetASERTApproximationError(blocks[i].get(), nBits, blocks[i-1].get());
        if (dErr    < dMin)    dMin    = dErr;
        if (dErr    > dMax)    dMax    = dErr;
        if (dRelErr < dRelMin) dRelMin = dRelErr;
        if (dRelErr > dRelMax) dRelMax = dRelErr;
        BOOST_CHECK_MESSAGE(fabs(dErr) < dMaxErr,
                            strprintf("solveTime: %d\tStep size: %.8f%%\tdErr: %.8f%%\tnBits: %0x\n",
                                      int64_t(blocks[i]->nTime) - blocks[i-1]->nTime, dStep*100, dErr*100, nBits));
        BOOST_CHECK_MESSAGE(fabs(dRelErr) < dMaxErr,
                            strprintf("solveTime: %d\tStep size: %.8f%%\tdRelErr: %.8f%%\tnBits: %0x\n",
                                      int64_t(blocks[i]->nTime) - blocks[i-1]->nTime, dStep*100, dRelErr*100, nBits));
    }
    auto failMsg = strprintf("Min error: %16.14f%%\tMax error: %16.14f%%\tMax step: %16.14f%%\n", dMin*100, dMax*100, dMaxStep*100);
    BOOST_CHECK_MESSAGE(   dMin < -0.0001013168981059
                        && dMin > -0.0001013168981060
                        && dMax >  0.0001166792656485
                        && dMax <  0.0001166792656486,
                        failMsg);
    failMsg = strprintf("Min relError: %16.14f%%\tMax relError: %16.14f%%\n", dRelMin*100, dRelMax*100);
    BOOST_CHECK_MESSAGE(   dRelMin < -0.0001013168981059
                        && dRelMin > -0.0001013168981060
                        && dRelMax >  0.0001166792656485
                        && dRelMax <  0.0001166792656486,
                        failMsg);

    // Difficulty increases as long as we produce fast blocks
    for (size_t j = 0; j < 100; i++, j++) {
        uint32_t nextBits;
        arith_uint256 currentTarget;
        currentTarget.SetCompact(nBits);

        blocks[i] = GetBlockIndex(blocks[i - 1].get(), 500, nBits);
        nextBits = GetNextASERTWorkRequired(blocks[i].get(), &blkHeaderDummy, params, blocks[1].get());
        arith_uint256 nextTarget;
        nextTarget.SetCompact(nextBits);

        // Make sure that target is decreased
        BOOST_CHECK(nextTarget <= currentTarget);

        nBits = nextBits;
    }

}

std::string StrPrintCalcArgs(const arith_uint256 refTarget,
                             const int64_t targetSpacing,
                             const int64_t timeDiff,
                             const int64_t heightDiff,
                             const arith_uint256 expectedTarget,
                             const uint32_t expectednBits) {
    return strprintf("\n"
                     "ref=         %s\n"
                     "spacing=     %d\n"
                     "timeDiff=    %d\n"
                     "heightDiff=  %d\n"
                     "expTarget=   %s\n"
                     "exp nBits=   0x%08x\n",
                     refTarget.ToString(),
                     targetSpacing,
                     timeDiff,
                     heightDiff,
                     expectedTarget.ToString(),
                     expectednBits);
}


// Tests of the CalculateASERT function.
BOOST_AUTO_TEST_CASE(calculate_asert_test) {
    DummyConfig config(CBaseChainParams::MAIN);
    const Consensus::Params &params = config.GetChainParams().GetConsensus();
    const int64_t nHalfLife = params.nASERTHalfLife;

    const arith_uint256 powLimit = UintToArith256(params.powLimit);
    arith_uint256 initialTarget = powLimit >> 4;
    int64_t height = 0;

    // The CalculateASERT function uses the absolute ASERT formulation
    // and adds +1 to the height difference that it receives.
    // The time difference passed to it must factor in the difference
    // to the *parent* of the reference block.
    // We assume the parent is ideally spaced in time before the reference block.
    static const int64_t parent_time_diff = 600;

    // Steady
    arith_uint256 nextTarget = CalculateASERT(initialTarget, params.nPowTargetSpacing, parent_time_diff + 600 /* nTimeDiff */, ++height, powLimit, nHalfLife);
    BOOST_CHECK(nextTarget == initialTarget);

    // A block that arrives in half the expected time
    nextTarget = CalculateASERT(initialTarget, params.nPowTargetSpacing, parent_time_diff + 600 + 300, ++height, powLimit, nHalfLife);
    BOOST_CHECK(nextTarget < initialTarget);

    // A block that makes up for the shortfall of the previous one, restores the target to initial
    arith_uint256 prevTarget = nextTarget;
    nextTarget = CalculateASERT(initialTarget, params.nPowTargetSpacing, parent_time_diff + 600 + 300 + 900, ++height, powLimit, nHalfLife);
    BOOST_CHECK(nextTarget > prevTarget);
    BOOST_CHECK(nextTarget == initialTarget);

    // Two days ahead of schedule should double the target (halve the difficulty)
    prevTarget = nextTarget;
    nextTarget = CalculateASERT(prevTarget, params.nPowTargetSpacing, parent_time_diff + 288*1200, 288, powLimit, nHalfLife);
    BOOST_CHECK(nextTarget == prevTarget * 2);

    // Two days behind schedule should halve the target (double the difficulty)
    prevTarget = nextTarget;
    nextTarget = CalculateASERT(prevTarget, params.nPowTargetSpacing, parent_time_diff + 288*0, 288, powLimit, nHalfLife);
    BOOST_CHECK(nextTarget == prevTarget / 2);
    BOOST_CHECK(nextTarget == initialTarget);

    // Ramp up from initialTarget to PowLimit - should only take 4 doublings...
    uint32_t powLimit_nBits = powLimit.GetCompact();
    uint32_t next_nBits;
    for (size_t k = 0; k < 3; k++) {
        prevTarget = nextTarget;
        nextTarget = CalculateASERT(prevTarget, params.nPowTargetSpacing, parent_time_diff + 288*1200, 288, powLimit, nHalfLife);
        BOOST_CHECK(nextTarget == prevTarget * 2);
        BOOST_CHECK(nextTarget < powLimit);
        next_nBits = nextTarget.GetCompact();
        BOOST_CHECK(next_nBits != powLimit_nBits);
    }

    prevTarget = nextTarget;
    nextTarget = CalculateASERT(prevTarget, params.nPowTargetSpacing, parent_time_diff + 288*1200, 288, powLimit, nHalfLife);
    next_nBits = nextTarget.GetCompact();
    BOOST_CHECK(nextTarget == prevTarget * 2);
    BOOST_CHECK(next_nBits == powLimit_nBits);

    // Fast periods now cannot increase target beyond POW limit, even if we try to overflow nextTarget.
    // prevTarget is a uint256, so 256*2 = 512 days would overflow nextTarget unless CalculateASERT
    // correctly detects this error
    nextTarget = CalculateASERT(prevTarget, params.nPowTargetSpacing, parent_time_diff + 512*144*600, 0, powLimit, nHalfLife);
    next_nBits = nextTarget.GetCompact();
    BOOST_CHECK(next_nBits == powLimit_nBits);

    // We also need to watch for underflows on nextTarget. We need to withstand an extra ~446 days worth of blocks.
    // This should bring down a powLimit target to the a minimum target of 1.
    nextTarget = CalculateASERT(powLimit, params.nPowTargetSpacing, 0, 2*(256-33)*144, powLimit, nHalfLife);
    next_nBits = nextTarget.GetCompact();
    BOOST_CHECK_EQUAL(next_nBits, arith_uint256(1).GetCompact());

    // Define a structure holding parameters to pass to CalculateASERT.
    // We are going to check some expected results  against a vector of
    // possible arguments.
    struct calc_params {
        arith_uint256 refTarget;
        int64_t targetSpacing;
        int64_t timeDiff;
        int64_t heightDiff;
        arith_uint256 expectedTarget;
        uint32_t expectednBits;
    };

    // Define some named input argument values
    const arith_uint256 SINGLE_300_TARGET { "00000000ffb1ffffffffffffffffffffffffffffffffffffffffffffffffffff" };
    const arith_uint256 FUNNY_REF_TARGET { "000000008000000000000000000fffffffffffffffffffffffffffffffffffff" };

    // Define our expected input and output values.
    // The timeDiff entries exclude the `parent_time_diff` - this is
    // added in the call to CalculateASERT in the test loop.
    const std::vector<calc_params> calculate_args = {

        /* refTarget, targetSpacing, timeDiff, heightDiff, expectedTarget, expectednBits */

        { powLimit, 600, 0, 2*144, powLimit >> 1, 0x1c7fffff },
        { powLimit, 600, 0, 4*144, powLimit >> 2, 0x1c3fffff },
        { powLimit >> 1, 600, 0, 2*144, powLimit >> 2, 0x1c3fffff },
        { powLimit >> 2, 600, 0, 2*144, powLimit >> 3, 0x1c1fffff },
        { powLimit >> 3, 600, 0, 2*144, powLimit >> 4, 0x1c0fffff },
        { powLimit, 600, 0, 2*(256-34)*144, 3, 0x01030000 },
        { powLimit, 600, 0, 2*(256-34)*144 + 119, 3, 0x01030000 },
        { powLimit, 600, 0, 2*(256-34)*144 + 120, 2, 0x01020000 },
        { powLimit, 600, 0, 2*(256-33)*144-1, 2, 0x01020000 },
        { powLimit, 600, 0, 2*(256-33)*144, 1, 0x01010000 },  // 1 bit less since we do not need to shift to 0
        { powLimit, 600, 0, 2*(256-32)*144, 1, 0x01010000 },  // more will not decrease below 1
        { 1, 600, 0, 2*(256-32)*144, 1, 0x01010000 },
        { powLimit, 600, 2*(512-32)*144, 0, powLimit, powLimit_nBits },
        { 1, 600, (512-64)*144*600, 0, powLimit, powLimit_nBits },
        { powLimit, 600, 300, 1, SINGLE_300_TARGET, 0x1d00ffb1 },  // clamps to powLimit
        { FUNNY_REF_TARGET, 600, 600*2*33*144, 0, powLimit, powLimit_nBits }, // confuses any attempt to detect overflow by inspecting result
        { 1, 600, 600*2*256*144, 0, powLimit, powLimit_nBits }, // overflow to exactly 2^256
        { 1, 600, 600*2*224*144 - 1, 0, arith_uint256(0xffff8) << 204, powLimit_nBits }, // just under powlimit (not clamped) yet over powlimit_nbits
    };

    for (auto& v : calculate_args) {
        nextTarget = CalculateASERT(v.refTarget, v.targetSpacing, parent_time_diff + v.timeDiff, v.heightDiff, powLimit, nHalfLife);
        next_nBits = nextTarget.GetCompact();
        const auto failMsg =
            StrPrintCalcArgs(v.refTarget, v.targetSpacing, parent_time_diff + v.timeDiff, v.heightDiff, v.expectedTarget, v.expectednBits)
            + strprintf("nextTarget=  %s\nnext nBits=  0x%08x\n", nextTarget.ToString(), next_nBits);
        BOOST_CHECK_MESSAGE(nextTarget == v.expectedTarget && next_nBits == v.expectednBits, failMsg);
    }
}

/**
 * Test transition of cw144 to ASERT algorithm, which involves the selection
 * of an anchor block.
 */
BOOST_AUTO_TEST_CASE(asert_activation_anchor_test) {
    // Make a custom chain params based on mainnet, activating the cw144 DAA
    // at a lower height than usual, so we don't need to waste time making a
    // 504000-long chain.
    Consensus::Params params = CreateChainParams(CBaseChainParams::MAIN)->GetConsensus();
    params.asertAnchorParams.reset(); // clear hard-coded anchor block so that we may test the activation below
    params.daaHeight = 2016;
    const int64_t activationTime =
        gArgs.GetArg("-axionactivationtime", params.axionActivationTime);
    CBlockHeader blkHeaderDummy;

    // an arbitrary compact target for our chain (based on fxx chain ~ Aug 10 2020).
    uint32_t initialBits = 0x1802a842;

    // Block store for anonymous blocks; needs to be big enough to fit all generated
    // blocks in this test.
    std::vector<CBlockIndexPtr> blocks(10000);
    int bidx = 1;

    // Genesis block.
    blocks[0] = MkCBlockIndexPtr();
    blocks[0]->nHeight = 0;
    blocks[0]->nTime = 1269211443;
    blocks[0]->nBits = initialBits;
    blocks[0]->nChainWork = GetBlockProof(*blocks[0]);

    // Pile up a random number of blocks to establish some history of random height.
    // cw144 DAA requires us to have height at least 2016, dunno why that much.
    for (int i = 1; i < 2000 + int(InsecureRandRange(1000)); i++) {
        blocks[bidx] = GetBlockIndex(blocks[bidx-1].get(), 600, initialBits);
        bidx++;
        BOOST_REQUIRE(bidx < int(blocks.size()));
    }

    // Start making blocks prior to activation. First, make a block about 1 day before activation.
    // Then put down 145 more blocks with 500 second solvetime each, such that
    // the MTP on the final block is 1 second short of activationTime.
    {
        blocks[bidx] = GetBlockIndex(blocks[bidx-1].get(), 600, initialBits);
        blocks[bidx]->nTime = activationTime - 140*500 - 1;
        bidx++;
    }
    for (int i = 0; i < 145; i++) {
        BOOST_REQUIRE(bidx < int(blocks.size()));
        blocks[bidx] = GetBlockIndex(blocks[bidx-1].get(), 500, initialBits);
        bidx++;
    }
    CBlockIndex *pindexPreActivation = blocks[bidx-1].get();
    BOOST_CHECK_EQUAL(pindexPreActivation->nTime, activationTime + 5*500 - 1);
    BOOST_CHECK_EQUAL(pindexPreActivation->GetMedianTimePast(), activationTime - 1);
    BOOST_CHECK(IsDAAEnabled(params, pindexPreActivation));

    // If we consult DAA, then it uses cw144 which returns a significantly lower target because
    // we have been mining too fast by a ratio 600/500 for a whole day.
    BOOST_CHECK(!IsAxionEnabled(params, pindexPreActivation));
    BOOST_CHECK_EQUAL(GetNextWorkRequired(pindexPreActivation, &blkHeaderDummy, params), 0x180236e1);

    // ASERT has never run yet, so cache is unpopulated.
    BOOST_CHECK_EQUAL(GetASERTAnchorBlockCache(), nullptr);

    /**
     * Now we'll try adding on blocks to activate ASERT. The activation block
     * is going to be our anchor block. We will make several distinct anchor blocks.
     */

    // Create an activating block with expected solvetime, taking the cw144 difficulty we just
    // saw. Since solvetime is expected the next target is unchanged.
    CBlockIndexPtr indexActivation0 = GetBlockIndex(pindexPreActivation, 600, 0x180236e1);
    BOOST_CHECK(IsAxionEnabled(params, indexActivation0.get()));
    BOOST_CHECK_EQUAL(GetNextWorkRequired(indexActivation0.get(), &blkHeaderDummy, params), 0x180236e1);
    // second call will have used anchor cache, shouldn't change anything
    BOOST_CHECK_EQUAL(GetASERTAnchorBlockCache(), indexActivation0.get());
    BOOST_CHECK_EQUAL(GetNextWorkRequired(indexActivation0.get(), &blkHeaderDummy, params), 0x180236e1);

    // Now we'll generate some more activations/anchors, using unique targets for each one
    // (if the algo gets confused between different anchors, we will know).

    // Create an activating block with 0 solvetime, which will drop target by ~415/416.
    CBlockIndexPtr indexActivation1 = GetBlockIndex(pindexPreActivation, 0, 0x18023456);
    BOOST_CHECK(IsAxionEnabled(params, indexActivation1.get()));
    // cache will be stale here, and we should get the right result regardless:
    BOOST_CHECK_EQUAL(GetNextWorkRequired(indexActivation1.get(), &blkHeaderDummy, params), 0x180232fd);
    // second call will have used anchor cache, shouldn't change anything
    BOOST_CHECK_EQUAL(GetASERTAnchorBlockCache(), indexActivation1.get());
    BOOST_CHECK_EQUAL(GetNextWorkRequired(indexActivation1.get(), &blkHeaderDummy, params), 0x180232fd);
    // for good measure, try again with wiped cache
    ResetASERTAnchorBlockCache();
    BOOST_CHECK_EQUAL(GetNextWorkRequired(indexActivation1.get(), &blkHeaderDummy, params), 0x180232fd);
    BOOST_CHECK_EQUAL(GetASERTAnchorBlockCache(), indexActivation1.get());

    // Try activation with expected solvetime, which will keep target the same.
    uint32_t anchorBits2 = 0x180210fe;
    CBlockIndexPtr indexActivation2 = GetBlockIndex(pindexPreActivation, 600, anchorBits2);
    BOOST_CHECK(IsAxionEnabled(params, indexActivation2.get()));
    BOOST_CHECK_EQUAL(GetNextWorkRequired(indexActivation2.get(), &blkHeaderDummy, params), anchorBits2);
    BOOST_CHECK_EQUAL(GetASERTAnchorBlockCache(), indexActivation2.get());

    // Try a three-month solvetime which will cause us to hit powLimit.
    uint32_t anchorBits3 = 0x18034567;
    CBlockIndexPtr indexActivation3 = GetBlockIndex(pindexPreActivation, 86400*90, anchorBits3);
    BOOST_CHECK(IsAxionEnabled(params, indexActivation2.get()));
    BOOST_CHECK_EQUAL(GetNextWorkRequired(indexActivation3.get(), &blkHeaderDummy, params), 0x1d00ffff);
    // If the next block jumps back in time, we get back our original difficulty level.
    CBlockIndexPtr indexActivation3_return = GetBlockIndex(indexActivation3.get(), -86400*90 + 2*600, anchorBits3);
    BOOST_CHECK_EQUAL(GetNextWorkRequired(indexActivation3_return.get(), &blkHeaderDummy, params), anchorBits3);
    // Retry for cache
    BOOST_CHECK_EQUAL(GetNextWorkRequired(indexActivation3_return.get(), &blkHeaderDummy, params), anchorBits3);
    BOOST_CHECK_EQUAL(GetASERTAnchorBlockCache(), indexActivation3.get());

    // Make an activation with MTP == activation exactly. This is a backwards timestamp jump
    // so the resulting target is 1.2% lower.
    CBlockIndexPtr indexActivation4 = GetBlockIndex(pindexPreActivation, 0, 0x18011111);
    indexActivation4->nTime = activationTime;
    BOOST_CHECK_EQUAL(indexActivation4->GetMedianTimePast(), activationTime);
    BOOST_CHECK(IsAxionEnabled(params, indexActivation4.get()));
    BOOST_CHECK_EQUAL(GetNextWorkRequired(indexActivation4.get(), &blkHeaderDummy, params), 0x18010db3);
    BOOST_CHECK_EQUAL(GetASERTAnchorBlockCache(), indexActivation4.get());

    // Finally create a random chain on top of our second activation, using ASERT targets all the way.
    // Erase cache so that this will do a fresh search for anchor at every step (fortauntely this is
    // not too slow, due to the skiplist traversal)
    CBlockIndex *pindexChain2 = indexActivation2.get();
    for (int i = 1; i < 1000; i++) {
        BOOST_REQUIRE(bidx < int(blocks.size()));
        ResetASERTAnchorBlockCache();
        uint32_t nextBits = GetNextWorkRequired(pindexChain2, &blkHeaderDummy, params);
        BOOST_CHECK_EQUAL(GetASERTAnchorBlockCache(), indexActivation2.get());
        blocks[bidx] = GetBlockIndex(pindexChain2, InsecureRandRange(1200), nextBits);
        pindexChain2 = blocks[bidx++].get();
    }
    // Scan back down to make sure all targets are same when we keep cached anchor.
    for (CBlockIndex *pindex = pindexChain2; pindex != indexActivation2.get() ; pindex = pindex->pprev) {
        uint32_t nextBits = GetNextWorkRequired(pindex->pprev, &blkHeaderDummy, params);
        BOOST_CHECK_EQUAL(nextBits, pindex->nBits);
        BOOST_CHECK_EQUAL(GetASERTAnchorBlockCache(), indexActivation2.get());
    }
}

BOOST_AUTO_TEST_SUITE_END()
