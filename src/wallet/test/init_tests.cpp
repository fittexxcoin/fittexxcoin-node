// Copyright (c) 2018 The Fittexxcoin Core developers
// Copyright (c) 2020 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <init.h>
#include <noui.h>
#include <wallet/wallet.h>
#include <walletinitinterface.h>

#include <test/setup_common.h>
#include <wallet/test/init_test_fixture.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(init_tests, InitWalletDirTestingSetup)

BOOST_AUTO_TEST_CASE(walletinit_verify_walletdir_default) {
    SetWalletDir(m_walletdir_path_cases["default"]);
    bool result = m_chain_client->verify(Params());
    BOOST_CHECK(result == true);
    fs::path walletdir = gArgs.GetArg("-walletdir", "");
    fs::path expected_path = fs::canonical(m_walletdir_path_cases["default"]);
    BOOST_CHECK(walletdir == expected_path);
}

BOOST_AUTO_TEST_CASE(walletinit_verify_walletdir_custom) {
    SetWalletDir(m_walletdir_path_cases["custom"]);
    bool result = m_chain_client->verify(Params());
    BOOST_CHECK(result == true);
    fs::path walletdir = gArgs.GetArg("-walletdir", "");
    fs::path expected_path = fs::canonical(m_walletdir_path_cases["custom"]);
    BOOST_CHECK(walletdir == expected_path);
}

BOOST_AUTO_TEST_CASE(walletinit_verify_walletdir_does_not_exist) {
    SetWalletDir(m_walletdir_path_cases["nonexistent"]);
    noui_suppress();
    bool result = m_chain_client->verify(Params());
    noui_reconnect();
    BOOST_CHECK(result == false);
}

BOOST_AUTO_TEST_CASE(walletinit_verify_walletdir_is_not_directory) {
    SetWalletDir(m_walletdir_path_cases["file"]);
    noui_suppress();
    bool result = m_chain_client->verify(Params());
    noui_reconnect();
    BOOST_CHECK(result == false);
}

BOOST_AUTO_TEST_CASE(walletinit_verify_walletdir_is_not_relative) {
    SetWalletDir(m_walletdir_path_cases["relative"]);
    noui_suppress();
    bool result = m_chain_client->verify(Params());
    noui_reconnect();
    BOOST_CHECK(result == false);
}

BOOST_AUTO_TEST_CASE(walletinit_verify_walletdir_no_trailing) {
    SetWalletDir(m_walletdir_path_cases["trailing"]);
    bool result = m_chain_client->verify(Params());
    BOOST_CHECK(result == true);
    fs::path walletdir = gArgs.GetArg("-walletdir", "");
    fs::path expected_path = fs::canonical(m_walletdir_path_cases["default"]);
    BOOST_CHECK(walletdir == expected_path);
}

BOOST_AUTO_TEST_CASE(walletinit_verify_walletdir_no_trailing2) {
    SetWalletDir(m_walletdir_path_cases["trailing2"]);
    bool result = m_chain_client->verify(Params());
    BOOST_CHECK(result == true);
    fs::path walletdir = gArgs.GetArg("-walletdir", "");
    fs::path expected_path = fs::canonical(m_walletdir_path_cases["default"]);
    BOOST_CHECK(walletdir == expected_path);
}

BOOST_AUTO_TEST_SUITE_END()
