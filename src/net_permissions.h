// Copyright (c) 2009-2018 The Fittexxcoin Core developers
// Copyright (c) 2020-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <netaddress.h>

#include <string>
#include <vector>

enum NetPermissionFlags {
    PF_NONE = 0,
    // Can query bloomfilter even if -peerbloomfilters is false
    PF_BLOOMFILTER = (1U << 1),
    // Relay and accept transactions from this peer, even if -blocksonly is true
    PF_RELAY = (1U << 3),
    // Always relay transactions from this peer, even if already in mempool or
    // rejected from policy Keep parameter interaction: forcerelay implies relay
    PF_FORCERELAY = (1U << 2) | PF_RELAY,
    // Can't be banned/disconnected/discouraged for misbehavior
    PF_NOBAN = (1U << 4),
    // Can query the mempool
    PF_MEMPOOL = (1U << 5),
    // Can request addrs without hitting a privacy-preserving cache
    PF_ADDR = (1U << 7),

    // True if the user did not specifically set fine grained permissions
    PF_ISIMPLICIT = (1U << 31),
    PF_ALL = PF_BLOOMFILTER | PF_FORCERELAY | PF_RELAY | PF_NOBAN | PF_MEMPOOL | PF_ADDR,
};
class NetPermissions {
public:
    NetPermissionFlags m_flags;
    static std::vector<std::string> ToStrings(NetPermissionFlags flags);
    static inline bool HasFlag(const NetPermissionFlags &flags,
                               NetPermissionFlags f) {
        return (flags & f) == f;
    }
    static inline void AddFlag(NetPermissionFlags &flags,
                               NetPermissionFlags f) {
        flags = static_cast<NetPermissionFlags>(flags | f);
    }
    static inline void ClearFlag(NetPermissionFlags &flags,
                                 NetPermissionFlags f) {
        flags = static_cast<NetPermissionFlags>(flags & ~f);
    }
};
class NetWhitebindPermissions : public NetPermissions {
public:
    static bool TryParse(const std::string str, NetWhitebindPermissions &output,
                         std::string &error);
    CService m_service;
};

class NetWhitelistPermissions : public NetPermissions {
public:
    static bool TryParse(const std::string str, NetWhitelistPermissions &output,
                         std::string &error);
    CSubNet m_subnet;
};
