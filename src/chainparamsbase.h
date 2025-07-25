// Copyright (c) 2014-2015 The Fittexxcoin Core developers
// Copyright (c) 2017-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <memory>
#include <string>
#include <vector>

/**
 * CBaseChainParams defines the base parameters
 * (shared between fittexxcoin-cli and fittexxcoind)
 * of a given instance of the Fittexxcoin system.
 */
class CBaseChainParams {
public:
    /** BIP70 chain name strings (main, test or regtest) */
    static const std::string MAIN;
    static const std::string TESTNET;
    static const std::string TESTNET4;
    static const std::string SCALENET;
    static const std::string CHIPNET;
    static const std::string REGTEST;

    const std::string &DataDir() const { return strDataDir; }
    uint16_t RPCPort() const { return m_rpc_port; }
    uint16_t OnionServiceTargetPort() const { return m_onion_service_target_port; }

    CBaseChainParams() = delete;
    CBaseChainParams(const std::string &data_dir, uint16_t  rpc_port, uint16_t onion_service_target_port)
        : m_rpc_port(rpc_port), m_onion_service_target_port(onion_service_target_port), strDataDir(data_dir) {}

private:
    const uint16_t m_rpc_port;
    const uint16_t m_onion_service_target_port;
    std::string strDataDir;
};

/**
 * Creates and returns a std::unique_ptr<CBaseChainParams> of the chosen chain.
 * @returns a CBaseChainParams* of the chosen chain.
 * @throws a std::runtime_error if the chain is not supported.
 */
std::unique_ptr<CBaseChainParams>
CreateBaseChainParams(const std::string &chain);

/**
 * Set the arguments for chainparams.
 */
void SetupChainParamsBaseOptions();

/**
 * Return the currently selected parameters. This won't change after app
 * startup, except for unit tests.
 */
const CBaseChainParams &BaseParams();

/** Sets the params returned by Params() to those for the given network. */
void SelectBaseParams(const std::string &chain);
