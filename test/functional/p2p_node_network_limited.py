#!/usr/bin/env python3
# Copyright (c) 2017 The Fittexxcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Tests NODE_NETWORK_LIMITED.

Tests that a node configured with -prune=550 signals NODE_NETWORK_LIMITED correctly
and that it responds to getdata requests for blocks correctly:
    - send a block within 288 + 2 of the tip
    - disconnect peers who request blocks older than that."""
from test_framework.messages import (
    CInv,
    MSG_BLOCK,
    msg_getdata,
    msg_verack,
    NODE_FITTEXXCOIN_CASH,
    NODE_BLOOM,
    NODE_NETWORK_LIMITED,
)
from test_framework.p2p import (
    p2p_lock,
    P2PInterface,
)
from test_framework.test_framework import FittexxcoinTestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes_bi,
    disconnect_nodes,
    wait_until,
)


class P2PIgnoreInv(P2PInterface):
    firstAddrnServices = 0

    def on_inv(self, message):
        # The node will send us invs for other blocks. Ignore them.
        pass

    def on_addr(self, message):
        self.firstAddrnServices = message.addrs[0].nServices

    def wait_for_addr(self, timeout=5):
        def test_function(): return self.last_message.get("addr")
        wait_until(test_function, timeout=timeout, lock=p2p_lock)

    def send_getdata_for_block(self, blockhash):
        getdata_request = msg_getdata()
        getdata_request.inv.append(CInv(MSG_BLOCK, int(blockhash, 16)))
        self.send_message(getdata_request)


class NodeNetworkLimitedTest(FittexxcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 3
        self.extra_args = [['-prune=550', '-addrmantest'], [], []]

    def disconnect_all(self):
        disconnect_nodes(self.nodes[0], self.nodes[1])
        disconnect_nodes(self.nodes[1], self.nodes[0])
        disconnect_nodes(self.nodes[2], self.nodes[1])
        disconnect_nodes(self.nodes[2], self.nodes[0])
        disconnect_nodes(self.nodes[0], self.nodes[2])
        disconnect_nodes(self.nodes[1], self.nodes[2])

    def setup_network(self):
        super(NodeNetworkLimitedTest, self).setup_network()
        self.disconnect_all()

    def run_test(self):
        node = self.nodes[0].add_p2p_connection(P2PIgnoreInv())

        expected_services = NODE_BLOOM | NODE_FITTEXXCOIN_CASH | NODE_NETWORK_LIMITED

        self.log.info("Check that node has signalled expected services.")
        assert_equal(node.nServices, expected_services)

        self.log.info("Check that the localservices is as expected.")
        assert_equal(int(self.nodes[0].getnetworkinfo()[
                     'localservices'], 16), expected_services)

        self.log.info(
            "Mine enough blocks to reach the NODE_NETWORK_LIMITED range.")
        connect_nodes_bi(self.nodes[0], self.nodes[1])
        blocks = self.generatetoaddress(self.nodes[1],
                                        292, self.nodes[1].get_deterministic_priv_key().address)
        self.sync_blocks([self.nodes[0], self.nodes[1]])

        self.log.info("Make sure we can max retrieve block at tip-288.")
        # last block in valid range
        node.send_getdata_for_block(blocks[1])
        node.wait_for_block(int(blocks[1], 16), timeout=3)

        self.log.info(
            "Requesting block at height 2 (tip-289) must fail (ignored).")
        # first block outside of the 288+2 limit
        node.send_getdata_for_block(blocks[0])
        node.wait_for_disconnect(5)

        self.log.info("Check local address relay, do a fresh connection.")
        self.nodes[0].disconnect_p2ps()
        node1 = self.nodes[0].add_p2p_connection(P2PIgnoreInv())
        node1.send_message(msg_verack())

        node1.wait_for_addr()
        # must relay address with NODE_NETWORK_LIMITED
        assert_equal(node1.firstAddrnServices, expected_services)

        self.nodes[0].disconnect_p2ps()
        node1.wait_for_disconnect()

        # connect unsynced node 2 with pruned NODE_NETWORK_LIMITED peer
        # because node 2 is in IBD and node 0 is a NODE_NETWORK_LIMITED peer,
        # sync must not be possible
        connect_nodes_bi(self.nodes[0], self.nodes[2])
        try:
            self.sync_blocks([self.nodes[0], self.nodes[2]], timeout=5)
        except Exception:
            pass
        # node2 must remain at heigh 0
        assert_equal(self.nodes[2].getblockheader(
            self.nodes[2].getbestblockhash())['height'], 0)

        # now connect also to node 1 (non pruned)
        connect_nodes_bi(self.nodes[1], self.nodes[2])

        # sync must be possible
        self.sync_blocks()

        # disconnect all peers
        self.disconnect_all()

        # mine 10 blocks on node 0 (pruned node)
        self.generatetoaddress(self.nodes[0],
                               10, self.nodes[0].get_deterministic_priv_key().address)

        # connect node1 (non pruned) with node0 (pruned) and check if the can
        # sync
        connect_nodes_bi(self.nodes[0], self.nodes[1])

        # sync must be possible, node 1 is no longer in IBD and should
        # therefore connect to node 0 (NODE_NETWORK_LIMITED)
        self.sync_blocks([self.nodes[0], self.nodes[1]])


if __name__ == '__main__':
    NodeNetworkLimitedTest().main()
