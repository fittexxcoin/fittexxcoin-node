// Copyright (c) 2017-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstddef>
#include <cstdint>

constexpr int MAX_LABEL_LENGTH = 63;
constexpr int MAX_QUERY_NAME_LENGTH = 255;
// Max size of the null-terminated buffer parse_name() writes to.
constexpr int MAX_QUERY_NAME_BUFFER_LENGTH = MAX_QUERY_NAME_LENGTH + 1;

/// Encapsulates either an IPv4 or IPv6 address, depending on whether member .v == 4 or .v == 6
struct AddrGeneric {
    int v;
    union {
        uint8_t v4[4];
        uint8_t v6[16];
    } data;
};

struct dns_opt_t {
    int port;
    int datattl;
    int nsttl;
    const char *host;
    const char *ns;
    const char *mbox;
    uint32_t (*cb)(void *opt, const char *requested_hostname, AddrGeneric *addr,
                   uint32_t max, uint32_t ipv4, uint32_t ipv6);
    // stats
    uint64_t nRequests;
};

enum class ParseNameStatus {
    OK,
    // Premature end of input, forward reference, component > 63 char, invalid
    // character
    InputError,
    // Insufficient space in output
    OutputBufferError,
};

ParseNameStatus parse_name(const uint8_t **inpos, const uint8_t *inend,
                           const uint8_t *inbuf, char *buf, size_t bufsize);

//  0: k
// -1: component > 63 characters
// -2: insufficient space in output
// -3: two subsequent dots
int write_name(uint8_t **outpos, const uint8_t *outend, const char *name,
               int offset);

int dnsserver(dns_opt_t *opt);
