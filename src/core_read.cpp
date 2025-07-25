// Copyright (c) 2009-2016 The Fittexxcoin Core developers
// Copyright (c) 2020-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <core_io.h>

#include <primitives/block.h>
#include <primitives/transaction.h>
#include <psbt.h>
#include <script/script.h>
#include <script/sign.h>
#include <serialize.h>
#include <streams.h>
#include <util/strencodings.h>
#include <util/string.h>
#include <util/system.h>
#include <version.h>

#include <algorithm>

CScript ParseScript(const std::string &s) {
    CScript result;

    static std::map<std::string, opcodetype> mapOpNames;

    if (mapOpNames.empty()) {
        for (int op = 0; op < FIRST_UNDEFINED_OP_VALUE; op++) {
            if (op < OP_PUSHDATA1) {
                continue;
            }

            const char *name = GetOpName(static_cast<opcodetype>(op));
            if (strcmp(name, "OP_UNKNOWN") == 0) {
                continue;
            }

            std::string strName(name);
            mapOpNames[strName] = static_cast<opcodetype>(op);
            // Convenience: OP_ADD and just ADD are both recognized:
            if (strName.substr(0,3) == "OP_") strName.erase(0, 3);

            mapOpNames[strName] = static_cast<opcodetype>(op);
        }
    }

    std::vector<std::string> words;
    Split(words, s, " \t\n", true);

    size_t push_size = 0, next_push_size = 0;
    size_t script_size = 0;
    // Deal with PUSHDATA1 operation with some more hacks.
    size_t push_data_size = 0;

    for (const auto &w : words) {
        if (w.empty()) {
            // Empty string, ignore. Split given '' will return one
            // word)
            continue;
        }

        // Update script size.
        script_size = result.size();

        // Make sure we keep track of the size of push operations.
        push_size = next_push_size;
        next_push_size = 0;

        // Decimal numbers
        if (std::all_of(w.begin(), w.end(), ::IsDigit) ||
            (w.front() == '-' && w.size() > 1 &&
             std::all_of(w.begin() + 1, w.end(), ::IsDigit))) {
            // Number
            int64_t n = atoi64(w);
            auto res = ScriptInt::fromInt(n);
            if ( ! res) {
                throw std::runtime_error("-9223372036854775808 is a forbidden value");
            }
            result << *res;
            goto next;
        }

        // Hex Data
        if (w.substr(0, 2) == "0x" && w.size() > 2) {
            if (!IsHex(std::string(w.begin() + 2, w.end()))) {
                // Should only arrive here for improperly formatted hex values
                throw std::runtime_error("Hex numbers expected to be formatted "
                                         "in full-byte chunks (ex: 0x00 "
                                         "instead of 0x0)");
            }

            // Raw hex data, inserted NOT pushed onto stack:
            std::vector<uint8_t> raw = ParseHex(std::string(w.begin() + 2, w.end()));
            result.insert(result.end(), raw.begin(), raw.end());
            goto next;
        }

        if (w.size() >= 2 && w.front() == '\'' && w.back() == '\'') {
            // Single-quoted string, pushed as data. NOTE: this is poor-man's
            // parsing, spaces/tabs/newlines in single-quoted strings won't
            // work.
            std::vector<uint8_t> value(w.begin() + 1, w.end() - 1);
            result << value;
            goto next;
        }

        if (mapOpNames.count(w)) {
            // opcode, e.g. OP_ADD or ADD:
            opcodetype op = mapOpNames[w];

            result << op;
            goto next;
        }

        throw std::runtime_error("Error parsing script: " + s);

    next:
        size_t size_change = result.size() - script_size;

        // If push_size is set, ensure have added the right amount of stuff.
        if (push_size != 0 && size_change != push_size) {
            throw std::runtime_error(
                "Wrong number of bytes being pushed. Expected:" +
                std::to_string(push_size) +
                " Pushed:" + std::to_string(size_change));
        }

        // If push_size is set, and we have push_data_size set, then we have a
        // PUSHDATAX opcode.  We need to read it's push size as a LE value for
        // the next iteration of this loop.
        if (push_size != 0 && push_data_size != 0) {
            auto offset = &result[script_size];

            // Push data size is not a CScriptNum (Because it is
            // 2's-complement instead of 1's complement).  We need to use
            // ReadLE(N) instead of converting to a CScriptNum.
            if (push_data_size == 1) {
                next_push_size = *offset;
            } else if (push_data_size == 2) {
                next_push_size = ReadLE16(offset);
            } else if (push_data_size == 4) {
                next_push_size = ReadLE32(offset);
            }

            push_data_size = 0;
        }

        // If push_size is unset, but size_change is 1, that means we have an
        // opcode in the form of `0x00` or <opcodename>.  We will check to see
        // if it is a push operation and set state accordingly
        if (push_size == 0 && size_change == 1) {
            opcodetype op = opcodetype(*result.rbegin());

            // If we have what looks like an immediate push, figure out its
            // size.
            if (op < OP_PUSHDATA1) {
                next_push_size = op;
                continue;
            }

            switch (op) {
                case OP_PUSHDATA1:
                    push_data_size = next_push_size = 1;
                    break;
                case OP_PUSHDATA2:
                    push_data_size = next_push_size = 2;
                    break;
                case OP_PUSHDATA4:
                    push_data_size = next_push_size = 4;
                    break;
                default:
                    break;
            }
        }
    }

    return result;
}

