// Copyright (c) 2019 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/command.h>
#include <rpc/jsonrpcrequest.h>

UniValue
RPCCommandWithArgsContext::Execute(const JSONRPCRequest &request) const {
    return Execute(request.params);
}
