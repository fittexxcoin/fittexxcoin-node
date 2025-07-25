#!/usr/bin/env python3
# Copyright (c) 2017-2019 The Fittexxcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Class for fittexxcoind node under test"""

import contextlib
import decimal
from enum import Enum
import errno
import http.client
import json
import logging
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.parse
import collections

from .authproxy import JSONRPCException
from .messages import COIN, CTransaction, FromHex
from .util import (
    append_config,
    delete_cookie_file,
    get_rpc_proxy,
    p2p_port,
    rpc_url,
    wait_until,
)

FITTEXXCOIND_PROC_WAIT_TIMEOUT = 60


class FailedToStartError(Exception):
    """Raised when a node fails to start correctly."""


class ErrorMatch(Enum):
    FULL_TEXT = 1
    FULL_REGEX = 2
    PARTIAL_REGEX = 3


class TestNode():
    """A class for representing a fittexxcoind node under test.

    This class contains:

    - state about the node (whether it's running, etc)
    - a Python subprocess.Popen object representing the running process
    - an RPC connection to the node
    - one or more P2P connections to the node

    To make things easier for the test writer, any unrecognised messages will
    be dispatched to the RPC connection."""

    def __init__(self, i, datadir, *, chain, host, rpc_port, p2p_port, timewait, fittexxcoind,
                 fittexxcoin_cli, mocktime, coverage_dir, extra_conf=None, extra_args=None, use_cli=False, emulator=None):
        self.index = i
        self.datadir = datadir
        self.fittexxcoinconf = os.path.join(self.datadir, "fittexxcoin.conf")
        self.stdout_dir = os.path.join(self.datadir, "stdout")
        self.stderr_dir = os.path.join(self.datadir, "stderr")
        self.chain = chain
        self.host = host
        self.rpc_port = rpc_port
        self.p2p_port = p2p_port
        self.name = "testnode-{}".format(i)
        self.rpc_timeout = timewait
        self.binary = fittexxcoind
        if not os.path.isfile(self.binary):
            raise FileNotFoundError(
                "Binary '{}' could not be found.\nTry setting it manually:\n\tFITTEXXCOIND=<path/to/fittexxcoind> {}".format(self.binary, sys.argv[0]))
        self.coverage_dir = coverage_dir
        if extra_conf is not None:
            append_config(datadir, extra_conf)
        # Most callers will just need to add extra args to the default list
        # below.
        # For those callers that need more flexibility, they can access the
        # default args using the provided facilities.
        # Note that common args are set in the config file (see
        # initialize_datadir)
        self.extra_args = extra_args
        self.default_args = [
            "-datadir=" + self.datadir,
            "-logtimemicros",
            "-debug",
            "-debugexclude=libevent",
            "-debugexclude=leveldb",
            "-mocktime=" + str(mocktime),
            "-uacomment=" + self.name,
            "-noprinttoconsole",
        ]

        if emulator is not None:
            if not os.path.isfile(emulator):
                emulator = shutil.which(emulator)
                if not os.path.isfile(emulator):
                    raise FileNotFoundError("Emulator '{}' could not be found.".format(emulator))
        self.emulator = emulator

        if use_cli and not os.path.isfile(fittexxcoin_cli):
            raise FileNotFoundError(
                "Binary '{}' could not be found.\nTry setting it manually:\n\tFITTEXXCOINCLI=<path/to/fittexxcoin-cli> {}".format(fittexxcoin_cli, sys.argv[0]))
        self.cli = TestNodeCLI(fittexxcoin_cli, self.datadir, self.emulator)
        self.use_cli = use_cli

        self.running = False
        self.process = None
        self.rpc_connected = False
        self.rpc = None
        self.url = None
        self.relay_fee_cache = None
        self.log = logging.getLogger('TestFramework.node{}'.format(i))
        # Whether to kill the node when this object goes away
        self.cleanup_on_exit = True
        self.p2ps = []

    def get_deterministic_priv_key(self):
        """Return a deterministic priv key in base58, that only depends on the node's index"""
        AddressKeyPair = collections.namedtuple(
            'AddressKeyPair', ['address', 'key'])
        PRIV_KEYS = [
            # address , privkey
            AddressKeyPair('mjTkW3DjgyZck4KbiRusZsqTgaYTxdSz6z',
                           'cVpF924EspNh8KjYsfhgY96mmxvT6DgdWiTYMtMjuM74hJaU5psW'),
            AddressKeyPair('msX6jQXvxiNhx3Q62PKeLPrhrqZQdSimTg',
                           'cUxsWyKyZ9MAQTaAhUQWJmBbSvHMwSmuv59KgxQV7oZQU3PXN3KE'),
            AddressKeyPair('mnonCMyH9TmAsSj3M59DsbH8H63U3RKoFP',
                           'cTrh7dkEAeJd6b3MRX9bZK8eRmNqVCMH3LSUkE3dSFDyzjU38QxK'),
            AddressKeyPair('mqJupas8Dt2uestQDvV2NH3RU8uZh2dqQR',
                           'cVuKKa7gbehEQvVq717hYcbE9Dqmq7KEBKqWgWrYBa2CKKrhtRim'),
            AddressKeyPair('msYac7Rvd5ywm6pEmkjyxhbCDKqWsVeYws',
                           'cQDCBuKcjanpXDpCqacNSjYfxeQj8G6CAtH1Dsk3cXyqLNC4RPuh'),
            AddressKeyPair('n2rnuUnwLgXqf9kk2kjvVm8R5BZK1yxQBi',
                           'cQakmfPSLSqKHyMFGwAqKHgWUiofJCagVGhiB4KCainaeCSxeyYq'),
            AddressKeyPair('myzuPxRwsf3vvGzEuzPfK9Nf2RfwauwYe6',
                           'cQMpDLJwA8DBe9NcQbdoSb1BhmFxVjWD5gRyrLZCtpuF9Zi3a9RK'),
            AddressKeyPair('mumwTaMtbxEPUswmLBBN3vM9oGRtGBrys8',
                           'cSXmRKXVcoouhNNVpcNKFfxsTsToY5pvB9DVsFksF1ENunTzRKsy'),
            AddressKeyPair('mpV7aGShMkJCZgbW7F6iZgrvuPHjZjH9qg',
                           'cSoXt6tm3pqy43UMabY6eUTmR3eSUYFtB2iNQDGgb3VUnRsQys2k'),
        ]
        return PRIV_KEYS[self.index]

    def _node_msg(self, msg: str) -> str:
        """Return a modified msg that identifies this node by its index as a debugging aid."""
        return "[node {}] {}".format(self.index, msg)

    def _raise_assertion_error(self, msg: str):
        """Raise an AssertionError with msg modified to identify this node."""
        raise AssertionError(self._node_msg(msg))

    def __del__(self):
        # Ensure that we don't leave any fittexxcoind processes lying around after
        # the test ends
        if self.process and self.cleanup_on_exit:
            # Should only happen on test failure
            # Avoid using logger, as that may have already been shutdown when
            # this destructor is called.
            print(self._node_msg("Cleaning up leftover process"))
            self.process.kill()

    def __getattr__(self, name):
        """Dispatches any unrecognised method name to the RPC connection or a CLI instance."""
        return self._get_rpc_or_cli_method(name)

    def _get_rpc_or_cli_method(self, name):
        """Returns the CLI or RPC connection method 'name'."""
        if self.use_cli:
            return getattr(self.cli, name)
        else:
            assert self.rpc is not None, self._node_msg(
                "Error: RPC not initialized")
            assert self.rpc_connected, self._node_msg(
                "Error: No RPC connection")
            return getattr(self.rpc, name)

    def call_rpc(self, name: str, *args, **kwargs):
        """Wrapper for any rpc or cli call that optionally accepts the kwarg
        `ignore_error=<str>` where <str> is some error message substring we
        tolerate as a JSONRPCException. If no exception is raised or if no
        `ignore_error` kwarg is present, returns the result of calling rpc
        `name`. If `ignore_error` is specified and there is a matching
        exception message, returns None. This wrapper was originally designed
        to facilitate dsproof tests."""
        ignore_error = kwargs.pop('ignore_error', None)
        try:
            return self._get_rpc_or_cli_method(name)(*args, **kwargs)
        except JSONRPCException as exc:
            if ignore_error is None or ignore_error not in str(exc):
                raise exc
            self.log.info("call_rpc: Ignoring exception from '{}': {}".format(name, repr(exc)))
            return None

    def clear_default_args(self):
        self.default_args.clear()

    def extend_default_args(self, args):
        self.default_args.extend(args)

    def remove_default_args(self, args):
        for rm_arg in args:
            # Remove all occurrences of rm_arg in self.default_args:
            #  - if the arg is a flag (-flag), then the names must match
            #  - if the arg is a value (-key=value) then the name must starts
            #    with "-key=" (the '"' char is to avoid removing "-key_suffix"
            #    arg is "-key" is the argument to remove).
            self.default_args = [def_arg for def_arg in self.default_args
                                 if rm_arg != def_arg and not def_arg.startswith(rm_arg + '=')]

    def start(self, extra_args=None, stdout=None,
              stderr=None, *args, **kwargs):
        """Start the node."""
        if extra_args is None:
            extra_args = self.extra_args

        # Add a new stdout and stderr file each time fittexxcoind is started
        if stderr is None:
            stderr = tempfile.NamedTemporaryFile(
                dir=self.stderr_dir, delete=False)
        if stdout is None:
            stdout = tempfile.NamedTemporaryFile(
                dir=self.stdout_dir, delete=False)
        self.stderr = stderr
        self.stdout = stdout

        # Delete any existing cookie file -- if such a file exists (eg due to
        # unclean shutdown), it will get overwritten anyway by fittexxcoind, and
        # potentially interfere with our attempt to authenticate
        delete_cookie_file(self.datadir, self.chain)

        # add environment variable LIBC_FATAL_STDERR_=1 so that libc errors are
        # written to stderr and not the terminal
        subp_env = dict(os.environ, LIBC_FATAL_STDERR_="1")

        p_args = [self.binary] + self.default_args + extra_args
        if self.emulator is not None:
            p_args = [self.emulator] + p_args
        self.process = subprocess.Popen(
            p_args,
            env=subp_env,
            stdout=stdout,
            stderr=stderr,
            *args,
            **kwargs)

        self.running = True
        self.log.debug("fittexxcoind started, waiting for RPC to come up")

    def wait_for_rpc_connection(self):
        """Sets up an RPC connection to the fittexxcoind process. Returns False if unable to connect."""
        # Poll at a rate of four times per second
        poll_per_s = 4
        for _ in range(poll_per_s * self.rpc_timeout):
            if self.process.poll() is not None:
                raise FailedToStartError(self._node_msg(
                    'fittexxcoind exited with status {} during initialization'.format(self.process.returncode)))
            try:
                rpc = get_rpc_proxy(
                    rpc_url(self.datadir, self.chain, self.host, self.rpc_port),
                    self.index, timeout=self.rpc_timeout, coveragedir=self.coverage_dir)
                rpc.getblockcount()
                # If the call to getblockcount() succeeds then the RPC
                # connection is up

                wait_until(lambda: rpc.getmempoolinfo()['loaded'])
                # Wait for the node to finish reindex, block import, and
                # loading the mempool. Usually importing happens fast or
                # even "immediate" when the node is started. However, there
                # is no guarantee and sometimes ThreadImport might finish
                # later. This is going to cause intermittent test failures,
                # because generally the tests assume the node is fully
                # ready after being started.
                #
                # For example, the node will reject block messages from p2p
                # when it is still importing with the error "Unexpected
                # block message received"
                #
                # The wait is done here to make tests as robust as possible
                # and prevent racy tests and intermittent failures as much
                # as possible. Some tests might not need this, but the
                # overhead is trivial, and the added gurantees are worth
                # the minimal performance cost.

                self.log.debug("RPC successfully started")
                if self.use_cli:
                    return
                self.rpc = rpc
                self.rpc_connected = True
                self.url = self.rpc.url
                return
            except IOError as e:
                if e.errno != errno.ECONNREFUSED:  # Port not yet open?
                    raise  # unknown IO error
            except JSONRPCException as e:  # Initialization phase
                # -28 RPC in warmup
                # -342 Service unavailable, RPC server started but is shutting down due to error
                if e.error['code'] != -28 and e.error['code'] != -342:
                    raise  # unknown JSON RPC exception
            except ValueError as e:  # cookie file not found and no rpcuser or rpcassword. fittexxcoind still starting
                if "No RPC credentials" not in str(e):
                    raise
            time.sleep(1.0 / poll_per_s)
        self._raise_assertion_error("Unable to connect to fittexxcoind")

    def get_wallet_rpc(self, wallet_name):
        if self.use_cli:
            return self.cli("-rpcwallet={}".format(wallet_name))
        else:
            assert self.rpc is not None, self._node_msg(
                "Error: RPC not initialized")
            assert self.rpc_connected, self._node_msg(
                "Error: RPC not connected")
            wallet_path = "wallet/{}".format(urllib.parse.quote(wallet_name))
            return self.rpc / wallet_path

    def stop_node(self, expected_stderr='', wait=0):
        """Stop the node."""
        if not self.running:
            return
        self.log.debug("Stopping node")
        try:
            self.stop(wait=wait)
        except http.client.CannotSendRequest:
            self.log.exception("Unable to stop node.")

        # Check that stderr is as expected
        self.stderr.seek(0)
        stderr = self.stderr.read().decode('utf-8').strip()
        if stderr != expected_stderr:
            raise AssertionError(
                "Unexpected stderr {} != {}".format(stderr, expected_stderr))

        del self.p2ps[:]

    def is_node_stopped(self):
        """Checks whether the node has stopped.

        Returns True if the node has stopped. False otherwise.
        This method is responsible for freeing resources (self.process)."""
        if not self.running:
            return True
        return_code = self.process.poll()
        if return_code is None:
            return False

        # process has stopped. Assert that it didn't return an error code.
        assert return_code == 0, self._node_msg(
            "Node returned non-zero exit code ({}) when stopping".format(return_code))
        self.running = False
        self.process = None
        self.rpc_connected = False
        self.rpc = None
        self.log.debug("Node stopped")
        return True

    def wait_until_stopped(self, timeout=FITTEXXCOIND_PROC_WAIT_TIMEOUT):
        wait_until(self.is_node_stopped, timeout=timeout)

    @contextlib.contextmanager
    def assert_debug_log(self, expected_msgs, timeout=2):
        time_end = time.time() + timeout
        debug_log = os.path.join(self.datadir, self.chain, 'debug.log')
        with open(debug_log, encoding='utf-8') as dl:
            dl.seek(0, 2)
            prev_size = dl.tell()
        try:
            yield
        finally:
            while True:
                found = True
                with open(debug_log, encoding='utf-8') as dl:
                    dl.seek(prev_size)
                    log = dl.read()
                print_log = " - " + "\n - ".join(log.splitlines())
                for expected_msg in expected_msgs:
                    if re.search(re.escape(expected_msg), log,
                                 flags=re.MULTILINE) is None:
                        found = False
                if found:
                    return
                if time.time() >= time_end:
                    break
                time.sleep(0.05)
            self._raise_assertion_error(
                'Expected messages "{}" does not partially match log:\n\n{}\n\n'.format(
                    str(expected_msgs), print_log))

    def assert_start_raises_init_error(
            self, extra_args=None, expected_msg=None, match=ErrorMatch.FULL_TEXT, *args, **kwargs):
        """Attempt to start the node and expect it to raise an error.

        extra_args: extra arguments to pass through to fittexxcoind
        expected_msg: regex that stderr should match when fittexxcoind fails

        Will throw if fittexxcoind starts without an error.
        Will throw if an expected_msg is provided and it does not match fittexxcoind's stdout."""
        with tempfile.NamedTemporaryFile(dir=self.stderr_dir, delete=False) as log_stderr, \
                tempfile.NamedTemporaryFile(dir=self.stdout_dir, delete=False) as log_stdout:
            try:
                self.start(extra_args, stdout=log_stdout,
                           stderr=log_stderr, *args, **kwargs)
                self.wait_for_rpc_connection()
                self.stop_node()
                self.wait_until_stopped()
            except FailedToStartError as e:
                self.log.debug('fittexxcoind failed to start: {}'.format(e))
                self.running = False
                self.process = None
                # Check stderr for expected message
                if expected_msg is not None:
                    log_stderr.seek(0)
                    stderr = log_stderr.read().decode('utf-8').strip()
                    if match == ErrorMatch.PARTIAL_REGEX:
                        if re.search(expected_msg, stderr,
                                     flags=re.MULTILINE) is None:
                            self._raise_assertion_error(
                                'Expected message "{}" does not partially match stderr:\n"{}"'.format(expected_msg, stderr))
                    elif match == ErrorMatch.FULL_REGEX:
                        if re.fullmatch(expected_msg, stderr) is None:
                            self._raise_assertion_error(
                                'Expected message "{}" does not fully match stderr:\n"{}"'.format(expected_msg, stderr))
                    elif match == ErrorMatch.FULL_TEXT:
                        if expected_msg != stderr:
                            self._raise_assertion_error(
                                'Expected message "{}" does not fully match stderr:\n"{}"'.format(expected_msg, stderr))
            else:
                if expected_msg is None:
                    assert_msg = "fittexxcoind should have exited with an error"
                else:
                    assert_msg = "fittexxcoind should have exited with expected error " + expected_msg
                self._raise_assertion_error(assert_msg)

    def relay_fee(self, cached=True):
        if not self.relay_fee_cache or not cached:
            self.relay_fee_cache = self.getnetworkinfo()["relayfee"]

        return self.relay_fee_cache

    def calculate_fee(self, tx):
        """ Estimate the necessary fees (in sats) for an unsigned CTransaction assuming:
        - the current relayfee on node
        - all inputs are compressed-key p2pkh, and will be signed ecdsa or schnorr
        - all inputs currently unsigned (empty scriptSig)
        """
        billable_size_estimate = tx.billable_size()
        # Add some padding for signatures / public keys
        # 107 = length of PUSH(longest_sig = 72 bytes), PUSH(pubkey = 33 bytes)
        billable_size_estimate += len(tx.vin) * 107

        # relay_fee gives a value in fxx per kB.
        return int(self.relay_fee() / 1000 * billable_size_estimate * COIN)

    def calculate_fee_from_txid(self, txid):
        ctx = FromHex(CTransaction(), self.getrawtransaction(txid))
        return self.calculate_fee(ctx)

    def add_p2p_connection(self, p2p_conn, *, wait_for_verack=True, **kwargs):
        """Add a p2p connection to the node.

        This method adds the p2p connection to the self.p2ps list and also
        returns the connection to the caller."""
        if 'dstport' not in kwargs:
            kwargs['dstport'] = p2p_port(self.index)
        if 'dstaddr' not in kwargs:
            kwargs['dstaddr'] = '127.0.0.1'

        p2p_conn.peer_connect(**kwargs, net=self.chain)()
        self.p2ps.append(p2p_conn)
        if wait_for_verack:
            # Wait for the node to send us the version and verack
            p2p_conn.wait_for_verack()
            # At this point we have sent our version message and received the version and verack, however the full node
            # has not yet received the verack from us (in reply to their version). So, the connection is not yet fully
            # established (fSuccessfullyConnected).
            #
            # This shouldn't lead to any issues when sending messages, since the verack will be in-flight before the
            # message we send. However, it might lead to races where we are expecting to receive a message. E.g. a
            # transaction that will be added to the mempool as soon as we return here.
            #
            # So syncing here is redundant when we only want to send a message, but the cost is low (a few milliseconds)
            # in comparision to the upside of making tests less fragile and unexpected intermittent errors less likely.
            p2p_conn.sync_with_ping()

        return p2p_conn

    @property
    def p2p(self):
        """Return the first p2p connection

        Convenience property - most tests only use a single p2p connection to each
        node, so this saves having to write node.p2ps[0] many times."""
        assert self.p2ps, self._node_msg("No p2p connection")
        return self.p2ps[0]

    def disconnect_p2ps(self):
        """Close all p2p connections to the node."""
        for p in self.p2ps:
            p.peer_disconnect()
        del self.p2ps[:]


