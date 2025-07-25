// Copyright (c) 2009-2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#if defined(HAVE_CONFIG_H)
#include <config/fittexxcoin-config.h>
#endif // HAVE_CONFIG_H
#include <config/version.h>

// Check that required client information is defined
#if !defined(CLIENT_VERSION_MAJOR) || !defined(CLIENT_VERSION_MINOR) ||        \
    !defined(CLIENT_VERSION_REVISION) || !defined(COPYRIGHT_YEAR) ||           \
    !defined(CLIENT_VERSION_IS_RELEASE)
#error Client version information missing: version is not defined by fittexxcoin-config.h nor defined any other way
#endif

/**
 * Converts the parameter X to a string after macro replacement on X has been
 * performed.
 * Don't merge these into one macro!
 */
#define STRINGIZE(X) DO_STRINGIZE(X)
#define DO_STRINGIZE(X) #X

//! Copyright string used in Windows .rc files
#define COPYRIGHT_STR                                                          \
    "20025-" STRINGIZE(COPYRIGHT_YEAR) " " COPYRIGHT_HOLDERS_FINAL

/**
 * fittexxcoind-res.rc includes this file, but it cannot cope with real c++ code.
 * WINDRES_PREPROC is defined to indicate that its pre-processor is running.
 * Anything other than a define should be guarded below.
 */

#if !defined(WINDRES_PREPROC)

#include <string>
#include <vector>

static constexpr int CLIENT_VERSION = 1000000 * CLIENT_VERSION_MAJOR +
                                      10000 * CLIENT_VERSION_MINOR +
                                      100 * CLIENT_VERSION_REVISION;

extern const std::string CLIENT_NAME;
extern const std::string CLIENT_BUILD;

std::string FormatFullVersion();
std::string FormatSubVersion(const std::string &name, int nClientVersion,
                             const std::vector<std::string> &comments);

#endif // WINDRES_PREPROC
