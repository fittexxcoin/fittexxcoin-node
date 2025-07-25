// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <netbase.h>

#include <hash.h>
#include <random.h>
#include <sync.h>
#include <uint256.h>
#include <util/bit_cast.h>
#include <util/strencodings.h>
#include <util/string.h>
#include <util/system.h>

#include <tinyformat.h>

#include <atomic>
#include <cstdint>
#include <limits>

#ifndef WIN32
#include <fcntl.h>
#endif

#ifdef USE_POLL
#include <poll.h>
#endif

#if !defined(MSG_NOSIGNAL)
#define MSG_NOSIGNAL 0
#endif

// Settings
static RecursiveMutex cs_proxyInfos;
static proxyType proxyInfo[NET_MAX] GUARDED_BY(cs_proxyInfos);
static proxyType nameProxy GUARDED_BY(cs_proxyInfos);
int nConnectTimeout = DEFAULT_CONNECT_TIMEOUT;
bool fNameLookup = DEFAULT_NAME_LOOKUP;

// Need ample time for negotiation for very slow proxies such as Tor
// (milliseconds)
static const int SOCKS5_RECV_TIMEOUT = 20 * 1000;
static std::atomic<bool> interruptSocks5Recv(false);

enum Network ParseNetwork(const std::string &net_in) {
    std::string net = ToLower(net_in);
    if (net == "ipv4") return NET_IPV4;
    if (net == "ipv6") return NET_IPV6;
    if (net == "onion") return NET_ONION;
    if (net == "tor") {
        LogPrintf("Warning: net name 'tor' is deprecated and will be removed "
                  "in the future. You should use 'onion' instead.\n");
        return NET_ONION;
    }
    return NET_UNROUTABLE;
}

std::string GetNetworkName(enum Network net) {
    switch (net) {
        case NET_IPV4:
            return "ipv4";
        case NET_IPV6:
            return "ipv6";
        case NET_ONION:
            return "onion";
        default:
            return "";
    }
}

static bool LookupIntern(const std::string &name, std::vector<CNetAddr> &vIP, unsigned int nMaxSolutions,
                         bool fAllowLookup) {
    vIP.clear();

    if (!ValidAsCString(name)) {
        return false;
    }

    {
        CNetAddr addr;
        // From our perspective, onion addresses are not hostnames but rather
        // direct encodings of CNetAddr much like IPv4 dotted-decimal notation
        // or IPv6 colon-separated hextet notation. Since we can't use
        // getaddrinfo to decode them and it wouldn't make sense to resolve
        // them, we return a network address representing it instead. See
        // CNetAddr::SetSpecial(const std::string&) for more details.
        if (addr.SetSpecial(name)) {
            vIP.push_back(addr);
            return true;
        }
    }
#ifdef WIN32
    // Windows: WSAStartup must be called before getaddrinfo below will work.
    // SetupNetworking() is a no-op on all platforms except Windows, and on
    // Windows it is guaranteed to be ok to call more than once in this
    // codebase. This was added after MR !786, which ends up calling into here
    // during static initialization, before SetupNetworking() could be called
    // by init.cpp.
    static const bool setupOnce = SetupNetworking(); // leverage C++ guarantee this is called just once.
    (void)setupOnce;
#endif

    struct addrinfo aiHint;
    memset(&aiHint, 0, sizeof(struct addrinfo));

    // We want a TCP port, which is a streaming socket type
    aiHint.ai_socktype = SOCK_STREAM;
    aiHint.ai_protocol = IPPROTO_TCP;
    // We don't care which address family (IPv4 or IPv6) is returned
    aiHint.ai_family = AF_UNSPEC;
    // If we allow lookups of hostnames, use the AI_ADDRCONFIG flag to only
    // return addresses whose family we have an address configured for.
    //
    // If we don't allow lookups, then use the AI_NUMERICHOST flag for
    // getaddrinfo to only decode numerical network addresses and suppress
    // hostname lookups.
    aiHint.ai_flags = fAllowLookup ? AI_ADDRCONFIG : AI_NUMERICHOST;
    struct addrinfo *aiRes = nullptr;
    int nErr = getaddrinfo(name.c_str(), nullptr, &aiHint, &aiRes);
    if (nErr) {
        return false;
    }

    // Traverse the linked list starting with aiTrav, add all non-internal
    // IPv4,v6 addresses to vIP while respecting nMaxSolutions.
    struct addrinfo *aiTrav = aiRes;
    while (aiTrav != nullptr &&
           (nMaxSolutions == 0 || vIP.size() < nMaxSolutions)) {
        CNetAddr resolved;
        if (aiTrav->ai_family == AF_INET) {
            assert(aiTrav->ai_addrlen >= sizeof(sockaddr_in));
            resolved = CNetAddr(bit_cast<sockaddr_in>(*aiTrav->ai_addr).sin_addr);
        }

        if (aiTrav->ai_family == AF_INET6) {
            assert(aiTrav->ai_addrlen >= sizeof(sockaddr_in6));
            const auto s6 = bit_cast_unsafe<sockaddr_in6>(*aiTrav->ai_addr);
            resolved = CNetAddr(s6.sin6_addr, s6.sin6_scope_id);
        }

        // Never allow resolving to an internal address. Consider any such
        // result invalid.
        if (!resolved.IsInternal()) {
            vIP.push_back(resolved);
        }

        aiTrav = aiTrav->ai_next;
    }

    freeaddrinfo(aiRes);

    return (vIP.size() > 0);
}

