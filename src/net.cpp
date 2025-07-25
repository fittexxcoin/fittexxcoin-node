// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Fittexxcoin Core developers
// Copyright (c) 2021-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/fittexxcoin-config.h>
#endif

#include <net.h>

#include <banman.h>
#include <chainparams.h>
#include <clientversion.h>
#include <config.h>
#include <consensus/consensus.h>
#include <crypto/common.h>
#include <crypto/sha256.h>
#include <hash.h>
#include <net_permissions.h>
#include <netbase.h>
#include <primitives/transaction.h>
#include <protocol.h>
#include <scheduler.h>
#include <span.h>
#include <ui_interface.h>
#include <util/bit_cast.h>
#include <util/strencodings.h>

#ifdef WIN32
#include <cstring>
#else
#include <fcntl.h>
#endif

#ifdef USE_POLL
#include <poll.h>
#endif

#ifdef USE_UPNP
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/miniwget.h>
#include <miniupnpc/upnpcommands.h>
#include <miniupnpc/upnperrors.h>
#endif

#include <cmath>
#include <unordered_map>

// Dump addresses to peers.dat every 15 minutes (900s)
static constexpr int DUMP_PEERS_INTERVAL = 15 * 60;

// We add a random period time (0 to 1 seconds) to feeler connections to prevent
// synchronization.
#define FEELER_SLEEP_WINDOW 1

// MSG_NOSIGNAL is not available on some platforms, if it doesn't exist define
// it as 0
#if !defined(MSG_NOSIGNAL)
#define MSG_NOSIGNAL 0
#endif

// MSG_DONTWAIT is not available on some platforms, if it doesn't exist define
// it as 0
#if !defined(MSG_DONTWAIT)
#define MSG_DONTWAIT 0
#endif

/** Used to pass flags to the Bind() function */
enum BindFlags {
    BF_NONE = 0,
    BF_EXPLICIT = (1U << 0),
    BF_REPORT_ERROR = (1U << 1),
    /**
     * Do not call AddLocal() for our special addresses, e.g., for incoming
     * Tor connections, to prevent gossiping them over the network.
     */
    BF_DONT_ADVERTISE = (1U << 2),
};

// The set of sockets cannot be modified while waiting
// The sleep time needs to be small to avoid new sockets stalling
static const uint64_t SELECT_TIMEOUT_MILLISECONDS = 50;

const static std::string NET_MESSAGE_TYPE_OTHER = "*other*";

// SHA256("netgroup")[0:8]
static const uint64_t RANDOMIZER_ID_NETGROUP = 0x6c0edd8036ef4036ULL;
// SHA256("localhostnonce")[0:8]
static const uint64_t RANDOMIZER_ID_LOCALHOSTNONCE = 0xd93e69e2bbfa5735ULL;
// SHA256("addrcache")[0:8]
static const uint64_t RANDOMIZER_ID_ADDRCACHE = 0x1cf2e4ddd306dda9ULL;
//
// Global state variables
//
bool fDiscover = true;
bool fListen = true;
bool g_relay_txes = !DEFAULT_BLOCKSONLY;
RecursiveMutex cs_mapLocalHost;
std::map<CNetAddr, LocalServiceInfo> mapLocalHost GUARDED_BY(cs_mapLocalHost);
static bool vfLimited[NET_MAX] GUARDED_BY(cs_mapLocalHost) = {};

void CConnman::AddOneShot(const std::string &strDest) {
    LOCK(cs_vOneShots);
    vOneShots.push_back(strDest);
}

unsigned short GetListenPort() {
    return (unsigned short)(gArgs.GetArg("-port", Params().GetDefaultPort()));
}

// find 'best' local address for a particular peer
bool GetLocal(CService &addr, const CNetAddr *paddrPeer) {
    if (!fListen) {
        return false;
    }

    int nBestScore = -1;
    int nBestReachability = -1;
    {
        LOCK(cs_mapLocalHost);
        for (const auto &entry : mapLocalHost) {
            int nScore = entry.second.nScore;
            int nReachability = entry.first.GetReachabilityFrom(paddrPeer);
            if (nReachability > nBestReachability ||
                (nReachability == nBestReachability && nScore > nBestScore)) {
                addr = CService(entry.first, entry.second.nPort);
                nBestReachability = nReachability;
                nBestScore = nScore;
            }
        }
    }
    return nBestScore >= 0;
}

//! Convert the pnSeed6 array into usable address objects.
static std::vector<CAddress>
convertSeed6(const std::vector<SeedSpec6> &vSeedsIn) {
    // It'll only connect to one or two seed nodes because once it connects,
    // it'll get a pile of addresses with newer timestamps. Seed nodes are given
    // a random 'last seen time' of between one and two weeks ago.
    const int64_t nOneWeek = 7 * 24 * 60 * 60;
    std::vector<CAddress> vSeedsOut;
    vSeedsOut.reserve(vSeedsIn.size());
    FastRandomContext rng;
    for (const auto &seed_in : vSeedsIn) {
        CAddress addr(seed_in, GetDesirableServiceFlags(NODE_NONE));
        addr.nTime = GetTime() - rng.randrange(nOneWeek) - nOneWeek;
        vSeedsOut.push_back(addr);
    }
    return vSeedsOut;
}

// Get best local address for a particular peer as a CAddress. Otherwise, return
// the unroutable 0.0.0.0 but filled in with the normal parameters, since the IP
// may be changed to a useful one by discovery.
CAddress GetLocalAddress(const CNetAddr *paddrPeer,
                         ServiceFlags nLocalServices) {
    CAddress ret(CService(CNetAddr(), GetListenPort()), nLocalServices);
    CService addr;
    if (GetLocal(addr, paddrPeer)) {
        ret = CAddress(addr, nLocalServices);
    }
    ret.nTime = GetAdjustedTime();
    return ret;
}

static int GetnScore(const CService &addr) {
    LOCK(cs_mapLocalHost);
    if (mapLocalHost.count(addr) == 0) {
        return 0;
    }
    return mapLocalHost[addr].nScore;
}

// Is our peer's addrLocal potentially useful as an external IP source?
bool IsPeerAddrLocalGood(CNode *pnode) {
    CService addrLocal = pnode->GetAddrLocal();
    return fDiscover && pnode->addr.IsRoutable() && addrLocal.IsRoutable() &&
           IsReachable(addrLocal.GetNetwork());
}

// Pushes our own address to a peer.
void AdvertiseLocal(CNode *pnode) {
    if (fListen && pnode->fSuccessfullyConnected) {
        CAddress addrLocal =
            GetLocalAddress(&pnode->addr, pnode->GetLocalServices());
        if (gArgs.GetBoolArg("-addrmantest", false)) {
            // use IPv4 loopback during addrmantest
            addrLocal =
                CAddress(CService(LookupNumeric("127.0.0.1", GetListenPort())),
                         pnode->GetLocalServices());
        }
        // If discovery is enabled, sometimes give our peer the address it
        // tells us that it sees us as in case it has a better idea of our
        // address than we do.
        FastRandomContext rng;
        if (IsPeerAddrLocalGood(pnode) &&
            (!addrLocal.IsRoutable() ||
             rng.randbits((GetnScore(addrLocal) > LOCAL_MANUAL) ? 3 : 1) ==
                 0)) {
            addrLocal.SetIP(pnode->GetAddrLocal());
        }
        if (addrLocal.IsRoutable() || gArgs.GetBoolArg("-addrmantest", false)) {
            LogPrint(BCLog::NET, "AdvertiseLocal: advertising address %s\n",
                     addrLocal.ToString());
            pnode->PushAddress(addrLocal, rng);
        }
    }
}

// Learn a new local address.
bool AddLocal(const CService &addr, int nScore) {
    if (!addr.IsRoutable()) {
        return false;
    }

    if (!fDiscover && nScore < LOCAL_MANUAL) {
        return false;
    }

    if (!IsReachable(addr)) {
        return false;
    }

    LogPrintf("AddLocal(%s,%i)\n", addr.ToString(), nScore);

    {
        LOCK(cs_mapLocalHost);
        bool fAlready = mapLocalHost.count(addr) > 0;
        LocalServiceInfo &info = mapLocalHost[addr];
        if (!fAlready || nScore >= info.nScore) {
            info.nScore = nScore + (fAlready ? 1 : 0);
            info.nPort = addr.GetPort();
        }
    }

    return true;
}

bool AddLocal(const CNetAddr &addr, int nScore) {
    return AddLocal(CService(addr, GetListenPort()), nScore);
}

void RemoveLocal(const CService &addr) {
    LOCK(cs_mapLocalHost);
    LogPrintf("RemoveLocal(%s)\n", addr.ToString());
    mapLocalHost.erase(addr);
}

void SetReachable(enum Network net, bool reachable) {
    if (net == NET_UNROUTABLE || net == NET_INTERNAL) {
        return;
    }
    LOCK(cs_mapLocalHost);
    vfLimited[net] = !reachable;
}

bool IsReachable(enum Network net) {
    LOCK(cs_mapLocalHost);
    return !vfLimited[net];
}

bool IsReachable(const CNetAddr &addr) {
    return IsReachable(addr.GetNetwork());
}

/** vote for a local address */
bool SeenLocal(const CService &addr) {
    LOCK(cs_mapLocalHost);
    if (mapLocalHost.count(addr) == 0) {
        return false;
    }
    mapLocalHost[addr].nScore++;
    return true;
}

/** check whether a given address is potentially local */
bool IsLocal(const CService &addr) {
    LOCK(cs_mapLocalHost);
    return mapLocalHost.count(addr) > 0;
}

CNode *CConnman::FindNode(const CNetAddr &ip) {
    LOCK(cs_vNodes);
    for (CNode *pnode : vNodes) {
        if (static_cast<CNetAddr>(pnode->addr) == ip) {
            return pnode;
        }
    }
    return nullptr;
}

CNode *CConnman::FindNode(const CSubNet &subNet) {
    LOCK(cs_vNodes);
    for (CNode *pnode : vNodes) {
        if (subNet.Match(static_cast<CNetAddr>(pnode->addr))) {
            return pnode;
        }
    }
    return nullptr;
}

CNode *CConnman::FindNode(const std::string &addrName) {
    LOCK(cs_vNodes);
    for (CNode *pnode : vNodes) {
        if (pnode->GetAddrName() == addrName) {
            return pnode;
        }
    }
    return nullptr;
}

CNode *CConnman::FindNode(const CService &addr) {
    LOCK(cs_vNodes);
    for (CNode *pnode : vNodes) {
        if (static_cast<CService>(pnode->addr) == addr) {
            return pnode;
        }
    }
    return nullptr;
}

bool CConnman::CheckIncomingNonce(uint64_t nonce) {
    LOCK(cs_vNodes);
    for (const CNode *pnode : vNodes) {
        if (!pnode->fSuccessfullyConnected && !pnode->fInbound &&
            pnode->GetLocalNonce() == nonce) {
            return false;
        }
    }
    return true;
}

/** Get the bind address for a socket as CAddress */
static CAddress GetBindAddress(SOCKET sock) {
    CAddress addr_bind;
    struct sockaddr_storage sockaddr_bind;
    socklen_t sockaddr_bind_len = sizeof(sockaddr_bind);
    if (sock != INVALID_SOCKET) {
        if (!getsockname(sock, (struct sockaddr *)&sockaddr_bind,
                         &sockaddr_bind_len)) {
            addr_bind.SetSockAddr(sockaddr_bind);
        } else {
            LogPrint(BCLog::NET, "Warning: getsockname failed\n");
        }
    }
    return addr_bind;
}

