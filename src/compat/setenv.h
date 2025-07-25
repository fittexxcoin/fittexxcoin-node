// Copyright (c) 2018-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#if defined(WIN32)

#include <cstdlib>

int setenv(const char *name, const char *value, int overwrite) {
    return _putenv_s(name, value);
}

#endif