/**
 * Resolve a host string to its corresponding network addresses.
 *
 * @param name    The string representing a host. Could be a name or a numerical
 *                IP address (IPv6 addresses in their bracketed form are
 *                allowed).
 * @param[out] vIP The resulting network addresses to which the specified host
 *                 string resolved.
 *
 * @returns Whether or not the specified host string successfully resolved to
 *          any resulting network addresses.
 *
 * @see Lookup(const char *, std::vector<CService>&, int, bool, unsigned int)
 *      for additional parameter descriptions.
 */
bool LookupHost(const std::string &name, std::vector<CNetAddr> &vIP, unsigned int nMaxSolutions, bool fAllowLookup) {
    if (!ValidAsCString(name)) {
        return false;
    }
    std::string strHost = name;
    if (strHost.empty()) {
        return false;
    }
    if (strHost.front() == '[' && strHost.back() == ']') {
        strHost = strHost.substr(1, strHost.size() - 2);
    }

    return LookupIntern(strHost, vIP, nMaxSolutions, fAllowLookup);
}

 /**
 * Resolve a host string to its first corresponding network address.
 *
 * @see LookupHost(const std::string&, std::vector<CNetAddr>&, unsigned int, bool) for
 *      additional parameter descriptions.
 */
bool LookupHost(const std::string &name, CNetAddr &addr, bool fAllowLookup) {
    if (!ValidAsCString(name)) {
        return false;
    }
    std::vector<CNetAddr> vIP;
    LookupHost(name, vIP, 1, fAllowLookup);
    if (vIP.empty()) {
        return false;
    }
    addr = vIP.front();
    return true;
}

/**
 * Resolve a service string to its corresponding service.
 *
 * @param name    The string representing a service. Could be a name or a
 *                numerical IP address (IPv6 addresses should be in their
 *                disambiguated bracketed form), optionally followed by a port
 *                number. (e.g. example.com:7890 or
 *                [2001:db8:85a3:8d3:1319:8a2e:370:7348]:420)
 * @param[out] vAddr The resulting services to which the specified service string
 *                   resolved.
 * @param portDefault The default port for resulting services if not specified
 *                    by the service string.
 * @param fAllowLookup Whether or not hostname lookups are permitted. If yes,
 *                     external queries may be performed.
 * @param nMaxSolutions The maximum number of results we want, specifying 0
 *                      means "as many solutions as we get."
 *
 * @returns Whether or not the service string successfully resolved to any
 *          resulting services.
 */
bool Lookup(const std::string &name, std::vector<CService> &vAddr, int portDefault, bool fAllowLookup,
            unsigned int nMaxSolutions) {
    if (name.empty() || !ValidAsCString(name)) {
        return false;
    }
    int port = portDefault;
    std::string hostname;
    SplitHostPort(name, port, hostname);

    std::vector<CNetAddr> vIP;
    bool fRet = LookupIntern(hostname, vIP, nMaxSolutions, fAllowLookup);
    if (!fRet) {
        return false;
    }
    vAddr.resize(vIP.size());
    for (unsigned int i = 0; i < vIP.size(); i++) {
        vAddr[i] = CService(vIP[i], port);
    }
    return true;
}

/**
 * Resolve a service string to its first corresponding service.
 *
 * @see Lookup(const char *, std::vector<CService>&, int, bool, unsigned int)
 *      for additional parameter descriptions.
 */
bool Lookup(const std::string &name, CService &addr, int portDefault, bool fAllowLookup) {
    if (!ValidAsCString(name)) {
        return false;
    }
    std::vector<CService> vService;
    bool fRet = Lookup(name, vService, portDefault, fAllowLookup, 1);
    if (!fRet) {
        return false;
    }
    addr = vService[0];
    return true;
}

/**
 * Resolve a service string with a numeric IP to its first corresponding
 * service.
 *
 * @returns The resulting CService if the resolution was successful, [::]:0
 *          otherwise.
 *
 * @see Lookup(const char *, CService&, int, bool) for additional parameter
 *      descriptions.
 */
