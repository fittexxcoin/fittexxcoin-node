// Copyright (c) 2011-2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <key.h>
#include <keystore.h>
#include <policy/policy.h>
#include <script/interpreter.h>
#include <script/ismine.h>
#include <script/script.h>
#include <script/script_error.h>
#include <script/sighashtype.h>
#include <script/sign.h>
#include <script/standard.h>
#include <tinyformat.h>
#include <uint256.h>

#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(multisig_tests, BasicTestingSetup)

static CScript sign_multisig(const CScript &scriptPubKey,
                             const std::vector<CKey> &keys,
                             const CMutableTransaction &tx, int whichIn) {
    const CTxOut fakeUtxo{Amount::zero(), scriptPubKey};
    const ScriptExecutionContext limitedContext{unsigned(whichIn), fakeUtxo, tx};
    uint256 hash = SignatureHash(scriptPubKey, limitedContext, SigHashType(), nullptr, STANDARD_SCRIPT_VERIFY_FLAGS);

    CScript result;
    // CHECKMULTISIG bug workaround
    result << OP_0;
    for (const CKey &key : keys) {
        std::vector<uint8_t> vchSig;
        BOOST_CHECK(key.SignECDSA(hash, vchSig));
        vchSig.push_back(uint8_t(SIGHASH_ALL));
        result << vchSig;
    }
    return result;
}