CNode *CConnman::ConnectNode(CAddress addrConnect, const char *pszDest,
                             bool fCountFailure, bool manual_connection) {
    if (pszDest == nullptr) {
        if (IsLocal(addrConnect)) {
            return nullptr;
        }

        // Look for an existing connection
        CNode *pnode = FindNode(static_cast<CService>(addrConnect));
        if (pnode) {
            LogPrintf("Failed to open new connection, already connected\n");
            return nullptr;
        }
    }

    /// debug print
    LogPrint(BCLog::NET, "trying connection %s lastseen=%.1fhrs\n",
             pszDest ? pszDest : addrConnect.ToString(),
             pszDest
                 ? 0.0
                 : (double)(GetAdjustedTime() - addrConnect.nTime) / 3600.0);

    // Resolve
    const int default_port = Params().GetDefaultPort();
    if (pszDest) {
        std::vector<CService> resolved;
        if (Lookup(pszDest, resolved, default_port,
                   fNameLookup && !HaveNameProxy(), 256) &&
            !resolved.empty()) {
            addrConnect =
                CAddress(resolved[GetRand(resolved.size())], NODE_NONE);
            if (!addrConnect.IsValid()) {
                LogPrint(BCLog::NET,
                         "Resolver returned invalid address %s for %s\n",
                         addrConnect.ToString(), pszDest);
                return nullptr;
            }
            // It is possible that we already have a connection to the IP/port
            // pszDest resolved to. In that case, drop the connection that was
            // just created, and return the existing CNode instead. Also store
            // the name we used to connect in that CNode, so that future
            // FindNode() calls to that name catch this early.
            LOCK(cs_vNodes);
            CNode *pnode = FindNode(static_cast<CService>(addrConnect));
            if (pnode) {
                pnode->MaybeSetAddrName(std::string(pszDest));
                LogPrintf("Failed to open new connection, already connected\n");
                return nullptr;
            }
        }
    }

    // Connect
    bool connected = false;
    SOCKET hSocket = INVALID_SOCKET;
    proxyType proxy;
    if (addrConnect.IsValid()) {
        bool proxyConnectionFailed = false;

        if (GetProxy(addrConnect.GetNetwork(), proxy)) {
            hSocket = CreateSocket(proxy.proxy);
            if (hSocket == INVALID_SOCKET) {
                return nullptr;
            }
            connected = ConnectThroughProxy(proxy, addrConnect.ToStringIP(), addrConnect.GetPort(), hSocket,
                                            nConnectTimeout, proxyConnectionFailed);
        } else {
            // no proxy needed (none set for target network)
            hSocket = CreateSocket(addrConnect);
            if (hSocket == INVALID_SOCKET) {
                return nullptr;
            }
            connected = ConnectSocketDirectly(
                addrConnect, hSocket, nConnectTimeout, manual_connection);
        }
        if (!proxyConnectionFailed) {
            // If a connection to the node was attempted, and failure (if any)
            // is not caused by a problem connecting to the proxy, mark this as
            // an attempt.
            addrman.Attempt(addrConnect, fCountFailure);
        }
    } else if (pszDest && GetNameProxy(proxy)) {
        hSocket = CreateSocket(proxy.proxy);
        if (hSocket == INVALID_SOCKET) {
            return nullptr;
        }
        std::string host;
        int port = default_port;
        SplitHostPort(std::string(pszDest), port, host);
        bool proxyConnectionFailed;
        connected = ConnectThroughProxy(proxy, host, port, hSocket, nConnectTimeout, proxyConnectionFailed);
    }
    if (!connected) {
        CloseSocket(hSocket);
        return nullptr;
    }

    // Add node
    NodeId id = GetNewNodeId();
    uint64_t nonce = GetDeterministicRandomizer(RANDOMIZER_ID_LOCALHOSTNONCE)
                         .Write(id)
                         .Finalize();
    CAddress addr_bind = GetBindAddress(hSocket);
    CNode *pnode = new CNode(id, nLocalServices, GetBestHeight(), hSocket,
                             addrConnect, CalculateKeyedNetGroup(addrConnect),
                             nonce, addr_bind, pszDest ? pszDest : "", false);
    pnode->AddRef();

    return pnode;
}

void CNode::CloseSocketDisconnect() {
    fDisconnect = true;
    LOCK(cs_hSocket);
    if (hSocket != INVALID_SOCKET) {
        LogPrint(BCLog::NET, "disconnecting peer=%d\n", id);
        CloseSocket(hSocket);
    }
}

void CConnman::AddWhitelistPermissionFlags(NetPermissionFlags &flags,
                                           const CNetAddr &addr) const {
    for (const auto &subnet : vWhitelistedRange) {
        if (subnet.m_subnet.Match(addr)) {
            NetPermissions::AddFlag(flags, subnet.m_flags);
        }
    }
}

std::string CNode::GetAddrName() const {
    LOCK(cs_addrName);
    return addrName;
}

void CNode::MaybeSetAddrName(const std::string &addrNameIn) {
    LOCK(cs_addrName);
    if (addrName.empty()) {
        addrName = addrNameIn;
    }
}

CService CNode::GetAddrLocal() const {
    LOCK(cs_addrLocal);
    return addrLocal;
}

void CNode::SetAddrLocal(const CService &addrLocalIn) {
    LOCK(cs_addrLocal);
    if (addrLocal.IsValid()) {
        error("Addr local already set for node: %i. Refusing to change from %s "
              "to %s",
              id, addrLocal.ToString(), addrLocalIn.ToString());
    } else {
        addrLocal = addrLocalIn;
    }
}

void CNode::copyStats(CNodeStats &stats, const std::vector<bool> &m_asmap) {
    stats.nodeid = this->GetId();
    stats.nServices = nServices;
    stats.addr = addr;
    stats.addrBind = addrBind;
    stats.m_mapped_as = addr.GetMappedAS(m_asmap);
    {
        LOCK(cs_filter);
        stats.fRelayTxes = fRelayTxes;
    }
    stats.nLastSend = nLastSend;
    stats.nLastRecv = nLastRecv;
    stats.nTimeConnected = nTimeConnected;
    stats.nTimeOffset = nTimeOffset;
    stats.addrName = GetAddrName();
    stats.nVersion = nVersion;
    {
        LOCK(cs_SubVer);
        stats.cleanSubVer = cleanSubVer;
    }
    stats.fInbound = fInbound;
    stats.m_manual_connection = m_manual_connection;
    stats.nStartingHeight = nStartingHeight;
    {
        LOCK(cs_vSend);
        stats.mapSendBytesPerMsgType = mapSendBytesPerMsgType;
        stats.nSendBytes = nSendBytes;
    }
    {
        LOCK(cs_vRecv);
        stats.mapRecvBytesPerMsgType = mapRecvBytesPerMsgType;
        stats.nRecvBytes = nRecvBytes;
    }
    stats.m_legacyWhitelisted = m_legacyWhitelisted;
    stats.m_permissionFlags = m_permissionFlags;
    {
        LOCK(cs_feeFilter);
        stats.minFeeFilter = minFeeFilter;
    }

    // It is common for nodes with good ping times to suddenly become lagged,
    // due to a new block arriving or other large transfer. Merely reporting
    // pingtime might fool the caller into thinking the node was still
    // responsive, since pingtime does not update until the ping is complete,
    // which might take a while. So, if a ping is taking an unusually long time
    // in flight, the caller can immediately detect that this is happening.
    int64_t nPingUsecWait = 0;
    if ((0 != nPingNonceSent) && (0 != nPingUsecStart)) {
        nPingUsecWait = GetTimeMicros() - nPingUsecStart;
    }

    // Raw ping time is in microseconds, but show it to user as whole seconds
    // (Fittexxcoin users should be well used to small numbers with many decimal
    // places by now :)
    stats.dPingTime = ((double(nPingUsecTime)) / 1e6);
    stats.dMinPing = ((double(nMinPingUsecTime)) / 1e6);
    stats.dPingWait = ((double(nPingUsecWait)) / 1e6);

    // Leave string empty if addrLocal invalid (not filled in yet)
    CService addrLocalUnlocked = GetAddrLocal();
    stats.addrLocal =
        addrLocalUnlocked.IsValid() ? addrLocalUnlocked.ToString() : "";
}

static bool IsOversizedMessage(const Config &config, const CNetMessage &msg) {
    if (!msg.in_data) {
        // Header only, cannot be oversized.
        return false;
    }

    return msg.hdr.IsOversized(config);
}

bool CNode::ReceiveMsgBytes(const Config &config, const char *pch,
                            uint32_t nBytes, bool &complete) {
    complete = false;
    int64_t nTimeMicros = GetTimeMicros();
    LOCK(cs_vRecv);
    nLastRecv = nTimeMicros / 1000000;
    nRecvBytes += nBytes;
    while (nBytes > 0) {
        // Get current incomplete message, or create a new one.
        if (vRecvMsg.empty() || vRecvMsg.back().complete()) {
            vRecvMsg.push_back(CNetMessage(config.GetChainParams().NetMagic(),
                                           SER_NETWORK, INIT_PROTO_VERSION));
        }

        CNetMessage &msg = vRecvMsg.back();

        // Absorb network data.
        int handled;
        if (!msg.in_data) {
            handled = msg.readHeader(config, pch, nBytes);
        } else {
            handled = msg.readData(pch, nBytes);
        }

        if (handled < 0) {
            return false;
        }

        if (IsOversizedMessage(config, msg)) {
            LogPrint(BCLog::NET,
                     "Oversized message from peer=%i, disconnecting\n",
                     GetId());
            return false;
        }

        pch += handled;
        nBytes -= handled;

        if (msg.complete()) {
            // Store received bytes per message command to prevent a memory DOS,
            // only allow valid commands.
            mapMsgTypeSize::iterator i =
                mapRecvBytesPerMsgType.find(msg.hdr.pchCommand.data());
            if (i == mapRecvBytesPerMsgType.end()) {
                i = mapRecvBytesPerMsgType.find(NET_MESSAGE_TYPE_OTHER);
            }

            assert(i != mapRecvBytesPerMsgType.end());
            i->second += msg.hdr.nMessageSize + CMessageHeader::HEADER_SIZE;

            msg.nTime = nTimeMicros;
            complete = true;
        }
    }

    return true;
}

void CNode::SetSendVersion(int nVersionIn) {
    // Send version may only be changed in the version message, and only one
    // version message is allowed per session. We can therefore treat this value
    // as const and even atomic as long as it's only used once a version message
    // has been successfully processed. Any attempt to set this twice is an
    // error.
    if (nSendVersion != 0) {
        error("Send version already set for node: %i. Refusing to change from "
              "%i to %i",
              id, nSendVersion, nVersionIn);
    } else {
        nSendVersion = nVersionIn;
    }
}

int CNode::GetSendVersion() const {
    // The send version should always be explicitly set to INIT_PROTO_VERSION
    // rather than using this value until SetSendVersion has been called.
    if (nSendVersion == 0) {
        error("Requesting unset send version for node: %i. Using %i", id,
              INIT_PROTO_VERSION);
        return INIT_PROTO_VERSION;
    }
    return nSendVersion;
}

int CNetMessage::readHeader(const Config &config, const char *pch,
                            uint32_t nBytes) {
    // copy data to temporary parsing buffer
    uint32_t nRemaining = CMessageHeader::HEADER_SIZE - nHdrPos;
    uint32_t nCopy = std::min(nRemaining, nBytes);

    memcpy(&hdrbuf[nHdrPos], pch, nCopy);
    nHdrPos += nCopy;

    // if header incomplete, exit
    if (nHdrPos < CMessageHeader::HEADER_SIZE) {
        return nCopy;
    }

    // deserialize to CMessageHeader
    try {
        hdrbuf >> hdr;
    } catch (const std::exception &) {
        return -1;
    }

    // Reject oversized messages
    if (hdr.IsOversized(config)) {
        LogPrint(BCLog::NET, "Oversized header detected\n");
        return -1;
    }

    // switch state to reading message data
    in_data = true;

    return nCopy;
}

int CNetMessage::readData(const char *pch, uint32_t nBytes) {
    unsigned int nRemaining = hdr.nMessageSize - nDataPos;
    unsigned int nCopy = std::min(nRemaining, nBytes);

    if (vRecv.size() < nDataPos + nCopy) {
        // Allocate up to 256 KiB ahead, but never more than the total message
        // size.
        vRecv.resize(std::min(hdr.nMessageSize, nDataPos + nCopy + 256 * 1024));
    }

    hasher.Write({UInt8Cast(pch), nCopy});
    memcpy(&vRecv[nDataPos], pch, nCopy);
    nDataPos += nCopy;

    return nCopy;
}

const uint256 &CNetMessage::GetMessageHash() const {
    assert(complete());
    if (data_hash.IsNull()) {
        hasher.Finalize(data_hash);
    }
    return data_hash;
}

size_t CConnman::SocketSendData(CNode *pnode) const
    EXCLUSIVE_LOCKS_REQUIRED(pnode->cs_vSend) {
    size_t nSentSize = 0;
    size_t nMsgCount = 0;

    for (const auto &data : pnode->vSendMsg) {
        assert(data.size() > pnode->nSendOffset);
        int nBytes = 0;

        {
            LOCK(pnode->cs_hSocket);
            if (pnode->hSocket == INVALID_SOCKET) {
                break;
            }

            nBytes = send(pnode->hSocket,
                          reinterpret_cast<const char *>(data.data()) +
                              pnode->nSendOffset,
                          data.size() - pnode->nSendOffset,
                          MSG_NOSIGNAL | MSG_DONTWAIT);
        }

        if (nBytes == 0) {
            // couldn't send anything at all
            break;
        }

        if (nBytes < 0) {
            // error
            int nErr = WSAGetLastError();
            if (nErr != WSAEWOULDBLOCK && nErr != WSAEMSGSIZE &&
                nErr != WSAEINTR && nErr != WSAEINPROGRESS) {
                LogPrintf("socket send error %s\n", NetworkErrorString(nErr));
                pnode->CloseSocketDisconnect();
            }

            break;
        }

        assert(nBytes > 0);
        pnode->nLastSend = GetSystemTimeInSeconds();
        pnode->nSendBytes += nBytes;
        pnode->nSendOffset += nBytes;
        nSentSize += nBytes;
        if (pnode->nSendOffset != data.size()) {
            // could not send full message; stop sending more
            break;
        }

        pnode->nSendOffset = 0;
        pnode->nSendSize -= data.size();
        pnode->fPauseSend = pnode->nSendSize > nSendBufferMaxSize;
        nMsgCount++;
    }

    pnode->vSendMsg.erase(pnode->vSendMsg.begin(),
                          pnode->vSendMsg.begin() + nMsgCount);

    if (pnode->vSendMsg.empty()) {
        assert(pnode->nSendOffset == 0);
        assert(pnode->nSendSize == 0);
    }

    return nSentSize;
}

