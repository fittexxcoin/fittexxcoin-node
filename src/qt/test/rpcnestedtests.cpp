// Copyright (c) 2016 The Fittexxcoin Core developers
// Copyright (c) 2020 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/test/rpcnestedtests.h>

#include <chainparams.h>
#include <config.h>
#include <consensus/validation.h>
#include <fs.h>
#include <interfaces/node.h>
#include <qt/rpcconsole.h>
#include <rpc/register.h>
#include <rpc/server.h>
#include <util/system.h>
#include <validation.h>

#include <test/setup_common.h>

#include <QDir>
#include <QtGlobal>

#include <univalue.h>

static UniValue rpcNestedTest_rpc(const Config &config,
                                  const JSONRPCRequest &request) {
    if (request.fHelp) {
        return "help message";
    }
    return UniValue::stringify(request.params);
}

static const ContextFreeRPCCommand vRPCCommands[] = {
    {"test", "rpcNestedTest", &rpcNestedTest_rpc, {}},
};

void RPCNestedTests::rpcNestedTests() {
    // do some test setup
    // could be moved to a more generic place when we add more tests on QT level
    tableRPC.appendCommand("rpcNestedTest", &vRPCCommands[0]);
    // mempool.setSanityCheck(1.0);

    TestingSetup test;

    std::string result;
    std::string result2;
    std::string filtered;
    auto node = interfaces::MakeNode();

    // Simple result filtering with path.
    RPCConsole::RPCExecuteCommandLine(*node, result,
                                      "getblockchaininfo()[chain]", &filtered);
    QVERIFY(result == "main");
    QVERIFY(filtered == "getblockchaininfo()[chain]");

    // Simple 2 level nesting.
    RPCConsole::RPCExecuteCommandLine(*node, result,
                                      "getblock(getbestblockhash())");
    RPCConsole::RPCExecuteCommandLine(
        *node, result, "getblock(getblock(getbestblockhash())[hash], true)");

    // 4 level nesting with whitespace, filtering path and boolean parameter.
    RPCConsole::RPCExecuteCommandLine(
        *node, result,
        "getblock( getblock( getblock(getbestblockhash())[hash] "
        ")[hash], true)");

    RPCConsole::RPCExecuteCommandLine(*node, result, "getblockchaininfo");
    QVERIFY(result.substr(0, 1) == "{");

    RPCConsole::RPCExecuteCommandLine(*node, result, "getblockchaininfo()");
    QVERIFY(result.substr(0, 1) == "{");

    // Whitespace at the end will be tolerated.
    RPCConsole::RPCExecuteCommandLine(*node, result, "getblockchaininfo ");
    QVERIFY(result.substr(0, 1) == "{");

    // Quote path identifier are allowed, but look after a child containing the
    // quotes in the key.
    (RPCConsole::RPCExecuteCommandLine(*node, result,
                                       "getblockchaininfo()[\"chain\"]"));
    QVERIFY(result == "null");

    // parameter not in brackets are allowed.
    (RPCConsole::RPCExecuteCommandLine(*node, result,
                                       "createrawtransaction [] {} 0"));
    // Parameter in brackets are allowed.
    (RPCConsole::RPCExecuteCommandLine(*node, result2,
                                       "createrawtransaction([],{},0)"));
    QVERIFY(result == result2);
    // Whitespace between parameters is allowed.
    (RPCConsole::RPCExecuteCommandLine(
        *node, result2, "createrawtransaction( [],  {} , 0   )"));
    QVERIFY(result == result2);

    RPCConsole::RPCExecuteCommandLine(
        *node, result, "getblock(getbestblockhash())[tx][0]", &filtered);
    QVERIFY(result ==
            "ccfeb2a869400272dc0562a210c05fb32de725092f5a2bc55f6999f2bb032792");
    QVERIFY(filtered == "getblock(getbestblockhash())[tx][0]");

    RPCConsole::RPCParseCommandLine(nullptr, result, "importprivkey", false,
                                    &filtered);
    QVERIFY(filtered == "importprivkey(…)");
    RPCConsole::RPCParseCommandLine(
        nullptr, result, "signmessagewithprivkey abc", false, &filtered);
    QVERIFY(filtered == "signmessagewithprivkey(…)");
    RPCConsole::RPCParseCommandLine(
        nullptr, result, "signmessagewithprivkey abc,def", false, &filtered);
    QVERIFY(filtered == "signmessagewithprivkey(…)");
    RPCConsole::RPCParseCommandLine(
        nullptr, result, "signrawtransactionwithkey(abc)", false, &filtered);
    QVERIFY(filtered == "signrawtransactionwithkey(…)");
    RPCConsole::RPCParseCommandLine(nullptr, result, "walletpassphrase(help())",
                                    false, &filtered);
    QVERIFY(filtered == "walletpassphrase(…)");
    RPCConsole::RPCParseCommandLine(
        nullptr, result,
        "walletpassphrasechange(help(walletpassphrasechange(abc)))", false,
        &filtered);
    QVERIFY(filtered == "walletpassphrasechange(…)");
    RPCConsole::RPCParseCommandLine(
        nullptr, result, "help(encryptwallet(abc, def))", false, &filtered);
    QVERIFY(filtered == "help(encryptwallet(…))");
    RPCConsole::RPCParseCommandLine(nullptr, result, "help(importprivkey())",
                                    false, &filtered);
    QVERIFY(filtered == "help(importprivkey(…))");
    RPCConsole::RPCParseCommandLine(
        nullptr, result, "help(importprivkey(help()))", false, &filtered);
    QVERIFY(filtered == "help(importprivkey(…))");
    RPCConsole::RPCParseCommandLine(
        nullptr, result, "help(importprivkey(abc), walletpassphrase(def))",
        false, &filtered);
    QVERIFY(filtered == "help(importprivkey(…), walletpassphrase(…))");

    RPCConsole::RPCExecuteCommandLine(*node, result, "rpcNestedTest");
    QVERIFY(result == "[]");
    RPCConsole::RPCExecuteCommandLine(*node, result, "rpcNestedTest ''");
    QVERIFY(result == "[\"\"]");
    RPCConsole::RPCExecuteCommandLine(*node, result, "rpcNestedTest \"\"");
    QVERIFY(result == "[\"\"]");
    RPCConsole::RPCExecuteCommandLine(*node, result, "rpcNestedTest '' abc");
    QVERIFY(result == "[\"\",\"abc\"]");
    RPCConsole::RPCExecuteCommandLine(*node, result,
                                      "rpcNestedTest abc '' abc");
    QVERIFY(result == "[\"abc\",\"\",\"abc\"]");
    RPCConsole::RPCExecuteCommandLine(*node, result, "rpcNestedTest abc  abc");
    QVERIFY(result == "[\"abc\",\"abc\"]");
    RPCConsole::RPCExecuteCommandLine(*node, result,
                                      "rpcNestedTest abc\t\tabc");
    QVERIFY(result == "[\"abc\",\"abc\"]");
    RPCConsole::RPCExecuteCommandLine(*node, result, "rpcNestedTest(abc )");
    QVERIFY(result == "[\"abc\"]");
    RPCConsole::RPCExecuteCommandLine(*node, result, "rpcNestedTest( abc )");
    QVERIFY(result == "[\"abc\"]");
    RPCConsole::RPCExecuteCommandLine(*node, result,
                                      "rpcNestedTest(   abc   ,   cba )");
    QVERIFY(result == "[\"abc\",\"cba\"]");

    // invalid syntax
    QVERIFY_EXCEPTION_THROWN(RPCConsole::RPCExecuteCommandLine(
                                 *node, result, "getblockchaininfo() .\n"),
                             std::runtime_error);

    // invalid syntax
    QVERIFY_EXCEPTION_THROWN(
        RPCConsole::RPCExecuteCommandLine(
            *node, result, "getblockchaininfo() getblockchaininfo()"),
        std::runtime_error);
    // tolerate non closing brackets if we have no arguments.
    (RPCConsole::RPCExecuteCommandLine(*node, result, "getblockchaininfo("));

    // tolerate non command brackts
    (RPCConsole::RPCExecuteCommandLine(*node, result,
                                       "getblockchaininfo()()()"));

    // invalid argument
    QVERIFY_EXCEPTION_THROWN(RPCConsole::RPCExecuteCommandLine(
                                 *node, result, "getblockchaininfo(True)"),
                             JSONRPCError);
    // method not found
    QVERIFY_EXCEPTION_THROWN(RPCConsole::RPCExecuteCommandLine(
                                 *node, result, "a(getblockchaininfo(True))"),
                             JSONRPCError);
    // don't tollerate empty arguments when using ,
    QVERIFY_EXCEPTION_THROWN(RPCConsole::RPCExecuteCommandLine(
                                 *node, result, "rpcNestedTest abc,,abc"),
                             std::runtime_error);
    // don't tollerate empty arguments when using ,
    QVERIFY_EXCEPTION_THROWN(RPCConsole::RPCExecuteCommandLine(
                                 *node, result, "rpcNestedTest(abc,,abc)"),
                             std::runtime_error);
    // don't tollerate empty arguments when using ,
    QVERIFY_EXCEPTION_THROWN(RPCConsole::RPCExecuteCommandLine(
                                 *node, result, "rpcNestedTest(abc,,)"),
                             std::runtime_error);
}
