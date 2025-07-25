// Copyright (c) 2017-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <cashaddrenc.h>

#include <cashaddr.h>
#include <chainparams.h>
#include <pubkey.h>
#include <script/script.h>
#include <util/strencodings.h>

#include <boost/variant/static_visitor.hpp>

#include <algorithm>

namespace {

// Convert the data part to a 5 bit representation.
template <class T>
std::vector<uint8_t> PackAddrData(const T &id, uint8_t type) {
    uint8_t version_byte(type << 3);
    size_t size = id.size();
    uint8_t encoded_size = 0;
    switch (size * 8) {
        case 160:
            encoded_size = 0;
            break;
        case 192:
            encoded_size = 1;
            break;
        case 224:
            encoded_size = 2;
            break;
        case 256:
            encoded_size = 3;
            break;
        case 320:
            encoded_size = 4;
            break;
        case 384:
            encoded_size = 5;
            break;
        case 448:
            encoded_size = 6;
            break;
        case 512:
            encoded_size = 7;
            break;
        default:
            throw std::runtime_error(
                "Error packing cashaddr: invalid address length");
    }
    version_byte |= encoded_size;
    std::vector<uint8_t> data = {version_byte};
    data.insert(data.end(), std::begin(id), std::end(id));

    std::vector<uint8_t> converted;
    // Reserve the number of bytes required for a 5-bit packed version of a
    // hash, with version byte.  Add half a byte(4) so integer math provides
    // the next multiple-of-5 that would fit all the data.
    converted.reserve(((size + 1) * 8 + 4) / 5);
    ConvertBits<8, 5, true>([&](uint8_t c) { converted.push_back(c); },
                            std::begin(data), std::end(data));

    return converted;
}

// Implements encoding of CTxDestination using cashaddr.
struct CashAddrEncoder : boost::static_visitor<std::string> {
    CashAddrEncoder(const CChainParams &p, bool t) : params(p), tokenAwareType(t) {}

    std::string operator()(const CKeyID &id) const {
        std::vector<uint8_t> data = PackAddrData(id, tokenAwareType ? TOKEN_PUBKEY_TYPE : PUBKEY_TYPE);
        return cashaddr::Encode(params.CashAddrPrefix(), data);
    }

    std::string operator()(const ScriptID &id) const {
        std::vector<uint8_t> data = PackAddrData(id, tokenAwareType ? TOKEN_SCRIPT_TYPE : SCRIPT_TYPE);
        return cashaddr::Encode(params.CashAddrPrefix(), data);
    }

    std::string operator()(const CNoDestination &) const { return ""; }

private:
    const CChainParams &params;
    const bool tokenAwareType;
};

} // namespace

std::string EncodeCashAddr(const CTxDestination &dst, const CChainParams &params, const bool tokenAwareType) {
    return boost::apply_visitor(CashAddrEncoder(params, tokenAwareType), dst);
}

std::string EncodeCashAddr(const std::string &prefix, const CashAddrContent &content) {
    std::vector<uint8_t> data = PackAddrData(content.hash, content.type);
    return cashaddr::Encode(prefix, data);
}

CTxDestination DecodeCashAddr(const std::string &addr, const CChainParams &params, bool *tokenAwareTypeOut) {
    CTxDestination ret = CNoDestination{};
    if (const CashAddrContent content = DecodeCashAddrContent(addr, params.CashAddrPrefix()); !content.IsNull()) {
        if (tokenAwareTypeOut) {
            *tokenAwareTypeOut = content.IsTokenAwareType();
        }
        ret = DecodeCashAddrDestination(content);
    }
    return ret;
}

CashAddrContent DecodeCashAddrContent(const std::string &addr, const std::string &expectedPrefix) {
    std::string prefix;
    std::vector<uint8_t> payload;
    std::tie(prefix, payload) = cashaddr::Decode(addr, expectedPrefix);

    if (prefix != expectedPrefix) {
        return {};
    }

    if (payload.empty()) {
        return {};
    }

    std::vector<uint8_t> data;
    data.reserve(payload.size() * 5 / 8);
    if (!ConvertBits<5, 8, false>([&](uint8_t c) { data.push_back(c); },
                                  begin(payload), end(payload))) {
        return {};
    }

    // Decode type and size from the version.
    uint8_t version = data[0];
    if (version & 0x80) {
        // First bit is reserved.
        return {};
    }

    auto type = CashAddrType((version >> 3) & 0x1f);
    uint32_t hash_size = 20 + 4 * (version & 0x03);
    if (version & 0x04) {
        hash_size *= 2;
    }

    // Check that we decoded the exact number of bytes we expected.
    if (data.size() != hash_size + 1) {
        return {};
    }

    // Pop the version.
    data.erase(data.begin());
    return {type, std::move(data)};
}

CTxDestination DecodeCashAddrDestination(const CashAddrContent &content) {
    const bool is20Bytes{content.hash.size() == 20};

    if (!is20Bytes && content.hash.size() != 32) {
        // Only accept 20 or 32 byte hashes. A 20-byte hash is for p2sh and/or p2pkh; a 32-byte hash is for p2sh_32.
        return CNoDestination{};
    }

    switch (content.type) {
        case PUBKEY_TYPE:
        case TOKEN_PUBKEY_TYPE:
            // p2pkh only supports 20-byte hashes
            if (is20Bytes) {
                return CKeyID(uint160(content.hash));
            } else {
                return CNoDestination{};
            }
        case SCRIPT_TYPE:
        case TOKEN_SCRIPT_TYPE:
            if (is20Bytes) {
                return ScriptID(uint160(content.hash)); // p2sh
            } else {
                return ScriptID(uint256(content.hash)); // p2sh_32
            }
        default:
            return CNoDestination{};
    }
}

// PackCashAddrContent allows for testing PackAddrData in unittests due to
// template definitions.
std::vector<uint8_t> PackCashAddrContent(const CashAddrContent &content) {
    return PackAddrData(content.hash, content.type);
}