bool DecodeHexTx(CMutableTransaction &tx, const std::string &strHexTx) {
    if (!IsHex(strHexTx)) {
        return false;
    }

    std::vector<uint8_t> txData(ParseHex(strHexTx));

    CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
    try {
        ssData >> tx;
        if (ssData.eof()) {
            return true;
        }
    } catch (const std::exception &e) {
        // Fall through.
    }

    return false;
}

bool DecodeHexBlockHeader(CBlockHeader &header, const std::string &hex_header) {
    if (!IsHex(hex_header)) {
        return false;
    }

    const std::vector<uint8_t> header_data{ParseHex(hex_header)};
    CDataStream ser_header(header_data, SER_NETWORK, PROTOCOL_VERSION);
    try {
        ser_header >> header;
    } catch (const std::exception &) {
        return false;
    }
    return true;
}

bool DecodeHexBlk(CBlock &block, const std::string &strHexBlk) {
    if (!IsHex(strHexBlk)) {
        return false;
    }

    std::vector<uint8_t> blockData(ParseHex(strHexBlk));
    CDataStream ssBlock(blockData, SER_NETWORK, PROTOCOL_VERSION);
    try {
        ssBlock >> block;
    } catch (const std::exception &) {
        return false;
    }

    return true;
}

bool DecodePSBT(PartiallySignedTransaction &psbt, const std::string &base64_tx,
                std::string &error) {
    bool base64_invalid = false;
    std::vector<uint8_t> tx_data = DecodeBase64(base64_tx.c_str(), &base64_invalid);
    if (base64_invalid) {
        error = "invalid base64";
        return false;
    }

    CDataStream ss_data(tx_data, SER_NETWORK, PROTOCOL_VERSION);
    try {
        ss_data >> psbt;
        if (!ss_data.empty()) {
            error = "extra data after PSBT";
            return false;
        }
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
    return true;
}

bool ParseHashStr(const std::string &strHex, uint256 &result) {
    if ((strHex.size() != 64) || !IsHex(strHex)) {
        return false;
    }

    result.SetHex(strHex);
    return true;
}

bool ParseHashStr(const std::string &strHex, uint160 &result) {
    if ((strHex.size() != 40) || !IsHex(strHex)) {
        return false;
    }
    result.SetHex(strHex);
    return true;
}

std::vector<uint8_t> ParseHexUV(const UniValue &v, const std::string &strName) {
    std::string strHex;
    if (v.isStr()) {
        strHex = v.getValStr();
    }

    if (!IsHex(strHex)) {
        throw std::runtime_error(
            strName + " must be hexadecimal string (not '" + strHex + "')");
    }

    return ParseHex(strHex);
}

SigHashType ParseSighashString(const UniValue &sighash) {
    SigHashType sigHashType = SigHashType().withFork();
    if (!sighash.isNull()) {
        static std::map<std::string, int> map_sighash_values = {
            {"ALL", SIGHASH_ALL},
            {"ALL|ANYONECANPAY", SIGHASH_ALL | SIGHASH_ANYONECANPAY},
            {"ALL|UTXOS", SIGHASH_ALL | SIGHASH_UTXOS},

            {"ALL|FORKID", SIGHASH_ALL | SIGHASH_FORKID},
            {"ALL|FORKID|ANYONECANPAY", SIGHASH_ALL | SIGHASH_FORKID | SIGHASH_ANYONECANPAY},
            {"ALL|FORKID|UTXOS", SIGHASH_ALL | SIGHASH_FORKID | SIGHASH_UTXOS},

            {"NONE", SIGHASH_NONE },
            {"NONE|ANYONECANPAY", SIGHASH_NONE | SIGHASH_ANYONECANPAY},
            {"NONE|UTXOS", SIGHASH_NONE | SIGHASH_UTXOS},

            {"NONE|FORKID", SIGHASH_NONE | SIGHASH_FORKID},
            {"NONE|FORKID|ANYONECANPAY", SIGHASH_NONE | SIGHASH_FORKID | SIGHASH_ANYONECANPAY},
            {"NONE|FORKID|UTXOS", SIGHASH_NONE | SIGHASH_FORKID | SIGHASH_UTXOS},

            {"SINGLE", SIGHASH_SINGLE},
            {"SINGLE|ANYONECANPAY", SIGHASH_SINGLE | SIGHASH_ANYONECANPAY},
            {"SINGLE|UTXOS", SIGHASH_SINGLE | SIGHASH_UTXOS},

            {"SINGLE|FORKID", SIGHASH_SINGLE | SIGHASH_FORKID},
            {"SINGLE|FORKID|ANYONECANPAY", SIGHASH_SINGLE | SIGHASH_FORKID | SIGHASH_ANYONECANPAY},
            {"SINGLE|FORKID|UTXOS", SIGHASH_SINGLE | SIGHASH_FORKID | SIGHASH_UTXOS},
        };
        std::string strHashType = sighash.get_str();
        const auto &it = map_sighash_values.find(strHashType);
        if (it != map_sighash_values.end()) {
            sigHashType = SigHashType(it->second);
        } else {
            throw std::runtime_error(strHashType +
                                     " is not a valid sighash parameter.");
        }
    }
    return sigHashType;
}

token::OutputData DecodeTokenDataUV(const UniValue &obj) {
    token::Id category;
    token::SafeAmount amount;
    bool hasNFT{}, isMutable{}, isMinting{};
    token::NFTCommitment comm;

    if (!obj.isObject()) throw std::runtime_error("Bad tokenData; expected JSON object");
    const UniValue::Object &o = obj.get_obj();

    if (auto *val = o.locate("category")) {
        if (!ParseHashStr(val->get_str(), category)) {
            throw std::runtime_error("Parse error for \"category\"");
        }
    } else {
        throw std::runtime_error("Missing \"category\" in tokenData");
    }
    if (auto *val = o.locate("amount")) {
        // may be a string (to encode very large amounts) or an integer
        amount = DecodeSafeAmount(*val);
    }

    if (auto *val = o.locate("nft")) {
        if (!val->isObject()) throw std::runtime_error("Bad tokenData; expected JSON object for the \"nft\" key");
        const UniValue::Object &o_nft = val->get_obj();

        hasNFT = true;

        if ((val = o_nft.locate("capability"))) { // optional, defaults to "none"
            if (const auto lc_cap = ToLower(val->get_str()); lc_cap == "none") { /* pass */}
            else if (lc_cap == "mutable") isMutable = true;
            else if (lc_cap == "minting") isMinting = true;
            else {
                throw std::runtime_error("Invalid \"capability\" in tokenData; must be one of: "
                                         "\"none\", \"minting\", or \"mutable\"");
            }
        }

        if ((val = o_nft.locate("commitment"))) { // optional, defaults to "empty"
            std::vector<uint8_t> vec;
            if (!IsHex(val->get_str())
                    || (vec = ParseHex(val->get_str())).size() > token::MAX_CONSENSUS_COMMITMENT_LENGTH) {
                throw std::runtime_error("Invalid \"commitment\" in tokenData");
            }
            comm.assign(vec.begin(), vec.end());
        }
    }

    if (!hasNFT && amount.getint64() == 0) {
        throw std::runtime_error("Fungible amount must be >0 for fungible-only tokens");
    }

    token::OutputData ret(category, amount, comm);
    ret.SetNFT(hasNFT, isMutable, isMinting);

    if (!ret.IsValidBitfield()) {
        throw std::runtime_error(strprintf("Invalid bitfield: %x", ret.GetBitfieldByte()));
    }

    return ret;
}

token::SafeAmount DecodeSafeAmount(const UniValue &obj) {
    // Incoming amount may be a string (to encode very large amounts > 53bit), or an integer
    if (obj.isStr() || obj.isNum()) {
        // use the univalue parser (better than atoi64())
        const UniValue objAsNumeric{UniValue::VNUM, obj.getValStr()};
        // may throw on parse error (not an integer number string)
        const auto optAmt = token::SafeAmount::fromInt(objAsNumeric.get_int64());
        if (!optAmt) throw std::runtime_error("Invalid \"amount\" in tokenData");
        return *optAmt;
    }
    // else ...
    throw std::runtime_error("Expected a number or a string for \"amount\" in tokenData");
}