CService LookupNumeric(const std::string &name, int portDefault) {
    if (!ValidAsCString(name)) {
        return {};
    }
    CService addr;
    // "1.2:345" will fail to resolve the ip, but will still set the port.
    // If the ip fails to resolve, re-init the result.
    if (!Lookup(name, addr, portDefault, false)) {
        addr = CService();
    }
    return addr;
}

struct timeval MillisToTimeval(int64_t nTimeout) {
    struct timeval timeout;
    timeout.tv_sec = nTimeout / 1000;
    timeout.tv_usec = (nTimeout % 1000) * 1000;
    return timeout;
}

/** SOCKS version */
enum SOCKSVersion : uint8_t { SOCKS4 = 0x04, SOCKS5 = 0x05 };

/** Values defined for METHOD in RFC1928 */
enum SOCKS5Method : uint8_t {
    NOAUTH = 0x00,        //!< No authentication required
    GSSAPI = 0x01,        //!< GSSAPI
    USER_PASS = 0x02,     //!< Username/password
    NO_ACCEPTABLE = 0xff, //!< No acceptable methods
};

/** Values defined for CMD in RFC1928 */
enum SOCKS5Command : uint8_t {
    CONNECT = 0x01,
    BIND = 0x02,
    UDP_ASSOCIATE = 0x03
};

/** Values defined for REP in RFC1928 */
enum SOCKS5Reply : uint8_t {
    SUCCEEDED = 0x00,        //!< Succeeded
    GENFAILURE = 0x01,       //!< General failure
    NOTALLOWED = 0x02,       //!< Connection not allowed by ruleset
    NETUNREACHABLE = 0x03,   //!< Network unreachable
    HOSTUNREACHABLE = 0x04,  //!< Network unreachable
    CONNREFUSED = 0x05,      //!< Connection refused
    TTLEXPIRED = 0x06,       //!< TTL expired
    CMDUNSUPPORTED = 0x07,   //!< Command not supported
    ATYPEUNSUPPORTED = 0x08, //!< Address type not supported
};

/** Values defined for ATYPE in RFC1928 */
enum SOCKS5Atyp : uint8_t {
    IPV4 = 0x01,
    DOMAINNAME = 0x03,
    IPV6 = 0x04,
};

/** Status codes that can be returned by InterruptibleRecv */
enum class IntrRecvError {
    OK,
    Timeout,
    Disconnected,
    NetworkError,
    Interrupted
};

/**
 * Try to read a specified number of bytes from a socket. Please read the "see
 * also" section for more detail.
 *
 * @param data The buffer where the read bytes should be stored.
 * @param len The number of bytes to read into the specified buffer.
 * @param timeout The total timeout in milliseconds for this read.
 * @param hSocket The socket (has to be in non-blocking mode) from which to read
 *                bytes.
 *
 * @returns An IntrRecvError indicating the resulting status of this read.
 *          IntrRecvError::OK only if all of the specified number of bytes were
 *          read.
 *
 * @see This function can be interrupted by calling InterruptSocks5(bool).
 *      Sockets can be made non-blocking with SetSocketNonBlocking(const
 *      SOCKET&, bool).
 */
static IntrRecvError InterruptibleRecv(uint8_t *data, size_t len, int timeout,
                                       const SOCKET &hSocket) {
    int64_t curTime = GetTimeMillis();
    int64_t endTime = curTime + timeout;
    // Maximum time to wait for I/O readiness. It will take up until this time
    // (in millis) to break off in case of an interruption.
    const int64_t maxWait = 1000;
    while (len > 0 && curTime < endTime) {
        // Optimistically try the recv first
        ssize_t ret = recv(hSocket, (char *)data, len, 0);
        if (ret > 0) {
            len -= ret;
            data += ret;
        } else if (ret == 0) {
            // Unexpected disconnection
            return IntrRecvError::Disconnected;
        } else {
            // Other error or blocking
            int nErr = WSAGetLastError();
            if (nErr == WSAEINPROGRESS || nErr == WSAEWOULDBLOCK ||
                nErr == WSAEINVAL) {
                if (!IsSelectableSocket(hSocket)) {
                    return IntrRecvError::NetworkError;
                }
                // Only wait at most maxWait milliseconds at a time, unless
                // we're approaching the end of the specified total timeout
                int timeout_ms = std::min(endTime - curTime, maxWait);
#ifdef USE_POLL
                struct pollfd pollfd = {};
                pollfd.fd = hSocket;
                pollfd.events = POLLIN;
                int nRet = poll(&pollfd, 1, timeout_ms);
#else
                struct timeval tval = MillisToTimeval(timeout_ms);
                fd_set fdset;
                FD_ZERO(&fdset);
                FD_SET(hSocket, &fdset);
                int nRet = select(hSocket + 1, &fdset, nullptr, nullptr, &tval);
#endif
                if (nRet == SOCKET_ERROR) {
                    return IntrRecvError::NetworkError;
                }
            } else {
                return IntrRecvError::NetworkError;
            }
        }
        if (interruptSocks5Recv) {
            return IntrRecvError::Interrupted;
        }
        curTime = GetTimeMillis();
    }
    return len == 0 ? IntrRecvError::OK : IntrRecvError::Timeout;
}

