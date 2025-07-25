#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Fittexxcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test node responses to invalid transactions.

In this test we connect to one node over p2p, and test tx requests.
"""

from test_framework.blocktools import (
    create_block,
    create_coinbase,
    create_tx_with_script,
)
from test_framework.txtools import pad_tx
from test_framework.messages import (
    COIN,
    COutPoint,
    CTransaction,
    CTxIn,
    CTxOut,
)
from test_framework.p2p import P2PDataStore
from test_framework.test_framework import FittexxcoinTestFramework
from test_framework.util import (
    assert_equal,
    wait_until,
)
from test_framework.script import (
    CScript,
    MAX_SCRIPT_SIZE,
    OP_ENDIF,
    OP_FALSE,
    OP_IF,
    OP_RESERVED,
    OP_RETURN,
    OP_TRUE,
)


class InvalidTxRequestTest(FittexxcoinTestFramework):

    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [
            ["-acceptnonstdtxn=1", ]
        ]
        self.setup_clean_chain = True

    def bootstrap_p2p(self, *, num_connections=1):
        """Add a P2P connection to the node.

        Helper to connect and wait for version handshake."""
        for _ in range(num_connections):
            self.nodes[0].add_p2p_connection(P2PDataStore())

    def reconnect_p2p(self, **kwargs):
        """Tear down and bootstrap the P2P connection to the node.

        The node gets disconnected several times in this test. This helper
        method reconnects the p2p and restarts the network thread."""
        self.nodes[0].disconnect_p2ps()
        self.bootstrap_p2p(**kwargs)

    def run_test(self):
        node = self.nodes[0]  # convenience reference to the node

        self.bootstrap_p2p()  # Add one p2p connection to the node

        best_block = self.nodes[0].getbestblockhash()
        tip = int(best_block, 16)
        best_block_time = self.nodes[0].getblock(best_block)['time']
        block_time = best_block_time + 1

        self.log.info("Create a new block with an anyone-can-spend coinbase.")
        height = 1
        block = create_block(tip, create_coinbase(height), block_time)
        block.solve()
        # Save the coinbase for later
        block1 = block
        tip = block.sha256
        node.p2p.send_blocks_and_test([block], node, success=True)

        self.log.info("Mature the block.")
        self.generatetoaddress(self.nodes[0],
                               100, self.nodes[0].get_deterministic_priv_key().address)

        # b'\x64' is OP_NOTIF
        # Transaction will be rejected with code 16 (REJECT_INVALID)
        # and we get disconnected immediately
        self.log.info('Test a transaction that is rejected')
        tx1 = create_tx_with_script(
            block1.vtx[0], 0, script_sig=b'\x64' * 35, amount=50 * COIN - 12000)
        node.p2p.send_txs_and_test(
            [tx1], node, success=False, expect_disconnect=True)

        # Make two p2p connections to provide the node with orphans
        # * p2ps[0] will send valid orphan txs (one with low fee)
        # * p2ps[1] will send an invalid orphan tx (and is later disconnected for that)
        self.reconnect_p2p(num_connections=2)

        self.log.info('Test orphan transaction handling ... ')
        # Create a root transaction that we withold until all dependend transactions
        # are sent out and in the orphan cache
        SCRIPT_PUB_KEY_OP_TRUE = CScript([OP_TRUE])
        tx_withhold = CTransaction()
        tx_withhold.vin.append(
            CTxIn(outpoint=COutPoint(block1.vtx[0].sha256, 0)))
        tx_withhold.vout.append(
            CTxOut(nValue=50 * COIN - 12000, scriptPubKey=SCRIPT_PUB_KEY_OP_TRUE))
        pad_tx(tx_withhold)
        tx_withhold.calc_sha256()

        # Our first orphan tx with some outputs to create further orphan txs
        tx_orphan_1 = CTransaction()
        tx_orphan_1.vin.append(
            CTxIn(outpoint=COutPoint(tx_withhold.sha256, 0)))
        tx_orphan_1.vout = [CTxOut(nValue=10 * COIN, scriptPubKey=SCRIPT_PUB_KEY_OP_TRUE)] * 3
        pad_tx(tx_orphan_1)
        tx_orphan_1.calc_sha256()

        # A valid transaction with low fee
        tx_orphan_2_no_fee = CTransaction()
        tx_orphan_2_no_fee.vin.append(
            CTxIn(outpoint=COutPoint(tx_orphan_1.sha256, 0)))
        tx_orphan_2_no_fee.vout.append(
            CTxOut(nValue=10 * COIN, scriptPubKey=SCRIPT_PUB_KEY_OP_TRUE))
        pad_tx(tx_orphan_2_no_fee)

        # A valid transaction with sufficient fee
        tx_orphan_2_valid = CTransaction()
        tx_orphan_2_valid.vin.append(
            CTxIn(outpoint=COutPoint(tx_orphan_1.sha256, 1)))
        tx_orphan_2_valid.vout.append(
            CTxOut(nValue=10 * COIN - 12000, scriptPubKey=SCRIPT_PUB_KEY_OP_TRUE))
        tx_orphan_2_valid.calc_sha256()
        pad_tx(tx_orphan_2_valid)

        # An invalid transaction with negative fee
        tx_orphan_2_invalid = CTransaction()
        tx_orphan_2_invalid.vin.append(
            CTxIn(outpoint=COutPoint(tx_orphan_1.sha256, 2)))
        tx_orphan_2_invalid.vout.append(
            CTxOut(nValue=11 * COIN, scriptPubKey=SCRIPT_PUB_KEY_OP_TRUE))
        tx_orphan_2_invalid.calc_sha256()
        pad_tx(tx_orphan_2_invalid)

        self.log.info('Send the orphans ... ')
        # Send valid orphan txs from p2ps[0]
        node.p2p.send_txs_and_test(
            [tx_orphan_1, tx_orphan_2_no_fee, tx_orphan_2_valid], node, success=False)
        # Send invalid tx from p2ps[1]
        node.p2ps[1].send_txs_and_test(
            [tx_orphan_2_invalid], node, success=False)

        # Mempool should be empty
        assert_equal(0, node.getmempoolinfo()['size'])
        # p2ps[1] is still connected
        assert_equal(2, len(node.getpeerinfo()))

        self.log.info('Send the withhold tx ... ')
        with node.assert_debug_log(expected_msgs=["bad-txns-in-belowout"]):
            node.p2p.send_txs_and_test([tx_withhold], node, success=True)

        # Transactions that should end up in the mempool
        expected_mempool = {
            t.hash
            for t in [
                tx_withhold,  # The transaction that is the root for all orphans
                tx_orphan_1,  # The orphan transaction that splits the coins
                # The valid transaction (with sufficient fee)
                tx_orphan_2_valid,
            ]
        }
        # Transactions that do not end up in the mempool
        # tx_orphan_no_fee, because it has too low fee (p2ps[0] is not disconnected for relaying that tx)
        # tx_orphan_invaid, because it has negative fee (p2ps[1] is
        # disconnected for relaying that tx)

        # p2ps[1] is no longer connected
        wait_until(lambda: 1 == len(node.getpeerinfo()), timeout=12)
        assert_equal(expected_mempool, set(node.getrawmempool()))

        self.log.info('Test orphan pool overflow')
        orphan_tx_pool = [CTransaction() for _ in range(101)]
        for i in range(len(orphan_tx_pool)):
            orphan_tx_pool[i].vin.append(CTxIn(outpoint=COutPoint(i, 333)))
            orphan_tx_pool[i].vout.append(CTxOut(nValue=11 * COIN, scriptPubKey=SCRIPT_PUB_KEY_OP_TRUE))
            pad_tx(orphan_tx_pool[i])

        with node.assert_debug_log(['mapOrphan overflow, removed 1 tx']):
            node.p2p.send_txs_and_test(orphan_tx_pool, node, success=False)

        rejected_parent = CTransaction()
        rejected_parent.vin.append(CTxIn(outpoint=COutPoint(tx_orphan_2_invalid.sha256, 0)))
        rejected_parent.vout.append(CTxOut(nValue=11 * COIN, scriptPubKey=SCRIPT_PUB_KEY_OP_TRUE))
        pad_tx(rejected_parent)
        rejected_parent.rehash()
        with node.assert_debug_log(['not keeping orphan with rejected parents {}'.format(rejected_parent.hash)]):
            node.p2p.send_txs_and_test([rejected_parent], node, success=False)

        # Restart the node with -persistmempool=0 to clear the mempool, and create a txn that spends an oversized output
        self.log.info('Test spending of oversized outputs fails')
        self.restart_node(0, self.extra_args[0] + ['-persistmempool=0'])
        self.reconnect_p2p(num_connections=1)
        # We can create an oversized output
        spk_oversized = CScript([OP_TRUE, OP_FALSE, OP_IF] + ([OP_RESERVED] * MAX_SCRIPT_SIZE) + [OP_ENDIF])
        tx_oversized_output = create_tx_with_script(block1.vtx[0], 0, script_sig=b'', amount=50 * COIN - 12000,
                                                    script_pub_key=spk_oversized)
        node.p2p.send_txs_and_test([tx_oversized_output], node, success=True)
        assert_equal({tx_oversized_output.hash}, set(node.getrawmempool()))
        tx_spend_oversized_output = create_tx_with_script(tx_oversized_output, 0, script_sig=b'',
                                                          amount=tx_oversized_output.vout[0].nValue - 12000,
                                                          script_pub_key=CScript([OP_TRUE]))
        # But we cannot spend it
        node.p2p.send_txs_and_test([tx_spend_oversized_output], node, success=False, expect_disconnect=True,
                                   reject_reason="bad-txns-input-scriptpubkey-unspendable")
        assert_equal({tx_oversized_output.hash}, set(node.getrawmempool()))

        # Restart the node with -persistmempool=0 to clear the mempool, and create a txn that spends an OP_RETURN output
        self.log.info('Test spending of OP_RETURN outputs fails')
        self.restart_node(0, self.extra_args[0] + ['-persistmempool=0'])
        self.reconnect_p2p(num_connections=1)
        # We can create an OP_RETURN utxo
        spk_opreturn = CScript([OP_RETURN] + ([OP_RESERVED] * 10))
        tx_opreturn_output = create_tx_with_script(block1.vtx[0], 0, script_sig=b'', amount=50 * COIN - 12000,
                                                   script_pub_key=spk_opreturn)
        node.p2p.send_txs_and_test([tx_opreturn_output], node, success=True)
        assert_equal({tx_opreturn_output.hash}, set(node.getrawmempool()))
        tx_spend_opreturn_output = create_tx_with_script(tx_opreturn_output, 0, script_sig=b'',
                                                         amount=tx_opreturn_output.vout[0].nValue - 12000,
                                                         script_pub_key=CScript([OP_TRUE]))
        # But we cannot spend it
        node.p2p.send_txs_and_test([tx_spend_opreturn_output], node, success=False, expect_disconnect=True,
                                   reject_reason="bad-txns-input-scriptpubkey-unspendable")
        assert_equal({tx_opreturn_output.hash}, set(node.getrawmempool()))

        # restart node with sending BIP61 messages disabled, check that it
        # disconnects without sending the reject message
        self.log.info(
            'Test a transaction that is rejected, with BIP61 disabled')
        self.restart_node(
            0, self.extra_args[0] + ['-enablebip61=0', '-persistmempool=0'])
        self.reconnect_p2p(num_connections=1)
        node.p2p.send_txs_and_test(
            [tx1], node, success=False, reject_reason="{} from peer=0 was not accepted: mandatory-script-verify-flag-failed (Invalid OP_IF construction) (code 16)".format(tx1.hash), expect_disconnect=True)


if __name__ == '__main__':
    InvalidTxRequestTest().main()