struct NodeEvictionCandidate {
    NodeId id;
    int64_t nTimeConnected;
    int64_t nMinPingUsecTime;
    int64_t nLastBlockTime;
    int64_t nLastTXTime;
    bool fRelevantServices;
    bool fRelayTxes;
    bool fBloomFilter;
    CAddress addr;
    uint64_t nKeyedNetGroup;
    bool prefer_evict;
};

static bool ReverseCompareNodeMinPingTime(const NodeEvictionCandidate &a,
                                          const NodeEvictionCandidate &b) {
    return a.nMinPingUsecTime > b.nMinPingUsecTime;
}

static bool ReverseCompareNodeTimeConnected(const NodeEvictionCandidate &a,
                                            const NodeEvictionCandidate &b) {
    return a.nTimeConnected > b.nTimeConnected;
}

static bool CompareNetGroupKeyed(const NodeEvictionCandidate &a,
                                 const NodeEvictionCandidate &b) {
    return a.nKeyedNetGroup < b.nKeyedNetGroup;
}

static bool CompareNodeBlockTime(const NodeEvictionCandidate &a,
                                 const NodeEvictionCandidate &b) {
    // There is a fall-through here because it is common for a node to have many
    // peers which have not yet relayed a block.
    if (a.nLastBlockTime != b.nLastBlockTime) {
        return a.nLastBlockTime < b.nLastBlockTime;
    }

    if (a.fRelevantServices != b.fRelevantServices) {
        return b.fRelevantServices;
    }

    return a.nTimeConnected > b.nTimeConnected;
}

static bool CompareNodeTXTime(const NodeEvictionCandidate &a,
                              const NodeEvictionCandidate &b) {
    // There is a fall-through here because it is common for a node to have more
    // than a few peers that have not yet relayed txn.
    if (a.nLastTXTime != b.nLastTXTime) {
        return a.nLastTXTime < b.nLastTXTime;
    }

    if (a.fRelayTxes != b.fRelayTxes) {
        return b.fRelayTxes;
    }

    if (a.fBloomFilter != b.fBloomFilter) {
        return a.fBloomFilter;
    }

    return a.nTimeConnected > b.nTimeConnected;
}

//! Sort an array by the specified comparator, then erase the last K elements.
template <typename T, typename Comparator>
static void EraseLastKElements(std::vector<T> &elements, Comparator comparator,
                               size_t k) {
    std::sort(elements.begin(), elements.end(), comparator);
    size_t eraseSize = std::min(k, elements.size());
    elements.erase(elements.end() - eraseSize, elements.end());
}

/**
 * Try to find a connection to evict when the node is full.
 * Extreme care must be taken to avoid opening the node to attacker triggered
 * network partitioning.
 * The strategy used here is to protect a small number of peers for each of
 * several distinct characteristics which are difficult to forge. In order to
 * partition a node the attacker must be simultaneously better at all of them
 * than honest peers.
 */
bool CConnman::AttemptToEvictConnection() {
    std::vector<NodeEvictionCandidate> vEvictionCandidates;
    {
        LOCK(cs_vNodes);

        for (const CNode *node : vNodes) {
            if (node->HasPermission(PF_NOBAN)) {
                continue;
            }
            if (!node->fInbound) {
                continue;
            }
            if (node->fDisconnect) {
                continue;
            }
            LOCK(node->cs_filter);
            NodeEvictionCandidate candidate = {
                node->GetId(),
                node->nTimeConnected,
                node->nMinPingUsecTime,
                node->nLastBlockTime,
                node->nLastTXTime,
                HasAllDesirableServiceFlags(node->nServices),
                node->fRelayTxes,
                node->pfilter != nullptr,
                node->addr,
                node->nKeyedNetGroup,
                node->m_prefer_evict};
            vEvictionCandidates.push_back(candidate);
        }
    }

    // Protect connections with certain characteristics

    // Deterministically select 4 peers to protect by netgroup.
    // An attacker cannot predict which netgroups will be protected
    EraseLastKElements(vEvictionCandidates, CompareNetGroupKeyed, 4);
    // Protect the 8 nodes with the lowest minimum ping time.
    // An attacker cannot manipulate this metric without physically moving nodes
    // closer to the target.
    EraseLastKElements(vEvictionCandidates, ReverseCompareNodeMinPingTime, 8);
    // Protect 4 nodes that most recently sent us transactions.
    // An attacker cannot manipulate this metric without performing useful work.
    EraseLastKElements(vEvictionCandidates, CompareNodeTXTime, 4);
    // Protect 4 nodes that most recently sent us blocks.
    // An attacker cannot manipulate this metric without performing useful work.
    EraseLastKElements(vEvictionCandidates, CompareNodeBlockTime, 4);
    // Protect the half of the remaining nodes which have been connected the
    // longest. This replicates the non-eviction implicit behavior, and
    // precludes attacks that start later.
    EraseLastKElements(vEvictionCandidates, ReverseCompareNodeTimeConnected,
                       vEvictionCandidates.size() / 2);

    if (vEvictionCandidates.empty()) {
        return false;
    }

    // If any remaining peers are preferred for eviction consider only them.
    // This happens after the other preferences since if a peer is really the
    // best by other criteria (esp relaying blocks)
    // then we probably don't want to evict it no matter what.
    if (std::any_of(
            vEvictionCandidates.begin(), vEvictionCandidates.end(),
            [](NodeEvictionCandidate const &n) { return n.prefer_evict; })) {
        vEvictionCandidates.erase(
            std::remove_if(
                vEvictionCandidates.begin(), vEvictionCandidates.end(),
                [](NodeEvictionCandidate const &n) { return !n.prefer_evict; }),
            vEvictionCandidates.end());
    }

    // Identify the network group with the most connections and youngest member.
    // (vEvictionCandidates is already sorted by reverse connect time)
    uint64_t naMostConnections;
    unsigned int nMostConnections = 0;
    int64_t nMostConnectionsTime = 0;
    std::map<uint64_t, std::vector<NodeEvictionCandidate>> mapNetGroupNodes;
    for (const NodeEvictionCandidate &node : vEvictionCandidates) {
        std::vector<NodeEvictionCandidate> &group =
            mapNetGroupNodes[node.nKeyedNetGroup];
        group.push_back(node);
        int64_t grouptime = group[0].nTimeConnected;
        size_t group_size = group.size();
        if (group_size > nMostConnections ||
            (group_size == nMostConnections &&
             grouptime > nMostConnectionsTime)) {
            nMostConnections = group_size;
            nMostConnectionsTime = grouptime;
            naMostConnections = node.nKeyedNetGroup;
        }
    }

    // Reduce to the network group with the most connections
    vEvictionCandidates = std::move(mapNetGroupNodes[naMostConnections]);

    // Disconnect from the network group with the most connections
    NodeId evicted = vEvictionCandidates.front().id;
    LOCK(cs_vNodes);
    for (CNode *pnode : vNodes) {
        if (pnode->GetId() == evicted) {
            pnode->fDisconnect = true;
            return true;
        }
    }
    return false;
}

void CConnman::AcceptConnection(const ListenSocket &hListenSocket) {
    struct sockaddr_storage sockaddr;
    socklen_t len = sizeof(sockaddr);
    SOCKET hSocket =
        accept(hListenSocket.socket, (struct sockaddr *)&sockaddr, &len);
    CAddress addr;
    int nInbound = 0;
    int nMaxInbound = nMaxConnections - (nMaxOutbound + nMaxFeeler);

    if (hSocket != INVALID_SOCKET) {
        if (!addr.SetSockAddr(sockaddr)) {
            LogPrintf("Warning: Unknown socket family\n");
        }
    }

    NetPermissionFlags permissionFlags = NetPermissionFlags::PF_NONE;
    hListenSocket.AddSocketPermissionFlags(permissionFlags);
    AddWhitelistPermissionFlags(permissionFlags, addr);
    bool legacyWhitelisted = false;
    if (NetPermissions::HasFlag(permissionFlags,
                                NetPermissionFlags::PF_ISIMPLICIT)) {
        NetPermissions::ClearFlag(permissionFlags, PF_ISIMPLICIT);
        if (gArgs.GetBoolArg("-whitelistforcerelay", DEFAULT_WHITELISTFORCERELAY)) {
            NetPermissions::AddFlag(permissionFlags, PF_FORCERELAY);
        }
        if (gArgs.GetBoolArg("-whitelistrelay", DEFAULT_WHITELISTRELAY)) {
            NetPermissions::AddFlag(permissionFlags, PF_RELAY);
        }
        NetPermissions::AddFlag(permissionFlags, PF_MEMPOOL);
        NetPermissions::AddFlag(permissionFlags, PF_NOBAN);
        legacyWhitelisted = true;
    }

    {
        LOCK(cs_vNodes);
        for (const CNode *pnode : vNodes) {
            if (pnode->fInbound) {
                nInbound++;
            }
        }
    }

    if (hSocket == INVALID_SOCKET) {
        int nErr = WSAGetLastError();
        if (nErr != WSAEWOULDBLOCK) {
            LogPrintf("socket error accept failed: %s\n",
                      NetworkErrorString(nErr));
        }
        return;
    }

    if (!fNetworkActive) {
        LogPrintf("connection from %s dropped: not accepting new connections\n",
                  addr.ToString());
        CloseSocket(hSocket);
        return;
    }

    if (!IsSelectableSocket(hSocket)) {
        LogPrintf("connection from %s dropped: non-selectable socket\n",
                  addr.ToString());
        CloseSocket(hSocket);
        return;
    }

    // According to the internet TCP_NODELAY is not carried into accepted
    // sockets on all platforms.  Set it again here just to be sure.
    SetSocketNoDelay(hSocket);

    // Don't accept connections from banned peers.
    const bool banned = m_banman && m_banman->IsBanned(addr);
    if (banned && !NetPermissions::HasFlag(permissionFlags, NetPermissionFlags::PF_NOBAN)) {
        LogPrint(BCLog::NET, "connection from %s dropped (banned)\n", addr.ToString());
        CloseSocket(hSocket);
        return;
    }

    // Only accept connections from discouraged peers if our inbound slots aren't (almost) full.
    const bool discouraged = m_banman && m_banman->IsDiscouraged(addr);
    if (discouraged && !NetPermissions::HasFlag(permissionFlags, NetPermissionFlags::PF_NOBAN)
            && nInbound + 1 >= nMaxInbound) {
        LogPrint(BCLog::NET, "connection from %s dropped (discouraged)\n", addr.ToString());
        CloseSocket(hSocket);
        return;
    }

    if (nInbound >= nMaxInbound) {
        if (!AttemptToEvictConnection()) {
            // No connection to evict, disconnect the new connection
            LogPrint(BCLog::NET, "failed to find an eviction candidate - "
                                 "connection dropped (full)\n");
            CloseSocket(hSocket);
            return;
        }
    }

    NodeId id = GetNewNodeId();
    uint64_t nonce = GetDeterministicRandomizer(RANDOMIZER_ID_LOCALHOSTNONCE)
                         .Write(id)
                         .Finalize();
    CAddress addr_bind = GetBindAddress(hSocket);

    ServiceFlags nodeServices = nLocalServices;
    if (NetPermissions::HasFlag(permissionFlags, PF_BLOOMFILTER)) {
        nodeServices = static_cast<ServiceFlags>(nodeServices | NODE_BLOOM);
    }
    CNode *pnode =
        new CNode(id, nodeServices, GetBestHeight(), hSocket, addr,
                  CalculateKeyedNetGroup(addr), nonce, addr_bind, "", true);
    pnode->AddRef();
    pnode->m_permissionFlags = permissionFlags;
    // If this flag is present, the user probably expect that RPC and QT report
    // it as whitelisted (backward compatibility)
    pnode->m_legacyWhitelisted = legacyWhitelisted;
    pnode->m_prefer_evict = banned || discouraged;
    m_msgproc->InitializeNode(*config, pnode);

    LogPrint(BCLog::NET, "connection from %s accepted\n", addr.ToString());

    {
        LOCK(cs_vNodes);
        vNodes.push_back(pnode);
    }
}

void CConnman::DisconnectNodes() {
    {
        LOCK(cs_vNodes);

        if (!fNetworkActive) {
            // Disconnect any connected nodes
            for (CNode *pnode : vNodes) {
                if (!pnode->fDisconnect) {
                    LogPrint(BCLog::NET,
                             "Network not active, dropping peer=%d\n",
                             pnode->GetId());
                    pnode->fDisconnect = true;
                }
            }
        }

        // Disconnect unused nodes
        std::vector<CNode *> vNodesCopy = vNodes;
        for (CNode *pnode : vNodesCopy) {
            if (pnode->fDisconnect) {
                // remove from vNodes
                vNodes.erase(remove(vNodes.begin(), vNodes.end(), pnode),
                             vNodes.end());

                // release outbound grant (if any)
                pnode->grantOutbound.Release();

                // close socket and cleanup
                pnode->CloseSocketDisconnect();

                // hold in disconnected pool until all refs are released
                pnode->Release();
                vNodesDisconnected.push_back(pnode);
            }
        }
    }
    {
        // Delete disconnected nodes
        std::list<CNode *> vNodesDisconnectedCopy = vNodesDisconnected;
        for (CNode *pnode : vNodesDisconnectedCopy) {
            // wait until threads are done using it
            if (pnode->GetRefCount() <= 0) {
                bool fDelete = false;
                {
                    TRY_LOCK(pnode->cs_inventory, lockInv);
                    if (lockInv) {
                        TRY_LOCK(pnode->cs_vSend, lockSend);
                        if (lockSend) {
                            fDelete = true;
                        }
                    }
                }
                if (fDelete) {
                    vNodesDisconnected.remove(pnode);
                    DeleteNode(pnode);
                }
            }
        }
    }
}