/** Credentials for proxy authentication */
struct ProxyCredentials {
    std::string username;
    std::string password;
};

/** Convert SOCKS5 reply to an error message */
static std::string Socks5ErrorString(uint8_t err) {
    switch (err) {
        case SOCKS5Reply::GENFAILURE:
            return "general failure";
        case SOCKS5Reply::NOTALLOWED:
            return "connection not allowed";
        case SOCKS5Reply::NETUNREACHABLE:
            return "network unreachable";
        case SOCKS5Reply::HOSTUNREACHABLE:
            return "host unreachable";
        case SOCKS5Reply::CONNREFUSED:
            return "connection refused";
        case SOCKS5Reply::TTLEXPIRED:
            return "TTL expired";
        case SOCKS5Reply::CMDUNSUPPORTED:
            return "protocol error";
        case SOCKS5Reply::ATYPEUNSUPPORTED:
            return "address type not supported";
        default:
            return "unknown";
    }
}

/**
 * Connect to a specified destination service through an already connected
 * SOCKS5 proxy.
 *
 * @param strDest The destination fully-qualified domain name.
 * @param port The destination port.
 * @param auth The credentials with which to authenticate with the specified
 *             SOCKS5 proxy.
 * @param hSocket The SOCKS5 proxy socket.
 *
 * @returns Whether or not the operation succeeded.
 *
 * @note The specified SOCKS5 proxy socket must already be connected to the
 *       SOCKS5 proxy.
 *
 * @see <a href="https://www.ietf.org/rfc/rfc1928.txt">RFC1928: SOCKS Protocol
 *      Version 5</a>
 */
