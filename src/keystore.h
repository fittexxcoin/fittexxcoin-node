// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Fittexxcoin Core developers
// Copyright (c) 2017-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <key.h>
#include <pubkey.h>
#include <script/script.h>
#include <script/sign.h>
#include <script/standard.h>
#include <sync.h>

#include <boost/signals2/signal.hpp>

/** A virtual base class for key stores */
class CKeyStore : public SigningProvider {
protected:
    mutable RecursiveMutex cs_KeyStore;

public:
    //! Add a key to the store.
    virtual bool AddKeyPubKey(const CKey &key, const CPubKey &pubkey) = 0;

    //! Check whether a key corresponding to a given address is present in the
    //! store.
    virtual std::set<CKeyID> GetKeys() const = 0;

    //! Support for BIP 0013 : see
    //! https://github.com/fittexxcoin/bips/blob/master/bip-0013.mediawiki
    virtual bool AddCScript(const CScript &redeemScript, bool is_p2sh32) = 0;
    virtual std::set<ScriptID> GetCScripts() const = 0;

    //! Support for Watch-only addresses
    virtual bool AddWatchOnly(const CScript &dest) = 0;
    virtual bool RemoveWatchOnly(const CScript &dest) = 0;
    virtual bool HaveWatchOnly(const CScript &dest) const = 0;
    virtual bool HaveWatchOnly() const = 0;
};

/** Basic key store, that keeps keys in an address->secret map */
class CBasicKeyStore : public CKeyStore {
protected:
    mutable RecursiveMutex cs_KeyStore;

    using KeyMap = std::map<CKeyID, CKey>;
    using WatchKeyMap = std::map<CKeyID, CPubKey>;
    using ScriptMap = std::map<ScriptID, CScript>;
    using WatchOnlySet = std::set<CScript>;

    KeyMap mapKeys GUARDED_BY(cs_KeyStore);
    WatchKeyMap mapWatchKeys GUARDED_BY(cs_KeyStore);
    ScriptMap mapScripts GUARDED_BY(cs_KeyStore);
    WatchOnlySet setWatchOnly GUARDED_BY(cs_KeyStore);

    void ImplicitlyLearnRelatedKeyScripts(const CPubKey &pubkey)
        EXCLUSIVE_LOCKS_REQUIRED(cs_KeyStore);

public:
    bool AddKeyPubKey(const CKey &key, const CPubKey &pubkey) override;
    bool AddKey(const CKey &key) { return AddKeyPubKey(key, key.GetPubKey()); }
    bool GetPubKey(const CKeyID &address, CPubKey &vchPubKeyOut) const override;
    bool HaveKey(const CKeyID &address) const override;
    std::set<CKeyID> GetKeys() const override;
    bool GetKey(const CKeyID &address, CKey &keyOut) const override;
    bool AddCScript(const CScript &redeemScript, bool is_p2sh32) override;
    bool HaveCScript(const ScriptID &hash) const override;
    std::set<ScriptID> GetCScripts() const override;
    bool GetCScript(const ScriptID &hash, CScript &redeemScriptOut) const override;

    bool AddWatchOnly(const CScript &dest) override;
    bool RemoveWatchOnly(const CScript &dest) override;
    bool HaveWatchOnly(const CScript &dest) const override;
    bool HaveWatchOnly() const override;
};

/**
 * Return the CKeyID of the key involved in a script (if there is a unique one).
 */
CKeyID GetKeyForDestination(const CKeyStore &store, const CTxDestination &dest);

/**
 * Checks if a CKey is in the given CKeyStore compressed or otherwise
 */
bool HaveKey(const CKeyStore &store, const CKey &key);
