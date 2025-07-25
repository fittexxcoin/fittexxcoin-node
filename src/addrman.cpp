// Copyright (c) 2012 Pieter Wuille
// Copyright (c) 2012-2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <addrman.h>

#include <hash.h>
#include <logging.h>
#include <serialize.h>
#include <streams.h>

#include <cmath>
#include <cstdio>
#include <cstring>

int CAddrInfo::GetTriedBucket(const uint256 &nKey, const std::vector<bool> &asmap) const {
    uint64_t hash1 =
        (CHashWriter(SER_GETHASH, 0) << nKey << GetKey()).GetCheapHash();
    uint64_t hash2 =
        (CHashWriter(SER_GETHASH, 0) << nKey << GetGroup(asmap) << (hash1 % ADDRMAN_TRIED_BUCKETS_PER_GROUP))
            .GetCheapHash();
    int tried_bucket = hash2 % ADDRMAN_TRIED_BUCKET_COUNT;
    uint32_t mapped_as = GetMappedAS(asmap);
    LogPrint(BCLog::NET, "IP %s mapped to AS%i belongs to tried bucket %i\n", ToStringIP(), mapped_as, tried_bucket);
    return tried_bucket;
}

int CAddrInfo::GetNewBucket(const uint256 &nKey, const CNetAddr &src, const std::vector<bool> &asmap) const {
    std::vector<uint8_t> vchSourceGroupKey = src.GetGroup(asmap);
    uint64_t hash1 = (CHashWriter(SER_GETHASH, 0) << nKey << GetGroup(asmap) << vchSourceGroupKey).GetCheapHash();
    uint64_t hash2 = (CHashWriter(SER_GETHASH, 0)
                      << nKey << vchSourceGroupKey
                      << (hash1 % ADDRMAN_NEW_BUCKETS_PER_SOURCE_GROUP))
                         .GetCheapHash();
    int new_bucket = hash2 % ADDRMAN_NEW_BUCKET_COUNT;
    uint32_t mapped_as = GetMappedAS(asmap);
    LogPrint(BCLog::NET, "IP %s mapped to AS%i belongs to new bucket %i\n", ToStringIP(), mapped_as, new_bucket);
    return new_bucket;
}

int CAddrInfo::GetBucketPosition(const uint256 &nKey, bool fNew,
                                 int nBucket) const {
    uint64_t hash1 = (CHashWriter(SER_GETHASH, 0)
                      << nKey << (fNew ? 'N' : 'K') << nBucket << GetKey())
                         .GetCheapHash();
    return hash1 % ADDRMAN_BUCKET_SIZE;
}

bool CAddrInfo::IsTerrible(int64_t nNow) const {
    // never remove things tried in the last minute
    if (nLastTry && nLastTry >= nNow - 60) {
        return false;
    }

    // came in a flying DeLorean
    if (nTime > nNow + 10 * 60) {
        return true;
    }

    // not seen in recent history
    if (nTime == 0 || nNow - nTime > ADDRMAN_HORIZON_DAYS * 24 * 60 * 60) {
        return true;
    }

    // tried N times and never a success
    if (nLastSuccess == 0 && nAttempts >= ADDRMAN_RETRIES) {
        return true;
    }

    if (nNow - nLastSuccess > ADDRMAN_MIN_FAIL_DAYS * 24 * 60 * 60 &&
        nAttempts >= ADDRMAN_MAX_FAILURES) {
        // N successive failures in the last week
        return true;
    }

    return false;
}

double CAddrInfo::GetChance(int64_t nNow) const {
    double fChance = 1.0;
    int64_t nSinceLastTry = std::max<int64_t>(nNow - nLastTry, 0);

    // deprioritize very recent attempts away
    if (nSinceLastTry < 60 * 10) {
        fChance *= 0.01;
    }

    // deprioritize 66% after each failed attempt, but at most 1/28th to avoid
    // the search taking forever or overly penalizing outages.
    fChance *= std::pow(0.66, std::min(nAttempts, 8));

    return fChance;
}

