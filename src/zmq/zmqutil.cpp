// Copyright (c) 2014-2018 The Fittexxcoin Core developers
// Copyright (c) 2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <logging.h>
#include <zmq/zmqutil.h>

#include <zmq.h>

void zmqError(const char* str) {
    LogPrint(BCLog::ZMQ, "zmq: Error: %s, errno=%s\n", str, zmq_strerror(errno));
}
