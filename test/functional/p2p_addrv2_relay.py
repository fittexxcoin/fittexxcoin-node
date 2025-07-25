#!/usr/bin/env python3
# Copyright (c) 2020 The Fittexxcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
Test addrv2 relay
"""

import time

from test_framework.messages import (
    CAddress,
    msg_addrv2,
    NODE_NETWORK,
)
from test_framework.p2p import P2PInterface
from test_framework.test_framework import FittexxcoinTestFramework
from test_framework.util import assert_equal

ADDRS = []
for i in range(10):
    addr = CAddress()
    addr.time = int(time.time()) + i
    addr.nServices = NODE_NETWORK
    addr.ip = "123.123.123.{}".format(i % 256)
    addr.port = 7890 + i
    ADDRS.append(addr)


class AddrReceiver(P2PInterface):
    addrv2_received_and_checked = False

    def __init__(self):
        super().__init__(support_addrv2 = True)

    def on_addrv2(self, message):
        for addr in message.addrs:
            assert_equal(addr.nServices, 1)
            assert addr.ip.startswith('123.123.123.')
            assert (7890 <= addr.port < 8343)
        self.addrv2_received_and_checked = True

    def wait_for_addrv2(self):
        self.wait_until(lambda: self.addrv2_received_and_checked)


class AddrTest(FittexxcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def run_test(self):
        self.log.info('Create connection that sends addrv2 messages')
        addr_source = self.nodes[0].add_p2p_connection(P2PInterface())
        msg = msg_addrv2()

        self.log.info('Send too-large addrv2 message')
        msg.addrs = ADDRS * 101
        with self.nodes[0].assert_debug_log(['addrv2 message size = 1010']):
            addr_source.send_and_ping(msg)

        self.log.info('Check that addrv2 message content is relayed and added to addrman')
        addr_receiver = self.nodes[0].add_p2p_connection(AddrReceiver())
        msg.addrs = ADDRS
        with self.nodes[0].assert_debug_log([
                'Added 10 addresses from 127.0.0.1: 0 tried',
                'received: addrv2 (131 bytes) peer=0',
                'sending addrv2 (131 bytes) peer=1',
        ]):
            addr_source.send_and_ping(msg)
            self.nodes[0].setmocktime(int(time.time()) + 30 * 60)
            addr_receiver.wait_for_addrv2()


if __name__ == '__main__':
    AddrTest().main()