CAddrInfo *CAddrMan::Find(const CNetAddr &addr, int *pnId) {
    std::map<CNetAddr, int>::iterator it = mapAddr.find(addr);
    if (it == mapAddr.end()) {
        return nullptr;
    }
    if (pnId) {
        *pnId = (*it).second;
    }
    std::map<int, CAddrInfo>::iterator it2 = mapInfo.find((*it).second);
    if (it2 != mapInfo.end()) {
        return &(*it2).second;
    }
    return nullptr;
}

CAddrInfo *CAddrMan::Create(const CAddress &addr, const CNetAddr &addrSource,
                            int *pnId) {
    int nId = nIdCount++;
    mapInfo[nId] = CAddrInfo(addr, addrSource);
    mapAddr[addr] = nId;
    mapInfo[nId].nRandomPos = vRandom.size();
    vRandom.push_back(nId);
    if (pnId) {
        *pnId = nId;
    }
    return &mapInfo[nId];
}

void CAddrMan::SwapRandom(unsigned int nRndPos1, unsigned int nRndPos2) {
    if (nRndPos1 == nRndPos2) {
        return;
    }

    assert(nRndPos1 < vRandom.size() && nRndPos2 < vRandom.size());

    int nId1 = vRandom[nRndPos1];
    int nId2 = vRandom[nRndPos2];

    assert(mapInfo.count(nId1) == 1);
    assert(mapInfo.count(nId2) == 1);

    mapInfo[nId1].nRandomPos = nRndPos2;
    mapInfo[nId2].nRandomPos = nRndPos1;

    vRandom[nRndPos1] = nId2;
    vRandom[nRndPos2] = nId1;
}

void CAddrMan::Delete(int nId) {
    assert(mapInfo.count(nId) != 0);
    CAddrInfo &info = mapInfo[nId];
    assert(!info.fInTried);
    assert(info.nRefCount == 0);

    SwapRandom(info.nRandomPos, vRandom.size() - 1);
    vRandom.pop_back();
    mapAddr.erase(info);
    mapInfo.erase(nId);
    nNew--;
}

void CAddrMan::ClearNew(int nUBucket, int nUBucketPos) {
    // if there is an entry in the specified bucket, delete it.
    if (vvNew[nUBucket][nUBucketPos] != -1) {
        int nIdDelete = vvNew[nUBucket][nUBucketPos];
        CAddrInfo &infoDelete = mapInfo[nIdDelete];
        assert(infoDelete.nRefCount > 0);
        infoDelete.nRefCount--;
        vvNew[nUBucket][nUBucketPos] = -1;
        if (infoDelete.nRefCount == 0) {
            Delete(nIdDelete);
        }
    }
}

void CAddrMan::MakeTried(CAddrInfo &info, int nId) {
    // remove the entry from all new buckets
    for (int bucket = 0; bucket < ADDRMAN_NEW_BUCKET_COUNT; bucket++) {
        int pos = info.GetBucketPosition(nKey, true, bucket);
        if (vvNew[bucket][pos] == nId) {
            vvNew[bucket][pos] = -1;
            info.nRefCount--;
        }
    }
    nNew--;

    assert(info.nRefCount == 0);

    // which tried bucket to move the entry to
    int nKBucket = info.GetTriedBucket(nKey, m_asmap);
    int nKBucketPos = info.GetBucketPosition(nKey, false, nKBucket);

    // first make space to add it (the existing tried entry there is moved to
    // new, deleting whatever is there).
    if (vvTried[nKBucket][nKBucketPos] != -1) {
        // find an item to evict
        int nIdEvict = vvTried[nKBucket][nKBucketPos];
        assert(mapInfo.count(nIdEvict) == 1);
        CAddrInfo &infoOld = mapInfo[nIdEvict];

        // Remove the to-be-evicted item from the tried set.
        infoOld.fInTried = false;
        vvTried[nKBucket][nKBucketPos] = -1;
        nTried--;

        // find which new bucket it belongs to
        int nUBucket = infoOld.GetNewBucket(nKey, m_asmap);
        int nUBucketPos = infoOld.GetBucketPosition(nKey, true, nUBucket);
        ClearNew(nUBucket, nUBucketPos);
        assert(vvNew[nUBucket][nUBucketPos] == -1);

        // Enter it into the new set again.
        infoOld.nRefCount = 1;
        vvNew[nUBucket][nUBucketPos] = nIdEvict;
        nNew++;
    }
    assert(vvTried[nKBucket][nKBucketPos] == -1);

    vvTried[nKBucket][nKBucketPos] = nId;
    nTried++;
    info.fInTried = true;
}

