// Copyright (c) 2014-2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <base58.h>

#include <hash.h>
#include <uint256.h>
#include <util/strencodings.h>
#include <util/string.h>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>

#include <limits>

/** All alphanumeric characters except for "0", "I", "O", and "l" */
static const char *pszBase58 =
    "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
static const int8_t mapBase58[256] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,  7,
    8,  -1, -1, -1, -1, -1, -1, -1, 9,  10, 11, 12, 13, 14, 15, 16, -1, 17, 18,
    19, 20, 21, -1, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, -1, -1, -1, -1,
    -1, -1, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, -1, 44, 45, 46, 47, 48,
    49, 50, 51, 52, 53, 54, 55, 56, 57, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

bool DecodeBase58(const char *psz, std::vector<uint8_t> &vch, size_t max_ret_len) {
    // Skip leading spaces.
    while (*psz && IsSpace(*psz)) {
        psz++;
    }
    // Skip and count leading '1's.
    size_t zeroes = 0;
    size_t length = 0;
    while (*psz == '1') {
        zeroes++;
        if (zeroes > max_ret_len) return false;
        psz++;
    }
    // Allocate enough space in big-endian base256 representation.
    // log(58) / log(256), rounded up.
    size_t size = std::strlen(psz) * 733 / 1000 + 1;
    std::vector<uint8_t> b256(size);
    // Process the characters.
    // guarantee not out of range
    static_assert(std::size(mapBase58) == 256, "mapBase58.size() should be 256");
    while (*psz && !IsSpace(*psz)) {
        // Decode base58 character
        int carry = mapBase58[(uint8_t)*psz];
        // Invalid b58 character
        if (carry == -1) {
            return false;
        }
        size_t i = 0;
        for (std::vector<uint8_t>::reverse_iterator it = b256.rbegin();
             (carry != 0 || i < length) && (it != b256.rend()); ++it, ++i) {
            carry += 58 * (*it);
            *it = carry % 256;
            carry /= 256;
        }
        assert(carry == 0);
        length = i;
        if (length + zeroes > max_ret_len) return false;
        psz++;
    }
    // Skip trailing spaces.
    while (IsSpace(*psz)) {
        psz++;
    }
    if (*psz != 0) {
        return false;
    }
    // Skip leading zeroes in b256.
    std::vector<uint8_t>::iterator it = b256.begin() + (size - length);
    // Copy result into output vector.
    vch.reserve(zeroes + (b256.end() - it));
    vch.assign(zeroes, 0x00);
    while (it != b256.end()) {
        vch.push_back(*(it++));
    }
    return true;
}

std::string EncodeBase58(const uint8_t *pbegin, const uint8_t *pend) {
    // Skip & count leading zeroes.
    size_t zeroes = 0;
    size_t length = 0;
    while (pbegin != pend && *pbegin == 0) {
        pbegin++;
        zeroes++;
    }
    // Allocate enough space in big-endian base58 representation.
    // log(256) / log(58), rounded up.
    size_t size = (pend - pbegin) * 138 / 100 + 1;
    std::vector<uint8_t> b58(size);
    // Process the bytes.
    while (pbegin != pend) {
        int carry = *pbegin;
        size_t i = 0;
        // Apply "b58 = b58 * 256 + ch".
        for (std::vector<uint8_t>::reverse_iterator it = b58.rbegin();
             (carry != 0 || i < length) && (it != b58.rend()); it++, i++) {
            carry += 256 * (*it);
            *it = carry % 58;
            carry /= 58;
        }

        assert(carry == 0);
        length = i;
        pbegin++;
    }
    // Skip leading zeroes in base58 result.
    std::vector<uint8_t>::iterator it = b58.begin() + (size - length);
    while (it != b58.end() && *it == 0) {
        it++;
    }
    // Translate the result into a string.
    std::string str;
    str.reserve(zeroes + (b58.end() - it));
    str.assign(zeroes, '1');
    while (it != b58.end()) {
        str += pszBase58[*(it++)];
    }
    return str;
}

std::string EncodeBase58(const std::vector<uint8_t> &vch) {
    return EncodeBase58(vch.data(), vch.data() + vch.size());
}

bool DecodeBase58(const std::string &str, std::vector<uint8_t> &vchRet, size_t max_ret_len) {
    if (!ValidAsCString(str)) {
        return false;
    }
    return DecodeBase58(str.c_str(), vchRet, max_ret_len);
}

std::string EncodeBase58Check(const std::vector<uint8_t> &vchIn) {
    // add 4-byte hash check to the end
    std::vector<uint8_t> vch(vchIn);
    const uint256 hash = Hash(vch);
    vch.insert(vch.end(), hash.begin(), hash.begin() + 4);
    return EncodeBase58(vch);
}

bool DecodeBase58Check(const char *psz, std::vector<uint8_t> &vchRet, size_t max_ret_len) {
    if (!DecodeBase58(psz, vchRet,
                      max_ret_len > std::numeric_limits<size_t>::max() - 4 ? std::numeric_limits<size_t>::max()
                                                                           : max_ret_len + 4) ||
        (vchRet.size() < 4)) {
        vchRet.clear();
        return false;
    }
    // re-calculate the checksum, ensure it matches the included 4-byte checksum
    const uint256 hash = Hash(Span{vchRet}.first(vchRet.size() - 4));
    if (std::memcmp(hash.data(), &vchRet[vchRet.size() - 4], 4) != 0) {
        vchRet.clear();
        return false;
    }
    vchRet.resize(vchRet.size() - 4);
    return true;
}

bool DecodeBase58Check(const std::string &str, std::vector<uint8_t> &vchRet, size_t max_ret_len) {
    if (!ValidAsCString(str)) {
        return false;
    }
    return DecodeBase58Check(str.c_str(), vchRet, max_ret_len);
}
