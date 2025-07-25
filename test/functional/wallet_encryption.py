#!/usr/bin/env python3
# Copyright (c) 2016 The Fittexxcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test Wallet encryption"""

import random
import time

from test_framework.test_framework import FittexxcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
    assert_greater_than,
    assert_greater_than_or_equal,
)


class WalletEncryptionTest(FittexxcoinTestFramework):

    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        passphrase = "WalletPassphrase"
        passphrase2 = "SecondWalletPassphrase"

        # Make sure the wallet isn't encrypted first
        address = self.nodes[0].getnewaddress()
        privkey = self.nodes[0].dumpprivkey(address)
        assert_equal(privkey[:1], "c")
        assert_equal(len(privkey), 52)
        assert_raises_rpc_error(-15,
                                "Error: running with an unencrypted wallet, but walletpassphrase was called",
                                self.nodes[0].walletpassphrase,
                                'ff',
                                1)
        assert_raises_rpc_error(-15,
                                "Error: running with an unencrypted wallet, but walletpassphrasechange was called.",
                                self.nodes[0].walletpassphrasechange,
                                'ff',
                                'ff')

        # Encrypt the wallet
        assert_raises_rpc_error(-8, "passphrase can not be empty", self.nodes[0].encryptwallet, '')
        self.nodes[0].encryptwallet(passphrase)

        # Check the encrypted wallet is marked as locked on initialization
        assert_equal(self.nodes[0].getwalletinfo()['unlocked_until'], 0)

        # Test that the wallet is encrypted
        assert_raises_rpc_error(
            -13, "Please enter the wallet passphrase with walletpassphrase first",
            self.nodes[0].dumpprivkey, address)
        assert_raises_rpc_error(-8, "passphrase can not be empty", self.nodes[0].walletpassphrase, '', 1)
        assert_raises_rpc_error(-8, "passphrase can not be empty", self.nodes[0].walletpassphrasechange, '', 'ff')

        # Check that walletpassphrase works
        self.nodes[0].walletpassphrase(passphrase, 2)
        assert_equal(privkey, self.nodes[0].dumpprivkey(address))

        # Check that the timeout is right
        time.sleep(3)
        assert_raises_rpc_error(
            -13, "Please enter the wallet passphrase with walletpassphrase first",
            self.nodes[0].dumpprivkey, address)

        # Test wrong passphrase
        assert_raises_rpc_error(-14, "wallet passphrase entered was incorrect",
                                self.nodes[0].walletpassphrase, passphrase + "wrong", 10)

        # Test walletlock and unlocked_until values
        self.mocktime = 1
        self.nodes[0].setmocktime(self.mocktime)
        self.nodes[0].walletpassphrase(passphrase, 84600)
        assert_equal(privkey, self.nodes[0].dumpprivkey(address))
        assert_equal(
            self.nodes[0].getwalletinfo()['unlocked_until'], 1 + 84600)
        self.nodes[0].walletlock()
        assert_raises_rpc_error(
            -13, "Please enter the wallet passphrase with walletpassphrase first",
            self.nodes[0].dumpprivkey, address)
        assert_equal(self.nodes[0].getwalletinfo()['unlocked_until'], 0)

        # Test passphrase changes
        self.nodes[0].walletpassphrasechange(passphrase, passphrase2)
        assert_raises_rpc_error(-14, "wallet passphrase entered was incorrect",
                                self.nodes[0].walletpassphrase, passphrase, 10)
        self.nodes[0].walletpassphrase(passphrase2, 10)
        assert_equal(privkey, self.nodes[0].dumpprivkey(address))
        self.nodes[0].walletlock()

        # Test timeout bounds
        assert_raises_rpc_error(-8, "Timeout cannot be negative.",
                                self.nodes[0].walletpassphrase, passphrase2, -10)
        # Check the timeout
        # Check a time less than the limit
        MAX_VALUE = 100000000
        expected_time = self.mocktime + MAX_VALUE - 600
        self.nodes[0].walletpassphrase(passphrase2, MAX_VALUE - 600)
        actual_time = self.nodes[0].getwalletinfo()['unlocked_until']
        assert_greater_than_or_equal(actual_time, expected_time)
        # 5 second buffer
        assert_greater_than(expected_time + 5, actual_time)
        # Check a time greater than the limit
        expected_time = self.mocktime + MAX_VALUE - 1
        self.nodes[0].walletpassphrase(passphrase2, MAX_VALUE + 1000)
        actual_time = self.nodes[0].getwalletinfo()['unlocked_until']
        assert_greater_than_or_equal(actual_time, expected_time)
        # 5 second buffer
        assert_greater_than(expected_time + 5, actual_time)

        for i in range(5):
            r = random.uniform(0.9, 1.1)
            self.log.info('Check that calling walletpassphrase does not freeze the node. sleep={}'.format(r))
            self.nodes[0].walletpassphrase(passphrase2, 1)
            time.sleep(r)
        self.nodes[0].walletlock()


if __name__ == '__main__':
    WalletEncryptionTest().main()
