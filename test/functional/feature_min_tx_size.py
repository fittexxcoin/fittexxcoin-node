#!/usr/bin/env python3
# Copyright (c) 2022 The Fittexxcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test enforcement of minimum tx size of 100 before Upgrade9 and of 65 after Upgrade9.."""
import time
from typing import Optional

from test_framework.blocktools import (
    create_block,
    create_coinbase,
    create_tx_with_script,
)
from test_framework.cdefs import (
    MIN_TX_SIZE_MAGNETIC_ANOMALY,
    MIN_TX_SIZE_UPGRADE9,
)
from test_framework.messages import (
    CBlock,
    CTransaction,
    FromHex,
)
from test_framework.p2p import P2PDataStore
from test_framework.script import (
    CScript,
    OP_NOP,
    OP_TRUE,
)
from test_framework.test_framework import FittexxcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_greater_than_or_equal,
)


class MinTxSizeTest(FittexxcoinTestFramework):

    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        # This is a consensus block test, we don't care about tx policy
        self.base_extra_args = ['-acceptnonstdtxn=1', '-expire=0', '-whitelist=127.0.0.1']
        self.extra_args = [['-upgrade9activationtime=999999999999'] + self.base_extra_args]

    def run_test(self):
        # Basic sanity check that the two sizes are not equal and 1 is greater than the other
        assert_greater_than(MIN_TX_SIZE_MAGNETIC_ANOMALY, MIN_TX_SIZE_UPGRADE9)

        node = self.nodes[0]  # convenience reference to the node

        self.block_heights = dict()

        self.bootstrap_p2p()  # Add one p2p connection to the node

        genesis = FromHex(CBlock(), node.getblock(node.getbestblockhash(), 0))

        # Create a new block
        b1 = self.create_block(genesis, min_tx_size_coinbase=MIN_TX_SIZE_MAGNETIC_ANOMALY, nTime=int(time.time()))
        self.send_blocks([b1])

        # Allow the block to mature, and then some
        blocks = [b1]
        for i in range(120):
            blocks.append(self.create_block(blocks[-1], min_tx_size_coinbase=MIN_TX_SIZE_MAGNETIC_ANOMALY,
                                            min_tx_size=MIN_TX_SIZE_MAGNETIC_ANOMALY))
        self.send_blocks(blocks[1:])

        for block in blocks:
            for txn in block.vtx:
                # Ensure all the txns are == 100 bytes
                assert_equal(len(txn.serialize()), MIN_TX_SIZE_MAGNETIC_ANOMALY)

        spend_index = 0

        # Test: Undersized coinbase, regular sized spend tx (Magnetic Anomaly, 100-bytes min size)
        self.log.info("Create a block where the coinbase is undersized (99 bytes)")
        undersized = self.create_block(blocks[-1], spend=blocks[spend_index].vtx[0],
                                       min_tx_size_coinbase=MIN_TX_SIZE_MAGNETIC_ANOMALY - 1,
                                       min_tx_size=MIN_TX_SIZE_MAGNETIC_ANOMALY)
        self.log.info("Coinbase is 99 bytes, non-coinbase is 100 bytes")
        assert_equal(len(undersized.vtx[0].serialize()), MIN_TX_SIZE_MAGNETIC_ANOMALY - 1)
        assert_equal(len(undersized.vtx[1].serialize()), MIN_TX_SIZE_MAGNETIC_ANOMALY)
        self.log.info("This block should fail to be accepted with bad-txns-undersize")
        self.send_blocks([undersized], success=False, reject_reason="bad-txns-undersize")

        # Test: Regular-sized coinbase, undersized spend tx (Magnetic Anomaly, 100-bytes min size)
        self.log.info("Create a block where the non-coinbase is undersized (99 bytes)")
        undersized = self.create_block(blocks[-1], spend=blocks[spend_index].vtx[0],
                                       min_tx_size_coinbase=MIN_TX_SIZE_MAGNETIC_ANOMALY,
                                       min_tx_size=MIN_TX_SIZE_MAGNETIC_ANOMALY - 1)
        self.log.info("Coinbase is 99 bytes, non-coinbase is 100 bytes")
        assert_equal(len(undersized.vtx[0].serialize()), MIN_TX_SIZE_MAGNETIC_ANOMALY)
        assert_equal(len(undersized.vtx[1].serialize()), MIN_TX_SIZE_MAGNETIC_ANOMALY - 1)
        self.log.info("This block should fail to be accepted with bad-txns-undersize")
        self.send_blocks([undersized], success=False, reject_reason="bad-txns-undersize")

        activation_time = blocks[-1].nTime
        # Restart the node, enabling upgrade9
        self.restart_node(0, extra_args=[f"-upgrade9activationtime={activation_time}"] + self.base_extra_args)
        self.reconnect_p2p()

        self.log.info("Advance blockchain forward to enable upgrade9")
        iters = 0
        est_median_time = node.getblockchaininfo()["mediantime"]
        while est_median_time < activation_time:
            iters += 1
            est_median_time += 1  # create_block below just advances mediantime by 1 second each time
            blocks.append(self.create_block(blocks[-1], spend=blocks[spend_index].vtx[0],
                                            min_tx_size_coinbase=MIN_TX_SIZE_MAGNETIC_ANOMALY,
                                            min_tx_size=MIN_TX_SIZE_MAGNETIC_ANOMALY))
            assert_equal(len(blocks[-1].vtx[0].serialize()), MIN_TX_SIZE_MAGNETIC_ANOMALY)
            assert_equal(len(blocks[-1].vtx[1].serialize()), MIN_TX_SIZE_MAGNETIC_ANOMALY)
            spend_index += 1
        assert iters > 0
        self.send_blocks(blocks[-iters:])
        # Paranoia: Ensure node accepted above chain
        assert_equal(node.getbestblockhash(), blocks[-1].sha256.to_bytes(length=32, byteorder="big").hex())
        # Ensure upgrade9 activated
        assert_greater_than_or_equal(node.getblockchaininfo()["mediantime"], activation_time)
        self.log.info(f"Iterated {iters} times to bring mediantime ({node.getblockchaininfo()['mediantime']}) up to "
                      f"activation_time ({activation_time}) -- Upgrade9 is now activated for the next block!")

        self.log.info("Ensure smaller tx size (65 bytes) works ok")
        b2 = self.create_block(blocks[-1], spend=blocks[spend_index].vtx[0],
                               min_tx_size_coinbase=MIN_TX_SIZE_UPGRADE9,
                               min_tx_size=MIN_TX_SIZE_UPGRADE9)
        assert_equal(len(b2.vtx[0].serialize()), MIN_TX_SIZE_UPGRADE9)
        assert_equal(len(b2.vtx[1].serialize()), MIN_TX_SIZE_UPGRADE9)

        self.send_blocks([b2])
        blocks.append(b2)
        spend_index += 1

        # Test: Undersized coinbase, regular sized spend tx (Upgrade9, 65-bytes min size)
        self.log.info("Try with undersized coinbase (64 bytes)")
        undersized = self.create_block(blocks[-1], spend=blocks[spend_index].vtx[0],
                                       min_tx_size_coinbase=MIN_TX_SIZE_UPGRADE9 - 1,
                                       min_tx_size=MIN_TX_SIZE_UPGRADE9)
        self.log.info("Coinbase is 64 bytes, non-coinbase tx is 65 bytes")
        assert_equal(len(undersized.vtx[0].serialize()), MIN_TX_SIZE_UPGRADE9 - 1)
        assert_equal(len(undersized.vtx[1].serialize()), MIN_TX_SIZE_UPGRADE9)
        self.log.info("This block should fail to be accepted with bad-txns-undersize")
        self.send_blocks([undersized], success=False, reject_reason="bad-txns-undersize")

        # Test: Regular sized coinbase, undersized spend tx (Upgrade9, 65-bytes min size)
        self.log.info("Try with undersized non-coinbase (64 bytes)")
        undersized = self.create_block(blocks[-1], spend=blocks[spend_index].vtx[0],
                                       min_tx_size_coinbase=MIN_TX_SIZE_UPGRADE9,
                                       min_tx_size=MIN_TX_SIZE_UPGRADE9 - 1)
        self.log.info("Coinbase is 65 bytes, spend tx is 64 bytes")
        assert_equal(len(undersized.vtx[0].serialize()), MIN_TX_SIZE_UPGRADE9)
        assert_equal(len(undersized.vtx[1].serialize()), MIN_TX_SIZE_UPGRADE9 - 1)
        self.log.info("This block should fail to be accepted with bad-txns-undersize")
        self.send_blocks([undersized], success=False, reject_reason="bad-txns-undersize")

        self.log.info("Try with undersized non-coinbase (63 bytes)")
        undersized = self.create_block(blocks[-1], spend=blocks[spend_index].vtx[0],
                                       min_tx_size_coinbase=MIN_TX_SIZE_UPGRADE9,
                                       min_tx_size=MIN_TX_SIZE_UPGRADE9 - 2)
        self.log.info("Coinbase is 63 bytes, spend tx is 65 bytes")
        assert_equal(len(undersized.vtx[0].serialize()), MIN_TX_SIZE_UPGRADE9)
        assert_equal(len(undersized.vtx[1].serialize()), MIN_TX_SIZE_UPGRADE9 - 2)
        self.log.info("This block should fail to be accepted with bad-txns-undersize")
        self.send_blocks([undersized], success=False, reject_reason="bad-txns-undersize")

        # Paranoia: Ensure blockchain didn't change with the above two bad blocks being sent
        assert_equal(node.getbestblockhash(), blocks[-1].sha256.to_bytes(length=32, byteorder="big").hex())

        # More paranoia: Ensure we can send 120 byte txns in a block
        blocks.append(self.create_block(blocks[-1], spend=blocks[spend_index].vtx[0],
                                        min_tx_size_coinbase=MIN_TX_SIZE_MAGNETIC_ANOMALY + 20,
                                        min_tx_size=MIN_TX_SIZE_MAGNETIC_ANOMALY + 20))
        assert_equal(len(blocks[-1].vtx[0].serialize()), MIN_TX_SIZE_MAGNETIC_ANOMALY + 20)
        assert_equal(len(blocks[-1].vtx[1].serialize()), MIN_TX_SIZE_MAGNETIC_ANOMALY + 20)
        self.send_blocks(blocks[-1:])
        spend_index += 1
        assert_equal(node.getbestblockhash(), blocks[-1].sha256.to_bytes(length=32, byteorder="big").hex())

        # More paranoia: Ensure we can send 99 byte txns in a block
        blocks.append(self.create_block(blocks[-1], spend=blocks[spend_index].vtx[0],
                                        min_tx_size_coinbase=MIN_TX_SIZE_MAGNETIC_ANOMALY - 1,
                                        min_tx_size=MIN_TX_SIZE_MAGNETIC_ANOMALY - 1))
        assert_equal(len(blocks[-1].vtx[0].serialize()), MIN_TX_SIZE_MAGNETIC_ANOMALY - 1)
        assert_equal(len(blocks[-1].vtx[1].serialize()), MIN_TX_SIZE_MAGNETIC_ANOMALY - 1)
        self.send_blocks(blocks[-1:])
        spend_index += 1
        assert_equal(node.getbestblockhash(), blocks[-1].sha256.to_bytes(length=32, byteorder="big").hex())

    def create_tx(self, spend_tx, n, value, min_tx_size=MIN_TX_SIZE_MAGNETIC_ANOMALY):
        """this is a little handier to use than the version in blocktools.py"""
        # Note that pad_to_size is very inaccurate under size 100 :(
        tx = create_tx_with_script(spend_tx, n, amount=value, script_pub_key=CScript([OP_TRUE]),
                                   pad_to_size=0)  # don't pad_to_size since it is unable to do it precisely
        tx.calc_sha256()
        size = len(tx.serialize())
        needed = max(0, min_tx_size - size)
        if needed:
            # trivial "signature"; pad_to_size is imprecise, we do it manually by just appending to
            # our OP_TRUE scriptSig
            self.log.info(f"Appending {needed} bytes to txn of size {size} to get it to {min_tx_size} bytes ...")
            tx.vout[0].scriptPubKey = CScript([OP_TRUE] + [OP_NOP] * needed)
        tx.rehash()
        return tx

    def create_block(self, prev_block: CBlock, *, spend: Optional[CTransaction] = None,
                     min_tx_size: int = MIN_TX_SIZE_MAGNETIC_ANOMALY,
                     min_tx_size_coinbase: int = MIN_TX_SIZE_MAGNETIC_ANOMALY,
                     nTime: int = None) -> CBlock:
        if prev_block.sha256 is None:
            prev_block.rehash()
        assert prev_block.sha256 is not None  # Satisfy linter with this assertion
        prev_block_hash = prev_block.sha256
        block_time = prev_block.nTime + 1 if nTime is None else nTime
        # First create the coinbase
        height = self.block_heights.get(prev_block_hash, 0) + 1
        coinbase = create_coinbase(height, pad_to_size=0)  # pad_to_size is buggy, we do it manually
        size = len(coinbase.serialize())
        needed = max(0, min_tx_size_coinbase - size)
        self.log.info(f"Appending {needed} bytes to coinbase of size {size} to get it to {min_tx_size_coinbase} "
                      f"bytes ...")
        coinbase.vin[0].scriptSig += CScript(b'.' * needed)
        assert len(coinbase.vin[0].scriptSig) < 100
        coinbase.rehash()

        if spend is not None:
            # all but one satoshi to fees
            coinbase.vout[0].nValue += spend.vout[0].nValue - 1
            coinbase.rehash()
            # spend 1 satoshi
            tx = self.create_tx(spend_tx=spend, n=0, value=1, min_tx_size=min_tx_size)
            tx.rehash()
            txns = [tx]
        else:
            txns = []

        block = create_block(prev_block_hash, coinbase, block_time, txns=txns)
        block.solve()
        self.block_heights[block.sha256] = height
        return block

    def bootstrap_p2p(self):
        """Add a P2P connection to the node.

        Helper to connect and wait for version handshake."""
        self.nodes[0].add_p2p_connection(P2PDataStore())
        # We need to wait for the initial getheaders from the peer before we
        # start populating our blockstore. If we don't, then we may run ahead
        # to the next subtest before we receive the getheaders. We'd then send
        # an INV for the next block and receive two getheaders - one for the
        # IBD and one for the INV. We'd respond to both and could get
        # unexpectedly disconnected if the DoS score for that error is 50.
        self.nodes[0].p2p.wait_for_getheaders(timeout=5)

    def reconnect_p2p(self):
        """Tear down and bootstrap the P2P connection to the node.

        The node gets disconnected several times in this test. This helper
        method reconnects the p2p and restarts the network thread."""
        bs, lbh, p2p = None, None, None
        if self.nodes[0].p2ps:
            p2p = self.nodes[0].p2p
            bs, lbh = p2p.block_store, p2p.last_block_hash
        self.nodes[0].disconnect_p2ps()
        self.bootstrap_p2p()
        if p2p and (bs or lbh):
            # Set up the block store again so that p2p node can adequately send headers again for everything
            # node might want after a restart
            p2p = self.nodes[0].p2p
            p2p.block_store, p2p.last_block_hash = bs, lbh

    def send_blocks(self, blocks, success=True, reject_reason=None,
                    request_block=True, reconnect=False, timeout=60):
        """Sends blocks to test node. Syncs and verifies that tip has advanced to most recent block.

        Call with success = False if the tip shouldn't advance to the most recent block."""
        self.nodes[0].p2p.send_blocks_and_test(blocks, self.nodes[0], success=success,
                                               reject_reason=reject_reason, request_block=request_block,
                                               timeout=timeout, expect_disconnect=reconnect)
        if reconnect:
            self.reconnect_p2p()


if __name__ == '__main__':
    MinTxSizeTest().main()
