#!/usr/bin/env python3
# Copyright (c) 2016 The Fittexxcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Creating a cache of the blockchain speeds up test execution when running
multiple functional tests. This helper script is executed by test_runner when multiple
tests are being run in parallel.
"""

from test_framework.test_framework import FittexxcoinTestFramework


class CreateCache(FittexxcoinTestFramework):
    # Test network and test nodes are not required:

    def set_test_params(self):
        self.num_nodes = 0
        self.supports_cli = True

    def setup_network(self):
        pass

    def run_test(self):
        pass


if __name__ == '__main__':
    CreateCache().main()
