// Copyright (c) 2017-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <seeder/db.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>

void CAddrInfo::Update(bool good) {
    int64_t now = std::time(nullptr);
    if (ourLastTry == 0) {
        ourLastTry = now - MIN_RETRY;
    }
    int age = now - ourLastTry;
    lastTry = now;
    ourLastTry = now;
    total++;
    if (good) {
        success++;
        ourLastSuccess = now;
    }
    stat2H.Update(good, age, 3600 * 2);
    stat8H.Update(good, age, 3600 * 8);
    stat1D.Update(good, age, 3600 * 24);
    stat1W.Update(good, age, 3600 * 24 * 7);
    stat1M.Update(good, age, 3600 * 24 * 30);
    int64_t ign = GetIgnoreTime();
    if (ign && (ignoreTill == 0 || ignoreTill < ign + now)) {
        ignoreTill = ign + now;
    }
    //  std::fprintf(stdout, "%s: got %s result: success=%i/%i;
    //  2H:%.2f%%-%.2f%%(%.2f) 8H:%.2f%%-%.2f%%(%.2f) 1D:%.2f%%-%.2f%%(%.2f)
    //  1W:%.2f%%-%.2f%%(%.2f) \n", ToString(ip).c_str(), good ? "good" : "bad",
    //  success, total, 100.0 * stat2H.reliability, 100.0 * (stat2H.reliability
    //  + 1.0 - stat2H.weight), stat2H.count, 100.0 * stat8H.reliability, 100.0
    //  * (stat8H.reliability + 1.0 - stat8H.weight), stat8H.count, 100.0 *
    //  stat1D.reliability, 100.0 * (stat1D.reliability + 1.0 - stat1D.weight),
    //  stat1D.count, 100.0 * stat1W.reliability, 100.0 * (stat1W.reliability
    //  + 1.0 - stat1W.weight), stat1W.count);
}

bool CAddrDb::Get_(CServiceResult &ip, int &wait) {
    int64_t now = std::time(nullptr);
    size_t tot = unkId.size() + ourId.size();
    if (tot == 0) {
        wait = 5;
        return false;
    }

    do {
        size_t rnd = std::rand() % tot;
        int ret;
        if (rnd < unkId.size()) {
            std::set<int>::iterator it = unkId.end();
            it--;
            ret = *it;
            unkId.erase(it);
        } else {
            ret = ourId.front();
            if (std::time(nullptr) - idToInfo[ret].ourLastTry < MIN_RETRY) {
                return false;
            }
            ourId.pop_front();
        }

        if (idToInfo[ret].ignoreTill && idToInfo[ret].ignoreTill < now) {
            ourId.push_back(ret);
            idToInfo[ret].ourLastTry = now;
        } else {
            ip.service = idToInfo[ret].ip;
            ip.ourLastSuccess = idToInfo[ret].ourLastSuccess;
            ip.lastAddressRequest = idToInfo[ret].lastAddressRequest;
            break;
        }
    } while (1);

    nDirty++;
    return true;
}

int CAddrDb::Lookup_(const CService &ip) const {
    auto it = ipToId.find(ip);
    if (it != ipToId.end()) {
        return it->second;
    }
    return -1;
}

void CAddrDb::Good_(const CServiceResult &res) {
    int id = Lookup_(res.service);
    if (id == -1) {
        return;
    }
    unkId.erase(id);
    banned.erase(res.service);
    CAddrInfo &info = idToInfo[id];
    info.clientVersion = res.nClientV;
    info.clientSubVersion = res.strClientV;
    info.blocks = res.nHeight;
    info.services = res.services;
    info.lastAddressRequest = res.lastAddressRequest;
    info.Update(true);
    if (info.IsReliable() && goodId.count(id) == 0) {
        goodId.insert(id);
        //    std::fprintf(stdout, "%s: good; %i good nodes now\n",
        //    ToString(addr).c_str(), (int)goodId.size());
    }
    nDirty++;
    ourId.push_back(id);
}

