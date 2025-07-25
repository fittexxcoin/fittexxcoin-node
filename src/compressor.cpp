// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Fittexxcoin Core developers
// Copyright (c) 2017-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <compressor.h>

#include <hash.h>
#include <pubkey.h>
#include <script/standard.h>

#include <cstring>
#include <utility>

/*
 * These check for scripts for which a special case with a shorter encoding is
 * defined. They are implemented separately from the CScript test, as these test
 * for exact byte sequence correspondences, and are more strict. For example,
 * IsToPubKey also verifies whether the public key is valid (as invalid ones
 * cannot be represented in compressed form).
 */

static bool IsToKeyID(const CScript &script, CKeyID &hash) {
    if (script.IsPayToPubKeyHash()) {
        static_assert(CKeyID::size() == 20);
        std::memcpy(&*hash.begin(), &script[3], 20);
        return true;
    }
    return false;
}

static bool IsToScriptID(const CScript &script, ScriptID &hash) {
    if (std::vector<uint8_t> payload; script.IsPayToScriptHash(0 /* no p2psh_32 */, &payload)) {
        if (payload.size() == uint160::size()) {
            hash = uint160{payload};
        } else if (payload.size() == uint256::size()) {
            // not reached in current code, but left in for future expansion
            assert(!"Current code should not be compressing p2sh_32 in TxOutCompression");
            hash = uint256{payload};
        } else {
            assert(!"Unexpected ScriptID payload size: expected a payload of size 20 or 32 bytes");
            return false; // not reached
        }
        return true;
    }
    return false;
}

static bool IsToPubKey(const CScript &script, CPubKey &pubkey) {
    if (script.size() == 35 && script[0] == 33 && script[34] == OP_CHECKSIG &&
        (script[1] == 0x02 || script[1] == 0x03)) {
        pubkey.Set(&script[1], &script[34]);
        return true;
    }
    if (script.size() == 67 && script[0] == 65 && script[66] == OP_CHECKSIG &&
        script[1] == 0x04) {
        pubkey.Set(&script[1], &script[66]);
        // if not fully valid, a case that would not be compressible
        return pubkey.IsFullyValid();
    }
    return false;
}

bool CompressScript(const CScript &script, std::vector<uint8_t> &out) {
    CKeyID keyID;
    if (IsToKeyID(script, keyID)) {
        out.resize(21);
        out[0] = 0x00;
        static_assert(keyID.size() == 20);
        std::memcpy(&out[1], keyID.data(), 20);
        return true;
    }
    ScriptID scriptID;
    if (IsToScriptID(script, scriptID)) {
        // Note: the scriptID will always be of size() == 20 here in current
        // code. If we wanted to add p2sh_32 support, we should just remove
        // this assert() and add another special script byte (maybe 0x6) to
        // indicate p2sh_32, and bump nSpecialScripts. Note that doing that
        // *would* break txdb and undo file compatibility, however!
        assert(scriptID.IsP2SH_20() && scriptID.size() == 20);
        out.resize(scriptID.size() + 1u);
        out[0] = 0x01; // 0x1 == p2sh_20
        std::memcpy(&out[1], std::as_const(scriptID).data(), scriptID.size());
        return true;
    }
    CPubKey pubkey;
    if (IsToPubKey(script, pubkey)) {
        out.resize(33);
        std::memcpy(&out[1], &pubkey[1], 32);
        if (pubkey[0] == 0x02 || pubkey[0] == 0x03) {
            out[0] = pubkey[0];
            return true;
        } else if (pubkey[0] == 0x04) {
            out[0] = 0x04 | (pubkey[64] & 0x01);
            return true;
        }
    }
    return false;
}

unsigned int GetSpecialScriptSize(unsigned int nSize) {
    if (nSize == 0 || nSize == 1) {
        return 20;
    }
    if (nSize == 2 || nSize == 3 || nSize == 4 || nSize == 5) {
        return 32;
    }
    return 0;
}

bool DecompressScript(CScript &script, unsigned int nSize, const std::vector<uint8_t> &in) {
    switch (nSize) {
        case 0x00: // p2pkh
            script.resize(25);
            script[0] = OP_DUP;
            script[1] = OP_HASH160;
            script[2] = 20;
            std::memcpy(&script[3], in.data(), 20);
            script[23] = OP_EQUALVERIFY;
            script[24] = OP_CHECKSIG;
            return true;
        case 0x01: // p2sh_20
            assert(in.size() == uint160::size()); // 20 bytes expected
            script.resize(in.size() + 3);
            script[0] = OP_HASH160; // if adding p2sh_32, add conditional for OP_HASH256 here.
            script[1] = in.size();
            std::memcpy(&script[2], in.data(), in.size());
            script[in.size() + 2] = OP_EQUAL;
            return true;
        case 0x02:
        case 0x03:
            script.resize(35);
            script[0] = 33;
            script[1] = nSize;
            std::memcpy(&script[2], in.data(), 32);
            script[34] = OP_CHECKSIG;
            return true;
        case 0x04:
        case 0x05:
            uint8_t vch[33] = {};
            vch[0] = nSize - 2;
            std::memcpy(&vch[1], in.data(), 32);
            CPubKey pubkey(&vch[0], &vch[33]);
            if (!pubkey.Decompress()) {
                return false;
            }
            assert(pubkey.size() == 65);
            script.resize(67);
            script[0] = 65;
            std::memcpy(&script[1], pubkey.data(), 65);
            script[66] = OP_CHECKSIG;
            return true;
    }
    return false;
}

// Amount compression:
// * If the amount is 0, output 0
// * first, divide the amount (in base units) by the largest power of 10
// possible; call the exponent e (e is max 9)
// * if e<9, the last digit of the resulting number cannot be 0; store it as d,
// and drop it (divide by 10)
//   * call the result n
//   * output 1 + 10*(9*n + d - 1) + e
// * if e==9, we only know the resulting number is not zero, so output 1 + 10*(n
// - 1) + 9
// (this is decodable, as d is in [1-9] and e is in [0-9])
uint64_t CompressAmount(Amount amt) {
    uint64_t n = amt / SATOSHI;
    if (n == 0) {
        return 0;
    }
    int e = 0;
    while (((n % 10) == 0) && e < 9) {
        n /= 10;
        e++;
    }
    if (e < 9) {
        int d = (n % 10);
        assert(d >= 1 && d <= 9);
        n /= 10;
        return 1 + (n * 9 + d - 1) * 10 + e;
    } else {
        return 1 + (n - 1) * 10 + 9;
    }
}

Amount DecompressAmount(uint64_t x) {
    // x = 0  OR  x = 1+10*(9*n + d - 1) + e  OR  x = 1+10*(n - 1) + 9
    if (x == 0) {
        return Amount::zero();
    }
    x--;
    // x = 10*(9*n + d - 1) + e
    int e = x % 10;
    x /= 10;
    uint64_t n = 0;
    if (e < 9) {
        // x = 9*n + d - 1
        int d = (x % 9) + 1;
        x /= 9;
        // x = n
        n = x * 10 + d;
    } else {
        n = x + 1;
    }
    while (e) {
        n *= 10;
        e--;
    }
    return int64_t(n) * SATOSHI;
}
