// Copyright (c) 2017-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <config.h>
#include <rpc/server.h>
#include <rpc/util.h>

#include <univalue.h>

static UniValue getexcessiveblock(const Config &config,
                                  const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 0) {
        throw std::runtime_error(RPCHelpMan{
            "getexcessiveblock",
            "\nReturn the excessive block size.",
            {},
            RPCResult{"  excessiveBlockSize (integer) block size in bytes\n"},
            RPCExamples{HelpExampleCli("getexcessiveblock", "") +
                        HelpExampleRpc("getexcessiveblock", "")},
        }.ToStringWithResultsAndExamples());
    }

    UniValue::Object ret;
    ret.reserve(1);
    ret.emplace_back("excessiveBlockSize", config.GetExcessiveBlockSize());
    return ret;
}

// clang-format off
static const ContextFreeRPCCommand commands[] = {
    //  category            name                      actor (function)        argNames
    //  ------------------- ------------------------  ----------------------  ----------
    { "network",            "getexcessiveblock",      getexcessiveblock,      {}},
};
// clang-format on

void RegisterABCRPCCommands(CRPCTable &t) {
    for (unsigned int vcidx = 0; vcidx < std::size(commands); ++vcidx) {
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
    }
}