void CAddrMan::Good_(const CService &addr, bool test_before_evict,
                     int64_t nTime) {
    int nId;

    nLastGood = nTime;

    CAddrInfo *pinfo = Find(addr, &nId);

    // if not found, bail out
    if (!pinfo) {
        return;
    }

    CAddrInfo &info = *pinfo;

    // check whether we are talking about the exact same CService (including
    // same port)
    if (info != addr) {
        return;
    }

    // update info
    info.nLastSuccess = nTime;
    info.nLastTry = nTime;
    info.nAttempts = 0;
    // nTime is not updated here, to avoid leaking information about
    // currently-connected peers.

    // if it is already in the tried set, don't do anything else
    if (info.fInTried) {
        return;
    }

    // find a bucket it is in now
    int nRnd = insecure_rand.randrange(ADDRMAN_NEW_BUCKET_COUNT);
    int nUBucket = -1;
    for (unsigned int n = 0; n < ADDRMAN_NEW_BUCKET_COUNT; n++) {
        int nB = (n + nRnd) % ADDRMAN_NEW_BUCKET_COUNT;
        int nBpos = info.GetBucketPosition(nKey, true, nB);
        if (vvNew[nB][nBpos] == nId) {
            nUBucket = nB;
            break;
        }
    }

    // if no bucket is found, something bad happened;
    // TODO: maybe re-add the node, but for now, just bail out
    if (nUBucket == -1) {
        return;
    }

    // which tried bucket to move the entry to
    int tried_bucket = info.GetTriedBucket(nKey, m_asmap);
    int tried_bucket_pos = info.GetBucketPosition(nKey, false, tried_bucket);

    // Will moving this address into tried evict another entry?
    if (test_before_evict && (vvTried[tried_bucket][tried_bucket_pos] != -1)) {
        // Output the entry we'd be colliding with, for debugging purposes
        auto colliding_entry =
            mapInfo.find(vvTried[tried_bucket][tried_bucket_pos]);
        LogPrint(BCLog::ADDRMAN,
                 "Collision inserting element into tried table (%s), moving %s "
                 "to m_tried_collisions=%d\n",
                 colliding_entry != mapInfo.end()
                     ? colliding_entry->second.ToString()
                     : "",
                 addr.ToString(), m_tried_collisions.size());
        if (m_tried_collisions.size() < ADDRMAN_SET_TRIED_COLLISION_SIZE) {
            m_tried_collisions.insert(nId);
        }
    } else {
        LogPrint(BCLog::ADDRMAN, "Moving %s to tried\n", addr.ToString());

        // move nId to the tried tables
        MakeTried(info, nId);
    }
}

