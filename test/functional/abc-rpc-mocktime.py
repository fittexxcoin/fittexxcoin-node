#!/usr/bin/env python3
# Copyright (c) 2020 The Fittexxcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
Test RPCs related to mock time.
"""

from test_framework.test_framework import FittexxcoinTestFramework
from test_framework.util import assert_raises_rpc_error


class MocktimeTest(FittexxcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def run_test(self):
        self.nodes[0].setmocktime(9223372036854775807)
        self.nodes[0].setmocktime(0)
        assert_raises_rpc_error(-8, "Timestamp must be 0 or greater",
                                self.nodes[0].setmocktime, -1)
        assert_raises_rpc_error(-8, "Timestamp must be 0 or greater",
                                self.nodes[0].setmocktime, -9223372036854775808)


if __name__ == '__main__':
    MocktimeTest().main()
