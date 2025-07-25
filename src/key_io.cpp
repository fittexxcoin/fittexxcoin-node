// Copyright (c) 2014-2016 The Fittexxcoin Core developers
// Copyright (c) 2019-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <key_io.h>

#include <base58.h>
#include <cashaddrenc.h>
#include <chainparams.h>
#include <config.h>
#include <script/script.h>
#include <util/strencodings.h>

#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>

#include <algorithm>
#include <cassert>
#include <cstring>

namespace {
class DestinationEncoder : public boost::static_visitor<std::string> {
private:
    const CChainParams &m_params;

public:
    explicit DestinationEncoder(const CChainParams &params)
        : m_params(params) {}

    std::string operator()(const CKeyID &id) const {
        std::vector<uint8_t> data =
            m_params.Base58Prefix(CChainParams::PUBKEY_ADDRESS);
        data.insert(data.end(), id.begin(), id.end());
        return EncodeBase58Check(data);
    }

    std::string operator()(const ScriptID &id) const {
        std::vector<uint8_t> data =
            m_params.Base58Prefix(CChainParams::SCRIPT_ADDRESS);
        data.insert(data.end(), id.begin(), id.end());
        return EncodeBase58Check(data);
    }

    std::string operator()(const CNoDestination &) const { return {}; }
};

CTxDestination DecodeLegacyDestination(const std::string &str,
                                       const CChainParams &params) {
    std::vector<uint8_t> data;
    uint160 hash{uint160::Uninitialized};
    if (!DecodeBase58Check(str, data, 33 /* max size is 33 (was 21 before p2sh_32), 33 is to support p2sh_32 */)) {
        return CNoDestination();
    }
    // base58-encoded Fittexxcoin addresses.
    // Public-key-hash-addresses have version 0 (or 111 testnet).
    // The data vector contains RIPEMD160(SHA256(pubkey)), where pubkey is
    // the serialized public key.
    const std::vector<uint8_t> &pubkey_prefix =
        params.Base58Prefix(CChainParams::PUBKEY_ADDRESS);
    if (data.size() == hash.size() + pubkey_prefix.size() &&
        std::equal(pubkey_prefix.begin(), pubkey_prefix.end(), data.begin())) {
        std::copy(data.begin() + pubkey_prefix.size(), data.end(),
                  hash.begin());
        return CKeyID(hash);
    }
    // Script-hash-addresses have version 5 (or 196 testnet).
    // The data vector contains RIPEMD160(SHA256(cscript)), where cscript is
    // the serialized redemption script.
    const std::vector<uint8_t> &script_prefix = params.Base58Prefix(CChainParams::SCRIPT_ADDRESS);
    if (data.size() == hash.size() + script_prefix.size() &&
        std::equal(script_prefix.begin(), script_prefix.end(), data.begin())) {
        std::copy(data.begin() + script_prefix.size(), data.end(), hash.begin());
        return ScriptID(hash); // p2sh_20
    }
    // p2sh_32 support
    // The data vector contains SHA256(SHA256(cscript)), where cscript is
    // the serialized redemption script.
    uint256 hash32{uint256::Uninitialized};
    if (data.size() == hash32.size() + script_prefix.size() &&
        std::equal(script_prefix.begin(), script_prefix.end(), data.begin())) {
        std::copy(data.begin() + script_prefix.size(), data.end(), hash32.begin());
        return ScriptID(hash32); // p2sh_32
    }
    return CNoDestination();
}
} // namespace

CKey DecodeSecret(const std::string &str) {
    CKey key;
    std::vector<uint8_t> data;
    if (DecodeBase58Check(str, data, 34)) {
        const std::vector<uint8_t> &privkey_prefix =
            Params().Base58Prefix(CChainParams::SECRET_KEY);
        if ((data.size() == 32 + privkey_prefix.size() ||
             (data.size() == 33 + privkey_prefix.size() && data.back() == 1)) &&
            std::equal(privkey_prefix.begin(), privkey_prefix.end(),
                       data.begin())) {
            bool compressed = data.size() == 33 + privkey_prefix.size();
            key.Set(data.begin() + privkey_prefix.size(),
                    data.begin() + privkey_prefix.size() + 32, compressed);
        }
    }
    if (!data.empty()) {
        memory_cleanse(data.data(), data.size());
    }
    return key;
}

