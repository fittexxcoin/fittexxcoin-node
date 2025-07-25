// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Fittexxcoin Core developers
// Copyright (c) 2020-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/client.h>
#include <rpc/protocol.h>
#include <util/system.h>

#include <cstdint>
#include <set>

#include <univalue.h>

class CRPCConvertParam {
public:
    std::string methodName; //!< method whose params want conversion
    int paramIdx;           //!< 0-based idx of param to convert
    std::string paramName;  //!< parameter name
};

/**
 * Specify a (method, idx, name) here if the argument is a non-string RPC
 * argument and needs to be converted from JSON.
 *
 * @note Parameter indexes start from 0.
 */
static const CRPCConvertParam vRPCConvertParams[] = {
    {"setmocktime", 0, "timestamp"},
    {"generate", 0, "nblocks"},
    {"generate", 1, "maxtries"},
    {"generatetoaddress", 0, "nblocks"},
    {"generatetoaddress", 2, "maxtries"},
    {"getnetworkhashps", 0, "nblocks"},
    {"getnetworkhashps", 1, "height"},
    {"sendtoaddress", 1, "amount"},
    {"sendtoaddress", 4, "subtractfeefromamount"},
    {"sendtoaddress", 5, "coinsel"},
    {"sendtoaddress", 6, "include_unsafe"},
    {"settxfee", 0, "amount"},
    {"sethdseed", 0, "newkeypool"},
    {"getreceivedbyaddress", 1, "minconf"},
    {"getreceivedbylabel", 1, "minconf"},
    {"listreceivedbyaddress", 0, "minconf"},
    {"listreceivedbyaddress", 1, "include_empty"},
    {"listreceivedbyaddress", 2, "include_watchonly"},
    {"listreceivedbyaddress", 3, "address_filter"},
    {"listreceivedbylabel", 0, "minconf"},
    {"listreceivedbylabel", 1, "include_empty"},
    {"listreceivedbylabel", 2, "include_watchonly"},
    {"getbalance", 1, "minconf"},
    {"getbalance", 2, "include_watchonly"},
    {"getblockhash", 0, "height"},
    {"waitforblockheight", 0, "height"},
    {"waitforblockheight", 1, "timeout"},
    {"waitforblock", 1, "timeout"},
    {"waitfornewblock", 0, "timeout"},
    {"listtransactions", 1, "count"},
    {"listtransactions", 2, "skip"},
    {"listtransactions", 3, "include_watchonly"},
    {"walletpassphrase", 1, "timeout"},
    {"getblocktemplate", 0, "template_request"},
    {"getblocktemplatelight", 0, "template_request"},
    {"getblocktemplatelight", 1, "additional_txs"},
    {"listsinceblock", 1, "target_confirmations"},
    {"listsinceblock", 2, "include_watchonly"},
    {"listsinceblock", 3, "include_removed"},
    {"sendmany", 1, "amounts"},
    {"sendmany", 2, "minconf"},
    {"sendmany", 4, "subtractfeefrom"},
    {"sendmany", 5, "coinsel"},
    {"sendmany", 6, "include_unsafe"},
    {"scantxoutset", 1, "scanobjects"},
    {"addmultisigaddress", 0, "nrequired"},
    {"addmultisigaddress", 1, "keys"},
    {"createmultisig", 0, "nrequired"},
    {"createmultisig", 1, "keys"},
    {"listunspent", 0, "minconf"},
    {"listunspent", 1, "maxconf"},
    {"listunspent", 2, "addresses"},
    {"listunspent", 3, "include_unsafe"},
    {"listunspent", 4, "query_options"},
    {"getblock", 1, "verbosity"},
    {"getblock", 1, "verbose"},
    {"getblockheader", 0, "blockhash"},
    {"getblockheader", 0, "hash_or_height"},
    {"getblockheader", 1, "verbose"},
    {"getchaintxstats", 0, "nblocks"},
    {"gettransaction", 1, "include_watchonly"},
    {"getrawtransaction", 1, "verbose"},
    {"createrawtransaction", 0, "inputs"},
    {"createrawtransaction", 1, "outputs"},
    {"createrawtransaction", 2, "locktime"},
    {"signrawtransactionwithkey", 1, "privkeys"},
    {"signrawtransactionwithkey", 2, "prevtxs"},
    {"signrawtransactionwithwallet", 1, "prevtxs"},
    {"sendrawtransaction", 1, "allowhighfees"},
    {"testmempoolaccept", 0, "rawtxs"},
    {"testmempoolaccept", 1, "allowhighfees"},
    {"combinerawtransaction", 0, "txs"},
    {"fundrawtransaction", 1, "options"},
    {"walletcreatefundedpsbt", 0, "inputs"},
    {"walletcreatefundedpsbt", 1, "outputs"},
    {"walletcreatefundedpsbt", 2, "locktime"},
    {"walletcreatefundedpsbt", 3, "options"},
    {"walletcreatefundedpsbt", 4, "bip32derivs"},
    {"walletprocesspsbt", 1, "sign"},
    {"walletprocesspsbt", 3, "bip32derivs"},
    {"createpsbt", 0, "inputs"},
    {"createpsbt", 1, "outputs"},
    {"createpsbt", 2, "locktime"},
    {"combinepsbt", 0, "txs"},
    {"finalizepsbt", 1, "extract"},
    {"converttopsbt", 1, "permitsigdata"},
    {"gettxout", 1, "n"},
    {"gettxout", 2, "include_mempool"},
    {"gettxoutproof", 0, "txids"},
    {"lockunspent", 0, "unlock"},
    {"lockunspent", 1, "transactions"},
    {"importprivkey", 2, "rescan"},
    {"importaddress", 2, "rescan"},
    {"importaddress", 3, "p2sh"},
    {"importpubkey", 2, "rescan"},
    {"importmulti", 0, "requests"},
    {"importmulti", 1, "options"},
    {"verifychain", 0, "checklevel"},
    {"verifychain", 1, "nblocks"},
    {"getblockstats", 0, "hash_or_height"},
    {"getblockstats", 1, "stats"},
    {"pruneblockchain", 0, "height"},
    {"keypoolrefill", 0, "newsize"},
    {"getrawmempool", 0, "verbose"},
    {"estimatefee", 0, "nblocks"},
    {"prioritisetransaction", 1, "dummy"},
    {"prioritisetransaction", 2, "fee_delta"},
    {"setban", 2, "bantime"},
    {"setban", 3, "absolute"},
    {"clearbanned", 0, "manual"},
    {"clearbanned", 1, "automatic"},
    {"setnetworkactive", 0, "state"},
    {"getmempoolancestors", 1, "verbose"},
    {"getmempooldescendants", 1, "verbose"},
    {"disconnectnode", 1, "nodeid"},
    {"logging", 0, "include"},
    {"logging", 1, "exclude"},
    // double spend proof facility
    {"getdsproof", 0, "dspid"},
    {"getdsproof", 0, "txid"},
    {"getdsproof", 0, "outpoint"},
    {"getdsproof", 1, "verbosity"},
    {"getdsproof", 1, "verbose"},
    {"getdsproof", 2, "recursive"},
    {"getdsprooflist", 0, "verbosity"},
    {"getdsprooflist", 0, "verbose"},
    {"getdsprooflist", 1, "include_orphans"},
    // Echo with conversion (For testing only)
    {"echojson", 0, "arg0"},
    {"echojson", 1, "arg1"},
    {"echojson", 2, "arg2"},
    {"echojson", 3, "arg3"},
    {"echojson", 4, "arg4"},
    {"echojson", 5, "arg5"},
    {"echojson", 6, "arg6"},
    {"echojson", 7, "arg7"},
    {"echojson", 8, "arg8"},
    {"echojson", 9, "arg9"},
    {"rescanblockchain", 0, "start_height"},
    {"rescanblockchain", 1, "stop_height"},
    {"createwallet", 1, "disable_private_keys"},
    {"createwallet", 2, "blank"},
    {"getnodeaddresses", 0, "count"},
    { "addpeeraddress", 1, "port"},
    {"stop", 0, "wait"},
};