void CAddrDb::Bad_(const CServiceResult &res) {
    int ban = res.nBanTime;
    int id = Lookup_(res.service);
    if (id == -1) {
        return;
    }
    unkId.erase(id);
    CAddrInfo &info = idToInfo[id];
    info.Update(false);
    std::time_t now = std::time(nullptr);
    int ter = info.GetBanTime();
    if (ter) {
        //    std::fprintf(stdout, "%s: terrible\n", ToString(addr).c_str());
        if (ban < ter) {
            ban = ter;
        }
    }
    if (ban > 0) {
        //    std::fprintf(stdout, "%s: ban for %i seconds\n",
        //    ToString(addr).c_str(), ban);
        banned[info.ip] = ban + now;
        ipToId.erase(info.ip);
        goodId.erase(id);
        idToInfo.erase(id);
    } else {
        if (/*!info.IsReliable() && */ goodId.count(id) == 1) {
            goodId.erase(id);
            //      std::fprintf(stdout, "%s: not good; %i good nodes left\n",
            //      ToString(addr).c_str(), (int)goodId.size());
        }
        ourId.push_back(id);
    }
    nDirty++;
}

void CAddrDb::Add_(const CAddress &addr, bool force) {
    if (!force && !addr.IsRoutable()) {
        return;
    }
    CService ipp(addr);
    if (banned.count(ipp)) {
        std::time_t bantime = banned[ipp];
        if (force || (bantime < std::time(nullptr) && static_cast<std::time_t>(addr.nTime) > bantime)) {
            banned.erase(ipp);
        } else {
            return;
        }
    }
    if (ipToId.count(ipp)) {
        CAddrInfo &ai = idToInfo[ipToId[ipp]];
        if (addr.nTime > ai.lastTry) {
            ai.lastTry = addr.nTime;
            //      std::fprintf(stdout, "%s: updated\n", ToString(addr).c_str());
        }
        if (force) {
            ai.ignoreTill = 0;
        }
        return;
    }

    CAddrInfo ai;
    ai.ip = ipp;
    ai.services = addr.nServices;
    ai.lastTry = addr.nTime;
    ai.ourLastTry = 0;
    ai.total = 0;
    ai.success = 0;
    int id = nId++;
    idToInfo[id] = ai;
    ipToId[ipp] = id;
    //  std::fprintf(stdout, "%s: added\n", ToString(ipp).c_str(), ipToId[ipp]);
    unkId.insert(id);
    nDirty++;
}

void CAddrDb::GetIPs_(std::set<CNetAddr> &ips, uint64_t requestedFlags,
                      uint32_t max, const bool *nets) {
    if (goodId.size() == 0) {
        int id = -1;
        if (ourId.size() == 0) {
            if (unkId.size() == 0) {
                return;
            }
            id = *unkId.begin();
        } else {
            id = *ourId.begin();
        }

        if (id >= 0 &&
            (idToInfo[id].services & requestedFlags) == requestedFlags) {
            ips.insert(idToInfo[id].ip);
        }
        return;
    }

    std::vector<int> goodIdFiltered;
    for (auto &id : goodId) {
        if ((idToInfo[id].services & requestedFlags) == requestedFlags) {
            goodIdFiltered.push_back(id);
        }
    }

    if (!goodIdFiltered.size()) {
        return;
    }

    if (max > goodIdFiltered.size() / 2) {
        max = goodIdFiltered.size() / 2;
    }

    if (max < 1) {
        max = 1;
    }

    std::set<int> ids;
    while (ids.size() < max) {
        ids.insert(goodIdFiltered[std::rand() % goodIdFiltered.size()]);
    }

    for (auto &id : ids) {
        CService &ip = idToInfo[id].ip;
        if (nets[ip.GetNetwork()]) {
            ips.insert(ip);
        }
    }
}