std::string EncodeSecret(const CKey &key) {
    assert(key.IsValid());
    std::vector<uint8_t> data = Params().Base58Prefix(CChainParams::SECRET_KEY);
    data.insert(data.end(), key.begin(), key.end());
    if (key.IsCompressed()) {
        data.push_back(1);
    }
    std::string ret = EncodeBase58Check(data);
    memory_cleanse(data.data(), data.size());
    return ret;
}

CExtPubKey DecodeExtPubKey(const std::string &str) {
    CExtPubKey key;
    std::vector<uint8_t> data;
    if (DecodeBase58Check(str, data, 78)) {
        const std::vector<uint8_t> &prefix =
            Params().Base58Prefix(CChainParams::EXT_PUBLIC_KEY);
        if (data.size() == BIP32_EXTKEY_SIZE + prefix.size() &&
            std::equal(prefix.begin(), prefix.end(), data.begin())) {
            key.Decode(data.data() + prefix.size());
        }
    }
    return key;
}

std::string EncodeExtPubKey(const CExtPubKey &key) {
    std::vector<uint8_t> data =
        Params().Base58Prefix(CChainParams::EXT_PUBLIC_KEY);
    size_t size = data.size();
    data.resize(size + BIP32_EXTKEY_SIZE);
    key.Encode(data.data() + size);
    std::string ret = EncodeBase58Check(data);
    return ret;
}

CExtKey DecodeExtKey(const std::string &str) {
    CExtKey key;
    std::vector<uint8_t> data;
    if (DecodeBase58Check(str, data, 78)) {
        const std::vector<uint8_t> &prefix =
            Params().Base58Prefix(CChainParams::EXT_SECRET_KEY);
        if (data.size() == BIP32_EXTKEY_SIZE + prefix.size() &&
            std::equal(prefix.begin(), prefix.end(), data.begin())) {
            key.Decode(data.data() + prefix.size());
        }
    }
    return key;
}

std::string EncodeExtKey(const CExtKey &key) {
    std::vector<uint8_t> data =
        Params().Base58Prefix(CChainParams::EXT_SECRET_KEY);
    size_t size = data.size();
    data.resize(size + BIP32_EXTKEY_SIZE);
    key.Encode(data.data() + size);
    std::string ret = EncodeBase58Check(data);
    memory_cleanse(data.data(), data.size());
    return ret;
}

std::string EncodeDestination(const CTxDestination &dest, const Config &config, const bool tokenAwareAddress) {
    const CChainParams &params = config.GetChainParams();
    return config.UseCashAddrEncoding() ? EncodeCashAddr(dest, params, tokenAwareAddress)
                                        : EncodeLegacyAddr(dest, params);
}

CTxDestination DecodeDestination(const std::string &addr, const CChainParams &params, bool *tokenAwareAddressOut) {
    CTxDestination dst = DecodeCashAddr(addr, params, tokenAwareAddressOut);
    if (IsValidDestination(dst)) {
        return dst;
    }
    if (tokenAwareAddressOut) *tokenAwareAddressOut = false; // legacy is never a token-aware address
    return DecodeLegacyAddr(addr, params);
}

bool IsValidDestinationString(const std::string &str, const CChainParams &params, bool *tokenAwareAddressOut) {
    return IsValidDestination(DecodeDestination(str, params, tokenAwareAddressOut));
}

std::string EncodeLegacyAddr(const CTxDestination &dest, const CChainParams &params) {
    return boost::apply_visitor(DestinationEncoder(params), dest);
}

CTxDestination DecodeLegacyAddr(const std::string &str, const CChainParams &params) {
    return DecodeLegacyDestination(str, params);
}
