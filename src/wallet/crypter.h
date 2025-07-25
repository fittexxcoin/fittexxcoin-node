// Copyright (c) 2009-2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <keystore.h>
#include <serialize.h>
#include <support/allocators/secure.h>

#include <atomic>

const unsigned int WALLET_CRYPTO_KEY_SIZE = 32;
const unsigned int WALLET_CRYPTO_SALT_SIZE = 8;
const unsigned int WALLET_CRYPTO_IV_SIZE = 16;

/**
 * Private key encryption is done based on a CMasterKey, which holds a salt and
 * random encryption key.
 *
 * CMasterKeys are encrypted using AES-256-CBC using a key derived using
 * derivation method nDerivationMethod (0 == EVP_sha512()) and derivation
 * iterations nDeriveIterations. vchOtherDerivationParameters is provided for
 * alternative algorithms which may require more parameters (such as scrypt).
 *
 * Wallet Private Keys are then encrypted using AES-256-CBC with the
 * double-sha256 of the public key as the IV, and the master key's key as the
 * encryption key (see keystore.[ch]).
 */

/** Master key for wallet encryption */
class CMasterKey {
public:
    std::vector<uint8_t> vchCryptedKey;
    std::vector<uint8_t> vchSalt;
    //! 0 = EVP_sha512()
    //! 1 = scrypt()
    unsigned int nDerivationMethod;
    unsigned int nDeriveIterations;
    //! Use this for more parameters to key derivation, such as the various
    //! parameters to scrypt
    std::vector<uint8_t> vchOtherDerivationParameters;

    SERIALIZE_METHODS(CMasterKey, obj) {
        READWRITE(obj.vchCryptedKey, obj.vchSalt, obj.nDerivationMethod, obj.nDeriveIterations, obj.vchOtherDerivationParameters);
    }

    CMasterKey() {
        // 25000 rounds is just under 0.1 seconds on a 1.86 GHz Pentium M
        // ie slightly lower than the lowest hardware we need bother supporting
        nDeriveIterations = 25000;
        nDerivationMethod = 0;
        vchOtherDerivationParameters = std::vector<uint8_t>(0);
    }
};

typedef std::vector<uint8_t, secure_allocator<uint8_t>> CKeyingMaterial;

namespace wallet_crypto_tests {
class TestCrypter;
}

/** Encryption/decryption context with key information */
class CCrypter {
    // for test access to chKey/chIV
    friend class wallet_crypto_tests::TestCrypter;

private:
    std::vector<uint8_t, secure_allocator<uint8_t>> vchKey;
    std::vector<uint8_t, secure_allocator<uint8_t>> vchIV;
    bool fKeySet;

    int BytesToKeySHA512AES(const std::vector<uint8_t> &chSalt,
                            const SecureString &strKeyData, int count,
                            uint8_t *key, uint8_t *iv) const;

public:
    bool SetKeyFromPassphrase(const SecureString &strKeyData,
                              const std::vector<uint8_t> &chSalt,
                              const unsigned int nRounds,
                              const unsigned int nDerivationMethod);
    bool Encrypt(const CKeyingMaterial &vchPlaintext,
                 std::vector<uint8_t> &vchCiphertext) const;
    bool Decrypt(const std::vector<uint8_t> &vchCiphertext,
                 CKeyingMaterial &vchPlaintext) const;
    bool SetKey(const CKeyingMaterial &chNewKey,
                const std::vector<uint8_t> &chNewIV);

    void CleanKey() {
        memory_cleanse(vchKey.data(), vchKey.size());
        memory_cleanse(vchIV.data(), vchIV.size());
        fKeySet = false;
    }

    CCrypter() {
        fKeySet = false;
        vchKey.resize(WALLET_CRYPTO_KEY_SIZE);
        vchIV.resize(WALLET_CRYPTO_IV_SIZE);
    }

    ~CCrypter() { CleanKey(); }
};

/**
 * Keystore which keeps the private keys encrypted.
 * It derives from the basic key store, which is used if no encryption is
 * active.
 */
class CCryptoKeyStore : public CBasicKeyStore {
private:
    CKeyingMaterial vMasterKey GUARDED_BY(cs_KeyStore);

    //! if fUseCrypto is true, mapKeys must be empty
    //! if fUseCrypto is false, vMasterKey must be empty
    std::atomic<bool> fUseCrypto;

    //! keeps track of whether Unlock has run a thorough check before
    bool fDecryptionThoroughlyChecked;

protected:
    using CryptedKeyMap =
        std::map<CKeyID, std::pair<CPubKey, std::vector<uint8_t>>>;

    bool SetCrypted();

    //! will encrypt previously unencrypted keys
    bool EncryptKeys(CKeyingMaterial &vMasterKeyIn);

    bool Unlock(const CKeyingMaterial &vMasterKeyIn,
                bool accept_no_keys = false);
    CryptedKeyMap mapCryptedKeys GUARDED_BY(cs_KeyStore);

public:
    CCryptoKeyStore()
        : fUseCrypto(false), fDecryptionThoroughlyChecked(false) {}

    bool IsCrypted() const { return fUseCrypto; }
    bool IsLocked() const;
    bool Lock();

    virtual bool AddCryptedKey(const CPubKey &vchPubKey,
                               const std::vector<uint8_t> &vchCryptedSecret);
    bool AddKeyPubKey(const CKey &key, const CPubKey &pubkey) override;
    bool HaveKey(const CKeyID &address) const override;
    bool GetKey(const CKeyID &address, CKey &keyOut) const override;
    bool GetPubKey(const CKeyID &address, CPubKey &vchPubKeyOut) const override;
    std::set<CKeyID> GetKeys() const override;

    /**
     * Wallet status (encrypted, locked) changed.
     * Note: Called without locks held.
     */
    boost::signals2::signal<void(CCryptoKeyStore *wallet)> NotifyStatusChanged;
};
