#!/usr/bin/env python3
# Copyright (c) 2014-2019 The Fittexxcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the wallet accounts properly when there are cloned transactions with malleated scriptsigs."""

from test_framework.test_framework import FittexxcoinTestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes,
    disconnect_nodes,
)


class TxnMallTest(FittexxcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 4
        self.extra_args = [["-noparkdeepreorg"], ["-noparkdeepreorg"], [], []]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def add_options(self, parser):
        parser.add_argument("--mineblock", dest="mine_block", default=False, action="store_true",
                            help="Test double-spend of 1-confirmed transaction")

    def setup_network(self):
        # Start with split network:
        super(TxnMallTest, self).setup_network()
        disconnect_nodes(self.nodes[1], self.nodes[2])
        disconnect_nodes(self.nodes[2], self.nodes[1])

    def run_test(self):
        output_type = "legacy"

        # All nodes should start with 1,250 fxx:
        starting_balance = 1250
        for i in range(4):
            assert_equal(self.nodes[i].getbalance(), starting_balance)
            # bug workaround, coins generated assigned to first getnewaddress!
            self.nodes[i].getnewaddress()

        self.nodes[0].settxfee(.001)

        node0_address1 = self.nodes[0].getnewaddress(address_type=output_type)
        node0_txid1 = self.nodes[0].sendtoaddress(node0_address1, 1219)
        node0_tx1 = self.nodes[0].gettransaction(node0_txid1)

        node0_address2 = self.nodes[0].getnewaddress(address_type=output_type)
        node0_txid2 = self.nodes[0].sendtoaddress(node0_address2, 29)
        node0_tx2 = self.nodes[0].gettransaction(node0_txid2)

        assert_equal(self.nodes[0].getbalance(),
                     starting_balance + node0_tx1["fee"] + node0_tx2["fee"])

        # Coins are sent to node1_address
        node1_address = self.nodes[1].getnewaddress()

        # Send tx1, and another transaction tx2 that won't be cloned
        txid1 = self.nodes[0].sendtoaddress(node1_address, 40)
        txid2 = self.nodes[0].sendtoaddress(node1_address, 20)

        # Construct a clone of tx1, to be malleated
        rawtx1 = self.nodes[0].getrawtransaction(txid1, 1)
        clone_inputs = [{"txid": rawtx1["vin"][0]["txid"],
                         "vout": rawtx1["vin"][0]["vout"],
                         "sequence": rawtx1["vin"][0]["sequence"]}]
        clone_outputs = {rawtx1["vout"][0]["scriptPubKey"]["addresses"][0]: rawtx1["vout"][0]["value"],
                         rawtx1["vout"][1]["scriptPubKey"]["addresses"][0]: rawtx1["vout"][1]["value"]}
        clone_locktime = rawtx1["locktime"]
        clone_raw = self.nodes[0].createrawtransaction(
            clone_inputs, clone_outputs, clone_locktime)

        # createrawtransaction randomizes the order of its outputs, so swap them if necessary.
        # output 0 is at version+#inputs+input+sigstub+sequence+#outputs
        # 40 fxx serialized is 00286bee00000000
        pos0 = 2 * (4 + 1 + 36 + 1 + 4 + 1)
        hex40 = "00286bee00000000"
        output_len = 16 + 2 + 2 * \
            int("0x" + clone_raw[pos0 + 16: pos0 + 16 + 2], 0)
        if (rawtx1["vout"][0]["value"] == 40 and clone_raw[pos0: pos0 + 16] != hex40 or
                rawtx1["vout"][0]["value"] != 40 and clone_raw[pos0: pos0 + 16] == hex40):
            output0 = clone_raw[pos0: pos0 + output_len]
            output1 = clone_raw[pos0 + output_len: pos0 + 2 * output_len]
            clone_raw = clone_raw[:pos0] + output1 + \
                output0 + clone_raw[pos0 + 2 * output_len:]

        # Use a different signature hash type to sign.  This creates an equivalent but malleated clone.
        # Don't send the clone anywhere yet
        tx1_clone = self.nodes[0].signrawtransactionwithwallet(
            clone_raw, None, "ALL|FORKID|ANYONECANPAY")
        assert_equal(tx1_clone["complete"], True)

        # Have node0 mine a block, if requested:
        if (self.options.mine_block):
            self.generate(self.nodes[0], 1)
            self.sync_blocks(self.nodes[0:2])

        tx1 = self.nodes[0].gettransaction(txid1)
        tx2 = self.nodes[0].gettransaction(txid2)

        # Node0's balance should be starting balance, plus 50BTC for another
        # matured block, minus tx1 and tx2 amounts, and minus transaction fees:
        expected = starting_balance + node0_tx1["fee"] + node0_tx2["fee"]
        if self.options.mine_block:
            expected += 50
        expected += tx1["amount"] + tx1["fee"]
        expected += tx2["amount"] + tx2["fee"]
        assert_equal(self.nodes[0].getbalance(), expected)

        if self.options.mine_block:
            assert_equal(tx1["confirmations"], 1)
            assert_equal(tx2["confirmations"], 1)
        else:
            assert_equal(tx1["confirmations"], 0)
            assert_equal(tx2["confirmations"], 0)

        # Send clone and its parent to miner
        self.nodes[2].sendrawtransaction(node0_tx1["hex"])
        txid1_clone = self.nodes[2].sendrawtransaction(tx1_clone["hex"])

        # ... mine a block...
        self.generate(self.nodes[2], 1)

        # Reconnect the split network, and sync chain:
        connect_nodes(self.nodes[1], self.nodes[2])
        self.nodes[2].sendrawtransaction(node0_tx2["hex"])
        self.nodes[2].sendrawtransaction(tx2["hex"])
        self.generate(self.nodes[2], 1)  # Mine another block to make sure we sync
        self.sync_blocks()

        # Re-fetch transaction info:
        tx1 = self.nodes[0].gettransaction(txid1)
        tx1_clone = self.nodes[0].gettransaction(txid1_clone)
        tx2 = self.nodes[0].gettransaction(txid2)

        # Verify expected confirmations
        assert_equal(tx1["confirmations"], -2)
        assert_equal(tx1_clone["confirmations"], 2)
        assert_equal(tx2["confirmations"], 1)

        # Check node0's total balance; should be same as before the clone, + 100 fxx for 2 matured,
        # less possible orphaned matured subsidy
        expected += 100
        if (self.options.mine_block):
            expected -= 50
        assert_equal(self.nodes[0].getbalance(), expected)


if __name__ == '__main__':
    TxnMallTest().main()
