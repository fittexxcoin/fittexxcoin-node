// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Fittexxcoin Core developers
// Copyright (c) 2020-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <fs.h>

#include <cstdint>
#include <list>
#include <map>
#include <string>

#include <univalue.h>

//! HTTP status codes
enum HTTPStatusCode {
    HTTP_OK = 200,
    HTTP_BAD_REQUEST = 400,
    HTTP_UNAUTHORIZED = 401,
    HTTP_FORBIDDEN = 403,
    HTTP_NOT_FOUND = 404,
    HTTP_BAD_METHOD = 405,
    HTTP_INTERNAL_SERVER_ERROR = 500,
    HTTP_SERVICE_UNAVAILABLE = 503,
};

//! Fittexxcoin RPC error codes
enum RPCErrorCode {
    //! Standard JSON-RPC 2.0 errors
    // RPC_INVALID_REQUEST is internally mapped to HTTP_BAD_REQUEST (400).
    // It should not be used for application-layer errors.
    RPC_INVALID_REQUEST = -32600,
    // RPC_METHOD_NOT_FOUND is internally mapped to HTTP_NOT_FOUND (404).
    // It should not be used for application-layer errors.
    RPC_METHOD_NOT_FOUND = -32601,
    RPC_INVALID_PARAMS = -32602,
    // RPC_INTERNAL_ERROR should only be used for genuine errors in fittexxcoind
    // (for example datadir corruption).
    RPC_INTERNAL_ERROR = -32603,
    RPC_PARSE_ERROR = -32700,

    //! General application defined errors
    //! std::exception thrown in command handling
    RPC_MISC_ERROR = -1,
    //! Unexpected type was passed as parameter
    RPC_TYPE_ERROR = -3,
    //! Invalid address or key
    RPC_INVALID_ADDRESS_OR_KEY = -5,
    //! Ran out of memory during operation
    RPC_OUT_OF_MEMORY = -7,
    //! Invalid, missing or duplicate parameter
    RPC_INVALID_PARAMETER = -8,
    //! Database error
    RPC_DATABASE_ERROR = -20,
    //! Error parsing or validating structure in raw format
    RPC_DESERIALIZATION_ERROR = -22,
    //! General error during transaction or block submission
    RPC_VERIFY_ERROR = -25,
    //! Transaction or block was rejected by network rules
    RPC_VERIFY_REJECTED = -26,
    //! Transaction already in chain
    RPC_VERIFY_ALREADY_IN_CHAIN = -27,
    //! Client still warming up
    RPC_IN_WARMUP = -28,
    //! RPC method is deprecated
    RPC_METHOD_DEPRECATED = -32,
    //! RPC method or subsystem is disabled by node configuration
    RPC_METHOD_DISABLED = -90,
    //! RPC is disabled due to blockchain conditions (see software_outdated.cpp)
    RPC_DISABLED = -100,

    //! Aliases for backward compatibility
    RPC_TRANSACTION_ERROR = RPC_VERIFY_ERROR,
    RPC_TRANSACTION_REJECTED = RPC_VERIFY_REJECTED,
    RPC_TRANSACTION_ALREADY_IN_CHAIN = RPC_VERIFY_ALREADY_IN_CHAIN,

    //! P2P client errors
    //! Fittexxcoin is not connected
    RPC_CLIENT_NOT_CONNECTED = -9,
    //! Still downloading initial blocks
    RPC_CLIENT_IN_INITIAL_DOWNLOAD = -10,
    //! Node is already added
    RPC_CLIENT_NODE_ALREADY_ADDED = -23,
    //! Node has not been added before
    RPC_CLIENT_NODE_NOT_ADDED = -24,
    //! Node to disconnect not found in connected nodes
    RPC_CLIENT_NODE_NOT_CONNECTED = -29,
    //! Invalid IP/Subnet
    RPC_CLIENT_INVALID_IP_OR_SUBNET = -30,
    //! No valid connection manager instance found
    RPC_CLIENT_P2P_DISABLED = -31,

    //! Wallet errors
    //! Unspecified problem with wallet (key not found etc.)
    RPC_WALLET_ERROR = -4,
    //! Not enough funds in wallet or account
    RPC_WALLET_INSUFFICIENT_FUNDS = -6,
    //! Invalid label name
    RPC_WALLET_INVALID_LABEL_NAME = -11,
    //! Keypool ran out, call keypoolrefill first
    RPC_WALLET_KEYPOOL_RAN_OUT = -12,
    //! Enter the wallet passphrase with walletpassphrase first
    RPC_WALLET_UNLOCK_NEEDED = -13,
    //! The wallet passphrase entered was incorrect
    RPC_WALLET_PASSPHRASE_INCORRECT = -14,
    //! Command given in wrong wallet encryption state (encrypting an encrypted
    //! wallet etc.)
    RPC_WALLET_WRONG_ENC_STATE = -15,
    //! Failed to encrypt the wallet
    RPC_WALLET_ENCRYPTION_FAILED = -16,
    //! Wallet is already unlocked
    RPC_WALLET_ALREADY_UNLOCKED = -17,
    //! Invalid wallet specified
    RPC_WALLET_NOT_FOUND = -18,
    //! No wallet specified (error when there are multiple wallets loaded)
    RPC_WALLET_NOT_SPECIFIED = -19,
    //! Backwards compatible aliases
    RPC_WALLET_INVALID_ACCOUNT_NAME = RPC_WALLET_INVALID_LABEL_NAME,

    //! Unused reserved codes, kept around for backwards compatibility. Do not
    //! reuse.
    //! Server is in safe mode, and command is not allowed in safe mode
    RPC_FORBIDDEN_BY_SAFE_MODE = -2,
};

UniValue::Object JSONRPCRequestObj(std::string&& strMethod, UniValue&& params, UniValue&& id);
UniValue::Object JSONRPCReplyObj(UniValue&& result, UniValue&& error, UniValue&& id);
std::string JSONRPCReply(UniValue&& result, UniValue&& error, UniValue&& id);
/** JSON-RPC error that can be thrown as an exception. */
struct JSONRPCError {
    RPCErrorCode code;
    std::string message;
    JSONRPCError(RPCErrorCode _code, const std::string& _message) : code(_code), message(_message) {}
    JSONRPCError(RPCErrorCode _code, std::string&& _message) noexcept : code(_code), message(std::move(_message)) {}
    /** Converts the error to a UniValue::Object. Error message is moved. */
    UniValue::Object toObj() &&;
};

/** Generate a new RPC authentication cookie and write it to disk */
bool GenerateAuthCookie(std::string *cookie_out);
/** Read the RPC authentication cookie from disk */
bool GetAuthCookie(std::string *cookie_out);
/** Delete RPC authentication cookie from disk */
void DeleteAuthCookie();
/** Parse JSON-RPC batch reply into a vector */
std::vector<UniValue> JSONRPCProcessBatchReply(const UniValue &in, size_t num);