static bool Socks5(const std::string &strDest, int port,
                   const ProxyCredentials *auth, const SOCKET &hSocket) {
    IntrRecvError recvr;
    LogPrint(BCLog::NET, "SOCKS5 connecting %s\n", strDest);
    if (strDest.size() > 255) {
        return error("Hostname too long");
    }
    // Construct the version identifier/method selection message
    std::vector<uint8_t> vSocks5Init;
    vSocks5Init.push_back(SOCKSVersion::SOCKS5); // We want the SOCK5 protocol
    if (auth) {
        vSocks5Init.push_back(0x02); // 2 method identifiers follow...
        vSocks5Init.push_back(SOCKS5Method::NOAUTH);
        vSocks5Init.push_back(SOCKS5Method::USER_PASS);
    } else {
        vSocks5Init.push_back(0x01); // 1 method identifier follows...
        vSocks5Init.push_back(SOCKS5Method::NOAUTH);
    }
    ssize_t ret = send(hSocket, (const char *)vSocks5Init.data(),
                       vSocks5Init.size(), MSG_NOSIGNAL);
    if (ret != (ssize_t)vSocks5Init.size()) {
        return error("Error sending to proxy");
    }
    uint8_t pchRet1[2];
    if ((recvr = InterruptibleRecv(pchRet1, 2, SOCKS5_RECV_TIMEOUT, hSocket)) !=
        IntrRecvError::OK) {
        LogPrintf("Socks5() connect to %s:%d failed: InterruptibleRecv() "
                  "timeout or other failure\n",
                  strDest, port);
        return false;
    }
    if (pchRet1[0] != SOCKSVersion::SOCKS5) {
        return error("Proxy failed to initialize");
    }
    if (pchRet1[1] == SOCKS5Method::USER_PASS && auth) {
        // Perform username/password authentication (as described in RFC1929)
        std::vector<uint8_t> vAuth;
        // Current (and only) version of user/pass subnegotiation
        vAuth.push_back(0x01);
        if (auth->username.size() > 255 || auth->password.size() > 255) {
            return error("Proxy username or password too long");
        }
        vAuth.push_back(auth->username.size());
        vAuth.insert(vAuth.end(), auth->username.begin(), auth->username.end());
        vAuth.push_back(auth->password.size());
        vAuth.insert(vAuth.end(), auth->password.begin(), auth->password.end());
        ret = send(hSocket, (const char *)vAuth.data(), vAuth.size(),
                   MSG_NOSIGNAL);
        if (ret != (ssize_t)vAuth.size()) {
            return error("Error sending authentication to proxy");
        }
        LogPrint(BCLog::PROXY, "SOCKS5 sending proxy authentication %s:%s\n",
                 auth->username, auth->password);
        uint8_t pchRetA[2];
        if ((recvr = InterruptibleRecv(pchRetA, 2, SOCKS5_RECV_TIMEOUT,
                                       hSocket)) != IntrRecvError::OK) {
            return error("Error reading proxy authentication response");
        }
        if (pchRetA[0] != 0x01 || pchRetA[1] != 0x00) {
            return error("Proxy authentication unsuccessful");
        }
    } else if (pchRet1[1] == SOCKS5Method::NOAUTH) {
        // Perform no authentication
    } else {
        return error("Proxy requested wrong authentication method %02x",
                     pchRet1[1]);
    }
    std::vector<uint8_t> vSocks5;
    // VER protocol version
    vSocks5.push_back(SOCKSVersion::SOCKS5);
    // CMD CONNECT
    vSocks5.push_back(SOCKS5Command::CONNECT);
    // RSV Reserved must be 0
    vSocks5.push_back(0x00);
    // ATYP DOMAINNAME
    vSocks5.push_back(SOCKS5Atyp::DOMAINNAME);
    // Length<=255 is checked at beginning of function
    vSocks5.push_back(strDest.size());
    vSocks5.insert(vSocks5.end(), strDest.begin(), strDest.end());
    vSocks5.push_back((port >> 8) & 0xFF);
    vSocks5.push_back((port >> 0) & 0xFF);
    ret = send(hSocket, (const char *)vSocks5.data(), vSocks5.size(),
               MSG_NOSIGNAL);
    if (ret != (ssize_t)vSocks5.size()) {
        return error("Error sending to proxy");
    }
    uint8_t pchRet2[4];
    if ((recvr = InterruptibleRecv(pchRet2, 4, SOCKS5_RECV_TIMEOUT, hSocket)) !=
        IntrRecvError::OK) {
        if (recvr == IntrRecvError::Timeout) {
            /**
             * If a timeout happens here, this effectively means we timed out
             * while connecting to the remote node. This is very common for Tor,
             * so do not print an error message.
             */
            return false;
        } else {
            return error("Error while reading proxy response");
        }
    }
    if (pchRet2[0] != SOCKSVersion::SOCKS5) {
        return error("Proxy failed to accept request");
    }
    if (pchRet2[1] != SOCKS5Reply::SUCCEEDED) {
        // Failures to connect to a peer that are not proxy errors
        LogPrintf("Socks5() connect to %s:%d failed: %s\n", strDest, port,
                  Socks5ErrorString(pchRet2[1]));
        return false;
    }
    // Reserved field must be 0
    if (pchRet2[2] != 0x00) {
        return error("Error: malformed proxy response");
    }
    uint8_t pchRet3[256];
    switch (pchRet2[3]) {
        case SOCKS5Atyp::IPV4:
            recvr = InterruptibleRecv(pchRet3, 4, SOCKS5_RECV_TIMEOUT, hSocket);
            break;
        case SOCKS5Atyp::IPV6:
            recvr =
                InterruptibleRecv(pchRet3, 16, SOCKS5_RECV_TIMEOUT, hSocket);
            break;
        case SOCKS5Atyp::DOMAINNAME: {
            recvr = InterruptibleRecv(pchRet3, 1, SOCKS5_RECV_TIMEOUT, hSocket);
            if (recvr != IntrRecvError::OK) {
                return error("Error reading from proxy");
            }
            int nRecv = pchRet3[0];
            recvr =
                InterruptibleRecv(pchRet3, nRecv, SOCKS5_RECV_TIMEOUT, hSocket);
            break;
        }
        default:
            return error("Error: malformed proxy response");
    }
    if (recvr != IntrRecvError::OK) {
        return error("Error reading from proxy");
    }
    if ((recvr = InterruptibleRecv(pchRet3, 2, SOCKS5_RECV_TIMEOUT, hSocket)) !=
        IntrRecvError::OK) {
        return error("Error reading from proxy");
    }
    LogPrint(BCLog::NET, "SOCKS5 connected %s\n", strDest);
    return true;
}

/**
 * Try to create a socket file descriptor with specific properties in the
 * communications domain (address family) of the specified service.
 *
 * For details on the desired properties, see the inline comments in the source
 * code.
 */
