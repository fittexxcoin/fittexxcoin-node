#!/usr/bin/env python3
# Copyright (c) 2015-2019 The Fittexxcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test a node with the -disablewallet option.

- Test that validateaddress RPC works when running with -disablewallet
- Test that it is not possible to mine to an invalid address.
"""

from test_framework.test_framework import FittexxcoinTestFramework
from test_framework.util import assert_raises_rpc_error


class DisableWalletTest (FittexxcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.extra_args = [["-disablewallet"]]

    def run_test(self):
        # Make sure wallet is really disabled
        assert_raises_rpc_error(-32601, 'Method not found',
                                self.nodes[0].getwalletinfo)
        x = self.nodes[0].validateaddress('3J98t1WpEZ73CNmQviecrnyiWrnqRhWNLy')
        assert x['isvalid'] is False
        x = self.nodes[0].validateaddress('mneYUmWYsuk7kySiURxCi3AGxrAqZxLgPZ')
        assert x['isvalid'] is True

        # Checking mining to an address without a wallet. Generating to a valid address should succeed
        # but generating to an invalid address will fail.
        self.generatetoaddress(self.nodes[0],
                               1, 'mneYUmWYsuk7kySiURxCi3AGxrAqZxLgPZ')
        assert_raises_rpc_error(-5, "Invalid address",
                                self.generatetoaddress, self.nodes[0], 1, '3J98t1WpEZ73CNmQviecrnyiWrnqRhWNLy')


if __name__ == '__main__':
    DisableWalletTest().main()