void CConnman::NotifyNumConnectionsChanged() {
    size_t vNodesSize;
    {
        LOCK(cs_vNodes);
        vNodesSize = vNodes.size();
    }
    if (vNodesSize != nPrevNodeCount) {
        nPrevNodeCount = vNodesSize;
        if (clientInterface) {
            clientInterface->NotifyNumConnectionsChanged(vNodesSize);
        }
    }
}

void CConnman::InactivityCheck(CNode *pnode) {
    int64_t nTime = GetSystemTimeInSeconds();
    if (nTime - pnode->nTimeConnected > m_peer_connect_timeout) {
        if (pnode->nLastRecv == 0 || pnode->nLastSend == 0) {
            LogPrint(BCLog::NET,
                     "socket no message in first %i seconds, %d %d from %d\n",
                     m_peer_connect_timeout, pnode->nLastRecv != 0,
                     pnode->nLastSend != 0, pnode->GetId());
            pnode->fDisconnect = true;
        } else if (nTime - pnode->nLastSend > TIMEOUT_INTERVAL) {
            LogPrintf("socket sending timeout: %is\n",
                      nTime - pnode->nLastSend);
            pnode->fDisconnect = true;
        } else if (nTime - pnode->nLastRecv > (pnode->nVersion > BIP0031_VERSION
                                                   ? TIMEOUT_INTERVAL
                                                   : 90 * 60)) {
            LogPrintf("socket receive timeout: %is\n",
                      nTime - pnode->nLastRecv);
            pnode->fDisconnect = true;
        } else if (pnode->nPingNonceSent &&
                   pnode->nPingUsecStart + TIMEOUT_INTERVAL * 1000000 <
                       GetTimeMicros()) {
            LogPrintf("ping timeout: %fs\n",
                      0.000001 * (GetTimeMicros() - pnode->nPingUsecStart));
            pnode->fDisconnect = true;
        } else if (!pnode->fSuccessfullyConnected) {
            LogPrint(BCLog::NET, "version handshake timeout from %d\n",
                     pnode->GetId());
            pnode->fDisconnect = true;
        }
    }
}

bool CConnman::GenerateSelectSet(std::set<SOCKET> &recv_set, std::set<SOCKET> &send_set, std::set<SOCKET> &error_set) {
    for (const ListenSocket &hListenSocket : vhListenSocket) {
        recv_set.insert(hListenSocket.socket);
    }

    {
        LOCK(cs_vNodes);
        for (CNode *pnode : vNodes) {
            // Implement the following logic:
            // * If there is data to send, select() for sending data. As this
            //   only happens when optimistic write failed, we choose to first
            //   drain the write buffer in this case before receiving more. This
            //   avoids needlessly queueing received data, if the remote peer is
            //   not themselves receiving data. This means properly utilizing
            //   TCP flow control signalling.
            // * Otherwise, if there is space left in the receive buffer,
            //   select() for receiving data.
            // * Hand off all complete messages to the processor, to be handled
            //   without blocking here.

            bool select_recv = !pnode->fPauseRecv;
            bool select_send;
            {
                LOCK(pnode->cs_vSend);
                select_send = !pnode->vSendMsg.empty();
            }

            LOCK(pnode->cs_hSocket);
            if (pnode->hSocket == INVALID_SOCKET) {
                continue;
            }

            error_set.insert(pnode->hSocket);
            if (select_send) {
                send_set.insert(pnode->hSocket);
                continue;
            }
            if (select_recv) {
                recv_set.insert(pnode->hSocket);
            }
        }
    }

    return !recv_set.empty() || !send_set.empty() || !error_set.empty();
}

#ifdef USE_POLL
void CConnman::SocketEvents(std::set<SOCKET> &recv_set, std::set<SOCKET> &send_set, std::set<SOCKET> &error_set) {
    std::set<SOCKET> recv_select_set, send_select_set, error_select_set;
    if (!GenerateSelectSet(recv_select_set, send_select_set, error_select_set)) {
        interruptNet.sleep_for(std::chrono::milliseconds(SELECT_TIMEOUT_MILLISECONDS));
        return;
    }

    std::unordered_map<SOCKET, struct pollfd> pollfds;
    for (SOCKET socket_id : recv_select_set) {
        pollfds[socket_id].fd = socket_id;
        pollfds[socket_id].events |= POLLIN;
    }

    for (SOCKET socket_id : send_select_set) {
        pollfds[socket_id].fd = socket_id;
        pollfds[socket_id].events |= POLLOUT;
    }

    for (SOCKET socket_id : error_select_set) {
        pollfds[socket_id].fd = socket_id;
        // These flags are ignored, but we set them for clarity
        pollfds[socket_id].events |= POLLERR | POLLHUP;
    }

    std::vector<struct pollfd> vpollfds;
    vpollfds.reserve(pollfds.size());
    for (auto it : pollfds) {
        vpollfds.push_back(std::move(it.second));
    }

    if (poll(vpollfds.data(), vpollfds.size(), SELECT_TIMEOUT_MILLISECONDS) <= 0) return;

    if (interruptNet) return;

    for (struct pollfd pollfd_entry : vpollfds) {
        if (pollfd_entry.revents & POLLIN) recv_set.insert(pollfd_entry.fd);
        if (pollfd_entry.revents & POLLOUT) send_set.insert(pollfd_entry.fd);
        if (pollfd_entry.revents & (POLLERR | POLLHUP)) error_set.insert(pollfd_entry.fd);
    }
}
#else
void CConnman::SocketEvents(std::set<SOCKET> &recv_set, std::set<SOCKET> &send_set, std::set<SOCKET> &error_set) {
    std::set<SOCKET> recv_select_set, send_select_set, error_select_set;
    if (!GenerateSelectSet(recv_select_set, send_select_set, error_select_set)) {
        interruptNet.sleep_for(std::chrono::milliseconds(SELECT_TIMEOUT_MILLISECONDS));
        return;
    }

    //
    // Find which sockets have data to receive
    //

    // frequency to poll pnode->vSend
    struct timeval timeout = MillisToTimeval(SELECT_TIMEOUT_MILLISECONDS);

    fd_set fdsetRecv;
    fd_set fdsetSend;
    fd_set fdsetError;
    FD_ZERO(&fdsetRecv);
    FD_ZERO(&fdsetSend);
    FD_ZERO(&fdsetError);
    SOCKET hSocketMax = 0;

    for (SOCKET hSocket : recv_select_set) {
        FD_SET(hSocket, &fdsetRecv);
        hSocketMax = std::max(hSocketMax, hSocket);
    }

    for (SOCKET hSocket : send_select_set) {
        FD_SET(hSocket, &fdsetSend);
        hSocketMax = std::max(hSocketMax, hSocket);
    }

    for (SOCKET hSocket : error_select_set) {
        FD_SET(hSocket, &fdsetError);
        hSocketMax = std::max(hSocketMax, hSocket);
    }

    int nSelect = select(hSocketMax + 1, &fdsetRecv, &fdsetSend, &fdsetError, &timeout);

    if (interruptNet || nSelect == 0) {
        return;
    }

    if (nSelect == SOCKET_ERROR) {
        int nErr = WSAGetLastError();
        LogPrintf("socket select error %s\n", NetworkErrorString(nErr));
        for (unsigned int i = 0; i <= hSocketMax; i++) {
            FD_SET(i, &fdsetRecv);
        }
        FD_ZERO(&fdsetSend);
        FD_ZERO(&fdsetError);
        if (!interruptNet.sleep_for(std::chrono::milliseconds(SELECT_TIMEOUT_MILLISECONDS))) {
            return;
        }
    }

    for (SOCKET hSocket : recv_select_set) {
        if (FD_ISSET(hSocket, &fdsetRecv)) {
            recv_set.insert(hSocket);
        }
    }

    for (SOCKET hSocket : send_select_set) {
        if (FD_ISSET(hSocket, &fdsetSend)) {
            send_set.insert(hSocket);
        }
    }

    for (SOCKET hSocket : error_select_set) {
        if (FD_ISSET(hSocket, &fdsetError)) {
            error_set.insert(hSocket);
        }
    }
}
#endif

void CConnman::SocketHandler()
{
    std::set<SOCKET> recv_set, send_set, error_set;
    SocketEvents(recv_set, send_set, error_set);

    if (interruptNet) return;

    //
    // Accept new connections
    //
    for (const ListenSocket &hListenSocket : vhListenSocket) {
        if (hListenSocket.socket != INVALID_SOCKET && recv_set.count(hListenSocket.socket) > 0) {
            AcceptConnection(hListenSocket);
        }
    }

    //
    // Service each socket
    //
    std::vector<CNode *> vNodesCopy;
    {
        LOCK(cs_vNodes);
        vNodesCopy = vNodes;
        for (CNode *pnode : vNodesCopy) {
            pnode->AddRef();
        }
    }
    for (CNode *pnode : vNodesCopy) {
        if (interruptNet) {
            return;
        }

        //
        // Receive
        //
        bool recvSet = false;
        bool sendSet = false;
        bool errorSet = false;
        {
            LOCK(pnode->cs_hSocket);
            if (pnode->hSocket == INVALID_SOCKET) {
                continue;
            }
            recvSet = recv_set.count(pnode->hSocket) > 0;
            sendSet = send_set.count(pnode->hSocket) > 0;
            errorSet = error_set.count(pnode->hSocket) > 0;
        }
        if (recvSet || errorSet) {
            // typical socket buffer is 8K-64K
            char pchBuf[0x10000];
            int32_t nBytes = 0;
            {
                LOCK(pnode->cs_hSocket);
                if (pnode->hSocket == INVALID_SOCKET) {
                    continue;
                }
                nBytes =
                    recv(pnode->hSocket, pchBuf, sizeof(pchBuf), MSG_DONTWAIT);
            }
            if (nBytes > 0) {
                bool notify = false;
                if (!pnode->ReceiveMsgBytes(*config, pchBuf, nBytes, notify)) {
                    pnode->CloseSocketDisconnect();
                }
                RecordBytesRecv(nBytes);
                if (notify) {
                    size_t nSizeAdded = 0;
                    auto it(pnode->vRecvMsg.begin());
                    for (; it != pnode->vRecvMsg.end(); ++it) {
                        if (!it->complete()) {
                            break;
                        }
                        nSizeAdded +=
                            it->vRecv.size() + CMessageHeader::HEADER_SIZE;
                    }
                    {
                        LOCK(pnode->cs_vProcessMsg);
                        pnode->vProcessMsg.splice(pnode->vProcessMsg.end(),
                                                  pnode->vRecvMsg,
                                                  pnode->vRecvMsg.begin(), it);
                        pnode->nProcessQueueSize += nSizeAdded;
                        pnode->fPauseRecv =
                            pnode->nProcessQueueSize > nReceiveFloodSize;
                    }
                    WakeMessageHandler();
                }
            } else if (nBytes == 0) {
                // socket closed gracefully
                if (!pnode->fDisconnect) {
                    LogPrint(BCLog::NET, "socket closed\n");
                }
                pnode->CloseSocketDisconnect();
            } else if (nBytes < 0) {
                // error
                int nErr = WSAGetLastError();
                if (nErr != WSAEWOULDBLOCK && nErr != WSAEMSGSIZE &&
                    nErr != WSAEINTR && nErr != WSAEINPROGRESS) {
                    if (!pnode->fDisconnect) {
                        LogPrintf("socket recv error %s\n",
                                  NetworkErrorString(nErr));
                    }
                    pnode->CloseSocketDisconnect();
                }
            }
        }

        //
        // Send
        //
        if (sendSet) {
            LOCK(pnode->cs_vSend);
            size_t nBytes = SocketSendData(pnode);
            if (nBytes) {
                RecordBytesSent(nBytes);
            }
        }

        InactivityCheck(pnode);
    }
    {
        LOCK(cs_vNodes);
        for (CNode *pnode : vNodesCopy) {
            pnode->Release();
        }
    }
}

void CConnman::ThreadSocketHandler() {
    while (!interruptNet) {
        DisconnectNodes();
        NotifyNumConnectionsChanged();
        SocketHandler();
    }
}

void CConnman::WakeMessageHandler() {
    {
        std::lock_guard<std::mutex> lock(mutexMsgProc);
        fMsgProcWake = true;
    }
    condMsgProc.notify_one();
}

