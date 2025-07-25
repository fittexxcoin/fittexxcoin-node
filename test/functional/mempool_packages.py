#!/usr/bin/env python3
# Copyright (c) 2014-2019 The Fittexxcoin Core developers
# Copyright (c) 2021 The Fittexxcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test descendant package tracking code."""

from decimal import Decimal

from test_framework.test_framework import FittexxcoinTestFramework
from test_framework.util import (
    assert_equal,
    satoshi_round,
)

OLD_MAX_ANCESTORS = 50
OLD_MAX_DESCENDANTS = 50


class MempoolPackagesTest(FittexxcoinTestFramework):

    def set_test_params(self):
        self.num_nodes = 2
        common_params = ["-maxorphantx=1000"]
        self.extra_args = [common_params,
                           common_params]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    # Build a transaction that spends parent_txid:vout
    # Return amount sent
    def chain_transaction(self, node, parent_txid, vout,
                          value, fee, num_outputs):
        send_value = satoshi_round((value - fee) / num_outputs)
        inputs = [{'txid': parent_txid, 'vout': vout}]
        outputs = {}
        for i in range(num_outputs):
            outputs[node.getnewaddress()] = send_value
        rawtx = node.createrawtransaction(inputs, outputs)
        signedtx = node.signrawtransactionwithwallet(rawtx)
        txid = node.sendrawtransaction(signedtx['hex'])
        fulltx = node.getrawtransaction(txid, 1)
        # make sure we didn't generate a change output
        assert len(fulltx['vout']) == num_outputs
        return (txid, send_value)

    def run_test(self):
        # Mine some blocks and have them mature.
        self.generate(self.nodes[0], 101)
        utxo = self.nodes[0].listunspent(10)
        txid = utxo[0]['txid']
        vout = utxo[0]['vout']
        value = utxo[0]['amount']

        fee = Decimal("0.0001")
        # OLD_MAX_ANCESTORS transactions off a confirmed tx should be fine
        chain = []
        for i in range(OLD_MAX_ANCESTORS):
            (txid, sent_value) = self.chain_transaction(
                self.nodes[0], txid, 0, value, fee, 1)
            value = sent_value
            chain.append(txid)

        # Check mempool has MAX_ANCESTORS transactions in it, and descendant and ancestor
        # count and fees should look correct
        mempool = self.nodes[0].getrawmempool(True)
        assert_equal(len(mempool), OLD_MAX_ANCESTORS)

        # Check that some of the keys we expect exist
        for txdict in mempool.values():
            assert 'size' in txdict
            assert 'fees' in txdict

        descendants = []
        ancestors = list(chain)
        for x in reversed(chain):
            # Check that getmempoolentry is consistent with getrawmempool
            entry = self.nodes[0].getmempoolentry(x)
            assert_equal(entry, mempool[x])

            # Check that the descendant calculations are correct
            assert_equal(mempool[x]['fees']['modified'], mempool[x]['fees']['base'])

            # Check that parent/child list is correct
            assert_equal(mempool[x]['spentby'], descendants[-1:])
            assert_equal(mempool[x]['depends'], ancestors[-2:-1])

            # Check that getmempooldescendants is correct
            assert_equal(sorted(descendants), sorted(
                self.nodes[0].getmempooldescendants(x)))

            # Check getmempooldescendants verbose output is correct
            for descendant, dinfo in self.nodes[0].getmempooldescendants(
                    x, True).items():
                assert_equal(dinfo['depends'], [
                             chain[chain.index(descendant) - 1]])
            descendants.append(x)

            # Check that getmempoolancestors is correct
            ancestors.remove(x)
            assert_equal(sorted(ancestors), sorted(
                self.nodes[0].getmempoolancestors(x)))

            # Check that getmempoolancestors verbose output is correct
            for ancestor, ainfo in self.nodes[0].getmempoolancestors(
                    x, True).items():
                assert_equal(ainfo['spentby'], [
                             chain[chain.index(ancestor) + 1]])

        # Check that getmempoolancestors/getmempooldescendants correctly handle
        # verbose=true
        v_ancestors = self.nodes[0].getmempoolancestors(chain[-1], True)
        assert_equal(len(v_ancestors), len(chain) - 1)
        for x in v_ancestors.keys():
            assert_equal(mempool[x], v_ancestors[x])
        assert chain[-1] not in v_ancestors.keys()

        v_descendants = self.nodes[0].getmempooldescendants(chain[0], True)
        assert_equal(len(v_descendants), len(chain) - 1)
        for x in v_descendants.keys():
            assert_equal(mempool[x], v_descendants[x])
        assert chain[0] not in v_descendants.keys()

        # Post-tachyon: Adding one more transaction on to the chain no longer fails
        self.chain_transaction(self.nodes[0], txid, vout, value, fee, 1)

        # Check that prioritising a tx before it's added to the mempool works
        # First clear the mempool by mining a block.
        self.generate(self.nodes[0], 1)
        self.sync_blocks()
        assert_equal(len(self.nodes[0].getrawmempool()), 0)
        # Prioritise a transaction that has been mined, then add it back to the
        # mempool by using invalidateblock.
        self.nodes[0].prioritisetransaction(txid=chain[-1], fee_delta=2000)
        self.nodes[0].invalidateblock(self.nodes[0].getbestblockhash())
        # Keep node1's tip synced with node0
        self.nodes[1].invalidateblock(self.nodes[1].getbestblockhash())

        # Now check that the transaction is in the mempool, with the right
        # modified fee
        mempool = self.nodes[0].getrawmempool(True)

        x = chain[-1]
        assert_equal(mempool[x]['fees']['modified'],
                     mempool[x]['fees']['base'] + satoshi_round(0.00002))

        # TODO: check that node1's mempool is as expected

        # Now test descendant chain limits
        txid = utxo[1]['txid']
        value = utxo[1]['amount']
        vout = utxo[1]['vout']

        transaction_package = []
        tx_children = []
        # First create one parent tx with 10 children
        (txid, sent_value) = self.chain_transaction(
            self.nodes[0], txid, vout, value, fee, 10)
        parent_transaction = txid
        for i in range(10):
            transaction_package.append(
                {'txid': txid, 'vout': i, 'amount': sent_value})

        # Sign and send up to OLD_MAX_DESCENDANT transactions chained off the
        # parent tx
        for i in range(OLD_MAX_DESCENDANTS - 1):
            utxo = transaction_package.pop(0)
            (txid, sent_value) = self.chain_transaction(
                self.nodes[0], utxo['txid'], utxo['vout'], utxo['amount'], fee, 10)
            if utxo['txid'] is parent_transaction:
                tx_children.append(txid)
            for j in range(10):
                transaction_package.append(
                    {'txid': txid, 'vout': j, 'amount': sent_value})

        mempool = self.nodes[0].getrawmempool(True)
        assert_equal(sorted(mempool[parent_transaction]
                            ['spentby']), sorted(tx_children))

        for child in tx_children:
            assert_equal(mempool[child]['depends'], [parent_transaction])

        # Post-tachyon: Sending one more chained transaction will succeed
        utxo = transaction_package.pop(0)
        self.chain_transaction(self.nodes[0], utxo['txid'], utxo['vout'], utxo['amount'], fee, 10)

        # TODO: check that node1's mempool is as expected

        # Test reorg handling
        # First, the basics:
        self.generate(self.nodes[0], 1)
        self.sync_blocks()
        self.nodes[1].invalidateblock(self.nodes[0].getbestblockhash())
        self.nodes[1].reconsiderblock(self.nodes[0].getbestblockhash())

        # Now test the case where node1 has a transaction T in its mempool that
        # depends on transactions A and B which are in a mined block, and the
        # block containing A and B is disconnected, AND B is not accepted back
        # into node1's mempool because its ancestor count is too high.

        # Create 8 transactions, like so:
        # Tx0 -> Tx1 (vout0)
        #   \--> Tx2 (vout1) -> Tx3 -> Tx4 -> Tx5 -> Tx6 -> Tx7
        #
        # Mine them in the next block, then generate a new tx8 that spends
        # Tx1 and Tx7, and add to node1's mempool, then disconnect the
        # last block.

        # Create tx0 with 2 outputs
        utxo = self.nodes[0].listunspent()
        txid = utxo[0]['txid']
        value = utxo[0]['amount']
        vout = utxo[0]['vout']

        send_value = satoshi_round((value - fee) / 2)
        inputs = [{'txid': txid, 'vout': vout}]
        outputs = {}
        for i in range(2):
            outputs[self.nodes[0].getnewaddress()] = send_value
        rawtx = self.nodes[0].createrawtransaction(inputs, outputs)
        signedtx = self.nodes[0].signrawtransactionwithwallet(rawtx)
        txid = self.nodes[0].sendrawtransaction(signedtx['hex'])
        tx0_id = txid
        value = send_value

        # Create tx1
        tx1_id, _ = self.chain_transaction(
            self.nodes[0], tx0_id, 0, value, fee, 1)

        # Create tx2-7
        vout = 1
        txid = tx0_id
        for i in range(6):
            (txid, sent_value) = self.chain_transaction(
                self.nodes[0], txid, vout, value, fee, 1)
            vout = 0
            value = sent_value

        # Mine these in a block
        self.generate(self.nodes[0], 1)
        self.sync_all()

        # Now generate tx8, with a big fee
        inputs = [{'txid': tx1_id, 'vout': 0}, {'txid': txid, 'vout': 0}]
        outputs = {self.nodes[0].getnewaddress(): send_value + value - 4 * fee}
        rawtx = self.nodes[0].createrawtransaction(inputs, outputs)
        signedtx = self.nodes[0].signrawtransactionwithwallet(rawtx)
        txid = self.nodes[0].sendrawtransaction(signedtx['hex'])
        self.sync_mempools()

        # Now try to disconnect the tip on each node...
        self.nodes[1].invalidateblock(self.nodes[1].getbestblockhash())
        self.nodes[0].invalidateblock(self.nodes[0].getbestblockhash())
        self.sync_blocks()


if __name__ == '__main__':
    MempoolPackagesTest().main()
