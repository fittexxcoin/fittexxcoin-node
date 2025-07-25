// Copyright (c) 2018-2020 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockindexworkcomparator.h>

#include <boost/test/unit_test.hpp>

#include <memory>

BOOST_AUTO_TEST_SUITE(work_comparator_tests)

BOOST_AUTO_TEST_CASE(work_comparator) {
    std::unique_ptr<CBlockIndex> indexA(new CBlockIndex), indexB(new CBlockIndex);

    // Differing chain work
    indexA->nChainWork = 0;
    indexB->nChainWork = 1;
    for (int sequenceIdA = 1; sequenceIdA < 1024; sequenceIdA *= 2) {
        for (int sequenceIdB = 1; sequenceIdB < 1024; sequenceIdB *= 2) {
            // Differing sequenceId doesn't affect chain work
            indexA->nSequenceId = sequenceIdA;
            indexB->nSequenceId = sequenceIdB;
            BOOST_CHECK(CBlockIndexWorkComparator()(indexA.get(), indexB.get()));
        }
    }

    // Same chain work, but differing sequenceId
    indexA.reset(new CBlockIndex);
    indexB.reset(new CBlockIndex);
    for (int sequenceIdA = 1; sequenceIdA < 1024; sequenceIdA *= 2) {
        for (int sequenceIdB = 1; sequenceIdB < 1024; sequenceIdB *= 2) {
            if (sequenceIdA == sequenceIdB) {
                continue;
            }

            indexA->nSequenceId = sequenceIdA;
            indexB->nSequenceId = sequenceIdB;
            if (sequenceIdA > sequenceIdB) {
                BOOST_CHECK(CBlockIndexWorkComparator()(indexA.get(), indexB.get()));
            } else {
                BOOST_CHECK(CBlockIndexWorkComparator()(indexB.get(), indexA.get()));
            }
        }
    }

    // All else equal, so checking pointer address as final check
    auto pindexA = std::make_unique<CBlockIndex>();
    auto pindexB = std::make_unique<CBlockIndex>();
    if (pindexA < pindexB) {
        BOOST_CHECK(CBlockIndexWorkComparator()(pindexB.get(), pindexA.get()));
    } else {
        BOOST_CHECK(CBlockIndexWorkComparator()(pindexA.get(), pindexB.get()));
    }

    // Same block should return false
    BOOST_CHECK(!CBlockIndexWorkComparator()(pindexA.get(), pindexA.get()));
}

BOOST_AUTO_TEST_SUITE_END()