#ifdef USE_UPNP
static CThreadInterrupt g_upnp_interrupt;
static std::thread g_upnp_thread;
static void ThreadMapPort() {
    std::string port = strprintf("%u", GetListenPort());
    const char *multicastif = nullptr;
    const char *minissdpdpath = nullptr;
    struct UPNPDev *devlist = nullptr;
    char lanaddr[64];

#ifndef UPNPDISCOVER_SUCCESS
    /* miniupnpc 1.5 */
    devlist = upnpDiscover(2000, multicastif, minissdpdpath, 0);
#elif MINIUPNPC_API_VERSION < 14
    /* miniupnpc 1.6 */
    int error = 0;
    devlist = upnpDiscover(2000, multicastif, minissdpdpath, 0, 0, &error);
#else
    /* miniupnpc 1.9.20150730 */
    int error = 0;
    devlist = upnpDiscover(2000, multicastif, minissdpdpath, 0, 0, 2, &error);
#endif

    struct UPNPUrls urls;
    struct IGDdatas data;
    int r;

    r = UPNP_GetValidIGD(devlist, &urls, &data, lanaddr, sizeof(lanaddr));
    if (r == 1) {
        if (fDiscover) {
            char externalIPAddress[40];
            r = UPNP_GetExternalIPAddress(
                urls.controlURL, data.first.servicetype, externalIPAddress);
            if (r != UPNPCOMMAND_SUCCESS) {
                LogPrintf("UPnP: GetExternalIPAddress() returned %d\n", r);
            } else {
                if (externalIPAddress[0]) {
                    CNetAddr resolved;
                    if (LookupHost(externalIPAddress, resolved, false)) {
                        LogPrintf("UPnP: ExternalIPAddress = %s\n",
                                  resolved.ToString().c_str());
                        AddLocal(resolved, LOCAL_UPNP);
                    }
                } else {
                    LogPrintf("UPnP: GetExternalIPAddress failed.\n");
                }
            }
        }

        std::string strDesc = "Fittexxcoin " + FormatFullVersion();

        do {
#ifndef UPNPDISCOVER_SUCCESS
            /* miniupnpc 1.5 */
            r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype,
                                    port.c_str(), port.c_str(), lanaddr,
                                    strDesc.c_str(), "TCP", 0);
#else
            /* miniupnpc 1.6 */
            r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype,
                                    port.c_str(), port.c_str(), lanaddr,
                                    strDesc.c_str(), "TCP", 0, "0");
#endif

            if (r != UPNPCOMMAND_SUCCESS) {
                LogPrintf(
                    "AddPortMapping(%s, %s, %s) failed with code %d (%s)\n",
                    port, port, lanaddr, r, strupnperror(r));
            } else {
                LogPrintf("UPnP Port Mapping successful.\n");
            }
        } while (g_upnp_interrupt.sleep_for(std::chrono::minutes(20)));

        r = UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype,
                                   port.c_str(), "TCP", 0);
        LogPrintf("UPNP_DeletePortMapping() returned: %d\n", r);
        freeUPNPDevlist(devlist);
        devlist = nullptr;
        FreeUPNPUrls(&urls);
    } else {
        LogPrintf("No valid UPnP IGDs found\n");
        freeUPNPDevlist(devlist);
        devlist = nullptr;
        if (r != 0) {
            FreeUPNPUrls(&urls);
        }
    }
}

void StartMapPort() {
    if (!g_upnp_thread.joinable()) {
        assert(!g_upnp_interrupt);
        g_upnp_thread = std::thread(
            (std::bind(&TraceThread<void (*)()>, "upnp", &ThreadMapPort)));
    }
}

void InterruptMapPort() {
    if (g_upnp_thread.joinable()) {
        g_upnp_interrupt();
    }
}

void StopMapPort() {
    if (g_upnp_thread.joinable()) {
        g_upnp_thread.join();
        g_upnp_interrupt.reset();
    }
}

#else
void StartMapPort() {
    // Intentionally left blank.
}
void InterruptMapPort() {
    // Intentionally left blank.
}
void StopMapPort() {
    // Intentionally left blank.
}
#endif

void CConnman::ThreadDNSAddressSeed() {
    // goal: only query DNS seeds if address need is acute.
    // Avoiding DNS seeds when we don't need them improves user privacy by
    // creating fewer identifying DNS requests, reduces trust by giving seeds
    // less influence on the network topology, and reduces traffic to the seeds.
    if ((addrman.size() > 0) &&
        (!gArgs.GetBoolArg("-forcednsseed", DEFAULT_FORCEDNSSEED))) {
        if (!interruptNet.sleep_for(std::chrono::seconds(11))) {
            return;
        }

        LOCK(cs_vNodes);
        int nRelevant = 0;
        for (const CNode *pnode : vNodes) {
            nRelevant += pnode->fSuccessfullyConnected && !pnode->fFeeler &&
                         !pnode->fOneShot && !pnode->m_manual_connection &&
                         !pnode->fInbound;
        }
        if (nRelevant >= 2) {
            LogPrintf("P2P peers available. Skipped DNS seeding.\n");
            return;
        }
    }

    const std::vector<std::string> &vSeeds =
        config->GetChainParams().DNSSeeds();
    int found = 0;

    LogPrintf("Loading addresses from DNS seeds (could take a while)\n");

    for (const std::string &seed : vSeeds) {
        if (interruptNet) {
            return;
        }
        if (HaveNameProxy()) {
            AddOneShot(seed);
        } else {
            std::vector<CNetAddr> vIPs;
            std::vector<CAddress> vAdd;
            ServiceFlags requiredServiceBits =
                GetDesirableServiceFlags(NODE_NONE);
            std::string host = strprintf("x%x.%s", requiredServiceBits, seed);
            CNetAddr resolveSource;
            if (!resolveSource.SetInternal(host)) {
                continue;
            }

            // Limits number of IPs learned from a DNS seed
            unsigned int nMaxIPs = 256;
            if (LookupHost(host, vIPs, nMaxIPs, true)) {
                for (const CNetAddr &ip : vIPs) {
                    int nOneDay = 24 * 3600;
                    CAddress addr = CAddress(
                        CService(ip, config->GetChainParams().GetDefaultPort()),
                        requiredServiceBits);
                    // Use a random age between 3 and 7 days old.
                    addr.nTime = GetTime() - 3 * nOneDay - GetRand(4 * nOneDay);
                    vAdd.push_back(addr);
                    found++;
                }
                addrman.Add(vAdd, resolveSource);
            } else {
                // We now avoid directly using results from DNS Seeds which do
                // not support service bit filtering, instead using them as a
                // oneshot to get nodes with our desired service bits.
                AddOneShot(seed);
            }
        }
    }

    LogPrintf("%d addresses found from DNS seeds\n", found);
}

void CConnman::DumpAddresses() {
    int64_t nStart = GetTimeMillis();

    CAddrDB adb(config->GetChainParams());
    adb.Write(addrman);

    LogPrint(BCLog::NET, "Flushed %d addresses to peers.dat  %dms\n",
             addrman.size(), GetTimeMillis() - nStart);
}

void CConnman::ProcessOneShot() {
    std::string strDest;
    {
        LOCK(cs_vOneShots);
        if (vOneShots.empty()) {
            return;
        }
        strDest = vOneShots.front();
        vOneShots.pop_front();
    }
    CAddress addr;
    CSemaphoreGrant grant(*semOutbound, true);
    if (grant) {
        OpenNetworkConnection(addr, false, &grant, strDest.c_str(), true);
    }
}

bool CConnman::GetTryNewOutboundPeer() {
    return m_try_another_outbound_peer;
}

void CConnman::SetTryNewOutboundPeer(bool flag) {
    m_try_another_outbound_peer = flag;
    LogPrint(BCLog::NET, "net: setting try another outbound peer=%s\n",
             flag ? "true" : "false");
}

// Return the number of peers we have over our outbound connection limit.
// Exclude peers that are marked for disconnect, or are going to be disconnected
// soon (eg one-shots and feelers).
// Also exclude peers that haven't finished initial connection handshake yet (so
// that we don't decide we're over our desired connection limit, and then evict
// some peer that has finished the handshake).
int CConnman::GetExtraOutboundCount() {
    int nOutbound = 0;
    {
        LOCK(cs_vNodes);
        for (const CNode *pnode : vNodes) {
            if (!pnode->fInbound && !pnode->m_manual_connection &&
                !pnode->fFeeler && !pnode->fDisconnect && !pnode->fOneShot &&
                pnode->fSuccessfullyConnected) {
                ++nOutbound;
            }
        }
    }
    return std::max(nOutbound - nMaxOutbound, 0);
}

void CConnman::ThreadOpenConnections(const std::vector<std::string> connect) {
    // Connect to specific addresses
    if (!connect.empty()) {
        for (int64_t nLoop = 0;; nLoop++) {
            ProcessOneShot();
            for (const std::string &strAddr : connect) {
                CAddress addr(CService(), NODE_NONE);
                OpenNetworkConnection(addr, false, nullptr, strAddr.c_str(),
                                      false, false, true);
                for (int i = 0; i < 10 && i < nLoop; i++) {
                    if (!interruptNet.sleep_for(
                            std::chrono::milliseconds(500))) {
                        return;
                    }
                }
            }
            if (!interruptNet.sleep_for(std::chrono::milliseconds(500))) {
                return;
            }
        }
    }

    // Initiate network connections
    int64_t nStart = GetTime();

    // Minimum time before next feeler connection (in microseconds).
    int64_t nNextFeeler =
        PoissonNextSend(nStart * 1000 * 1000, FEELER_INTERVAL);
    while (!interruptNet) {
        ProcessOneShot();

        if (!interruptNet.sleep_for(std::chrono::milliseconds(500))) {
            return;
        }

        CSemaphoreGrant grant(*semOutbound);
        if (interruptNet) {
            return;
        }

        // Add seed nodes if DNS seeds are all down (an infrastructure attack?).
        if (addrman.size() == 0 && (GetTime() - nStart > 60)) {
            static bool done = false;
            if (!done) {
                LogPrintf("Adding fixed seed nodes as DNS doesn't seem to be "
                          "available.\n");
                CNetAddr local;
                local.SetInternal("fixedseeds");
                addrman.Add(convertSeed6(config->GetChainParams().FixedSeeds()),
                            local);
                done = true;
            }
        }

        //
        // Choose an address to connect to based on most recently seen
        //
        CAddress addrConnect;

        // Only connect out to one peer per network group (/16 for IPv4).
        int nOutbound = 0;
        std::set<std::vector<uint8_t>> setConnected;
        {
            LOCK(cs_vNodes);
            for (const CNode *pnode : vNodes) {
                if (!pnode->fInbound && !pnode->m_manual_connection) {
                    // Netgroups for inbound and addnode peers are not excluded
                    // because our goal here is to not use multiple of our
                    // limited outbound slots on a single netgroup but inbound
                    // and addnode peers do not use our outbound slots. Inbound
                    // peers also have the added issue that they're attacker
                    // controlled and could be used to prevent us from
                    // connecting to particular hosts if we used them here.
                    setConnected.insert(pnode->addr.GetGroup(addrman.m_asmap));
                    nOutbound++;
                }
            }
        }

        // Feeler Connections
        //
        // Design goals:
        //  * Increase the number of connectable addresses in the tried table.
        //
        // Method:
        //  * Choose a random address from new and attempt to connect to it if
        //    we can connect successfully it is added to tried.
        //  * Start attempting feeler connections only after node finishes
        //    making outbound connections.
        //  * Only make a feeler connection once every few minutes.
        //
        bool fFeeler = false;

        if (nOutbound >= nMaxOutbound && !GetTryNewOutboundPeer()) {
            // The current time right now (in microseconds).
            int64_t nTime = GetTimeMicros();
            if (nTime > nNextFeeler) {
                nNextFeeler = PoissonNextSend(nTime, FEELER_INTERVAL);
                fFeeler = true;
            } else {
                continue;
            }
        }

        addrman.ResolveCollisions();

        int64_t nANow = GetAdjustedTime();
        int nTries = 0;
        while (!interruptNet) {
            CAddrInfo addr = addrman.SelectTriedCollision();

            // SelectTriedCollision returns an invalid address if it is empty.
            if (!fFeeler || !addr.IsValid()) {
                addr = addrman.Select(fFeeler);
            }

            // Require outbound connections, other than feelers, to be to
            // distinct network groups
            if (!fFeeler && setConnected.count(addr.GetGroup(addrman.m_asmap))) {
                break;
            }

            // if we selected an invalid or local address, restart
            if (!addr.IsValid() || IsLocal(addr)) {
                break;
            }

            // If we didn't find an appropriate destination after trying 100
            // addresses fetched from addrman, stop this loop, and let the outer
            // loop run again (which sleeps, adds seed nodes, recalculates
            // already-connected network ranges, ...) before trying new addrman
            // addresses.
            nTries++;
            if (nTries > 100) {
                break;
            }

            if (!IsReachable(addr)) {
                continue;
            }

            // only consider very recently tried nodes after 30 failed attempts
            if (nANow - addr.nLastTry < 600 && nTries < 30) {
                continue;
            }

            // for non-feelers, require all the services we'll want,
            // for feelers, only require they be a full node (only because most
            // SPV clients don't have a good address DB available)
            if (!fFeeler && !HasAllDesirableServiceFlags(addr.nServices)) {
                continue;
            }

            if (fFeeler && !MayHaveUsefulAddressDB(addr.nServices)) {
                continue;
            }

            // do not allow non-default ports, unless after 50 invalid addresses
            // selected already.
            if (addr.GetPort() != config->GetChainParams().GetDefaultPort() &&
                nTries < 50) {
                continue;
            }

            addrConnect = addr;
            break;
        }

        if (addrConnect.IsValid()) {
            if (fFeeler) {
                // Add small amount of random noise before connection to avoid
                // synchronization.
                int randsleep = GetRandInt(FEELER_SLEEP_WINDOW * 1000);
                if (!interruptNet.sleep_for(
                        std::chrono::milliseconds(randsleep))) {
                    return;
                }
                LogPrint(BCLog::NET, "Making feeler connection to %s\n",
                         addrConnect.ToString());
            }

            OpenNetworkConnection(addrConnect,
                                  (int)setConnected.size() >=
                                      std::min(nMaxConnections - 1, 2),
                                  &grant, nullptr, false, fFeeler);
        }
    }
}

