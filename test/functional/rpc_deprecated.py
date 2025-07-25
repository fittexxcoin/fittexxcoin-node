#!/usr/bin/env python3
# Copyright (c) 2017-2019 The Fittexxcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test deprecation of RPC calls."""
from test_framework.test_framework import FittexxcoinTestFramework


class DeprecatedRpcTest(FittexxcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.extra_args = [
            [], []]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        # This test should be used to verify correct behaviour of deprecated
        # RPC methods with and without the -deprecatedrpc flags. For example:
        #
        # self.log.info("Make sure that -deprecatedrpc=createmultisig allows it to take addresses")
        # assert_raises_rpc_error(-5, "Invalid public key", self.nodes[0].createmultisig, 1, [self.nodes[0].getnewaddress()])
        # self.nodes[1].createmultisig(1, [self.nodes[1].getnewaddress()])
        self.log.info("No tested deprecated RPC methods")


if __name__ == '__main__':
    DeprecatedRpcTest().main()