class CRPCConvertTable {
private:
    std::set<std::pair<std::string, int>> members;
    std::set<std::pair<std::string, std::string>> membersByName;

public:
    CRPCConvertTable();

    bool convert(const std::string &method, int idx) {
        return (members.count(std::make_pair(method, idx)) > 0);
    }
    bool convert(const std::string &method, const std::string &name) {
        return (membersByName.count(std::make_pair(method, name)) > 0);
    }
};

CRPCConvertTable::CRPCConvertTable() {
    const unsigned int n_elem =
        (sizeof(vRPCConvertParams) / sizeof(vRPCConvertParams[0]));

    for (unsigned int i = 0; i < n_elem; i++) {
        members.insert(std::make_pair(vRPCConvertParams[i].methodName,
                                      vRPCConvertParams[i].paramIdx));
        membersByName.insert(std::make_pair(vRPCConvertParams[i].methodName,
                                            vRPCConvertParams[i].paramName));
    }
}

static CRPCConvertTable rpcCvtTable;

UniValue::Array RPCConvertValues(const std::string &strMethod, const std::vector<std::string> &strParams) {
    UniValue::Array params;
    params.reserve(strParams.size());

    for (unsigned int idx = 0; idx < strParams.size(); idx++) {
        const std::string &strVal = strParams[idx];

        UniValue json_value;
        if (rpcCvtTable.convert(strMethod, idx) && json_value.read(strVal)) {
            // parse string as JSON, insert bool/number/object/etc. value
            params.push_back(std::move(json_value));
        } else {
            // insert string value directly
            params.emplace_back(strVal);
        }
    }

    return params;
}

UniValue::Object RPCConvertNamedValues(const std::string &strMethod, const std::vector<std::string> &strParams) {
    UniValue::Object params;
    params.reserve(strParams.size());

    for (const std::string &s : strParams) {
        size_t pos = s.find('=');
        if (pos == std::string::npos) {
            throw(std::runtime_error("No '=' in named argument '" + s +
                                     "', this needs to be present for every "
                                     "argument (even if it is empty)"));
        }

        std::string name = s.substr(0, pos);
        std::string value = s.substr(pos + 1);

        UniValue json_value;
        if (!rpcCvtTable.convert(strMethod, name) || !json_value.read(value)) {
            // insert string value directly
            params.emplace_back(std::move(name), std::move(value));
        } else {
            // parse string as JSON, insert bool/number/object/etc. value
            params.emplace_back(std::move(name), std::move(json_value));
        }
    }

    return params;
}