std::vector<AddedNodeInfo> CConnman::GetAddedNodeInfo() {
    std::vector<AddedNodeInfo> ret;

    std::list<std::string> lAddresses(0);
    {
        LOCK(cs_vAddedNodes);
        ret.reserve(vAddedNodes.size());
        std::copy(vAddedNodes.cbegin(), vAddedNodes.cend(),
                  std::back_inserter(lAddresses));
    }

    // Build a map of all already connected addresses (by IP:port and by name)
    // to inbound/outbound and resolved CService
    std::map<CService, bool> mapConnected;
    std::map<std::string, std::pair<bool, CService>> mapConnectedByName;
    {
        LOCK(cs_vNodes);
        for (const CNode *pnode : vNodes) {
            if (pnode->addr.IsValid()) {
                mapConnected[pnode->addr] = pnode->fInbound;
            }
            std::string addrName = pnode->GetAddrName();
            if (!addrName.empty()) {
                mapConnectedByName[std::move(addrName)] =
                    std::make_pair(pnode->fInbound,
                                   static_cast<const CService &>(pnode->addr));
            }
        }
    }

    for (const std::string &strAddNode : lAddresses) {
        CService service(LookupNumeric(strAddNode, Params().GetDefaultPort()));
        AddedNodeInfo addedNode{strAddNode, CService(), false, false};
        if (service.IsValid()) {
            // strAddNode is an IP:port
            auto it = mapConnected.find(service);
            if (it != mapConnected.end()) {
                addedNode.resolvedAddress = service;
                addedNode.fConnected = true;
                addedNode.fInbound = it->second;
            }
        } else {
            // strAddNode is a name
            auto it = mapConnectedByName.find(strAddNode);
            if (it != mapConnectedByName.end()) {
                addedNode.resolvedAddress = it->second.second;
                addedNode.fConnected = true;
                addedNode.fInbound = it->second.first;
            }
        }
        ret.emplace_back(std::move(addedNode));
    }

    return ret;
}

void CConnman::ThreadOpenAddedConnections() {
    while (true) {
        CSemaphoreGrant grant(*semAddnode);
        std::vector<AddedNodeInfo> vInfo = GetAddedNodeInfo();
        bool tried = false;
        for (const AddedNodeInfo &info : vInfo) {
            if (!info.fConnected) {
                if (!grant.TryAcquire()) {
                    // If we've used up our semaphore and need a new one, let's
                    // not wait here since while we are waiting the
                    // addednodeinfo state might change.
                    break;
                }
                tried = true;
                CAddress addr(CService(), NODE_NONE);
                OpenNetworkConnection(addr, false, &grant,
                                      info.strAddedNode.c_str(), false, false,
                                      true);
                if (!interruptNet.sleep_for(std::chrono::milliseconds(500))) {
                    return;
                }
            }
        }
        // Retry every 60 seconds if a connection was attempted, otherwise two
        // seconds.
        if (!interruptNet.sleep_for(std::chrono::seconds(tried ? 60 : 2))) {
            return;
        }
    }
}

// If successful, this moves the passed grant to the constructed node.
void CConnman::OpenNetworkConnection(const CAddress &addrConnect,
                                     bool fCountFailure,
                                     CSemaphoreGrant *grantOutbound,
                                     const char *pszDest, bool fOneShot,
                                     bool fFeeler, bool manual_connection) {
    //
    // Initiate outbound network connection
    //
    if (interruptNet) {
        return;
    }
    if (!fNetworkActive) {
        return;
    }
    if (!pszDest) {
        const bool banned_or_discouraged
                = m_banman && (m_banman->IsDiscouraged(addrConnect) || m_banman->IsBanned(addrConnect));
        if (banned_or_discouraged ||
            IsLocal(addrConnect) ||
            FindNode(static_cast<CNetAddr>(addrConnect)) ||
            FindNode(addrConnect.ToStringIPPort())) {
            return;
        }
    } else if (FindNode(std::string(pszDest))) {
        return;
    }

    CNode *pnode =
        ConnectNode(addrConnect, pszDest, fCountFailure, manual_connection);

    if (!pnode) {
        return;
    }
    if (grantOutbound) {
        grantOutbound->MoveTo(pnode->grantOutbound);
    }
    if (fOneShot) {
        pnode->fOneShot = true;
    }
    if (fFeeler) {
        pnode->fFeeler = true;
    }
    if (manual_connection) {
        pnode->m_manual_connection = true;
    }

    m_msgproc->InitializeNode(*config, pnode);
    {
        LOCK(cs_vNodes);
        vNodes.push_back(pnode);
    }
}

void CConnman::ThreadMessageHandler() {
    while (!flagInterruptMsgProc) {
        std::vector<CNode *> vNodesCopy;
        {
            LOCK(cs_vNodes);
            vNodesCopy = vNodes;
            for (CNode *pnode : vNodesCopy) {
                pnode->AddRef();
            }
        }

        bool fMoreWork = false;
        auto nSleepUntil = GetTime<std::chrono::microseconds>() + std::chrono::microseconds{100000};

        for (CNode *pnode : vNodesCopy) {
            if (pnode->fDisconnect) {
                continue;
            }

            // Receive messages
            bool fMoreNodeWork = m_msgproc->ProcessMessages(
                *config, pnode, flagInterruptMsgProc);
            fMoreWork |= (fMoreNodeWork && !pnode->fPauseSend);
            if (flagInterruptMsgProc) {
                return;
            }

            // Send messages
            {
                LOCK(pnode->cs_sendProcessing);
                m_msgproc->SendMessages(*config, pnode, flagInterruptMsgProc);
            }

            nSleepUntil = std::min(pnode->nNextInvSend, nSleepUntil);
            if (flagInterruptMsgProc) {
                return;
            }
        }

        {
            LOCK(cs_vNodes);
            for (CNode *pnode : vNodesCopy) {
                pnode->Release();
            }
        }

        WAIT_LOCK(mutexMsgProc, lock);
        if (!fMoreWork) {
            auto nSleepFor =
                std::max(std::chrono::microseconds{0}, std::min(std::chrono::microseconds{100000},
                                                                nSleepUntil - GetTime<std::chrono::microseconds>()));
            condMsgProc.wait_for(lock, nSleepFor, [this] { return fMsgProcWake; });
        }
        fMsgProcWake = false;
    }
}

bool CConnman::BindListenPort(const CService &addrBind, std::string &strError,
                              NetPermissionFlags permissions) {
    strError = "";
    int nOne = 1;

    // Create socket for listening for incoming connections
    const auto optPair = addrBind.GetSockAddr();
    if (!optPair) {
        strError = strprintf("Error: Bind address family for %s not supported",
                             addrBind.ToString());
        LogPrintf("%s\n", strError);
        return false;
    }
    auto & [sockaddr, len] = *optPair;

    SOCKET hListenSocket = CreateSocket(addrBind);
    if (hListenSocket == INVALID_SOCKET) {
        strError = strprintf("Error: Couldn't open socket for incoming "
                             "connections (socket returned error %s)",
                             NetworkErrorString(WSAGetLastError()));
        LogPrintf("%s\n", strError);
        return false;
    }

    // Allow binding if the port is still in TIME_WAIT state after
    // the program was closed and restarted.
    setsockopt(hListenSocket, SOL_SOCKET, SO_REUSEADDR, (sockopt_arg_type)&nOne,
               sizeof(int));

    // Some systems don't have IPV6_V6ONLY but are always v6only; others do have
    // the option and enable it by default or not. Try to enable it, if
    // possible.
    if (addrBind.IsIPv6()) {
#ifdef IPV6_V6ONLY
        setsockopt(hListenSocket, IPPROTO_IPV6, IPV6_V6ONLY,
                   (sockopt_arg_type)&nOne, sizeof(int));
#endif
#ifdef WIN32
        int nProtLevel = PROTECTION_LEVEL_UNRESTRICTED;
        setsockopt(hListenSocket, IPPROTO_IPV6, IPV6_PROTECTION_LEVEL,
                   (sockopt_arg_type)&nProtLevel, sizeof(int));
#endif
    }

    if (::bind(hListenSocket, (const struct sockaddr *)&sockaddr, len) ==
        SOCKET_ERROR) {
        int nErr = WSAGetLastError();
        if (nErr == WSAEADDRINUSE) {
            strError = strprintf(_("Unable to bind to %s on this computer. %s "
                                   "is probably already running."),
                                 addrBind.ToString(), PACKAGE_NAME);
        } else {
            strError = strprintf(_("Unable to bind to %s on this computer "
                                   "(bind returned error %s)"),
                                 addrBind.ToString(), NetworkErrorString(nErr));
        }
        LogPrintf("%s\n", strError);
        CloseSocket(hListenSocket);
        return false;
    }
    LogPrintf("Bound to %s\n", addrBind.ToString());

    // Listen for incoming connections
    if (listen(hListenSocket, SOMAXCONN) == SOCKET_ERROR) {
        strError = strprintf(_("Error: Listening for incoming connections "
                               "failed (listen returned error %s)"),
                             NetworkErrorString(WSAGetLastError()));
        LogPrintf("%s\n", strError);
        CloseSocket(hListenSocket);
        return false;
    }

    vhListenSocket.push_back(ListenSocket(hListenSocket, permissions));
    return true;
}

void Discover() {
    if (!fDiscover) {
        return;
    }

#ifdef WIN32
    // Get local host IP
    char pszHostName[256] = "";
    if (gethostname(pszHostName, sizeof(pszHostName)) != SOCKET_ERROR) {
        std::vector<CNetAddr> vaddr;
        if (LookupHost(pszHostName, vaddr, 0, true)) {
            for (const CNetAddr &addr : vaddr) {
                if (AddLocal(addr, LOCAL_IF)) {
                    LogPrintf("%s: %s - %s\n", __func__, pszHostName,
                              addr.ToString());
                }
            }
        }
    }
#elif (HAVE_DECL_GETIFADDRS && HAVE_DECL_FREEIFADDRS)
    // Get local host ip
    struct ifaddrs *myaddrs;
    if (getifaddrs(&myaddrs) == 0) {
        for (struct ifaddrs *ifa = myaddrs; ifa != nullptr;
             ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == nullptr || (ifa->ifa_flags & IFF_UP) == 0 ||
                strcmp(ifa->ifa_name, "lo") == 0 ||
                strcmp(ifa->ifa_name, "lo0") == 0) {
                continue;
            }
            if (ifa->ifa_addr->sa_family == AF_INET) {
                CNetAddr addr(bit_cast<sockaddr_in>(*ifa->ifa_addr).sin_addr);
                if (AddLocal(addr, LOCAL_IF)) {
                    LogPrintf("%s: IPv4 %s: %s\n", __func__, ifa->ifa_name,
                              addr.ToString());
                }
            } else if (ifa->ifa_addr->sa_family == AF_INET6) {
                CNetAddr addr(bit_cast_unsafe<sockaddr_in6>(*ifa->ifa_addr).sin6_addr);
                if (AddLocal(addr, LOCAL_IF)) {
                    LogPrintf("%s: IPv6 %s: %s\n", __func__, ifa->ifa_name,
                              addr.ToString());
                }
            }
        }
        freeifaddrs(myaddrs);
    }
#endif
}

void CConnman::SetNetworkActive(bool active) {
    LogPrint(BCLog::NET, "SetNetworkActive: %s\n", active);

    if (fNetworkActive == active) {
        return;
    }

    fNetworkActive = active;
    uiInterface.NotifyNetworkActiveChanged(fNetworkActive);
}

CConnman::CConnman(const Config &configIn, uint64_t nSeed0In, uint64_t nSeed1In)
    : config(&configIn), nSeed0(nSeed0In), nSeed1(nSeed1In), deleted(std::make_unique<std::atomic_bool>(false)) {
    SetTryNewOutboundPeer(false);

    Options connOptions;
    Init(connOptions);
}

NodeId CConnman::GetNewNodeId() {
    return nLastNodeId.fetch_add(1, std::memory_order_relaxed);
}

bool CConnman::Bind(const CService &addr, unsigned int flags,
                    NetPermissionFlags permissions) {
    if (!(flags & BF_EXPLICIT) && !IsReachable(addr)) {
        return false;
    }
    std::string strError;
    if (!BindListenPort(addr, strError, permissions)) {
        if ((flags & BF_REPORT_ERROR) && clientInterface) {
            clientInterface->ThreadSafeMessageBox(
                strError, "", CClientUIInterface::MSG_ERROR);
        }
        return false;
    }

    if (addr.IsRoutable() && fDiscover && !(flags & BF_DONT_ADVERTISE) && !(permissions & PF_NOBAN)) {
        AddLocal(addr, LOCAL_BIND);
    }

    return true;
}

