#!/usr/bin/env python3
# Copyright (c) 2022 The Fittexxcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Fittexxcoin test framework miscellaneous transaction tools."""
import random
from typing import Optional

from .cdefs import MAX_TXOUT_PUBKEY_SCRIPT, MIN_TX_SIZE_MAGNETIC_ANOMALY
from .messages import CTransaction, CTxOut, FromHex, ToHex, TokenOutputData
from .script import CScript, OP_CHECKSIG, OP_DUP, OP_EQUAL, OP_EQUALVERIFY, OP_HASH160, OP_HASH256, OP_RETURN


def pad_tx(tx, pad_to_size=MIN_TX_SIZE_MAGNETIC_ANOMALY):
    """
    Pad a transaction with op_return junk data until it is at least pad_to_size, or
    leave it alone if it's already bigger than that.
    """
    curr_size = len(tx.serialize())
    if curr_size >= pad_to_size:
        # Bail early txn is already big enough
        return

    # This code attempts to pad a transaction with opreturn vouts such that
    # it will be exactly pad_to_size.  In order to do this we have to create
    # vouts of size x (maximum OP_RETURN size - vout overhead), plus the final
    # one subsumes any runoff which would be less than vout overhead.
    #
    # There are two cases where this is not possible:
    # 1. The transaction size is between pad_to_size and pad_to_size - extrabytes
    # 2. The transaction is already greater than pad_to_size
    #
    # Visually:
    # | .. x  .. | .. x .. | .. x .. | .. x + desired_size % x |
    #    VOUT_1     VOUT_2    VOUT_3    VOUT_4
    # txout.value + txout.pk_script bytes + op_return
    extra_bytes = 8 + 1 + 1
    required_padding = pad_to_size - curr_size
    while required_padding > 0:
        # We need at least extra_bytes left over each time, or we can't
        # subsume the final (and possibly undersized) iteration of the loop
        padding_len = min(required_padding,
                          MAX_TXOUT_PUBKEY_SCRIPT - extra_bytes)
        assert padding_len >= 0, "Can't pad less than 0 bytes, trying {}".format(
            padding_len)
        # We will end up with less than 1 UTXO of bytes after this, add
        # them to this txn
        next_iteration_padding = required_padding - padding_len - extra_bytes
        if next_iteration_padding > 0 and next_iteration_padding < extra_bytes:
            padding_len += next_iteration_padding

        # If we're at exactly, or below, extra_bytes we don't want a 1 extra
        # byte padding
        if padding_len <= extra_bytes:
            tx.vout.append(CTxOut(0, CScript([OP_RETURN])))
        else:
            # Subtract the overhead for the TxOut
            padding_len -= extra_bytes
            padding = random.randrange(
                1 << 8 * padding_len - 2, 1 << 8 * padding_len - 1)
            tx.vout.append(
                CTxOut(0, CScript([OP_RETURN, padding])))

        curr_size = len(tx.serialize())
        required_padding = pad_to_size - curr_size
    assert curr_size >= pad_to_size, "{} !>= {}".format(curr_size, pad_to_size)
    tx.rehash()


def pad_raw_tx(rawtx_hex, min_size=MIN_TX_SIZE_MAGNETIC_ANOMALY):
    """
    Pad a raw transaction with OP_RETURN data until it reaches at least min_size
    """
    tx = CTransaction()
    FromHex(tx, rawtx_hex)
    pad_tx(tx, min_size)
    return ToHex(tx)


def calc_dust_limit(*, typ: Optional[str] = None, txout: Optional[CTxOut] = None, sats_per_kb: int = 1000,
                    token_data: Optional[TokenOutputData] = None) -> int:
    """
    Calculates the dust limit for an output (in satoshis). Pass either `typ` or a `txout`.
    :param typ: One of: 'p2pkh', 'p2sh', 'p2sh32' or None. If None, defaults to 'p2pkh'.
    :param txout: A CTxOut for which to calculate the dust limit.
    :param sats_per_kb: The fee rate in sats per kb. Default of 1000 is what most nodes use on non-full mempools.
    :param token_data: Use only in conjunction with the `typ` param to specify optional token data to add to the
                       calculation.
    :return: The dust limit it satoshis for this output type (if using `typ`), or or for this exact CTxOut if using
             `txout`.
    """
    if typ is None and txout is None:
        typ = 'p2pkh'
    if typ is not None and txout is None:
        # Fake an output to calculate size using a real CTxOut
        if typ == 'p2pkh':
            script = CScript([OP_DUP, OP_HASH160, bytes(20), OP_EQUALVERIFY, OP_CHECKSIG])
        elif typ == 'p2sh':
            script = CScript([OP_HASH160, bytes(20), OP_EQUAL])
        elif typ == 'p2sh32':
            script = CScript([OP_HASH256, bytes(32), OP_EQUAL])
        else:
            assert False, "typ param must be one of: 'p2pkh', 'p2sh', 'p2sh32'"
        return calc_dust_limit(txout=CTxOut(0, script, tokenData=token_data), sats_per_kb=sats_per_kb)
    elif txout is not None and typ is None:
        assert token_data is None, "Param `token_data` may only be used with param `typ`"
        # Dust limit formula is taken from fxxN sources: src/policy/policy.cpp
        return ((148 + len(txout.serialize())) * 3000) // sats_per_kb
    else:
        assert False, 'Must specify extactly one of one param typ= or param txout='