class TestNodeCLIAttr:
    def __init__(self, cli, command):
        self.cli = cli
        self.command = command

    def __call__(self, *args, **kwargs):
        return self.cli.send_cli(self.command, *args, **kwargs)

    def get_request(self, *args, **kwargs):
        return lambda: self(*args, **kwargs)


def arg_to_cli(arg):
    if isinstance(arg, bool):
        return str(arg).lower()
    elif isinstance(arg, dict) or isinstance(arg, list):
        return json.dumps(arg)
    else:
        return str(arg)


class TestNodeCLI():
    """Interface to fittexxcoin-cli for an individual node"""

    def __init__(self, binary, datadir, emulator=None):
        self.options = []
        self.binary = binary
        self.datadir = datadir
        self.input = None
        self.log = logging.getLogger('TestFramework.fittexxcoincli')
        self.emulator = emulator

    def __call__(self, *options, input=None):
        # TestNodeCLI is callable with fittexxcoin-cli command-line options
        cli = TestNodeCLI(self.binary, self.datadir, self.emulator)
        cli.options = [str(o) for o in options]
        cli.input = input
        return cli

    def __getattr__(self, command):
        return TestNodeCLIAttr(self, command)

    def batch(self, requests):
        results = []
        for request in requests:
            try:
                results.append(dict(result=request()))
            except JSONRPCException as e:
                results.append(dict(error=e))
        return results

    def send_cli(self, command=None, *args, **kwargs):
        """Run fittexxcoin-cli command. Deserializes returned string as python object."""
        pos_args = [arg_to_cli(arg) for arg in args]
        named_args = [str(key) + "=" + arg_to_cli(value)
                      for (key, value) in kwargs.items()]
        assert not (
            pos_args and named_args), "Cannot use positional arguments and named arguments in the same fittexxcoin-cli call"
        p_args = [self.binary, "-datadir=" + self.datadir] + self.options
        if named_args:
            p_args += ["-named"]
        if command is not None:
            p_args += [command]
        p_args += pos_args + named_args
        if self.emulator is not None:
            p_args = [self.emulator] + p_args
        self.log.debug("Running fittexxcoin-cli command: {}".format(command))
        process = subprocess.Popen(p_args, stdin=subprocess.PIPE,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
        cli_stdout, cli_stderr = process.communicate(input=self.input)
        returncode = process.poll()
        if returncode:
            match = re.match(
                r'error code: ([-0-9]+)\nerror message:\n(.*)', cli_stderr)
            if match:
                code, message = match.groups()
                raise JSONRPCException(dict(code=int(code), message=message))
            # Ignore cli_stdout, raise with cli_stderr
            raise subprocess.CalledProcessError(
                returncode, self.binary, output=cli_stderr)
        try:
            return json.loads(cli_stdout, parse_float=decimal.Decimal)
        except json.JSONDecodeError:
            return cli_stdout.rstrip("\n")
