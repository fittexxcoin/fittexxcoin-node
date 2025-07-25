#!/usr/bin/env bash
#
# Copyright (c) 2018 The Fittexxcoin Core developers
# Copyright (c) 2019 The Fittexxcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# Check the test suite naming conventions

export LC_ALL=C

TOPDIR=${TOPDIR:-$(git rev-parse --show-toplevel)}
EXIT_CODE=0
NAMING_INCONSISTENCIES=$(git grep -E '^BOOST_(AUTO|FIXTURE)_TEST_SUITE\(' -- \
    "${TOPDIR}/src/test/**.cpp" \
    "${TOPDIR}/src/seeder/test/**.cpp" \
    "${TOPDIR}/src/rpc/test/**.cpp" \
    "${TOPDIR}/src/wallet/test/**.cpp" | \
    grep -vE '/(.*?)\.cpp:BOOST_(AUTO|FIXTURE)_TEST_SUITE\(\1.*\)$')
if [[ ${NAMING_INCONSISTENCIES} != "" ]]; then
    EXIT_CODE=1
    echo "The test suite in file src/test/foo_tests.cpp should be named"
    echo "\"foo_tests\". Please make sure the following test suites follow"
    echo "that convention:"
    echo
    echo "${NAMING_INCONSISTENCIES}"
fi

TEST_SUITE_NAME_COLLISIONS=$(git grep -E '^BOOST_(AUTO|FIXTURE)_TEST_SUITE\(' -- \
    "${TOPDIR}/src/test/**.cpp" \
    "${TOPDIR}/src/seeder/test/**.cpp" \
    "${TOPDIR}/src/rpc/test/**.cpp" \
    "${TOPDIR}/src/wallet/test/**.cpp" | cut -f2 -d'(' | cut -f1 -d, | \
    cut -f1 -d\) | sort | uniq -d)
if [[ ${TEST_SUITE_NAME_COLLISIONS} != "" ]]; then
    EXIT_CODE=1
    echo "Test suite names must be unique. The following test suite names"
    echo "appear to be used more than once:"
    echo
    echo "${TEST_SUITE_NAME_COLLISIONS}"
fi

exit $EXIT_CODE