BOOST_AUTO_TEST_CASE(multisig_verify) {
    uint32_t flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_STRICTENC;

    ScriptError err;
    CKey key[4];
    Amount amount = Amount::zero();
    for (int i = 0; i < 4; i++) {
        key[i].MakeNewKey(true);
    }

    CScript a_and_b;
    a_and_b << OP_2 << ToByteVector(key[0].GetPubKey())
            << ToByteVector(key[1].GetPubKey()) << OP_2 << OP_CHECKMULTISIG;

    CScript a_or_b;
    a_or_b << OP_1 << ToByteVector(key[0].GetPubKey())
           << ToByteVector(key[1].GetPubKey()) << OP_2 << OP_CHECKMULTISIG;

    CScript escrow;
    escrow << OP_2 << ToByteVector(key[0].GetPubKey())
           << ToByteVector(key[1].GetPubKey())
           << ToByteVector(key[2].GetPubKey()) << OP_3 << OP_CHECKMULTISIG;

    // Funding transaction
    CMutableTransaction txFrom;
    txFrom.vout.resize(3);
    txFrom.vout[0].scriptPubKey = a_and_b;
    txFrom.vout[1].scriptPubKey = a_or_b;
    txFrom.vout[2].scriptPubKey = escrow;

    // Spending transaction
    CMutableTransaction txTo[3];
    for (int i = 0; i < 3; i++) {
        txTo[i].vin.resize(1);
        txTo[i].vout.resize(1);
        txTo[i].vin[0].prevout = COutPoint(txFrom.GetId(), i);
        txTo[i].vout[0].nValue = SATOSHI;
    }

    std::vector<CKey> keys;
    CScript s;

    // Test a AND b:
    keys.assign(1, key[0]);
    keys.push_back(key[1]);
    s = sign_multisig(a_and_b, keys, txTo[0], 0);
    BOOST_CHECK(VerifyScript(s, a_and_b, flags,
                             TransactionSignatureChecker(ScriptExecutionContext{0, CTxOut(amount, a_and_b), txTo[0]}),
                             &err));
    BOOST_CHECK_MESSAGE(err == ScriptError::OK, ScriptErrorString(err));

    for (int i = 0; i < 4; i++) {
        keys.assign(1, key[i]);
        s = sign_multisig(a_and_b, keys, txTo[0], 0);
        BOOST_CHECK_MESSAGE( ! VerifyScript(s, a_and_b, flags,
                                            TransactionSignatureChecker(ScriptExecutionContext{0,
                                                                                               CTxOut(amount, a_and_b),
                                                                                               txTo[0]}),
                                            &err),
            strprintf("a&b 1: %d", i));
        BOOST_CHECK_MESSAGE(err == ScriptError::INVALID_STACK_OPERATION, ScriptErrorString(err));

        keys.assign(1, key[1]);
        keys.push_back(key[i]);
        s = sign_multisig(a_and_b, keys, txTo[0], 0);
        BOOST_CHECK_MESSAGE( ! VerifyScript(s, a_and_b, flags,
                                            TransactionSignatureChecker(ScriptExecutionContext{0,
                                                                                               CTxOut(amount, a_and_b),
                                                                                               txTo[0]}),
                                            &err),
            strprintf("a&b 2: %d", i));
        BOOST_CHECK_MESSAGE(err == ScriptError::EVAL_FALSE, ScriptErrorString(err));
    }

    // Test a OR b:
    for (int i = 0; i < 4; i++) {
        keys.assign(1, key[i]);
        s = sign_multisig(a_or_b, keys, txTo[1], 0);
        if (i == 0 || i == 1) {
            BOOST_CHECK_MESSAGE(VerifyScript(s, a_or_b, flags,
                                             TransactionSignatureChecker(ScriptExecutionContext{0,
                                                                                                CTxOut(amount, a_or_b),
                                                                                                txTo[1]}),
                                             &err),
                                strprintf("a|b: %d", i));
            BOOST_CHECK_MESSAGE(err == ScriptError::OK, ScriptErrorString(err));
        } else {
            BOOST_CHECK_MESSAGE( ! VerifyScript(s, a_or_b, flags,
                                                TransactionSignatureChecker(ScriptExecutionContext{0,
                                                                                                   CTxOut(amount, a_or_b),
                                                                                                   txTo[1]}),
                                                &err),
                                strprintf("a|b: %d", i));
            BOOST_CHECK_MESSAGE(err == ScriptError::EVAL_FALSE, ScriptErrorString(err));
        }
    }
    s.clear();
    s << OP_0 << OP_1;
    BOOST_CHECK( ! VerifyScript(s, a_or_b, flags, TransactionSignatureChecker(ScriptExecutionContext{0, CTxOut(amount, a_or_b), txTo[1]}),
                                                                              &err));
    BOOST_CHECK_MESSAGE(err == ScriptError::SIG_DER, ScriptErrorString(err));

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            keys.assign(1, key[i]);
            keys.push_back(key[j]);
            s = sign_multisig(escrow, keys, txTo[2], 0);
            if (i < j && i < 3 && j < 3) {
                BOOST_CHECK_MESSAGE(VerifyScript(s, escrow, flags,
                                                 TransactionSignatureChecker(ScriptExecutionContext{0,
                                                                                                    CTxOut(amount, escrow),
                                                                                                    txTo[2]}),
                                                 &err),
                    strprintf("escrow 1: %d %d", i, j));
                BOOST_CHECK_MESSAGE(err == ScriptError::OK, ScriptErrorString(err));
            } else {
                BOOST_CHECK_MESSAGE( ! VerifyScript(s, escrow, flags,
                                                    TransactionSignatureChecker(ScriptExecutionContext{0,
                                                                                                       CTxOut(amount, escrow),
                                                                                                       txTo[2]}),
                                                    &err),
                    strprintf("escrow 2: %d %d", i, j));
                BOOST_CHECK_MESSAGE(err == ScriptError::EVAL_FALSE, ScriptErrorString(err));
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(multisig_IsStandard) {
    const uint32_t flags = STANDARD_SCRIPT_VERIFY_FLAGS & ~SCRIPT_ENABLE_P2SH_32; // no p2sh_32

    CKey key[4];
    for (int i = 0; i < 4; i++) {
        key[i].MakeNewKey(true);
    }

    txnouttype whichType;

    CScript a_and_b;
    a_and_b << OP_2 << ToByteVector(key[0].GetPubKey())
            << ToByteVector(key[1].GetPubKey()) << OP_2 << OP_CHECKMULTISIG;
    BOOST_CHECK(::IsStandard(a_and_b, whichType, flags));

    CScript a_or_b;
    a_or_b << OP_1 << ToByteVector(key[0].GetPubKey())
           << ToByteVector(key[1].GetPubKey()) << OP_2 << OP_CHECKMULTISIG;
    BOOST_CHECK(::IsStandard(a_or_b, whichType, flags));

    CScript escrow;
    escrow << OP_2 << ToByteVector(key[0].GetPubKey())
           << ToByteVector(key[1].GetPubKey())
           << ToByteVector(key[2].GetPubKey()) << OP_3 << OP_CHECKMULTISIG;
    BOOST_CHECK(::IsStandard(escrow, whichType, flags));

    CScript one_of_four;
    one_of_four << OP_1 << ToByteVector(key[0].GetPubKey())
                << ToByteVector(key[1].GetPubKey())
                << ToByteVector(key[2].GetPubKey())
                << ToByteVector(key[3].GetPubKey()) << OP_4 << OP_CHECKMULTISIG;
    BOOST_CHECK(!::IsStandard(one_of_four, whichType, flags));

    CScript malformed[6];
    malformed[0] << OP_3 << ToByteVector(key[0].GetPubKey())
                 << ToByteVector(key[1].GetPubKey()) << OP_2
                 << OP_CHECKMULTISIG;
    malformed[1] << OP_2 << ToByteVector(key[0].GetPubKey())
                 << ToByteVector(key[1].GetPubKey()) << OP_3
                 << OP_CHECKMULTISIG;
    malformed[2] << OP_0 << ToByteVector(key[0].GetPubKey())
                 << ToByteVector(key[1].GetPubKey()) << OP_2
                 << OP_CHECKMULTISIG;
    malformed[3] << OP_1 << ToByteVector(key[0].GetPubKey())
                 << ToByteVector(key[1].GetPubKey()) << OP_0
                 << OP_CHECKMULTISIG;
    malformed[4] << OP_1 << ToByteVector(key[0].GetPubKey())
                 << ToByteVector(key[1].GetPubKey()) << OP_CHECKMULTISIG;
    malformed[5] << OP_1 << ToByteVector(key[0].GetPubKey())
                 << ToByteVector(key[1].GetPubKey());

    for (int i = 0; i < 6; i++) {
        BOOST_CHECK(!::IsStandard(malformed[i], whichType, flags));
    }
}

BOOST_AUTO_TEST_CASE(multisig_Sign) {
    // Test SignSignature() (and therefore the version of Solver() that signs
    // transactions)
    const uint32_t flags = STANDARD_SCRIPT_VERIFY_FLAGS & ~SCRIPT_ENABLE_P2SH_32; // no p2sh_32
    CBasicKeyStore keystore;
    CKey key[4];
    for (int i = 0; i < 4; i++) {
        key[i].MakeNewKey(true);
        BOOST_CHECK(keystore.AddKey(key[i]));
    }

    CScript a_and_b;
    a_and_b << OP_2 << ToByteVector(key[0].GetPubKey())
            << ToByteVector(key[1].GetPubKey()) << OP_2 << OP_CHECKMULTISIG;

    CScript a_or_b;
    a_or_b << OP_1 << ToByteVector(key[0].GetPubKey())
           << ToByteVector(key[1].GetPubKey()) << OP_2 << OP_CHECKMULTISIG;

    CScript escrow;
    escrow << OP_2 << ToByteVector(key[0].GetPubKey())
           << ToByteVector(key[1].GetPubKey())
           << ToByteVector(key[2].GetPubKey()) << OP_3 << OP_CHECKMULTISIG;

    // Funding transaction
    CMutableTransaction txFrom;
    txFrom.vout.resize(3);
    txFrom.vout[0].scriptPubKey = a_and_b;
    txFrom.vout[1].scriptPubKey = a_or_b;
    txFrom.vout[2].scriptPubKey = escrow;

    // Spending transaction
    CMutableTransaction txTo[3];
    for (int i = 0; i < 3; i++) {
        txTo[i].vin.resize(1);
        txTo[i].vout.resize(1);
        txTo[i].vin[0].prevout = COutPoint(txFrom.GetId(), i);
        txTo[i].vout[0].nValue = SATOSHI;
    }

    auto const null_context = std::nullopt;
    for (int i = 0; i < 3; i++) {
        BOOST_CHECK_MESSAGE(SignSignature(keystore, CTransaction(txFrom),
                                          txTo[i], 0,
                                          SigHashType().withFork(), flags, null_context),
                            strprintf("SignSignature %d", i));
    }
}

BOOST_AUTO_TEST_SUITE_END()