bool CConnman::InitBinds(const std::vector<CService> &binds, const std::vector<NetWhitebindPermissions> &whiteBinds,
                         const std::vector<CService> &onion_binds) {
    bool fBound = false;
    for (const auto &addrBind : binds) {
        fBound |= Bind(addrBind, (BF_EXPLICIT | BF_REPORT_ERROR),
                       NetPermissionFlags::PF_NONE);
    }
    for (const auto &addrBind : whiteBinds) {
        fBound |= Bind(addrBind.m_service, (BF_EXPLICIT | BF_REPORT_ERROR),
                       addrBind.m_flags);
    }
    if (binds.empty() && whiteBinds.empty()) {
        struct in_addr inaddr_any;
        inaddr_any.s_addr = htonl(INADDR_ANY);
        struct in6_addr inaddr6_any = IN6ADDR_ANY_INIT;
        fBound |= Bind(CService(inaddr6_any, GetListenPort()), BF_NONE,
                       NetPermissionFlags::PF_NONE);
        fBound |= Bind(CService(inaddr_any, GetListenPort()),
                       !fBound ? BF_REPORT_ERROR : BF_NONE,
                       NetPermissionFlags::PF_NONE);
    }

    for (const auto& addr_bind : onion_binds) {
        fBound |= Bind(addr_bind, BF_EXPLICIT | BF_DONT_ADVERTISE, NetPermissionFlags::PF_NONE);
    }

    return fBound;
}

bool CConnman::Start(CScheduler &scheduler, const Options &connOptions) {
    Init(connOptions);

    {
        LOCK(cs_totalBytesRecv);
        nTotalBytesRecv = 0;
    }
    {
        LOCK(cs_totalBytesSent);
        nTotalBytesSent = 0;
        nMaxOutboundTotalBytesSentInCycle = 0;
        nMaxOutboundCycleStartTime = 0;
    }

    if (fListen && !InitBinds(connOptions.vBinds, connOptions.vWhiteBinds, connOptions.onion_binds)) {
        if (clientInterface) {
            clientInterface->ThreadSafeMessageBox(
                _("Failed to listen on any port. Use -listen=0 if you want "
                  "this."),
                "", CClientUIInterface::MSG_ERROR);
        }
        return false;
    }

    for (const auto &strDest : connOptions.vSeedNodes) {
        AddOneShot(strDest);
    }

    if (clientInterface) {
        clientInterface->InitMessage(_("Loading P2P addresses..."));
    }
    // Load addresses from peers.dat
    int64_t nStart = GetTimeMillis();
    {
        CAddrDB adb(config->GetChainParams());
        if (adb.Read(addrman)) {
            LogPrintf("Loaded %i addresses from peers.dat  %dms\n",
                      addrman.size(), GetTimeMillis() - nStart);
        } else {
            // Addrman can be in an inconsistent state after failure, reset it
            addrman.Clear();
            LogPrintf("Invalid or missing peers.dat; recreating\n");
            DumpAddresses();
        }
    }

    uiInterface.InitMessage(_("Starting network threads..."));

    fAddressesInitialized = true;

    if (semOutbound == nullptr) {
        // initialize semaphore
        semOutbound = std::make_unique<CSemaphore>(
            std::min((nMaxOutbound + nMaxFeeler), nMaxConnections));
    }
    if (semAddnode == nullptr) {
        // initialize semaphore
        semAddnode = std::make_unique<CSemaphore>(nMaxAddnode);
    }

    //
    // Start threads
    //
    assert(m_msgproc);
    InterruptSocks5(false);
    interruptNet.reset();
    flagInterruptMsgProc = false;

    {
        LOCK(mutexMsgProc);
        fMsgProcWake = false;
    }

    // Send and receive from sockets, accept connections
    threadSocketHandler = std::thread(
        &TraceThread<std::function<void()>>, "net",
        std::function<void()>(std::bind(&CConnman::ThreadSocketHandler, this)));

    if (!gArgs.GetBoolArg("-dnsseed", true)) {
        LogPrintf("DNS seeding disabled\n");
    } else {
        threadDNSAddressSeed =
            std::thread(&TraceThread<std::function<void()>>, "dnsseed",
                        std::function<void()>(
                            std::bind(&CConnman::ThreadDNSAddressSeed, this)));
    }

    // Initiate outbound connections from -addnode
    threadOpenAddedConnections =
        std::thread(&TraceThread<std::function<void()>>, "addcon",
                    std::function<void()>(std::bind(
                        &CConnman::ThreadOpenAddedConnections, this)));

    if (connOptions.m_use_addrman_outgoing &&
        !connOptions.m_specified_outgoing.empty()) {
        if (clientInterface) {
            clientInterface->ThreadSafeMessageBox(
                _("Cannot provide specific connections and have addrman find outgoing connections at the same time."),
                "", CClientUIInterface::MSG_ERROR);
        }
        return false;
    }
    if (connOptions.m_use_addrman_outgoing ||
        !connOptions.m_specified_outgoing.empty()) {
        threadOpenConnections =
            std::thread(&TraceThread<std::function<void()>>, "opencon",
                        std::function<void()>(
                            std::bind(&CConnman::ThreadOpenConnections, this,
                                      connOptions.m_specified_outgoing)));
    }

    // Process messages
    threadMessageHandler =
        std::thread(&TraceThread<std::function<void()>>, "msghand",
                    std::function<void()>(
                        std::bind(&CConnman::ThreadMessageHandler, this)));

    // Dump network addresses
    scheduler.scheduleEvery(
        [this, this_deleted = this->deleted]() {
            if (*this_deleted) return false; // guard against case where this instance may be deleted before scheduler
            this->DumpAddresses();
            return true;
        },
        DUMP_PEERS_INTERVAL * 1000);

    return true;
}

class CNetCleanup {
public:
    CNetCleanup() {}

    ~CNetCleanup() {
#ifdef WIN32
        // Shutdown Windows Sockets
        WSACleanup();
#endif
    }
} instance_of_cnetcleanup;

void CConnman::Interrupt() {
    {
        std::lock_guard<std::mutex> lock(mutexMsgProc);
        flagInterruptMsgProc = true;
    }
    condMsgProc.notify_all();

    interruptNet();
    InterruptSocks5(true);

    if (semOutbound) {
        for (int i = 0; i < (nMaxOutbound + nMaxFeeler); i++) {
            semOutbound->post();
        }
    }

    if (semAddnode) {
        for (int i = 0; i < nMaxAddnode; i++) {
            semAddnode->post();
        }
    }
}

void CConnman::Stop() {
    if (threadMessageHandler.joinable()) {
        threadMessageHandler.join();
    }
    if (threadOpenConnections.joinable()) {
        threadOpenConnections.join();
    }
    if (threadOpenAddedConnections.joinable()) {
        threadOpenAddedConnections.join();
    }
    if (threadDNSAddressSeed.joinable()) {
        threadDNSAddressSeed.join();
    }
    if (threadSocketHandler.joinable()) {
        threadSocketHandler.join();
    }

    if (fAddressesInitialized) {
        DumpAddresses();
        fAddressesInitialized = false;
    }

    // Close sockets
    for (CNode *pnode : vNodes) {
        pnode->CloseSocketDisconnect();
    }
    for (ListenSocket &hListenSocket : vhListenSocket) {
        if (hListenSocket.socket != INVALID_SOCKET) {
            if (!CloseSocket(hListenSocket.socket)) {
                LogPrintf("CloseSocket(hListenSocket) failed with error %s\n",
                          NetworkErrorString(WSAGetLastError()));
            }
        }
    }

    // clean up some globals (to help leak detection)
    for (CNode *pnode : vNodes) {
        DeleteNode(pnode);
    }
    for (CNode *pnode : vNodesDisconnected) {
        DeleteNode(pnode);
    }
    vNodes.clear();
    vNodesDisconnected.clear();
    vhListenSocket.clear();
    semOutbound.reset();
    semAddnode.reset();
}

void CConnman::DeleteNode(CNode *pnode) {
    assert(pnode);
    bool fUpdateConnectionTime = false;
    m_msgproc->FinalizeNode(*config, pnode->GetId(), fUpdateConnectionTime);
    if (fUpdateConnectionTime) {
        addrman.Connected(pnode->addr);
    }
    delete pnode;
}

CConnman::~CConnman() {
    Interrupt();
    Stop();
    *deleted = true; // guard againse use-after-free in case our periodic lambda is triggered again
}

void CConnman::SetServices(const CService &addr, ServiceFlags nServices) {
    addrman.SetServices(addr, nServices);
}

void CConnman::MarkAddressGood(const CAddress &addr) {
    addrman.Good(addr);
}

bool CConnman::AddNewAddresses(const std::vector<CAddress> &vAddr, const CAddress &addrFrom, int64_t nTimePenalty) {
    return addrman.Add(vAddr, addrFrom, nTimePenalty);
}

std::vector<CAddress> CConnman::GetAddresses(size_t max_addresses, size_t max_pct) {
    auto addresses = addrman.GetAddr(max_addresses, max_pct);
    if (m_banman) {
        auto toBeRemoved = [this](const CAddress &addr) {
            return m_banman->IsDiscouraged(addr) || m_banman->IsBanned(addr);
        };
        addresses.erase(std::remove_if(addresses.begin(), addresses.end(), toBeRemoved), addresses.end());
    }
    return addresses;
}

std::vector<CAddress> CConnman::GetAddressesUntrusted(CNode &requestor, size_t max_addresses, size_t max_pct) {
    SOCKET socket;
    WITH_LOCK(requestor.cs_hSocket, socket = requestor.hSocket);
    auto local_socket_bytes = GetBindAddress(socket).GetAddrBytes();
    uint64_t cache_id = GetDeterministicRandomizer(RANDOMIZER_ID_ADDRCACHE)
                            .Write(requestor.addr.GetNetwork())
                            .Write(local_socket_bytes.data(), local_socket_bytes.size())
                            .Finalize();
    const auto current_time = GetTime<std::chrono::microseconds>();

    LOCK(cs_addr_response_caches);

    // Either emplace a new default-constructed CachedAddrResponse (with m_update_addr_response == 0),
    // or lookup an existing one.
    CachedAddrResponse & cache_entry = m_addr_response_caches.try_emplace(cache_id).first->second;

    // If emplace() added new one it has expiration 0.
    if (cache_entry.m_cache_entry_expiration < current_time || cache_entry.m_addrs_response_cache.empty()) {
        cache_entry.m_addrs_response_cache = GetAddresses(max_addresses, max_pct);

        // Choosing a proper cache lifetime is a trade-off between the privacy leak minimization and the usefulness of
        // ADDR responses to honest users.
        //
        // Longer cache lifetime makes it more difficult for an attacker to scrape enough AddrMan data to maliciously
        // infer something useful. By the time an attacker scraped enough AddrMan records, most of  the records should
        // be old enough to not leak topology info by e.g. analyzing real-time changes in timestamps.
        //
        // It takes only several hundred requests to scrape everything from an AddrMan containing 100,000 nodes, so ~24
        // hours of cache lifetime indeed makes the data less inferable by the time most of it could be scraped
        // (considering that timestamps are updated via ADDR self-announcements and when nodes communicate). We also
        // should be robust to those attacks which may not require scraping *full* victim's AddrMan (because even
        // several timestamps of the same handful of nodes may leak privacy).
        //
        // On the other hand, longer cache lifetime makes ADDR responses outdated and less useful for an honest
        // requestor, e.g. if most nodes in the ADDR response are no longer active.
        //
        // However, the churn in the network is known to be rather low. Since we consider nodes to be "terrible" (see
        // IsTerrible()) if the timestamps are older than 30 days, max. 24 hours of "penalty" due to cache shouldn't
        // make any meaningful difference in terms of the freshness of the response.
        cache_entry.m_cache_entry_expiration =
            current_time + std::chrono::hours(21) + GetRandMillis(std::chrono::hours(6));
    }

    std::vector<CAddress> ret{cache_entry.m_addrs_response_cache};
    // Truncate results if they are larger than specified.
    if (max_addresses > 0 && ret.size() > max_addresses) ret.resize(max_addresses);
    return ret;
}

bool CConnman::AddNode(const std::string &strNode) {
    LOCK(cs_vAddedNodes);
    for (const std::string &it : vAddedNodes) {
        if (strNode == it) {
            return false;
        }
    }

    vAddedNodes.push_back(strNode);
    return true;
}

bool CConnman::RemoveAddedNode(const std::string &strNode) {
    LOCK(cs_vAddedNodes);
    for (std::vector<std::string>::iterator it = vAddedNodes.begin();
         it != vAddedNodes.end(); ++it) {
        if (strNode == *it) {
            vAddedNodes.erase(it);
            return true;
        }
    }
    return false;
}

size_t CConnman::GetNodeCount(NumConnections flags) {
    LOCK(cs_vNodes);
    // Shortcut if we want total
    if (flags == CConnman::CONNECTIONS_ALL) {
        return vNodes.size();
    }

    int nNum = 0;
    for (const auto &pnode : vNodes) {
        if (flags & (pnode->fInbound ? CONNECTIONS_IN : CONNECTIONS_OUT)) {
            nNum++;
        }
    }

    return nNum;
}

