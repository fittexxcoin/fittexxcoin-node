#!/usr/bin/env python3
# Copyright (c) 2010 ArtForz -- public domain half-a-node
# Copyright (c) 2012 Jeff Garzik
# Copyright (c) 2010-2019 The Fittexxcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test objects for interacting with a fittexxcoind node over the p2p protocol.

The P2PInterface objects interact with the fittexxcoind nodes under test using the
node's p2p interface. They can be used to send messages to the node, and
callbacks can be registered that execute when messages are received from the
node. Messages are sent to/received from the node on an asyncio event loop.
State held inside the objects must be guarded by the p2p_lock to avoid data
races between the main testing thread and the event loop.

P2PConnection: A low-level connection object to a node's P2P interface
P2PInterface: A high-level interface object for communicating to a node over P2P
P2PDataStore: A p2p interface class that keeps a store of transactions and blocks
              and can respond correctly to getdata and getheaders messages"""
import asyncio
from collections import defaultdict
from io import BytesIO
import logging
import struct
import sys
import threading

from test_framework.messages import (
    CBlockHeader,
    MAX_HEADERS_RESULTS,
    MIN_VERSION_SUPPORTED,
    msg_addr,
    msg_addrv2,
    msg_block,
    MSG_BLOCK,
    msg_blocktxn,
    msg_cmpctblock,
    msg_feefilter,
    msg_filteradd,
    msg_filterclear,
    msg_filterload,
    msg_getaddr,
    msg_getblocks,
    msg_getblocktxn,
    msg_getdata,
    msg_getheaders,
    msg_headers,
    msg_inv,
    msg_mempool,
    msg_merkleblock,
    msg_notfound,
    msg_ping,
    msg_pong,
    msg_reject,
    msg_sendaddrv2,
    msg_sendcmpct,
    msg_sendheaders,
    msg_tx,
    MSG_TX,
    MSG_TYPE_MASK,
    msg_verack,
    msg_version,
    msg_extversion,
    NODE_NETWORK,
    sha256,
    msg_dsproof
)
from test_framework.util import wait_until

logger = logging.getLogger("TestFramework.p2p")

MESSAGEMAP = {
    b"addr": msg_addr,
    b"addrv2": msg_addrv2,
    b"block": msg_block,
    b"blocktxn": msg_blocktxn,
    b"cmpctblock": msg_cmpctblock,
    b"feefilter": msg_feefilter,
    b"filteradd": msg_filteradd,
    b"filterclear": msg_filterclear,
    b"filterload": msg_filterload,
    b"getaddr": msg_getaddr,
    b"getblocks": msg_getblocks,
    b"getblocktxn": msg_getblocktxn,
    b"getdata": msg_getdata,
    b"getheaders": msg_getheaders,
    b"headers": msg_headers,
    b"inv": msg_inv,
    b"mempool": msg_mempool,
    b"merkleblock": msg_merkleblock,
    b"notfound": msg_notfound,
    b"ping": msg_ping,
    b"pong": msg_pong,
    b"reject": msg_reject,
    b"sendaddrv2": msg_sendaddrv2,
    b"sendcmpct": msg_sendcmpct,
    b"sendheaders": msg_sendheaders,
    b"tx": msg_tx,
    b"verack": msg_verack,
    b"version": msg_version,
    b"extversion": msg_extversion,
    b"dsproof-beta": msg_dsproof,
}

MAGIC_BYTES = {
    "mainnet": b"\xe3\xe1\xf3\xe8",
    "testnet3": b"\xf4\xe5\xf3\xf4",
    "testnet4": b"\xe2\xb7\xda\xaf",
    "scalenet": b"\xc3\xaf\xe1\xa2",
    "regtest": b"\xda\xb5\xbf\xfa",
}


class P2PConnection(asyncio.Protocol):
    """A low-level connection object to a node's P2P interface.

    This class is responsible for:

    - opening and closing the TCP connection to the node
    - reading bytes from and writing bytes to the socket
    - deserializing and serializing the P2P message header
    - logging messages as they are sent and received

    This class contains no logic for handing the P2P message payloads. It must be
    sub-classed and the on_message() callback overridden."""

    def __init__(self):
        # The underlying transport of the connection.
        # Should only call methods on this from the NetworkThread, c.f.
        # call_soon_threadsafe
        self._transport = None

    @property
    def is_connected(self):
        return self._transport is not None

    def peer_connect(self, dstaddr, dstport, *, net):
        assert not self.is_connected
        self.dstaddr = dstaddr
        self.dstport = dstport
        # The initial message to send after the connection was made:
        self.on_connection_send_msg = None
        self.on_connection_send_msg_is_raw = False
        self.recvbuf = bytearray()
        self.magic_bytes = MAGIC_BYTES[net]
        logger.debug('Connecting to Fittexxcoin Node: {}:{}'.format(
            self.dstaddr, self.dstport))

        loop = NetworkThread.network_event_loop
        conn_gen_unsafe = loop.create_connection(
            lambda: self, host=self.dstaddr, port=self.dstport)

        def conn_gen(): return loop.call_soon_threadsafe(
            loop.create_task, conn_gen_unsafe)
        return conn_gen

    def peer_disconnect(self):
        # Connection could have already been closed by other end.
        NetworkThread.network_event_loop.call_soon_threadsafe(
            lambda: self._transport and self._transport.abort())

    # Connection and disconnection methods

    def connection_made(self, transport):
        """asyncio callback when a connection is opened."""
        assert not self._transport
        logger.debug("Connected & Listening: {}:{}".format(
            self.dstaddr, self.dstport))
        self._transport = transport
        if self.on_connection_send_msg:
            if self.on_connection_send_msg_is_raw:
                self.send_raw_message(self.on_connection_send_msg)
            else:
                self.send_message(self.on_connection_send_msg)
            # Never used again
            self.on_connection_send_msg = None
        self.on_open()

    def connection_lost(self, exc):
        """asyncio callback when a connection is closed."""
        if exc:
            logger.warning("Connection lost to {}:{} due to {}".format(
                self.dstaddr, self.dstport, exc))
        else:
            logger.debug("Closed connection to: {}:{}".format(
                self.dstaddr, self.dstport))
        self._transport = None
        self.recvbuf = bytearray()
        self.on_close()

    # Socket read methods

    def data_received(self, t):
        """asyncio callback when data is read from the socket."""
        if len(t) < 1:
            return

        with p2p_lock:
            self.recvbuf.extend(t)

        while True:
            msg = self._on_data()
            if msg is None:
                break
            self.on_message(msg)

    def _on_data(self):
        """Try to read P2P messages from the recv buffer.

        This method reads data from the buffer in a loop. It deserializes,
        parses and verifies the P2P header, then passes the P2P payload to
        the on_message callback for processing."""
        try:
            with p2p_lock:
                if len(self.recvbuf) < 4:
                    return None
                if self.recvbuf[:4] != self.magic_bytes:
                    raise ValueError(
                        "got garbage {}".format(repr(self.recvbuf)))
                if len(self.recvbuf) < 4 + 12 + 4 + 4:
                    return None
                msglen = struct.unpack(
                    "<i", self.recvbuf[4 + 12:4 + 12 + 4])[0]
                if len(self.recvbuf) < 4 + 12 + 4 + 4 + msglen:
                    return None
                msg = self.recvbuf[4 + 12 + 4 + 4:4 + 12 + 4 + 4 + msglen]
                h = sha256(sha256(msg))
                checksum = self.recvbuf[4 + 12 + 4:4 + 12 + 4 + 4]
                if checksum != h[:4]:
                    raise ValueError("got bad checksum " + repr(self.recvbuf))
                msgtype = bytes(self.recvbuf[4:4 + 12].split(b"\x00", 1)[0])
                self.recvbuf = self.recvbuf[4 + 12 + 4 + 4 + msglen:]
                if msgtype not in MESSAGEMAP:
                    raise ValueError("Received unknown msgtype from {}:{}: '{}' {}".format(
                        self.dstaddr, self.dstport, msgtype, repr(msg)))
                f = BytesIO(msg)
                m = MESSAGEMAP[msgtype]()
                m.deserialize(f)
                self._log_message("receive", m)
                return m
        except Exception as e:
            logger.exception('Error reading message: {}'.format(repr(e)))
            raise

    def on_message(self, message):
        """Callback for processing a P2P payload. Must be overridden by derived class."""
        raise NotImplementedError

    # Socket write methods

    def send_message(self, message):
        """Send a P2P message over the socket.

        This method takes a P2P payload, builds the P2P header and adds
        the message to the send buffer to be sent over the socket."""
        if not self.is_connected:
            raise IOError('Not connected')
        tmsg = self.build_message(message)
        self._log_message("send", message)
        return self.send_raw_message(tmsg)

    def send_raw_message(self, raw_message_bytes):
        """Send any raw message over the socket.

        This method adds a raw message to the send buffer to be sent over the
        socket."""
        if not self.is_connected:
            raise IOError('Not connected')

        def maybe_write():
            if not self._transport:
                return
            if self._transport.is_closing():
                return
            self._transport.write(raw_message_bytes)
        NetworkThread.network_event_loop.call_soon_threadsafe(maybe_write)

    # Class utility methods

    def build_message(self, message):
        """Build a serialized P2P message"""
        msgtype = message.msgtype
        data = message.serialize()
        tmsg = self.magic_bytes
        tmsg += msgtype
        tmsg += b"\x00" * (12 - len(msgtype))
        tmsg += struct.pack("<I", len(data))
        th = sha256(data)
        h = sha256(th)
        tmsg += h[:4]
        tmsg += data
        return tmsg

    def _log_message(self, direction, msg):
        """Logs a message being sent or received over the connection."""
        if direction == "send":
            log_message = "Send message to "
        elif direction == "receive":
            log_message = "Received message from "
        log_message += "{}:{}: {}".format(
            self.dstaddr, self.dstport, repr(msg)[:500])
        if len(log_message) > 500:
            log_message += "... (msg truncated)"
        logger.debug(log_message)


class P2PInterface(P2PConnection):
    """A high-level P2P interface class for communicating with a Fittexxcoin node.

    This class provides high-level callbacks for processing P2P message
    payloads, as well as convenience methods for interacting with the
    node over P2P.

    Individual testcases should subclass this and override the on_* methods
    if they want to alter message handling behaviour."""

    def __init__(self, support_addrv2=False):
        super().__init__()

        # Track number of messages of each type received and the most recent
        # message of each type
        self.message_count = defaultdict(int)
        self.last_message = {}

        # A count of the number of ping messages we've sent to the node
        self.ping_counter = 1

        # The network services received from the peer
        self.nServices = 0

        self.support_addrv2 = support_addrv2

    def peer_connect(self, *args, services=NODE_NETWORK,
                     send_version=True, **kwargs):
        create_conn = super().peer_connect(*args, **kwargs)

        if send_version:
            # Send a version msg
            vt = msg_version()
            vt.nServices = services
            vt.addrTo.ip = self.dstaddr
            vt.addrTo.port = self.dstport
            vt.addrFrom.ip = "0.0.0.0"
            vt.addrFrom.port = 0
            # Will be sent soon after connection_made
            self.on_connection_send_msg = vt

        return create_conn

    # Message receiving methods

    def on_message(self, message):
        """Receive message and dispatch message to appropriate callback.

        We keep a count of how many of each message type has been received
        and the most recent message of each type."""
        with p2p_lock:
            try:
                msgtype = message.msgtype.decode('ascii')
                self.message_count[msgtype] += 1
                self.last_message[msgtype] = message
                msgtype_no_hyphens = msgtype.replace('-', '')
                getattr(self, 'on_' + msgtype_no_hyphens)(message)
            except Exception:
                print("ERROR delivering {} ({})".format(
                    repr(message), sys.exc_info()[0]))
                raise

    # Callback methods. Can be overridden by subclasses in individual test
    # cases to provide custom message handling behaviour.

    def on_open(self):
        pass

    def on_close(self):
        pass

    def on_addr(self, message): pass

    def on_addrv2(self, message): pass

    def on_block(self, message): pass

    def on_blocktxn(self, message): pass

    def on_cmpctblock(self, message): pass

    def on_feefilter(self, message): pass

    def on_filteradd(self, message): pass

    def on_filterclear(self, message): pass

    def on_filterload(self, message): pass

    def on_getaddr(self, message): pass

    def on_getblocks(self, message): pass

    def on_getblocktxn(self, message): pass

    def on_getdata(self, message): pass

    def on_dsproofbeta(self, message): pass

    def on_getheaders(self, message): pass

    def on_headers(self, message): pass

    def on_mempool(self, message): pass

    def on_merkleblock(self, message): pass

    def on_notfound(self, message): pass

    def on_pong(self, message): pass

    def on_reject(self, message): pass

    def on_sendaddrv2(self, message): pass

    def on_sendcmpct(self, message): pass

    def on_sendheaders(self, message): pass

    def on_tx(self, message): pass

    def on_inv(self, message):
        want = msg_getdata()
        for i in message.inv:
            if i.type != 0:
                want.inv.append(i)
        if len(want.inv):
            self.send_message(want)

    def on_ping(self, message):
        self.send_message(msg_pong(message.nonce))

    def on_verack(self, message):
        self.verack_received = True

    def on_version(self, message):
        assert message.nVersion >= MIN_VERSION_SUPPORTED, "Version {} received. Test framework only supports versions greater than {}".format(
            message.nVersion, MIN_VERSION_SUPPORTED)
        if self.support_addrv2:
            self.send_message(msg_sendaddrv2())
        self.send_message(msg_verack())
        self.nServices = message.nServices

    # Connection helper methods

    def wait_for_disconnect(self, timeout=60):
        def test_function(): return not self.is_connected
        wait_until(test_function, timeout=timeout, lock=p2p_lock)

    # Message receiving helper methods

    def wait_for_tx(self, txid, timeout=60):
        def test_function():
            if not self.last_message.get('tx'):
                return False
            return self.last_message['tx'].tx.rehash() == txid

        wait_until(test_function, timeout=timeout, lock=p2p_lock)

    def wait_for_block(self, blockhash, timeout=60):
        def test_function(): return self.last_message.get(
            "block") and self.last_message["block"].block.rehash() == blockhash
        wait_until(test_function, timeout=timeout, lock=p2p_lock)

    def wait_for_header(self, blockhash, timeout=60):
        def test_function():
            last_headers = self.last_message.get('headers')
            if not last_headers:
                return False
            return last_headers.headers[0].rehash() == int(blockhash, 16)

        wait_until(test_function, timeout=timeout, lock=p2p_lock)

    def wait_for_merkleblock(self, blockhash, timeout=60):
        def test_function():
            assert self.is_connected
            last_filtered_block = self.last_message.get('merkleblock')
            if not last_filtered_block:
                return False
            return last_filtered_block.merkleblock.header.rehash() == int(blockhash, 16)

        wait_until(test_function, timeout=timeout, lock=p2p_lock)

    def wait_for_getdata(self, timeout=60):
        """Waits for a getdata message.

        Receiving any getdata message will satisfy the predicate. the last_message["getdata"]
        value must be explicitly cleared before calling this method, or this will return
        immediately with success. TODO: change this method to take a hash value and only
        return true if the correct block/tx has been requested."""
        def test_function(): return self.last_message.get("getdata")
        wait_until(test_function, timeout=timeout, lock=p2p_lock)

    def wait_for_getheaders(self, timeout=60):
        """Waits for a getheaders message.

        Receiving any getheaders message will satisfy the predicate. the last_message["getheaders"]
        value must be explicitly cleared before calling this method, or this will return
        immediately with success. TODO: change this method to take a hash value and only
        return true if the correct block header has been requested."""
        def test_function(): return self.last_message.get("getheaders")
        wait_until(test_function, timeout=timeout, lock=p2p_lock)

    def wait_for_inv(self, expected_inv, timeout=60):
        """Waits for an INV message and checks that the first inv object in the message was as expected."""
        if len(expected_inv) > 1:
            raise NotImplementedError(
                "wait_for_inv() will only verify the first inv object")

        def test_function(): return self.last_message.get("inv") and \
            self.last_message["inv"].inv[0].type == expected_inv[0].type and \
            self.last_message["inv"].inv[0].hash == expected_inv[0].hash
        wait_until(test_function, timeout=timeout, lock=p2p_lock)

    def wait_for_verack(self, timeout=60):
        def test_function(): return self.message_count["verack"]
        wait_until(test_function, timeout=timeout, lock=p2p_lock)

    # Message sending helper functions

    def send_and_ping(self, message, timeout=60):
        self.send_message(message)
        self.sync_with_ping(timeout=timeout)

    # Sync up with the node
    def sync_with_ping(self, timeout=60):
        self.send_message(msg_ping(nonce=self.ping_counter))

        def test_function():
            if not self.last_message.get("pong"):
                return False
            return self.last_message["pong"].nonce == self.ping_counter
        wait_until(test_function, timeout=timeout, lock=p2p_lock)
        self.ping_counter += 1


# One lock for synchronizing all data access between the networking thread (see
# NetworkThread below) and the thread running the test logic.  For simplicity,
# P2PConnection acquires this lock whenever delivering a message to a P2PInterface.
# This lock should be acquired in the thread running the test logic to synchronize
# access to any data shared with the P2PInterface or P2PConnection.
p2p_lock = threading.RLock()


class NetworkThread(threading.Thread):
    network_event_loop = None

    def __init__(self):
        super().__init__(name="NetworkThread")
        # There is only one event loop and no more than one thread must be
        # created
        assert not self.network_event_loop

        NetworkThread.network_event_loop = asyncio.new_event_loop()

    def run(self):
        """Start the network thread."""
        self.network_event_loop.run_forever()

    def close(self, timeout=10):
        """Close the connections and network event loop."""
        self.network_event_loop.call_soon_threadsafe(
            self.network_event_loop.stop)
        wait_until(lambda: not self.network_event_loop.is_running(),
                   timeout=timeout)
        self.network_event_loop.close()
        self.join(timeout)


class P2PDataStore(P2PInterface):
    """A P2P data store class.

    Keeps a block and transaction store and responds correctly to getdata and getheaders requests."""

    def __init__(self):
        super().__init__()
        # store of blocks. key is block hash, value is a CBlock object
        self.block_store = {}
        self.last_block_hash = ''
        # store of txs. key is txid, value is a CTransaction object
        self.tx_store = {}
        self.getdata_requests = []

    def on_getdata(self, message):
        """Check for the tx/block in our stores and if found, reply with an inv message."""
        for inv in message.inv:
            self.getdata_requests.append(inv.hash)
            if (inv.type & MSG_TYPE_MASK) == MSG_TX and inv.hash in self.tx_store.keys():
                self.send_message(msg_tx(self.tx_store[inv.hash]))
            elif (inv.type & MSG_TYPE_MASK) == MSG_BLOCK and inv.hash in self.block_store.keys():
                self.send_message(msg_block(self.block_store[inv.hash]))
            else:
                logger.debug(
                    'getdata message type {} received.'.format(hex(inv.type)))

    def on_getheaders(self, message):
        """Search back through our block store for the locator, and reply with a headers message if found."""

        locator, hash_stop = message.locator, message.hashstop

        # Assume that the most recent block added is the tip
        if not self.block_store:
            return

        headers_list = [self.block_store[self.last_block_hash]]
        while headers_list[-1].sha256 not in locator.vHave:
            # Walk back through the block store, adding headers to headers_list
            # as we go.
            prev_block_hash = headers_list[-1].hashPrevBlock
            if prev_block_hash in self.block_store:
                prev_block_header = CBlockHeader(
                    self.block_store[prev_block_hash])
                headers_list.append(prev_block_header)
                if prev_block_header.sha256 == hash_stop:
                    # if this is the hashstop header, stop here
                    break
            else:
                logger.debug('block hash {} not found in block store'.format(
                    hex(prev_block_hash)))
                break

        # Truncate the list if there are too many headers
        headers_list = headers_list[:-MAX_HEADERS_RESULTS - 1:-1]
        response = msg_headers(headers_list)

        if response is not None:
            self.send_message(response)

    def send_blocks_and_test(self, blocks, node, *, success=True, request_block=True,
                             reject_reason=None, expect_disconnect=False, timeout=60):
        """Send blocks to test node and test whether the tip advances.

         - add all blocks to our block_store
         - send a headers message for the final block
         - the on_getheaders handler will ensure that any getheaders are responded to
         - if request_block is True: wait for getdata for each of the blocks. The on_getdata handler will
           ensure that any getdata messages are responded to
         - if success is True: assert that the node's tip advances to the most recent block
         - if success is False: assert that the node's tip doesn't advance
         - if reject_reason is set: assert that the correct reject message is logged"""

        with p2p_lock:
            for block in blocks:
                self.block_store[block.sha256] = block
                self.last_block_hash = block.sha256

        reject_reason = [reject_reason] if reject_reason else []
        with node.assert_debug_log(expected_msgs=reject_reason):
            self.send_message(msg_headers([CBlockHeader(blocks[-1])]))

            if request_block:
                wait_until(
                    lambda: blocks[-1].sha256 in self.getdata_requests, timeout=timeout, lock=p2p_lock)

            if expect_disconnect:
                self.wait_for_disconnect(timeout=timeout)
            else:
                self.sync_with_ping(timeout=timeout)

            if success:
                wait_until(lambda: node.getbestblockhash()
                           == blocks[-1].hash, timeout=timeout)
            else:
                assert node.getbestblockhash() != blocks[-1].hash

    def send_txs_and_test(self, txs, node, *, success=True,
                          expect_disconnect=False, reject_reason=None):
        """Send txs to test node and test whether they're accepted to the mempool.

         - add all txs to our tx_store
         - send tx messages for all txs
         - if success is True/False: assert that the txs are/are not accepted to the mempool
         - if expect_disconnect is True: Skip the sync with ping
         - if reject_reason is set: assert that the correct reject message is logged."""

        with p2p_lock:
            for tx in txs:
                self.tx_store[tx.sha256] = tx

        reject_reason = [reject_reason] if reject_reason else []
        with node.assert_debug_log(expected_msgs=reject_reason):
            for tx in txs:
                self.send_message(msg_tx(tx))

            if expect_disconnect:
                self.wait_for_disconnect()
            else:
                self.sync_with_ping()

            raw_mempool = node.getrawmempool()
            if success:
                # Check that all txs are now in the mempool
                for tx in txs:
                    assert tx.hash in raw_mempool, "{} not found in mempool".format(
                        tx.hash)
            else:
                # Check that none of the txs are now in the mempool
                for tx in txs:
                    assert tx.hash not in raw_mempool, "{} tx found in mempool".format(
                        tx.hash)