bool CAddrMan::Add_(const CAddress &addr, const CNetAddr &source,
                    int64_t nTimePenalty) {
    if (!addr.IsRoutable()) {
        return false;
    }

    bool fNew = false;
    int nId;
    CAddrInfo *pinfo = Find(addr, &nId);

    // Do not set a penalty for a source's self-announcement
    if (addr == source) {
        nTimePenalty = 0;
    }

    if (pinfo) {
        // periodically update nTime
        bool fCurrentlyOnline = (GetAdjustedTime() - addr.nTime < 24 * 60 * 60);
        int64_t nUpdateInterval = (fCurrentlyOnline ? 60 * 60 : 24 * 60 * 60);
        if (addr.nTime &&
            (!pinfo->nTime ||
             pinfo->nTime < addr.nTime - nUpdateInterval - nTimePenalty)) {
            pinfo->nTime = std::max((int64_t)0, addr.nTime - nTimePenalty);
        }

        // add services
        pinfo->nServices = ServiceFlags(pinfo->nServices | addr.nServices);

        // do not update if no new information is present
        if (!addr.nTime || (pinfo->nTime && addr.nTime <= pinfo->nTime)) {
            return false;
        }

        // do not update if the entry was already in the "tried" table
        if (pinfo->fInTried) {
            return false;
        }

        // do not update if the max reference count is reached
        if (pinfo->nRefCount == ADDRMAN_NEW_BUCKETS_PER_ADDRESS) {
            return false;
        }

        // stochastic test: previous nRefCount == N: 2^N times harder to
        // increase it
        int nFactor = 1;
        for (int n = 0; n < pinfo->nRefCount; n++) {
            nFactor *= 2;
        }

        if (nFactor > 1 && (insecure_rand.randrange(nFactor) != 0)) {
            return false;
        }
    } else {
        pinfo = Create(addr, source, &nId);
        pinfo->nTime =
            std::max((int64_t)0, (int64_t)pinfo->nTime - nTimePenalty);
        nNew++;
        fNew = true;
    }

    int nUBucket = pinfo->GetNewBucket(nKey, source, m_asmap);
    int nUBucketPos = pinfo->GetBucketPosition(nKey, true, nUBucket);
    if (vvNew[nUBucket][nUBucketPos] != nId) {
        bool fInsert = vvNew[nUBucket][nUBucketPos] == -1;
        if (!fInsert) {
            CAddrInfo &infoExisting = mapInfo[vvNew[nUBucket][nUBucketPos]];
            if (infoExisting.IsTerrible() ||
                (infoExisting.nRefCount > 1 && pinfo->nRefCount == 0)) {
                // Overwrite the existing new table entry.
                fInsert = true;
            }
        }
        if (fInsert) {
            ClearNew(nUBucket, nUBucketPos);
            pinfo->nRefCount++;
            vvNew[nUBucket][nUBucketPos] = nId;
        } else if (pinfo->nRefCount == 0) {
            Delete(nId);
        }
    }
    return fNew;
}

void CAddrMan::Attempt_(const CService &addr, bool fCountFailure,
                        int64_t nTime) {
    CAddrInfo *pinfo = Find(addr);

    // if not found, bail out
    if (!pinfo) {
        return;
    }

    CAddrInfo &info = *pinfo;

    // check whether we are talking about the exact same CService (including
    // same port)
    if (info != addr) {
        return;
    }

    // update info
    info.nLastTry = nTime;
    if (fCountFailure && info.nLastCountAttempt < nLastGood) {
        info.nLastCountAttempt = nTime;
        info.nAttempts++;
    }
}

