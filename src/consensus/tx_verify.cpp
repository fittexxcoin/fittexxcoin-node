// Copyright (c) 2018-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/tx_verify.h>

#include <amount.h>
#include <chain.h>
#include <coins.h>
#include <consensus/activation.h>
#include <consensus/consensus.h>
#include <consensus/params.h>
#include <consensus/validation.h>
#include <primitives/transaction.h>
#include <script/script_flags.h>
#include <util/moneystr.h> // For FormatMoney
#include <version.h>       // For PROTOCOL_VERSION

static bool IsFinalTx(const CTransaction &tx, int nBlockHeight,
                      int64_t nBlockTime) {
    if (tx.nLockTime == 0) {
        return true;
    }

    int64_t lockTime = tx.nLockTime;
    int64_t lockTimeLimit =
        (lockTime < LOCKTIME_THRESHOLD) ? nBlockHeight : nBlockTime;
    if (lockTime < lockTimeLimit) {
        return true;
    }

    for (const auto &txin : tx.vin) {
        if (txin.nSequence != CTxIn::SEQUENCE_FINAL) {
            return false;
        }
    }
    return true;
}

static uint64_t GetMinimumTxSize(const Consensus::Params &params, int nHeightPrev, int64_t nMedianTimePastPrev) {
    if (IsUpgrade9Enabled(params, nMedianTimePastPrev)) {
        return MIN_TX_SIZE_UPGRADE9;
    }
    if (IsMagneticAnomalyEnabled(params, nHeightPrev)) {
        return MIN_TX_SIZE_MAGNETIC_ANOMALY;
    }
    return 0;
}

uint64_t GetMinimumTxSize(const Consensus::Params &params, const CBlockIndex *pindexPrev) {
    if (!pindexPrev) return 0;
    return GetMinimumTxSize(params, pindexPrev->nHeight, pindexPrev->GetMedianTimePast());
}

bool ContextualCheckTransaction(const Consensus::Params &params,
                                const CTransaction &tx, CValidationState &state,
                                int nHeight, int64_t nLockTimeCutoff,
                                int64_t nMedianTimePastPrev) {
    if (!IsFinalTx(tx, nHeight, nLockTimeCutoff)) {
        // While this is only one transaction, we use txns in the error to
        // ensure continuity with other clients.
        return state.DoS(10, false, REJECT_INVALID, "bad-txns-nonfinal", false,
                         "non-final transaction");
    }

    // Enforce minimum tx size, if any
    // Note: nHeight is height of *this* block (and nMedianTimePastPrev is MTP of *prev* block),
    // but Is*Enabled() expects nHeight and MTP of *prev* block.
    const uint64_t minTxSize = GetMinimumTxSize(params, nHeight - 1, nMedianTimePastPrev);
    if (minTxSize && ::GetSerializeSize(tx, PROTOCOL_VERSION) < minTxSize) {
        return state.DoS(100, false, REJECT_INVALID, "bad-txns-undersize");
    }

    if (IsUpgrade9Enabled(params, nMedianTimePastPrev)) {
        // CHIP 2021-01 Restrict Transaction Version
        if (tx.nVersion > CTransaction::MAX_CONSENSUS_VERSION || tx.nVersion < CTransaction::MIN_CONSENSUS_VERSION) {
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-version");
        }
    }

    return true;
}

/**
 * Calculates the block height and previous block's median time past at
 * which the transaction will be considered final in the context of BIP 68.
 * Also removes from the vector of input heights any entries which did not
 * correspond to sequence locked inputs as they do not affect the calculation.
 */
