#!/usr/bin/env python3
# Copyright (c) 2017-2019 The Fittexxcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test message sending before handshake completion.

A node should never send anything other than VERSION/VERACK/REJECT until it's
received a VERACK.

This test connects to a node and sends it a few messages, trying to entice it
into sending us something it shouldn't.
"""

import time

from test_framework.messages import (
    msg_getaddr,
    msg_ping,
    msg_verack,
    msg_version,
)
from test_framework.p2p import (
    p2p_lock,
    P2PInterface,
)
from test_framework.test_framework import FittexxcoinTestFramework
from test_framework.util import wait_until

banscore = 10


class CLazyNode(P2PInterface):
    def __init__(self):
        super().__init__()
        self.unexpected_msg = False
        self.ever_connected = False

    def bad_message(self, message):
        self.unexpected_msg = True
        self.log.info(
            "should not have received message: {}".format(message.msgtype))

    def on_open(self):
        self.ever_connected = True

    def on_version(self, message): self.bad_message(message)

    def on_verack(self, message): self.bad_message(message)

    def on_reject(self, message): self.bad_message(message)

    def on_inv(self, message): self.bad_message(message)

    def on_addr(self, message): self.bad_message(message)

    def on_getdata(self, message): self.bad_message(message)

    def on_getblocks(self, message): self.bad_message(message)

    def on_tx(self, message): self.bad_message(message)

    def on_block(self, message): self.bad_message(message)

    def on_getaddr(self, message): self.bad_message(message)

    def on_headers(self, message): self.bad_message(message)

    def on_getheaders(self, message): self.bad_message(message)

    def on_ping(self, message): self.bad_message(message)

    def on_mempool(self, message): self.bad_message(message)

    def on_pong(self, message): self.bad_message(message)

    def on_feefilter(self, message): self.bad_message(message)

    def on_sendheaders(self, message): self.bad_message(message)

    def on_sendcmpct(self, message): self.bad_message(message)

    def on_cmpctblock(self, message): self.bad_message(message)

    def on_getblocktxn(self, message): self.bad_message(message)

    def on_blocktxn(self, message): self.bad_message(message)

# Node that never sends a version. We'll use this to send a bunch of messages
# anyway, and eventually get disconnected.


class CNodeNoVersionBan(CLazyNode):
    # send a bunch of veracks without sending a message. This should get us disconnected.
    # NOTE: implementation-specific check here. Remove if fittexxcoind ban
    # behavior changes
    def on_open(self):
        super().on_open()
        for i in range(banscore):
            self.send_message(msg_verack())

    def on_reject(self, message): pass

# Node that never sends a version. This one just sits idle and hopes to receive
# any message (it shouldn't!)


class CNodeNoVersionIdle(CLazyNode):
    def __init__(self):
        super().__init__()

# Node that sends a version but not a verack.


class CNodeNoVerackIdle(CLazyNode):
    def __init__(self):
        self.version_received = False
        super().__init__()

    def on_reject(self, message): pass

    def on_verack(self, message): pass
    # When version is received, don't reply with a verack. Instead, see if the
    # node will give us a message that it shouldn't. This is not an exhaustive
    # list!

    def on_version(self, message):
        self.version_received = True
        self.send_message(msg_ping())
        self.send_message(msg_getaddr())


class P2PLeakTest(FittexxcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [['-banscore=' + str(banscore)]]

    def run_test(self):
        no_version_bannode = self.nodes[0].add_p2p_connection(
            CNodeNoVersionBan(), send_version=False, wait_for_verack=False)
        no_version_idlenode = self.nodes[0].add_p2p_connection(
            CNodeNoVersionIdle(), send_version=False, wait_for_verack=False)
        no_verack_idlenode = self.nodes[0].add_p2p_connection(CNodeNoVerackIdle(), wait_for_verack=False)

        # Wait until we got the verack in response to the version. Though, don't wait for the other node to receive the
        # verack, since we never sent one
        no_verack_idlenode.wait_for_verack()

        wait_until(lambda: no_version_bannode.ever_connected,
                   timeout=10, lock=p2p_lock)
        wait_until(lambda: no_version_idlenode.ever_connected,
                   timeout=10, lock=p2p_lock)
        wait_until(lambda: no_verack_idlenode.version_received,
                   timeout=10, lock=p2p_lock)

        # Mine a block and make sure that it's not sent to the connected nodes
        self.generatetoaddress(self.nodes[0],
                               1, self.nodes[0].get_deterministic_priv_key().address)

        # Give the node enough time to possibly leak out a message
        time.sleep(5)

        # This node should have been banned
        assert not no_version_bannode.is_connected

        self.nodes[0].disconnect_p2ps()

        # Wait until all connections are closed
        wait_until(lambda: len(self.nodes[0].getpeerinfo()) == 0)

        # Make sure no unexpected messages came in
        assert not no_version_bannode.unexpected_msg
        assert not no_version_idlenode.unexpected_msg
        assert not no_verack_idlenode.unexpected_msg

        self.log.info('Check that old nodes are disconnected')
        p2p_old_node = self.nodes[0].add_p2p_connection(P2PInterface(), send_version=False, wait_for_verack=False)
        old_version_msg = msg_version()
        old_version_msg.nVersion = 31799
        wait_until(lambda: p2p_old_node.is_connected)
        with self.nodes[0].assert_debug_log(['peer=3 using obsolete version 31799; disconnecting']):
            p2p_old_node.send_message(old_version_msg)
            p2p_old_node.wait_for_disconnect()


if __name__ == '__main__':
    P2PLeakTest().main()