CAddrInfo CAddrMan::Select_(bool newOnly) {
    if (size() == 0) {
        return CAddrInfo();
    }

    if (newOnly && nNew == 0) {
        return CAddrInfo();
    }

    // Use a 50% chance for choosing between tried and new table entries.
    if (!newOnly &&
        (nTried > 0 && (nNew == 0 || insecure_rand.randbool() == 0))) {
        // use a tried node
        double fChanceFactor = 1.0;
        while (1) {
            int nKBucket = insecure_rand.randrange(ADDRMAN_TRIED_BUCKET_COUNT);
            int nKBucketPos = insecure_rand.randrange(ADDRMAN_BUCKET_SIZE);
            while (vvTried[nKBucket][nKBucketPos] == -1) {
                nKBucket = (nKBucket + insecure_rand.randbits(
                                           ADDRMAN_TRIED_BUCKET_COUNT_LOG2)) %
                           ADDRMAN_TRIED_BUCKET_COUNT;
                nKBucketPos = (nKBucketPos + insecure_rand.randbits(
                                                 ADDRMAN_BUCKET_SIZE_LOG2)) %
                              ADDRMAN_BUCKET_SIZE;
            }
            int nId = vvTried[nKBucket][nKBucketPos];
            assert(mapInfo.count(nId) == 1);
            CAddrInfo &info = mapInfo[nId];
            if (insecure_rand.randbits(30) <
                fChanceFactor * info.GetChance() * (1 << 30)) {
                return info;
            }
            fChanceFactor *= 1.2;
        }
    } else {
        // use a new node
        double fChanceFactor = 1.0;
        while (1) {
            int nUBucket = insecure_rand.randrange(ADDRMAN_NEW_BUCKET_COUNT);
            int nUBucketPos = insecure_rand.randrange(ADDRMAN_BUCKET_SIZE);
            while (vvNew[nUBucket][nUBucketPos] == -1) {
                nUBucket = (nUBucket + insecure_rand.randbits(
                                           ADDRMAN_NEW_BUCKET_COUNT_LOG2)) %
                           ADDRMAN_NEW_BUCKET_COUNT;
                nUBucketPos = (nUBucketPos + insecure_rand.randbits(
                                                 ADDRMAN_BUCKET_SIZE_LOG2)) %
                              ADDRMAN_BUCKET_SIZE;
            }
            int nId = vvNew[nUBucket][nUBucketPos];
            assert(mapInfo.count(nId) == 1);
            CAddrInfo &info = mapInfo[nId];
            if (insecure_rand.randbits(30) <
                fChanceFactor * info.GetChance() * (1 << 30)) {
                return info;
            }
            fChanceFactor *= 1.2;
        }
    }
}

#ifdef DEBUG_ADDRMAN
int CAddrMan::Check_() {
    std::set<int> setTried;
    std::map<int, int> mapNew;

    if (vRandom.size() != nTried + nNew) {
        return -7;
    }

    for (const auto &entry : mapInfo) {
        int n = entry.first;
        const CAddrInfo &info = entry.second;
        if (info.fInTried) {
            if (!info.nLastSuccess) {
                return -1;
            }
            if (info.nRefCount) {
                return -2;
            }
            setTried.insert(n);
        } else {
            if (info.nRefCount < 0 ||
                info.nRefCount > ADDRMAN_NEW_BUCKETS_PER_ADDRESS) {
                return -3;
            }
            if (!info.nRefCount) {
                return -4;
            }
            mapNew[n] = info.nRefCount;
        }
        if (mapAddr[info] != n) {
            return -5;
        }
        if (info.nRandomPos < 0 || info.nRandomPos >= vRandom.size() ||
            vRandom[info.nRandomPos] != n) {
            return -14;
        }
        if (info.nLastTry < 0) {
            return -6;
        }
        if (info.nLastSuccess < 0) {
            return -8;
        }
    }

    if (setTried.size() != nTried) {
        return -9;
    }
    if (mapNew.size() != nNew) {
        return -10;
    }

    for (int n = 0; n < ADDRMAN_TRIED_BUCKET_COUNT; n++) {
        for (int i = 0; i < ADDRMAN_BUCKET_SIZE; i++) {
            if (vvTried[n][i] != -1) {
                if (!setTried.count(vvTried[n][i])) {
                    return -11;
                }
                if (mapInfo[vvTried[n][i]].GetTriedBucket(nKey, m_asmap) != n) {
                    return -17;
                }
                if (mapInfo[vvTried[n][i]].GetBucketPosition(nKey, false, n) !=
                    i) {
                    return -18;
                }
                setTried.erase(vvTried[n][i]);
            }
        }
    }

    for (int n = 0; n < ADDRMAN_NEW_BUCKET_COUNT; n++) {
        for (int i = 0; i < ADDRMAN_BUCKET_SIZE; i++) {
            if (vvNew[n][i] != -1) {
                if (!mapNew.count(vvNew[n][i])) {
                    return -12;
                }
                if (mapInfo[vvNew[n][i]].GetBucketPosition(nKey, true, n) !=
                    i) {
                    return -19;
                }
                if (--mapNew[vvNew[n][i]] == 0) {
                    mapNew.erase(vvNew[n][i]);
                }
            }
        }
    }

    if (setTried.size()) {
        return -13;
    }
    if (mapNew.size()) {
        return -15;
    }
    if (nKey.IsNull()) {
        return -16;
    }

    return 0;
}
#endif

