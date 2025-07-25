// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Fittexxcoin Core developers
// Copyright (c) 2020-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/protocol.h>

#include <random.h>
#include <tinyformat.h>
#include <util/strencodings.h>
#include <util/system.h>
#include <util/time.h>
#include <version.h>

#include <cstdint>
#include <fstream>

/**
 * JSON-RPC protocol.  Fittexxcoin speaks version 1.0 for maximum compatibility, but
 * uses JSON-RPC 1.1/2.0 standards for parts of the 1.0 standard that were
 * unspecified (HTTP errors and contents of 'error').
 *
 * 1.0 spec: http://json-rpc.org/wiki/specification
 * 1.2 spec: http://jsonrpc.org/historical/json-rpc-over-http.html
 */

UniValue::Object JSONRPCRequestObj(std::string&& strMethod, UniValue&& params, UniValue&& id) {
    UniValue::Object request;
    request.reserve(3);
    request.emplace_back("method", std::move(strMethod));
    request.emplace_back("params", std::move(params));
    request.emplace_back("id", std::move(id));
    return request;
}

UniValue::Object JSONRPCReplyObj(UniValue&& result, UniValue&& error, UniValue&& id) {
    UniValue::Object reply;
    reply.reserve(3);
    if (!error.isNull()) {
        reply.emplace_back("result", UniValue::VNULL);
    } else {
        reply.emplace_back("result", std::move(result));
    }
    reply.emplace_back("error", std::move(error));
    reply.emplace_back("id", std::move(id));
    return reply;
}

std::string JSONRPCReply(UniValue&& result, UniValue&& error, UniValue&& id) {
    return UniValue::stringify(JSONRPCReplyObj(std::move(result), std::move(error), std::move(id))) + '\n';
}

UniValue::Object JSONRPCError::toObj() && {
    UniValue::Object error;
    error.reserve(2);
    error.emplace_back("code", code);
    error.emplace_back("message", std::move(message));
    return error;
}

/** Username used when cookie authentication is in use (arbitrary, only for
 * recognizability in debugging/logging purposes)
 */
static const std::string COOKIEAUTH_USER = "__cookie__";
/** Default name for auth cookie file */
static const std::string COOKIEAUTH_FILE = ".cookie";

/** Get name of RPC authentication cookie file */
static fs::path GetAuthCookieFile(bool temp = false) {
    std::string arg = gArgs.GetArg("-rpccookiefile", COOKIEAUTH_FILE);
    if (temp) {
        arg += ".tmp";
    }
    return AbsPathForConfigVal(fs::path(arg));
}

bool GenerateAuthCookie(std::string *cookie_out) {
    const size_t COOKIE_SIZE = 32;
    uint8_t rand_pwd[COOKIE_SIZE];
    GetRandBytes(rand_pwd, COOKIE_SIZE);
    std::string cookie = COOKIEAUTH_USER + ":" + HexStr(rand_pwd);

    /** the umask determines what permissions are used to create this file -
     * these are set to 077 in init.cpp unless overridden with -sysperms.
     */
    std::ofstream file;
    fs::path filepath_tmp = GetAuthCookieFile(true);
    file.open(filepath_tmp.string().c_str());
    if (!file.is_open()) {
        LogPrintf("Unable to open cookie authentication file %s for writing\n",
                  filepath_tmp.string());
        return false;
    }
    file << cookie;
    file.close();

    fs::path filepath = GetAuthCookieFile(false);
    if (!RenameOver(filepath_tmp, filepath)) {
        LogPrintf("Unable to rename cookie authentication file %s to %s\n",
                  filepath_tmp.string(), filepath.string());
        return false;
    }
    LogPrintf("Generated RPC authentication cookie %s\n", filepath.string());

    if (cookie_out) {
        *cookie_out = cookie;
    }
    return true;
}

bool GetAuthCookie(std::string *cookie_out) {
    std::ifstream file;
    std::string cookie;
    fs::path filepath = GetAuthCookieFile();
    file.open(filepath.string().c_str());
    if (!file.is_open()) {
        return false;
    }
    std::getline(file, cookie);
    file.close();

    if (cookie_out) {
        *cookie_out = cookie;
    }
    return true;
}

void DeleteAuthCookie() {
    try {
        fs::remove(GetAuthCookieFile());
    } catch (const fs::filesystem_error &e) {
        LogPrintf("%s: Unable to remove random auth cookie file: %s\n",
                  __func__, fsbridge::get_filesystem_error_message(e));
    }
}

std::vector<UniValue> JSONRPCProcessBatchReply(const UniValue &in, size_t num) {
    if (!in.isArray()) {
        throw std::runtime_error("Batch must be an array");
    }
    std::vector<UniValue> batch(num);
    for (size_t i = 0; i < in.size(); ++i) {
        const UniValue &rec = in[i];
        if (!rec.isObject()) {
            throw std::runtime_error("Batch member must be object");
        }
        size_t id = rec["id"].get_int();
        if (id >= num) {
            throw std::runtime_error("Batch member id larger than size");
        }
        batch[id] = rec;
    }
    return batch;
}
