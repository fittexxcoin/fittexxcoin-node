// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <compressor.h>
#include <core_memusage.h>
#include <memusage.h>
#include <primitives/blockhash.h>
#include <serialize.h>
#include <util/saltedhashers.h>

#include <cassert>
#include <cstdint>
#include <unordered_map>

/**
 * A UTXO entry.
 *
 * Serialized format:
 * - VARINT((coinbase ? 1 : 0) | (height << 1))
 * - the non-spent CTxOut (via CTxOutCompressor)
 */
class Coin {
    //! Unspent transaction output.
    CTxOut out;

    //! Whether containing transaction was a coinbase and height at which the
    //! transaction was included into a block.
    uint32_t nHeightAndIsCoinBase;

public:
    //! Empty constructor
    Coin() : nHeightAndIsCoinBase(0) {}

    //! Constructor from a CTxOut and height/coinbase information.
    Coin(CTxOut outIn, uint32_t nHeightIn, bool IsCoinbase)
        : out(std::move(outIn)),
          nHeightAndIsCoinBase((nHeightIn << 1) | IsCoinbase) {}

    uint32_t GetHeight() const { return nHeightAndIsCoinBase >> 1; }
    bool IsCoinBase() const { return nHeightAndIsCoinBase & 0x01; }
    bool IsSpent() const { return out.IsNull(); }

    CTxOut &GetTxOut() { return out; }
    const CTxOut &GetTxOut() const { return out; }

    void Clear() {
        out.SetNull();
        nHeightAndIsCoinBase = 0;
    }

    template <typename Stream> void Serialize(Stream &s) const {
        assert(!IsSpent());
        ::Serialize(s, VARINT(nHeightAndIsCoinBase));
        ::Serialize(s, Using<TxOutCompression>(out));
    }

    template <typename Stream> void Unserialize(Stream &s) {
        ::Unserialize(s, VARINT(nHeightAndIsCoinBase));
        ::Unserialize(s, Using<TxOutCompression>(out));
    }

    size_t DynamicMemoryUsage() const {
        return memusage::DynamicUsage(out.scriptPubKey) + RecursiveDynamicUsage(out.tokenDataPtr);
    }
};

struct CCoinsCacheEntry {
    // The actual cached data.
    Coin coin;
    uint8_t flags;

    enum Flags {
        // This cache entry is potentially different from the version in the
        // parent view.
        DIRTY = (1 << 0),
        // The parent view does not have this entry (or it is pruned).
        FRESH = (1 << 1),
        /* Note that FRESH is a performance optimization with which we can erase
           coins that are fully spent if we know we do not need to flush the
           changes to the parent cache. It is always safe to not mark FRESH if
           that condition is not guaranteed. */
    };

    CCoinsCacheEntry() : flags(0) {}
    explicit CCoinsCacheEntry(Coin coinIn)
        : coin(std::move(coinIn)), flags(0) {}
};

typedef std::unordered_map<COutPoint, CCoinsCacheEntry, SaltedOutpointHasher>
    CCoinsMap;

/** Cursor for iterating over CoinsView state */
class CCoinsViewCursor {
public:
    CCoinsViewCursor(const BlockHash &hashBlockIn) : hashBlock(hashBlockIn) {}
    virtual ~CCoinsViewCursor() {}

    virtual bool GetKey(COutPoint &key) const = 0;
    virtual bool GetValue(Coin &coin) const = 0;
    virtual unsigned int GetValueSize() const = 0;

    virtual bool Valid() const = 0;
    virtual void Next() = 0;

    //! Get best block at the time this cursor was created
    const BlockHash &GetBestBlock() const { return hashBlock; }

private:
    BlockHash hashBlock;
};

/** Abstract view on the open txout dataset. */
class CCoinsView {
public:
    /**
     * Retrieve the Coin (unspent transaction output) for a given outpoint.
     * Returns true only when an unspent coin was found, which is returned in
     * coin. When false is returned, coin's value is unspecified.
     */
    virtual bool GetCoin(const COutPoint &outpoint, Coin &coin) const;

    //! Just check whether a given outpoint is unspent.
    virtual bool HaveCoin(const COutPoint &outpoint) const;

    //! Retrieve the block hash whose state this CCoinsView currently represents
    virtual BlockHash GetBestBlock() const;

    //! Retrieve the range of blocks that may have been only partially written.
    //! If the database is in a consistent state, the result is the empty
    //! vector.
    //! Otherwise, a two-element vector is returned consisting of the new and
    //! the old block hash, in that order.
    virtual std::vector<BlockHash> GetHeadBlocks() const;

    //! Do a bulk modification (multiple Coin changes + BestBlock change).
    //! The passed mapCoins can be modified.
    virtual bool BatchWrite(CCoinsMap &mapCoins, const BlockHash &hashBlock);

    //! Get a cursor to iterate over the whole state
    virtual CCoinsViewCursor *Cursor(bool snapshot = false) const;

    //! As we use CCoinsViews polymorphically, have a virtual destructor
    virtual ~CCoinsView() {}

    //! Estimate database size (0 if not implemented)
    virtual size_t EstimateSize() const { return 0; }
};