std::pair<int, int64_t> CalculateSequenceLocks(const CTransaction &tx,
                                               int flags,
                                               std::vector<int> *prevHeights,
                                               const CBlockIndex &block) {
    assert(prevHeights->size() == tx.vin.size());

    // Will be set to the equivalent height- and time-based nLockTime
    // values that would be necessary to satisfy all relative lock-
    // time constraints given our view of block chain history.
    // The semantics of nLockTime are the last invalid height/time, so
    // use -1 to have the effect of any height or time being valid.
    int nMinHeight = -1;
    int64_t nMinTime = -1;

    // tx.nVersion is signed integer so requires cast to unsigned otherwise
    // we would be doing a signed comparison and half the range of nVersion
    // wouldn't support BIP 68.
    bool fEnforceBIP68 = static_cast<uint32_t>(tx.nVersion) >= 2 &&
                         flags & LOCKTIME_VERIFY_SEQUENCE;

    // Do not enforce sequence numbers as a relative lock time
    // unless we have been instructed to
    if (!fEnforceBIP68) {
        return std::make_pair(nMinHeight, nMinTime);
    }

    for (size_t txinIndex = 0; txinIndex < tx.vin.size(); txinIndex++) {
        const CTxIn &txin = tx.vin[txinIndex];

        // Sequence numbers with the most significant bit set are not
        // treated as relative lock-times, nor are they given any
        // consensus-enforced meaning at this point.
        if (txin.nSequence & CTxIn::SEQUENCE_LOCKTIME_DISABLE_FLAG) {
            // The height of this input is not relevant for sequence locks
            (*prevHeights)[txinIndex] = 0;
            continue;
        }

        int nCoinHeight = (*prevHeights)[txinIndex];

        if (txin.nSequence & CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG) {
            int64_t nCoinTime = block.GetAncestor(std::max(nCoinHeight - 1, 0))
                                    ->GetMedianTimePast();
            // NOTE: Subtract 1 to maintain nLockTime semantics.
            // BIP 68 relative lock times have the semantics of calculating the
            // first block or time at which the transaction would be valid. When
            // calculating the effective block time or height for the entire
            // transaction, we switch to using the semantics of nLockTime which
            // is the last invalid block time or height. Thus we subtract 1 from
            // the calculated time or height.

            // Time-based relative lock-times are measured from the smallest
            // allowed timestamp of the block containing the txout being spent,
            // which is the median time past of the block prior.
            nMinTime = std::max(
                nMinTime,
                nCoinTime +
                    int64_t((txin.nSequence & CTxIn::SEQUENCE_LOCKTIME_MASK)
                            << CTxIn::SEQUENCE_LOCKTIME_GRANULARITY) -
                    1);
        } else {
            nMinHeight = std::max(
                nMinHeight,
                nCoinHeight +
                    int(txin.nSequence & CTxIn::SEQUENCE_LOCKTIME_MASK) - 1);
        }
    }

    return std::make_pair(nMinHeight, nMinTime);
}

bool EvaluateSequenceLocks(const CBlockIndex &block,
                           std::pair<int, int64_t> lockPair) {
    assert(block.pprev);
    int64_t nBlockTime = block.pprev->GetMedianTimePast();
    if (lockPair.first >= block.nHeight || lockPair.second >= nBlockTime) {
        return false;
    }

    return true;
}

bool SequenceLocks(const CTransaction &tx, int flags,
                   std::vector<int> *prevHeights, const CBlockIndex &block) {
    return EvaluateSequenceLocks(
        block, CalculateSequenceLocks(tx, flags, prevHeights, block));
}

namespace Consensus {
bool CheckTxInputs(const CTransaction &tx, CValidationState &state,
                   const CCoinsViewCache &inputs, int nSpendHeight,
                   Amount &txfee) {
    assert(!tx.IsCoinBase()); // precondition of this function

    Amount nValueIn = Amount::zero();
    for (const auto &in : tx.vin) {
        const COutPoint &prevout = in.prevout;
        const Coin &coin = inputs.AccessCoin(prevout);

        // Is the actual input available?
        if (coin.IsSpent()) {
            return state.DoS(100, false, REJECT_INVALID,
                             "bad-txns-inputs-missingorspent", false,
                             strprintf("%s: inputs missing/spent", __func__));
        }

        // If prev is coinbase, check that it's matured
        if (coin.IsCoinBase() &&
            nSpendHeight - coin.GetHeight() < COINBASE_MATURITY) {
            return state.Invalid(
                false, REJECT_INVALID, "bad-txns-premature-spend-of-coinbase",
                strprintf("tried to spend coinbase at depth %d",
                          nSpendHeight - coin.GetHeight()));
        }

        // Fail now if the locking script is guaranteed to fail evaluation later on in the pipeline
        if (coin.GetTxOut().scriptPubKey.IsUnspendable()) {
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-input-scriptpubkey-unspendable", false,
                             strprintf("%s: input scriptPubKey is unspendable", __func__));
        }

        // Check for negative or overflow input values
        nValueIn += coin.GetTxOut().nValue;
        if (!MoneyRange(coin.GetTxOut().nValue) || !MoneyRange(nValueIn)) {
            return state.DoS(100, false, REJECT_INVALID,
                             "bad-txns-inputvalues-outofrange");
        }
    }

    const Amount value_out = tx.GetValueOut();
    if (nValueIn < value_out) {
        return state.DoS(
            100, false, REJECT_INVALID, "bad-txns-in-belowout", false,
            strprintf("value in (%s) < value out (%s)", FormatMoney(nValueIn),
                      FormatMoney(value_out)));
    }

    // Tally transaction fees
    const Amount txfee_aux = nValueIn - value_out;
    if (!MoneyRange(txfee_aux)) {
        return state.DoS(100, false, REJECT_INVALID, "bad-txns-fee-outofrange");
    }

    txfee = txfee_aux;
    return true;
}
} // namespace Consensus
