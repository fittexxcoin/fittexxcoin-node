// Copyright (c) 2015-2016 The Fittexxcoin Core developers
// Copyright (c) 2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <chainparams.h>
#include <config.h>
#include <primitives/blockhash.h>
#include <primitives/txid.h>
#include <rpc/server.h>
#include <streams.h>
#include <util/system.h>
#include <validation.h>
#include <zmq/zmqpublishnotifier.h>
#include <zmq/zmqutil.h>

#include <zmq.h>

#include <algorithm>
#include <cstdarg>
#include <cstddef>
#include <map>
#include <string>
#include <utility>

static std::multimap<std::string, CZMQAbstractPublishNotifier *>
    mapPublishNotifiers;

inline constexpr auto MSG_HASHBLOCK = "hashblock";
inline constexpr auto MSG_HASHTX = "hashtx";
inline constexpr auto MSG_RAWBLOCK = "rawblock";
inline constexpr auto MSG_RAWTX = "rawtx";
inline constexpr auto MSG_HASHDS = "hashds";
inline constexpr auto MSG_RAWDS = "rawds";

// Internal function to send multipart message
static int zmq_send_multipart(void *sock, const void *data, size_t size, ...) {
    va_list args;
    va_start(args, size);

    while (1) {
        zmq_msg_t msg;

        int rc = zmq_msg_init_size(&msg, size);
        if (rc != 0) {
            zmqError("Unable to initialize ZMQ msg");
            va_end(args);
            return -1;
        }

        void *buf = zmq_msg_data(&msg);
        memcpy(buf, data, size);

        data = va_arg(args, const void *);

        rc = zmq_msg_send(&msg, sock, data ? ZMQ_SNDMORE : 0);
        if (rc == -1) {
            zmqError("Unable to send ZMQ msg");
            zmq_msg_close(&msg);
            va_end(args);
            return -1;
        }

        zmq_msg_close(&msg);

        if (!data) {
            break;
        }

        size = va_arg(args, size_t);
    }
    va_end(args);
    return 0;
}

bool CZMQAbstractPublishNotifier::Initialize(void *pcontext) {
    assert(!psocket);

    // check if address is being used by other publish notifier
    std::multimap<std::string, CZMQAbstractPublishNotifier *>::iterator i =
        mapPublishNotifiers.find(address);

    if (i == mapPublishNotifiers.end()) {
        psocket = zmq_socket(pcontext, ZMQ_PUB);
        if (!psocket) {
            zmqError("Failed to create socket");
            return false;
        }

        int rc = zmq_bind(psocket, address.c_str());
        if (rc != 0) {
            zmqError("Failed to bind address");
            zmq_close(psocket);
            return false;
        }

        // register this notifier for the address, so it can be reused for other
        // publish notifier
        mapPublishNotifiers.insert(std::make_pair(address, this));
        return true;
    } else {
        LogPrint(BCLog::ZMQ, "zmq: Reusing socket for address %s\n", address);

        psocket = i->second->psocket;
        mapPublishNotifiers.insert(std::make_pair(address, this));

        return true;
    }
}

void CZMQAbstractPublishNotifier::Shutdown() {
    assert(psocket);

    int count = mapPublishNotifiers.count(address);

    // remove this notifier from the list of publishers using this address
    typedef std::multimap<std::string, CZMQAbstractPublishNotifier *>::iterator
        iterator;
    std::pair<iterator, iterator> iterpair =
        mapPublishNotifiers.equal_range(address);

    for (iterator it = iterpair.first; it != iterpair.second; ++it) {
        if (it->second == this) {
            mapPublishNotifiers.erase(it);
            break;
        }
    }

    if (count == 1) {
        LogPrint(BCLog::ZMQ, "Close socket at address %s\n", address);
        int linger = 0;
        zmq_setsockopt(psocket, ZMQ_LINGER, &linger, sizeof(linger));
        zmq_close(psocket);
    }

    psocket = nullptr;
}

bool CZMQAbstractPublishNotifier::SendZmqMessage(const char *command,
                                                 const void *data, size_t size) {
    assert(psocket);

    /* send three parts, command & data & a LE 4byte sequence number */
    uint8_t msgseq[sizeof(uint32_t)];
    WriteLE32(&msgseq[0], nSequence);
    int rc = zmq_send_multipart(psocket, command, strlen(command), data, size,
                                msgseq, (size_t)sizeof(uint32_t), nullptr);
    if (rc == -1) {
        return false;
    }

    /* increment memory only sequence number after sending */
    nSequence++;

    return true;
}

bool CZMQPublishHashBlockNotifier::NotifyBlock(const CBlockIndex *pindex) {
    BlockHash hash = pindex->GetBlockHash();
    LogPrint(BCLog::ZMQ, "zmq: Publish hashblock %s\n", hash.GetHex());
    char data[32];
    for (unsigned int i = 0; i < 32; i++) {
        data[31 - i] = hash.begin()[i];
    }
    return SendZmqMessage(MSG_HASHBLOCK, data, 32);
}

bool CZMQPublishHashTransactionNotifier::NotifyTransaction(
    const CTransaction &transaction) {
    TxId txid = transaction.GetId();
    LogPrint(BCLog::ZMQ, "zmq: Publish hashtx %s\n", txid.GetHex());
    char data[32];
    for (unsigned int i = 0; i < 32; i++) {
        data[31 - i] = txid.begin()[i];
    }
    return SendZmqMessage(MSG_HASHTX, data, 32);
}

bool CZMQPublishRawBlockNotifier::NotifyBlock(const CBlockIndex *pindex) {
    LogPrint(BCLog::ZMQ, "zmq: Publish rawblock %s\n",
             pindex->GetBlockHash().GetHex());

    const Config &config = GetConfig();
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION | RPCSerializationFlags());
    {
        LOCK(cs_main);
        CBlock block;
        if (!ReadBlockFromDisk(block, pindex,
                               config.GetChainParams().GetConsensus())) {
            zmqError("Can't read block from disk");
            return false;
        }

        ss << block;
    }

    return SendZmqMessage(MSG_RAWBLOCK, &(*ss.begin()), ss.size());
}

bool CZMQPublishRawTransactionNotifier::NotifyTransaction(
    const CTransaction &transaction) {
    TxId txid = transaction.GetId();
    LogPrint(BCLog::ZMQ, "zmq: Publish rawtx %s\n", txid.GetHex());
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION | RPCSerializationFlags());
    ss << transaction;
    return SendZmqMessage(MSG_RAWTX, &(*ss.begin()), ss.size());
}

bool CZMQPublishHashDoubleSpendNotifier::NotifyDoubleSpend(const CTransaction &transaction) {
    const TxId txid = transaction.GetId();
    LogPrint(BCLog::ZMQ, "zmq: Publish hashds %s\n", txid.GetHex());
    TxId revtxid{TxId::Uninitialized};
    std::reverse_copy(txid.begin(), txid.end(), revtxid.begin());
    return SendZmqMessage(MSG_HASHDS, &*revtxid.begin(), revtxid.size());
}

bool CZMQPublishRawDoubleSpendNotifier::NotifyDoubleSpend(const CTransaction &transaction) {
    LogPrint(BCLog::ZMQ, "zmq: Publish rawds %s\n", transaction.GetId().GetHex());
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION | RPCSerializationFlags());
    ss << transaction;
    return SendZmqMessage(MSG_RAWDS, &*ss.begin(), ss.size());
}