/** CCoinsView backed by another CCoinsView */
class CCoinsViewBacked : public CCoinsView {
protected:
    CCoinsView *base;

public:
    CCoinsViewBacked(CCoinsView *viewIn);
    bool GetCoin(const COutPoint &outpoint, Coin &coin) const override;
    bool HaveCoin(const COutPoint &outpoint) const override;
    BlockHash GetBestBlock() const override;
    std::vector<BlockHash> GetHeadBlocks() const override;
    void SetBackend(CCoinsView &viewIn);
    bool BatchWrite(CCoinsMap &mapCoins, const BlockHash &hashBlock) override;
    CCoinsViewCursor *Cursor(bool snapshot = false) const override;
    size_t EstimateSize() const override;
};

/**
 * CCoinsView that adds a memory cache for transactions to another CCoinsView
 */
class CCoinsViewCache : public CCoinsViewBacked {
protected:
    /**
     * Make mutable so that we can "fill the cache" even from Get-methods
     * declared as "const".
     */
    mutable BlockHash hashBlock;
    mutable CCoinsMap cacheCoins;

    /* Cached dynamic memory usage for the inner Coin objects. */
    mutable size_t cachedCoinsUsage;

public:
    CCoinsViewCache(CCoinsView *baseIn);

    /**
     * By deleting the copy constructor, we prevent accidentally using it when
     * one intends to create a cache on top of a base cache.
     */
    CCoinsViewCache(const CCoinsViewCache &) = delete;

    // Standard CCoinsView methods
    bool GetCoin(const COutPoint &outpoint, Coin &coin) const override;
    bool HaveCoin(const COutPoint &outpoint) const override;
    BlockHash GetBestBlock() const override;
    void SetBestBlock(const BlockHash &hashBlock);
    bool BatchWrite(CCoinsMap &mapCoins, const BlockHash &hashBlock) override;
    CCoinsViewCursor *Cursor(bool snapshot = false) const override {
        throw std::logic_error(
            "CCoinsViewCache cursor iteration not supported.");
    }

    /**
     * Check if we have the given utxo already loaded in this cache.
     * The semantics are the same as HaveCoin(), but no calls to the backing
     * CCoinsView are made.
     */
    bool HaveCoinInCache(const COutPoint &outpoint) const;

    /**
     * Return a reference to Coin in the cache, or a pruned one if not found.
     * This is more efficient than GetCoin.
     *
     * Generally, do not hold the reference returned for more than a short
     * scope. While the current implementation allows for modifications to the
     * contents of the cache while holding the reference, this behavior should
     * not be relied on! To be safe, best to not hold the returned reference
     * through any other calls to this cache.
     */
    const Coin &AccessCoin(const COutPoint &output) const;

    /**
     * Add a coin. Set potential_overwrite to true if a non-pruned version may
     * already exist.
     */
    void AddCoin(const COutPoint &outpoint, Coin coin,
                 bool potential_overwrite);

    /**
     * Spend a coin. Pass moveto in order to get the deleted data.
     * If no unspent output exists for the passed outpoint, this call has no
     * effect.
     */
    bool SpendCoin(const COutPoint &outpoint, Coin *moveto = nullptr);

    /**
     * Push the modifications applied to this cache to its base.
     * Failure to call this method before destruction will cause the changes to
     * be forgotten. If false is returned, the state of this cache (and its
     * backing view) will be undefined.
     */
    bool Flush();

    /**
     * Removes the UTXO with the given outpoint from the cache, if it is not
     * modified.
     */
    void Uncache(const COutPoint &outpoint);

    //! Calculate the size of the cache (in number of transaction outputs)
    unsigned int GetCacheSize() const;

    //! Calculate the size of the cache (in bytes)
    size_t DynamicMemoryUsage() const;

    /**
     * Amount of fittexxcoins coming in to a transaction
     * Note that lightweight clients may not know anything besides the hash of
     * previous transactions, so may not be able to calculate this.
     *
     * @param[in] tx	transaction for which we are checking input total
     * @return	Sum of value of all inputs (scriptSigs)
     */
    Amount GetValueIn(const CTransaction &tx) const;

    //! Check whether all prevouts of the transaction are present in the UTXO
    //! set represented by this view
    bool HaveInputs(const CTransaction &tx) const;

    const CTxOut &GetOutputFor(const CTxIn &input) const;

private:
    CCoinsMap::iterator FetchCoin(const COutPoint &outpoint) const;
};

//! Utility function to add all of a transaction's outputs to a cache.
//! When check is false, this assumes that overwrites are only possible for
//! coinbase transactions. When check is true, the underlying view may be
//! queried to determine whether an addition is an overwrite.
// TODO: pass in a boolean to limit these possible overwrites to known
// (pre-BIP34) cases.
void AddCoins(CCoinsViewCache &cache, const CTransaction &tx, int nHeight,
              bool check = false);

//! Utility function to find any unspent output with a given txid.
//! This function can be quite expensive because in the event of a transaction
//! which is not found in the cache, it can cause up to MAX_OUTPUTS_PER_BLOCK
//! lookups to database, so it should be used with care.
const Coin &AccessByTxid(const CCoinsViewCache &cache, const TxId &txid);