void CAddrMan::GetAddr_(std::vector<CAddress> &vAddr, size_t max_addresses, size_t max_pct) {
    size_t nNodes = vRandom.size();
    if (max_pct != 0) {
        nNodes = max_pct * nNodes / 100;
    }
    if (max_addresses != 0) {
        nNodes = std::min(nNodes, max_addresses);
    }

    // gather a list of random nodes, skipping those of low quality
    for (unsigned int n = 0; n < vRandom.size(); n++) {
        if (vAddr.size() >= nNodes) {
            break;
        }

        int nRndPos = insecure_rand.randrange(vRandom.size() - n) + n;
        SwapRandom(n, nRndPos);
        assert(mapInfo.count(vRandom[n]) == 1);

        const CAddrInfo &ai = mapInfo[vRandom[n]];
        if (!ai.IsTerrible()) {
            vAddr.push_back(ai);
        }
    }
}

void CAddrMan::Connected_(const CService &addr, int64_t nTime) {
    CAddrInfo *pinfo = Find(addr);

    // if not found, bail out
    if (!pinfo) {
        return;
    }

    CAddrInfo &info = *pinfo;

    // check whether we are talking about the exact same CService (including
    // same port)
    if (info != addr) {
        return;
    }

    // update info
    int64_t nUpdateInterval = 20 * 60;
    if (nTime - info.nTime > nUpdateInterval) {
        info.nTime = nTime;
    }
}

void CAddrMan::SetServices_(const CService &addr, ServiceFlags nServices) {
    CAddrInfo *pinfo = Find(addr);

    // if not found, bail out
    if (!pinfo) {
        return;
    }

    CAddrInfo &info = *pinfo;

    // check whether we are talking about the exact same CService (including
    // same port)
    if (info != addr) {
        return;
    }

    // update info
    info.nServices = nServices;
}