SOCKET CreateSocket(const CService &addrConnect) {
    // Create a sockaddr & length pair from the specified service.
    const auto optPair = addrConnect.GetSockAddr();
    if (!optPair) {
        LogPrintf("Cannot create socket for %s: unsupported network\n",
                  addrConnect.ToString());
        return INVALID_SOCKET;
    }
    auto & [sockaddr, len] = *optPair;

    // Create a TCP socket in the address family of the specified service.
    SOCKET hSocket = socket(sockaddr.ss_family,
                            SOCK_STREAM, IPPROTO_TCP);
    if (hSocket == INVALID_SOCKET) {
        return INVALID_SOCKET;
    }

    // Ensure that waiting for I/O on this socket won't result in undefined
    // behavior.
    if (!IsSelectableSocket(hSocket)) {
        CloseSocket(hSocket);
        LogPrintf("Cannot create connection: non-selectable socket created (fd "
                  ">= FD_SETSIZE ?)\n");
        return INVALID_SOCKET;
    }

#ifdef SO_NOSIGPIPE
    int set = 1;
    // Set the no-sigpipe option on the socket for BSD systems, other UNIXes
    // should use the MSG_NOSIGNAL flag for every send.
    setsockopt(hSocket, SOL_SOCKET, SO_NOSIGPIPE, (sockopt_arg_type)&set,
               sizeof(int));
#endif

    // Set the no-delay option (disable Nagle's algorithm) on the TCP socket.
    SetSocketNoDelay(hSocket);

    // Set the non-blocking option on the socket.
    if (!SetSocketNonBlocking(hSocket, true)) {
        CloseSocket(hSocket);
        LogPrintf("CreateSocket: Setting socket to non-blocking failed, error %s\n",
                  NetworkErrorString(WSAGetLastError()));
    }
    return hSocket;
}

template <typename... Args>
static void LogConnectFailure(bool manual_connection, const char *fmt,
                              const Args &... args) {
    std::string error_message = tfm::format(fmt, args...);
    if (manual_connection) {
        LogPrintf("%s\n", error_message);
    } else {
        LogPrint(BCLog::NET, "%s\n", error_message);
    }
}

/**
 * Try to connect to the specified service on the specified socket.
 *
 * @param addrConnect The service to which to connect.
 * @param hSocket The socket on which to connect.
 * @param nTimeout Wait this many milliseconds for the connection to be
 *                 established.
 * @param manual_connection Whether or not the connection was manually requested
 *                          (e.g. thru the addnode RPC)
 *
 * @returns Whether or not a connection was successfully made.
 */
bool ConnectSocketDirectly(const CService &addrConnect, const SOCKET &hSocket,
                           int nTimeout, bool manual_connection) {
    if (hSocket == INVALID_SOCKET) {
        LogPrintf("Cannot connect to %s: invalid socket\n",
                  addrConnect.ToString());
        return false;
    }
    // Create a sockaddr & length pair from the specified service.
    const auto optPair = addrConnect.GetSockAddr();
    if (!optPair) {
        LogPrintf("Cannot connect to %s: unsupported network\n",
                  addrConnect.ToString());
        return false;
    }
    auto & [sockaddr, len] = *optPair;

    // Connect to the addrConnect service on the hSocket socket.
    if (connect(hSocket, (struct sockaddr *)&sockaddr, len) == SOCKET_ERROR) {
        int nErr = WSAGetLastError();
        // WSAEINVAL is here because some legacy version of winsock uses it
        if (nErr == WSAEINPROGRESS || nErr == WSAEWOULDBLOCK ||
            nErr == WSAEINVAL) {
            // Connection didn't actually fail, but is being established
            // asynchronously. Thus, use async I/O api (select/poll)
            // synchronously to check for successful connection with a timeout.
#ifdef USE_POLL
            struct pollfd pollfd = {};
            pollfd.fd = hSocket;
            pollfd.events = POLLIN | POLLOUT;
            int nRet = poll(&pollfd, 1, nTimeout);
#else
            struct timeval timeout = MillisToTimeval(nTimeout);
            fd_set fdset;
            FD_ZERO(&fdset);
            FD_SET(hSocket, &fdset);
            int nRet = select(hSocket + 1, nullptr, &fdset, nullptr, &timeout);
#endif
            // Upon successful completion, both select and poll return the total
            // number of file descriptors that have been selected. A value of 0
            // indicates that the call timed out and no file descriptors have
            // been selected.
            if (nRet == 0) {
                LogPrint(BCLog::NET, "connection to %s timeout\n",
                         addrConnect.ToString());
                return false;
            }
            if (nRet == SOCKET_ERROR) {
                LogPrintf("select() for %s failed: %s\n",
                          addrConnect.ToString(),
                          NetworkErrorString(WSAGetLastError()));
                return false;
            }

            // Even if the select/poll was successful, the connect might not
            // have been successful. The reason for this failure is hidden away
            // in the SO_ERROR for the socket in modern systems. We read it into
            // nRet here.
            socklen_t nRetSize = sizeof(nRet);
            if (getsockopt(hSocket, SOL_SOCKET, SO_ERROR,
                           (sockopt_arg_type)&nRet,
                           &nRetSize) == SOCKET_ERROR) {
                LogPrintf("getsockopt() for %s failed: %s\n",
                          addrConnect.ToString(),
                          NetworkErrorString(WSAGetLastError()));
                return false;
            }
            if (nRet != 0) {
                LogConnectFailure(manual_connection,
                                  "connect() to %s failed after select(): %s",
                                  addrConnect.ToString(),
                                  NetworkErrorString(nRet));
                return false;
            }
        }
#ifdef WIN32
        else if (WSAGetLastError() != WSAEISCONN)
#else
        else
#endif
        {
            LogConnectFailure(manual_connection, "connect() to %s failed: %s",
                              addrConnect.ToString(),
                              NetworkErrorString(WSAGetLastError()));
            return false;
        }
    }
    return true;
}