void CConnman::GetNodeStats(std::vector<CNodeStats> &vstats) {
    vstats.clear();
    LOCK(cs_vNodes);
    vstats.reserve(vNodes.size());
    for (CNode *pnode : vNodes) {
        vstats.emplace_back();
        pnode->copyStats(vstats.back(), addrman.m_asmap);
    }
}

bool CConnman::DisconnectNode(const std::string &strNode) {
    LOCK(cs_vNodes);
    if (CNode *pnode = FindNode(strNode)) {
        pnode->fDisconnect = true;
        return true;
    }
    return false;
}

bool CConnman::DisconnectNode(const CSubNet &subnet) {
    bool disconnected = false;
    LOCK(cs_vNodes);
    for (CNode *pnode : vNodes) {
        if (subnet.Match(pnode->addr)) {
            pnode->fDisconnect = true;
            disconnected = true;
        }
    }
    return disconnected;
}

bool CConnman::DisconnectNode(const CNetAddr &addr) {
    return DisconnectNode(CSubNet(addr));
}

bool CConnman::DisconnectNode(NodeId id) {
    LOCK(cs_vNodes);
    for (CNode *pnode : vNodes) {
        if (id == pnode->GetId()) {
            pnode->fDisconnect = true;
            return true;
        }
    }
    return false;
}

void CConnman::RecordBytesRecv(uint64_t bytes) {
    LOCK(cs_totalBytesRecv);
    nTotalBytesRecv += bytes;
}

void CConnman::RecordBytesSent(uint64_t bytes) {
    LOCK(cs_totalBytesSent);
    nTotalBytesSent += bytes;

    uint64_t now = GetTime();
    if (nMaxOutboundCycleStartTime + nMaxOutboundTimeframe < now) {
        // timeframe expired, reset cycle
        nMaxOutboundCycleStartTime = now;
        nMaxOutboundTotalBytesSentInCycle = 0;
    }

    // TODO, exclude whitebind peers
    nMaxOutboundTotalBytesSentInCycle += bytes;
}

void CConnman::SetMaxOutboundTarget(uint64_t limit) {
    LOCK(cs_totalBytesSent);
    nMaxOutboundLimit = limit;
}

uint64_t CConnman::GetMaxOutboundTarget() {
    LOCK(cs_totalBytesSent);
    return nMaxOutboundLimit;
}

uint64_t CConnman::GetMaxOutboundTimeframe() {
    LOCK(cs_totalBytesSent);
    return nMaxOutboundTimeframe;
}

uint64_t CConnman::GetMaxOutboundTimeLeftInCycle() {
    LOCK(cs_totalBytesSent);
    if (nMaxOutboundLimit == 0) {
        return 0;
    }

    if (nMaxOutboundCycleStartTime == 0) {
        return nMaxOutboundTimeframe;
    }

    uint64_t cycleEndTime = nMaxOutboundCycleStartTime + nMaxOutboundTimeframe;
    uint64_t now = GetTime();
    return (cycleEndTime < now) ? 0 : cycleEndTime - GetTime();
}

void CConnman::SetMaxOutboundTimeframe(uint64_t timeframe) {
    LOCK(cs_totalBytesSent);
    if (nMaxOutboundTimeframe != timeframe) {
        // reset measure-cycle in case of changing the timeframe.
        nMaxOutboundCycleStartTime = GetTime();
    }
    nMaxOutboundTimeframe = timeframe;
}

bool CConnman::OutboundTargetReached(bool historicalBlockServingLimit) {
    LOCK(cs_totalBytesSent);
    if (nMaxOutboundLimit == 0) {
        return false;
    }

    if (historicalBlockServingLimit) {
        // keep a large enough buffer to at least relay each block once.
        uint64_t timeLeftInCycle = GetMaxOutboundTimeLeftInCycle();
        uint64_t buffer = timeLeftInCycle / 600 * ONE_MEGABYTE;
        if (buffer >= nMaxOutboundLimit ||
            nMaxOutboundTotalBytesSentInCycle >= nMaxOutboundLimit - buffer) {
            return true;
        }
    } else if (nMaxOutboundTotalBytesSentInCycle >= nMaxOutboundLimit) {
        return true;
    }

    return false;
}

uint64_t CConnman::GetOutboundTargetBytesLeft() {
    LOCK(cs_totalBytesSent);
    if (nMaxOutboundLimit == 0) {
        return 0;
    }

    return (nMaxOutboundTotalBytesSentInCycle >= nMaxOutboundLimit)
               ? 0
               : nMaxOutboundLimit - nMaxOutboundTotalBytesSentInCycle;
}

uint64_t CConnman::GetTotalBytesRecv() {
    LOCK(cs_totalBytesRecv);
    return nTotalBytesRecv;
}

uint64_t CConnman::GetTotalBytesSent() {
    LOCK(cs_totalBytesSent);
    return nTotalBytesSent;
}

ServiceFlags CConnman::GetLocalServices() const {
    return nLocalServices;
}

void CConnman::SetBestHeight(int height) {
    nBestHeight.store(height, std::memory_order_release);
}

int CConnman::GetBestHeight() const {
    return nBestHeight.load(std::memory_order_acquire);
}

unsigned int CConnman::GetReceiveFloodSize() const {
    return nReceiveFloodSize;
}

CNode::CNode(NodeId idIn, ServiceFlags nLocalServicesIn,
             int nMyStartingHeightIn, SOCKET hSocketIn, const CAddress &addrIn,
             uint64_t nKeyedNetGroupIn, uint64_t nLocalHostNonceIn,
             const CAddress &addrBindIn, const std::string &addrNameIn,
             bool fInboundIn)
    : nTimeConnected(GetSystemTimeInSeconds()), addr(addrIn),
      addrBind(addrBindIn), fInbound(fInboundIn),
      nKeyedNetGroup(nKeyedNetGroupIn), addrKnown(5000, 0.001),
      filterInventoryKnown(50000, 0.000001), id(idIn),
      nLocalHostNonce(nLocalHostNonceIn), nLocalServices(nLocalServicesIn),
      nMyStartingHeight(nMyStartingHeightIn) {
    hSocket = hSocketIn;
    addrName = addrNameIn == "" ? addr.ToStringIPPort() : addrNameIn;
    hashContinue = BlockHash();
    filterInventoryKnown.reset();
    pfilter = std::make_unique<CBloomFilter>();

    for (const std::string &msg : getAllNetMessageTypes()) {
        mapRecvBytesPerMsgType[msg] = 0;
    }
    mapRecvBytesPerMsgType[NET_MESSAGE_TYPE_OTHER] = 0;

    if (fLogIPs) {
        LogPrint(BCLog::NET, "Added connection to %s peer=%d\n", addrName, id);
    } else {
        LogPrint(BCLog::NET, "Added connection peer=%d\n", id);
    }
}

CNode::~CNode() {
    CloseSocket(hSocket);
}

bool CConnman::NodeFullyConnected(const CNode *pnode) {
    return pnode && pnode->fSuccessfullyConnected && !pnode->fDisconnect;
}

void CConnman::PushMessage(CNode *pnode, CSerializedNetMsg &&msg) {
    size_t nMessageSize = msg.data.size();
    size_t nTotalSize = nMessageSize + CMessageHeader::HEADER_SIZE;
    LogPrint(BCLog::NET, "sending %s (%d bytes) peer=%d\n", SanitizeString(msg.m_type.c_str()), nMessageSize,
             pnode->GetId());

    std::vector<uint8_t> serializedHeader;
    serializedHeader.reserve(CMessageHeader::HEADER_SIZE);
    uint256 hash = Hash(Span{msg.data}.first(nMessageSize));
    CMessageHeader hdr(config->GetChainParams().NetMagic(), msg.m_type.c_str(), nMessageSize);
    memcpy(hdr.pchChecksum, hash.begin(), CMessageHeader::CHECKSUM_SIZE);

    CVectorWriter{SER_NETWORK, INIT_PROTO_VERSION, serializedHeader, 0, hdr};

    size_t nBytesSent = 0;
    {
        LOCK(pnode->cs_vSend);
        bool optimisticSend(pnode->vSendMsg.empty());

        // log total amount of bytes per message type
        pnode->mapSendBytesPerMsgType[msg.m_type] += nTotalSize;
        pnode->nSendSize += nTotalSize;

        if (pnode->nSendSize > nSendBufferMaxSize) {
            pnode->fPauseSend = true;
        }
        pnode->vSendMsg.push_back(std::move(serializedHeader));
        if (nMessageSize) {
            pnode->vSendMsg.push_back(std::move(msg.data));
        }

        // If write queue empty, attempt "optimistic write"
        if (optimisticSend == true) {
            nBytesSent = SocketSendData(pnode);
        }
    }
    if (nBytesSent) {
        RecordBytesSent(nBytesSent);
    }
}

bool CConnman::ForNode(NodeId id, std::function<bool(CNode *pnode)> func) {
    CNode *found = nullptr;
    LOCK(cs_vNodes);
    for (auto &&pnode : vNodes) {
        if (pnode->GetId() == id) {
            found = pnode;
            break;
        }
    }
    return found != nullptr && NodeFullyConnected(found) && func(found);
}

int64_t CConnman::PoissonNextSendInbound(int64_t now, int average_interval_ms) {
    if (m_next_send_inv_to_incoming < now) {
        // If this function were called from multiple threads simultaneously
        // it would be possible that both update the next send variable, and
        // return a different result to their caller. This is not possible in
        // practice as only the net processing thread invokes this function.
        m_next_send_inv_to_incoming =
            PoissonNextSend(now, average_interval_ms);
    }
    return m_next_send_inv_to_incoming;
}

int64_t PoissonNextSend(int64_t now, int average_interval_ms) {
    return now + int64_t(log1p(GetRand(1ULL << 48) *
                               -0.0000000000000035527136788 /* -1/2^48 */) *
                             average_interval_ms * -1000.0 + 0.5);
}

CSipHasher CConnman::GetDeterministicRandomizer(uint64_t id) const {
    return CSipHasher(nSeed0, nSeed1).Write(id);
}

uint64_t CConnman::CalculateKeyedNetGroup(const CAddress &ad) const {
    std::vector<uint8_t> vchNetGroup(ad.GetGroup(addrman.m_asmap));

    return GetDeterministicRandomizer(RANDOMIZER_ID_NETGROUP)
        .Write(vchNetGroup.data(), vchNetGroup.size())
        .Finalize();
}

/**
 * This function convert MaxBlockSize from byte to
 * MB with a decimal precision one digit rounded down
 * E.g.
 * 1660000 -> 1.6
 * 2010000 -> 2.0
 * 1000000 -> 1.0
 * 230000  -> 0.2
 * 50000   -> 0.0
 *
 *  NB behavior for EB<1MB not standardized yet still
 *  the function applies the same algo used for
 *  EB greater or equal to 1MB
 */
std::string getSubVersionEB(uint64_t MaxBlockSize) {
    // Prepare EB string we are going to add to SubVer:
    // 1) translate from byte to MB and convert to string
    // 2) limit the EB string to the first decimal digit (floored)
    std::stringstream ebMBs;
    ebMBs.imbue(std::locale::classic());
    ebMBs << (MaxBlockSize / (ONE_MEGABYTE / 10));
    std::string eb = ebMBs.str();
    eb.insert(eb.size() - 1, ".", 1);
    if (eb.substr(0, 1) == ".") {
        eb = "0" + eb;
    }
    return eb;
}

std::string userAgent(const Config &config) {
    // format excessive blocksize value
    std::string eb = getSubVersionEB(config.GetExcessiveBlockSize());
    std::vector<std::string> uacomments;
    uacomments.push_back("EB" + eb);

    // Comments are checked for char compliance at startup, it is safe to add
    // them to the user agent string
    for (const std::string &cmt : gArgs.GetArgs("-uacomment")) {
        uacomments.push_back(cmt);
    }

    // Size compliance is checked at startup, it is safe to not check it again
    std::string subversion =
        FormatSubVersion(CLIENT_NAME, CLIENT_VERSION, uacomments);

    return subversion;
}

void CNode::ReadConfigFromExtversion()
{
    AssertLockHeld(cs_extversion);
    assert(bool(extversionEnabled));
    // Processing of the extversion message goes here if needed to set some node values
    // for example here is where BU sets tx chain limits or if the message checksum is checked

    // log the received version number, if any
    if (const auto ver = extversion.GetVersion())
        LogPrint(BCLog::NET, "extversion: peer=%d is using extversion %d.%d.%d\n", GetId(),
                 ver->Major(), ver->Minor(), ver->Revision());
    else
        LogPrint(BCLog::NET, "extversion: peer=%d did not send us their \"Version\" key\n", GetId());
}

//! Returns the number of bytes enqeueud (and eventually sent) for a particular command
uint64_t CNode::GetBytesSentForMsgType(const std::string &msg_type) const
{
    LOCK(cs_vSend);
    const auto it = mapSendBytesPerMsgType.find(msg_type);
    return it != mapSendBytesPerMsgType.end() ? it->second : 0;
}
