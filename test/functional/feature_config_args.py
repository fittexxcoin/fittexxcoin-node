#!/usr/bin/env python3
# Copyright (c) 2017-2019 The Fittexxcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test various command line arguments and configuration file parameters."""
import os

from test_framework.test_framework import FittexxcoinTestFramework


class ConfArgsTest(FittexxcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def test_config_file_parser(self):
        # Assume node is stopped

        inc_conf_file_path = os.path.join(
            self.nodes[0].datadir, 'include.conf')
        with open(os.path.join(self.nodes[0].datadir, 'fittexxcoin.conf'), 'a', encoding='utf-8') as conf:
            conf.write('includeconf={}\n'.format(inc_conf_file_path))

        with open(inc_conf_file_path, 'w', encoding='utf-8') as conf:
            conf.write('-dash=1\n')
        self.nodes[0].assert_start_raises_init_error(
            expected_msg='Error reading configuration file: parse error on line 1: -dash=1, options in configuration file must be specified without leading -')

        with open(inc_conf_file_path, 'w', encoding='utf-8') as conf:
            conf.write('regtest=0\n')  # mainnet
            conf.write('acceptnonstdtxn=1\n')
        self.nodes[0].assert_start_raises_init_error(
            expected_msg='Error: acceptnonstdtxn is not currently supported for main chain')

        if self.is_wallet_compiled():
            with open(inc_conf_file_path, 'w', encoding='utf8') as conf:
                conf.write("wallet=foo\n")
            self.nodes[0].assert_start_raises_init_error(
                expected_msg='Error: Config setting for -wallet only applied on regtest network when in [regtest] section.')

        with open(inc_conf_file_path, 'w', encoding='utf-8') as conf:
            conf.write('nono\n')
        self.nodes[0].assert_start_raises_init_error(
            expected_msg='Error reading configuration file: parse error on line 1: nono, if you intended to specify a negated option, use nono=1 instead')

        with open(inc_conf_file_path, 'w', encoding='utf-8') as conf:
            conf.write('server=1\nrpcuser=someuser\nrpcpassword=some#pass')
        self.nodes[0].assert_start_raises_init_error(
            expected_msg='Error reading configuration file: parse error on line 3, using # in rpcpassword can be ambiguous and should be avoided')

        with open(inc_conf_file_path, 'w', encoding='utf-8') as conf:
            conf.write('server=1\nrpcuser=someuser\nmain.rpcpassword=some#pass')
        self.nodes[0].assert_start_raises_init_error(
            expected_msg='Error reading configuration file: parse error on line 3, using # in rpcpassword can be ambiguous and should be avoided')

        with open(inc_conf_file_path, 'w', encoding='utf-8') as conf:
            conf.write(
                'server=1\nrpcuser=someuser\n[main]\nrpcpassword=some#pass')
        self.nodes[0].assert_start_raises_init_error(
            expected_msg='Error reading configuration file: parse error on line 4, using # in rpcpassword can be ambiguous and should be avoided')

        with open(inc_conf_file_path, 'w', encoding='utf-8') as conf:
            conf.write('finalizeheaderspenalty=101\n')
        self.nodes[0].assert_start_raises_init_error(
            expected_msg='Error: Invalid header finalization penalty (DoS score) (101) - must be between 0 and 100')

        with open(inc_conf_file_path, 'w', encoding='utf-8') as conf:
            conf.write('finalizeheaderspenalty=-1\n')
        self.nodes[0].assert_start_raises_init_error(
            expected_msg='Error: Invalid header finalization penalty (DoS score) (-1) - must be between 0 and 100')

        inc_conf_file2_path = os.path.join(
            self.nodes[0].datadir, 'include2.conf')
        with open(os.path.join(self.nodes[0].datadir, 'fittexxcoin.conf'), 'a', encoding='utf-8') as conf:
            conf.write('includeconf={}\n'.format(inc_conf_file2_path))

        with open(inc_conf_file_path, 'w', encoding='utf-8') as conf:
            conf.write('testnot.datadir=1\n')
        with open(inc_conf_file2_path, 'w', encoding='utf-8') as conf:
            conf.write('[testnet]\n')
        self.restart_node(0)
        self.nodes[0].stop_node(
            expected_stderr='Warning: ' +
            inc_conf_file_path +
            ':1 Section [testnot] is not recognized.' +
            os.linesep +
            'Warning: ' +
            inc_conf_file2_path +
            ':1 Section [testnet] is not recognized.')

        with open(inc_conf_file_path, 'w', encoding='utf-8') as conf:
            conf.write('')  # clear
        with open(inc_conf_file2_path, 'w', encoding='utf-8') as conf:
            conf.write('')  # clear

    def run_test(self):
        self.stop_node(0)

        self.test_config_file_parser()

        # Remove the -datadir argument so it doesn't override the config file
        self.nodes[0].remove_default_args(["-datadir"])

        default_data_dir = self.nodes[0].datadir
        new_data_dir = os.path.join(default_data_dir, 'newdatadir')
        new_data_dir_2 = os.path.join(default_data_dir, 'newdatadir2')

        # Check that using -datadir argument on non-existent directory fails
        self.nodes[0].datadir = new_data_dir
        self.nodes[0].assert_start_raises_init_error(
            ['-datadir=' + new_data_dir], 'Error: Specified data directory "' + new_data_dir + '" does not exist.')

        # Check that using non-existent datadir in conf file fails
        conf_file = os.path.join(default_data_dir, "fittexxcoin.conf")

        # datadir needs to be set before [regtest] section
        conf_file_contents = open(conf_file, encoding='utf8').read()
        with open(conf_file, 'w', encoding='utf8') as f:
            f.write("datadir=" + new_data_dir + "\n")
            f.write(conf_file_contents)

        self.nodes[0].assert_start_raises_init_error(
            ['-conf=' + conf_file], 'Error reading configuration file: specified data directory "' + new_data_dir + '" does not exist.')

        # Create the directory and ensure the config file now works
        os.mkdir(new_data_dir)
        self.start_node(0, ['-conf=' + conf_file, '-wallet=w1'])
        self.stop_node(0)
        assert os.path.exists(os.path.join(new_data_dir, self.chain, 'wallets', 'w1'))

        # Ensure command line argument overrides datadir in conf
        os.mkdir(new_data_dir_2)
        self.nodes[0].datadir = new_data_dir_2
        self.start_node(0, ['-datadir=' + new_data_dir_2,
                            '-conf=' + conf_file, '-wallet=w2'])
        assert os.path.exists(os.path.join(new_data_dir_2, self.chain, 'wallets', 'w2'))


if __name__ == '__main__':
    ConfArgsTest().main()