bool SetProxy(enum Network net, const proxyType &addrProxy) {
    assert(net >= 0 && net < NET_MAX);
    if (!addrProxy.IsValid()) {
        return false;
    }
    LOCK(cs_proxyInfos);
    proxyInfo[net] = addrProxy;
    return true;
}

bool GetProxy(enum Network net, proxyType &proxyInfoOut) {
    assert(net >= 0 && net < NET_MAX);
    LOCK(cs_proxyInfos);
    if (!proxyInfo[net].IsValid()) {
        return false;
    }
    proxyInfoOut = proxyInfo[net];
    return true;
}

/**
 * Set the name proxy to use for all connections to nodes specified by a
 * hostname. After setting this proxy, connecting to a node sepcified by a
 * hostname won't result in a local lookup of said hostname, rather, connect to
 * the node by asking the name proxy for a proxy connection to the hostname,
 * effectively delegating the hostname lookup to the specified proxy.
 *
 * This delegation increases privacy for those who set the name proxy as they no
 * longer leak their external hostname queries to their DNS servers.
 *
 * @returns Whether or not the operation succeeded.
 *
 * @note SOCKS5's support for UDP-over-SOCKS5 has been considered, but no SOCK5
 *       server in common use (most notably Tor) actually implements UDP
 *       support, and a DNS resolver is beyond the scope of this project.
 */
bool SetNameProxy(const proxyType &addrProxy) {
    if (!addrProxy.IsValid()) {
        return false;
    }
    LOCK(cs_proxyInfos);
    nameProxy = addrProxy;
    return true;
}

bool GetNameProxy(proxyType &nameProxyOut) {
    LOCK(cs_proxyInfos);
    if (!nameProxy.IsValid()) {
        return false;
    }
    nameProxyOut = nameProxy;
    return true;
}

bool HaveNameProxy() {
    LOCK(cs_proxyInfos);
    return nameProxy.IsValid();
}

bool IsProxy(const CNetAddr &addr) {
    LOCK(cs_proxyInfos);
    for (int i = 0; i < NET_MAX; i++) {
        if (addr == static_cast<CNetAddr>(proxyInfo[i].proxy)) {
            return true;
        }
    }
    return false;
}

/**
 * Connect to a specified destination service through a SOCKS5 proxy by first
 * connecting to the SOCKS5 proxy.
 *
 * @param proxy The SOCKS5 proxy.
 * @param strDest The destination service to which to connect.
 * @param port The destination port.
 * @param hSocket The socket on which to connect to the SOCKS5 proxy.
 * @param nTimeout Wait this many milliseconds for the connection to the SOCKS5
 *                 proxy to be established.
 * @param outProxyConnectionFailed[out] Whether or not the connection to the
 *                                      SOCKS5 proxy failed.
 *
 * @returns Whether or not the operation succeeded.
 */
bool ConnectThroughProxy(const proxyType &proxy, const std::string &strDest, int port, const SOCKET &hSocket,
                         int nTimeout, bool &outProxyConnectionFailed) {
    outProxyConnectionFailed = false;
    // first connect to proxy server
    if (!ConnectSocketDirectly(proxy.proxy, hSocket, nTimeout, true)) {
        outProxyConnectionFailed = true;
        return false;
    }
    // do socks negotiation
    if (proxy.randomize_credentials) {
        ProxyCredentials random_auth;
        static std::atomic_int counter(0);
        random_auth.username = random_auth.password =
            strprintf("%i", counter++);
        if (!Socks5(strDest, (unsigned short)port, &random_auth, hSocket)) {
            return false;
        }
    } else if (!Socks5(strDest, (unsigned short)port, 0, hSocket)) {
        return false;
    }
    return true;
}

