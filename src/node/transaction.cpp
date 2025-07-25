// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Fittexxcoin Core developers
// Copyright (c) 2020 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/transaction.h>

#include <config.h>
#include <consensus/validation.h>
#include <net.h>
#include <primitives/txid.h>
#include <rpc/server.h>
#include <txmempool.h>
#include <validation.h>
#include <validationinterface.h>

#include <future>

TxId BroadcastTransaction(const Config &config, const CTransactionRef tx,
                          const bool allowhighfees) {
    std::promise<void> promise;
    const TxId &txid = tx->GetId();

    Amount nMaxRawTxFee = maxTxFee;
    if (allowhighfees) {
        nMaxRawTxFee = Amount::zero();
    }

    { // cs_main scope
        LOCK(cs_main);
        CCoinsViewCache &view = *pcoinsTip;
        bool fHaveChain = false;
        for (size_t o = 0; !fHaveChain && o < tx->vout.size(); o++) {
            const Coin &existingCoin = view.AccessCoin(COutPoint(txid, o));
            fHaveChain = !existingCoin.IsSpent();
        }

        bool fHaveMempool = g_mempool.exists(txid);
        if (!fHaveMempool && !fHaveChain) {
            // Push to local node and sync with wallets.
            CValidationState state;
            bool fMissingInputs;
            if (!AcceptToMemoryPool(config, g_mempool, state, std::move(tx),
                                    &fMissingInputs, false /* bypass_limits */,
                                    nMaxRawTxFee)) {
                if (state.IsInvalid()) {
                    throw JSONRPCError(RPC_TRANSACTION_REJECTED,
                                       FormatStateMessage(state));
                }

                if (fMissingInputs) {
                    throw JSONRPCError(RPC_TRANSACTION_ERROR, "Missing inputs");
                }

                throw JSONRPCError(RPC_TRANSACTION_ERROR,
                                   FormatStateMessage(state));
            } else {
                // If wallet is enabled, ensure that the wallet has been made
                // aware of the new transaction prior to returning. This
                // prevents a race where a user might call sendrawtransaction
                // with a transaction to/from their wallet, immediately call
                // some wallet RPC, and get a stale result because callbacks
                // have not yet been processed.
                CallFunctionInValidationInterfaceQueue(
                    [&promise] { promise.set_value(); });
            }
        } else if (fHaveChain) {
            throw JSONRPCError(RPC_TRANSACTION_ALREADY_IN_CHAIN,
                               "transaction already in block chain");
        } else {
            // Make sure we don't block forever if re-sending a transaction
            // already in mempool.
            promise.set_value();
        }
    } // cs_main

    promise.get_future().wait();

    if (!g_connman) {
        throw JSONRPCError(
            RPC_CLIENT_P2P_DISABLED,
            "Error: Peer-to-peer functionality missing or disabled");
    }

    CInv inv(MSG_TX, txid);
    g_connman->ForEachNode([&inv](CNode *pnode) { pnode->PushInventory(inv); });

    return txid;
}
