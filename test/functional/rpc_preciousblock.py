#!/usr/bin/env python3
# Copyright (c) 2015-2019 The Fittexxcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the preciousblock RPC."""

from test_framework.test_framework import FittexxcoinTestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes_bi,
)


def unidirectional_node_sync_via_rpc(node_src, node_dest):
    blocks_to_copy = []
    blockhash = node_src.getbestblockhash()
    while True:
        try:
            assert len(node_dest.getblock(blockhash, False)) > 0
            break
        except Exception:
            blocks_to_copy.append(blockhash)
            blockhash = node_src.getblockheader(
                blockhash, True)['previousblockhash']
    blocks_to_copy.reverse()
    for blockhash in blocks_to_copy:
        blockdata = node_src.getblock(blockhash, False)
        assert node_dest.submitblock(blockdata) in (None, 'inconclusive')


def node_sync_via_rpc(nodes):
    for node_src in nodes:
        for node_dest in nodes:
            if node_src is node_dest:
                continue
            unidirectional_node_sync_via_rpc(node_src, node_dest)


class PreciousTest(FittexxcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 3
        self.extra_args = [["-noparkdeepreorg"],
                           ["-noparkdeepreorg"], ["-noparkdeepreorg"]]

    def setup_network(self):
        self.setup_nodes()

    def run_test(self):
        self.log.info(
            "Ensure submitblock can in principle reorg to a competing chain")
        # A non-wallet address to mine to

        def gen_address(
            i): return self.nodes[i].get_deterministic_priv_key().address
        self.generatetoaddress(self.nodes[0], 1, gen_address(0))
        assert_equal(self.nodes[0].getblockcount(), 1)
        hashZ = self.generatetoaddress(self.nodes[1], 2, gen_address(1))[-1]
        assert_equal(self.nodes[1].getblockcount(), 2)
        node_sync_via_rpc(self.nodes[0:3])
        assert_equal(self.nodes[0].getbestblockhash(), hashZ)

        self.log.info("Mine blocks A-B-C on Node 0")
        hashC = self.generatetoaddress(self.nodes[0], 3, gen_address(0))[-1]
        assert_equal(self.nodes[0].getblockcount(), 5)
        self.log.info("Mine competing blocks E-F-G on Node 1")
        hashG = self.generatetoaddress(self.nodes[1], 3, gen_address(1))[-1]
        assert_equal(self.nodes[1].getblockcount(), 5)
        assert hashC != hashG
        self.log.info("Connect nodes and check no reorg occurs")
        # Submit competing blocks via RPC so any reorg should occur before we
        # proceed (no way to wait on inaction for p2p sync)
        node_sync_via_rpc(self.nodes[0:2])
        connect_nodes_bi(self.nodes[0], self.nodes[1])
        assert_equal(self.nodes[0].getbestblockhash(), hashC)
        assert_equal(self.nodes[1].getbestblockhash(), hashG)
        self.log.info("Make Node0 prefer block G")
        self.nodes[0].preciousblock(hashG)
        assert_equal(self.nodes[0].getbestblockhash(), hashG)
        self.log.info("Make Node0 prefer block C again")
        self.nodes[0].preciousblock(hashC)
        assert_equal(self.nodes[0].getbestblockhash(), hashC)
        self.log.info("Make Node1 prefer block C")
        self.nodes[1].preciousblock(hashC)
        # wait because node 1 may not have downloaded hashC
        self.sync_blocks(self.nodes[0:2])
        assert_equal(self.nodes[1].getbestblockhash(), hashC)
        self.log.info("Make Node1 prefer block G again")
        self.nodes[1].preciousblock(hashG)
        assert_equal(self.nodes[1].getbestblockhash(), hashG)
        self.log.info("Make Node0 prefer block G again")
        self.nodes[0].preciousblock(hashG)
        assert_equal(self.nodes[0].getbestblockhash(), hashG)
        self.log.info("Make Node1 prefer block C again")
        self.nodes[1].preciousblock(hashC)
        assert_equal(self.nodes[1].getbestblockhash(), hashC)
        self.log.info(
            "Mine another block (E-F-G-)H on Node 0 and reorg Node 1")
        self.generatetoaddress(self.nodes[0], 1, gen_address(0))
        assert_equal(self.nodes[0].getblockcount(), 6)
        self.sync_blocks(self.nodes[0:2])
        hashH = self.nodes[0].getbestblockhash()
        assert_equal(self.nodes[1].getbestblockhash(), hashH)
        self.log.info("Node1 should not be able to prefer block C anymore")
        self.nodes[1].preciousblock(hashC)
        assert_equal(self.nodes[1].getbestblockhash(), hashH)
        self.log.info("Mine competing blocks I-J-K-L on Node 2")
        self.generatetoaddress(self.nodes[2], 4, gen_address(2))
        assert_equal(self.nodes[2].getblockcount(), 6)
        hashL = self.nodes[2].getbestblockhash()
        self.log.info("Connect nodes and check no reorg occurs")
        node_sync_via_rpc(self.nodes[1:3])
        connect_nodes_bi(self.nodes[1], self.nodes[2])
        connect_nodes_bi(self.nodes[0], self.nodes[2])
        assert_equal(self.nodes[0].getbestblockhash(), hashH)
        assert_equal(self.nodes[1].getbestblockhash(), hashH)
        assert_equal(self.nodes[2].getbestblockhash(), hashL)
        self.log.info("Make Node1 prefer block L")
        self.nodes[1].preciousblock(hashL)
        assert_equal(self.nodes[1].getbestblockhash(), hashL)
        self.log.info("Make Node2 prefer block H")
        self.nodes[2].preciousblock(hashH)
        assert_equal(self.nodes[2].getbestblockhash(), hashH)


if __name__ == '__main__':
    PreciousTest().main()