/**
 * Parse and resolve a specified subnet string into the appropriate internal
 * representation.
 *
 * @param strSubnet A string representation of a subnet of the form `network
 *                address [ "/", ( CIDR-style suffix | netmask ) ]`(e.g.
 *                `2001:db8::/32`, `192.0.2.0/255.255.255.0`, or `8.8.8.8`).
 * @param ret The resulting internal representation of a subnet.
 *
 * @returns Whether the operation succeeded or not.
 */
bool LookupSubNet(const std::string &strSubnet, CSubNet &ret) {
    if (!ValidAsCString(strSubnet)) {
        return false;
    }
    size_t slash = strSubnet.find_last_of('/');
    std::vector<CNetAddr> vIP;

    std::string strAddress = strSubnet.substr(0, slash);
    // TODO: Use LookupHost(const std::string&, CNetAddr&, bool) instead to just get
    //       one CNetAddr.
    if (LookupHost(strAddress, vIP, 1, false)) {
        CNetAddr network = vIP[0];
        if (slash != strSubnet.npos) {
            std::string strNetmask = strSubnet.substr(slash + 1);
            uint8_t n;
            if (ParseUInt8(strNetmask, &n)) {
                // If valid number, assume CIDR variable-length subnet masking
                ret = CSubNet(network, n);
                return ret.IsValid();
            } else {
                // If not a valid number, try full netmask syntax
                // Never allow lookup for netmask
                if (LookupHost(strNetmask, vIP, 1, false)) {
                    ret = CSubNet(network, vIP[0]);
                    return ret.IsValid();
                }
            }
        } else {
            ret = CSubNet(network);
            return ret.IsValid();
        }
    }
    return false;
}

#ifdef WIN32
std::string NetworkErrorString(int err) {
    char buf[256];
    buf[0] = 0;
    if (FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM |
                           FORMAT_MESSAGE_IGNORE_INSERTS |
                           FORMAT_MESSAGE_MAX_WIDTH_MASK,
                       nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       buf, sizeof(buf), nullptr)) {
        return strprintf("%s (%d)", buf, err);
    } else {
        return strprintf("Unknown error (%d)", err);
    }
}
#else
std::string NetworkErrorString(int err) {
    char buf[256];
    buf[0] = 0;
    /**
     * Too bad there are two incompatible implementations of the
     * thread-safe strerror.
     */
    const char *s;
#ifdef STRERROR_R_CHAR_P
    /* GNU variant can return a pointer outside the passed buffer */
    s = strerror_r(err, buf, sizeof(buf));
#else
    s = buf;
    /* POSIX variant always returns message in buffer */
    if (strerror_r(err, buf, sizeof(buf))) {
        buf[0] = 0;
    }
#endif
    return strprintf("%s (%d)", s, err);
}
#endif

bool CloseSocket(SOCKET &hSocket) {
    if (hSocket == INVALID_SOCKET) {
        return false;
    }
#ifdef WIN32
    int ret = closesocket(hSocket);
#else
    int ret = close(hSocket);
#endif
    if (ret) {
        LogPrintf("Socket close failed: %d. Error: %s\n", hSocket,
                  NetworkErrorString(WSAGetLastError()));
    }
    hSocket = INVALID_SOCKET;
    return ret != SOCKET_ERROR;
}

bool SetSocketNonBlocking(const SOCKET &hSocket, bool fNonBlocking) {
    if (fNonBlocking) {
#ifdef WIN32
        u_long nOne = 1;
        if (ioctlsocket(hSocket, FIONBIO, &nOne) == SOCKET_ERROR) {
#else
        int fFlags = fcntl(hSocket, F_GETFL, 0);
        if (fcntl(hSocket, F_SETFL, fFlags | O_NONBLOCK) == SOCKET_ERROR) {
#endif
            return false;
        }
    } else {
#ifdef WIN32
        u_long nZero = 0;
        if (ioctlsocket(hSocket, FIONBIO, &nZero) == SOCKET_ERROR) {
#else
        int fFlags = fcntl(hSocket, F_GETFL, 0);
        if (fcntl(hSocket, F_SETFL, fFlags & ~O_NONBLOCK) == SOCKET_ERROR) {
#endif
            return false;
        }
    }

    return true;
}

bool SetSocketNoDelay(const SOCKET &hSocket) {
    int set = 1;
    int rc = setsockopt(hSocket, IPPROTO_TCP, TCP_NODELAY,
                        (sockopt_arg_type)&set, sizeof(int));
    return rc == 0;
}

void InterruptSocks5(bool interrupt) {
    interruptSocks5Recv = interrupt;
}