void CAddrMan::ResolveCollisions_() {
    const int64_t adjustedTime = GetAdjustedTime();

    for (std::set<int>::iterator it = m_tried_collisions.begin();
         it != m_tried_collisions.end();) {
        int id_new = *it;

        bool erase_collision = false;

        // If id_new not found in mapInfo remove it from m_tried_collisions.
        auto id_new_it = mapInfo.find(id_new);
        if (id_new_it == mapInfo.end()) {
            erase_collision = true;
        } else {
            CAddrInfo &info_new = id_new_it->second;

            // Which tried bucket to move the entry to.
            int tried_bucket = info_new.GetTriedBucket(nKey, m_asmap);
            int tried_bucket_pos =
                info_new.GetBucketPosition(nKey, false, tried_bucket);
            if (!info_new.IsValid()) {
                // id_new may no longer map to a valid address
                erase_collision = true;
            } else if (vvTried[tried_bucket][tried_bucket_pos] != -1) {
                // The position in the tried bucket is not empty

                // Get the to-be-evicted address that is being tested
                int id_old = vvTried[tried_bucket][tried_bucket_pos];
                CAddrInfo &info_old = mapInfo[id_old];

                // Has successfully connected in last X hours
                if (adjustedTime - info_old.nLastSuccess <
                    ADDRMAN_REPLACEMENT_SECONDS) {
                    erase_collision = true;
                } else if (adjustedTime - info_old.nLastTry <
                           ADDRMAN_REPLACEMENT_SECONDS) {
                    // attempted to connect and failed in last X hours

                    // Give address at least 60 seconds to successfully connect
                    if (GetAdjustedTime() - info_old.nLastTry > 60) {
                        LogPrint(BCLog::ADDRMAN,
                                 "Replacing %s with %s in tried table\n",
                                 info_old.ToString(), info_new.ToString());

                        // Replaces an existing address already in the tried
                        // table with the new address
                        Good_(info_new, false, GetAdjustedTime());
                        erase_collision = true;
                    }
                } else if (GetAdjustedTime() - info_new.nLastSuccess >
                           ADDRMAN_TEST_WINDOW) {
                    // If the collision hasn't resolved in some reasonable
                    // amount of time, just evict the old entry -- we must not
                    // be able to connect to it for some reason.
                    LogPrint(BCLog::ADDRMAN,
                             "Unable to test; replacing %s with %s in tried "
                             "table anyway\n",
                             info_old.ToString(), info_new.ToString());
                    Good_(info_new, false, GetAdjustedTime());
                    erase_collision = true;
                }
            } else {
                // Collision is not actually a collision anymore
                Good_(info_new, false, adjustedTime);
                erase_collision = true;
            }
        }

        if (erase_collision) {
            m_tried_collisions.erase(it++);
        } else {
            it++;
        }
    }
}

CAddrInfo CAddrMan::SelectTriedCollision_() {
    if (m_tried_collisions.size() == 0) {
        return CAddrInfo();
    }

    std::set<int>::iterator it = m_tried_collisions.begin();

    // Selects a random element from m_tried_collisions
    std::advance(it, insecure_rand.randrange(m_tried_collisions.size()));
    int id_new = *it;

    // If id_new not found in mapInfo remove it from m_tried_collisions.
    auto id_new_it = mapInfo.find(id_new);
    if (id_new_it == mapInfo.end()) {
        m_tried_collisions.erase(it);
        return CAddrInfo();
    }

    CAddrInfo &newInfo = id_new_it->second;

    // which tried bucket to move the entry to
    int tried_bucket = newInfo.GetTriedBucket(nKey, m_asmap);
    int tried_bucket_pos = newInfo.GetBucketPosition(nKey, false, tried_bucket);

    int id_old = vvTried[tried_bucket][tried_bucket_pos];

    return mapInfo[id_old];
}

std::vector<bool> CAddrMan::DecodeAsmap(fs::path path) {
    std::vector<bool> bits;
    FILE *filestr = fsbridge::fopen(path, "rb");
    CAutoFile file(filestr, SER_DISK, CLIENT_VERSION);
    if (file.IsNull()) {
        LogPrintf("Failed to open asmap file %s: %s\n", path, std::strerror(errno));
        return bits;
    }
    std::fseek(filestr, 0, SEEK_END);
    const long length = std::ftell(filestr);
    if (length < 0L) {
        LogPrintf("Cannot determine size of asmap file %s: %s\n", path, std::strerror(errno));
        return bits;
    }
    LogPrintf("Opened asmap file %s (%d bytes) from disk\n", path, length);
    std::fseek(filestr, 0, SEEK_SET);
    uint8_t cur_byte;
    bits.reserve(length * 8);
    for (long i = 0; i < length; ++i) {
        file >> cur_byte;
        for (int bit = 0; bit < 8; ++bit) {
            bits.push_back((cur_byte >> bit) & 1);
        }
    }
    if (!SanityCheckASMap(bits)) {
        LogPrintf("Sanity check of asmap file %s failed\n", path);
        bits.clear();
    }
    return bits;
}
