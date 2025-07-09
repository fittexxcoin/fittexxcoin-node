# Copyright (c) 2018 The Fittexxcoin developers

project(test_fittexxcoin-qt)

include(TestSuite)
create_test_suite(fittexxcoin-qt)
add_dependencies(check check-fittexxcoin-qt)

add_test_to_suite(fittexxcoin-qt test_fittexxcoin-qt
	apptests.cpp
	fittexxcoinaddressvalidatortests.cpp
	compattests.cpp
	guiutiltests.cpp
	rpcnestedtests.cpp
	test_main.cpp
	uritests.cpp
	util.cpp

	# Test framework
	../../test/setup_common.cpp
)

target_link_libraries(test_fittexxcoin-qt fittexxcoin-qt-base Qt5::Test)

# Wallet
if(BUILD_FITTEXXCOIN_WALLET)
	# Add wallet functionality to test_fittexxcoin-qt
	target_sources(test_fittexxcoin-qt
		PRIVATE
			addressbooktests.cpp
			wallettests.cpp
			../../wallet/test/wallet_test_fixture.cpp
	)

	if(ENABLE_BIP70)
		target_sources(test_fittexxcoin-qt
			PRIVATE
				paymentservertests.cpp
		)
	endif()

	target_link_libraries(test_fittexxcoin-qt wallet)
endif()
