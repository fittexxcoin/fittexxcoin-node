// Copyright (c) 2009-2014 The Fittexxcoin Core developers
// Copyright (c) 2017-2019 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/fittexxcoin-config.h>
#endif

#include <cstring>

#if HAVE_DECL_STRNLEN == 0
size_t strnlen(const char *start, size_t max_len) {
    const char *end = (const char *)memchr(start, '\0', max_len);

    return end ? (size_t)(end - start) : max_len;
}
#endif // HAVE_DECL_STRNLEN
