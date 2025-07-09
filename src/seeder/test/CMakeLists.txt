# Copyright (c) 2019 The Fittexxcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

project(fittexxcoin-seeder-test)

include(TestSuite)
create_test_suite(fittexxcoin-seeder)
add_dependencies(check check-fittexxcoin-seeder)

add_boost_unit_tests_to_suite(fittexxcoin-seeder test_fittexxcoin-seeder
	# TODO: make a test library
	../../test/setup_common.cpp

	TESTS
		p2p_messaging_tests.cpp
		parse_name_tests.cpp
		write_name_tests.cpp
)

target_link_libraries(test_fittexxcoin-seeder
	common
	fittexxcoinconsensus
	seeder
	server
)
