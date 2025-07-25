# Copyright (c) 2018-2020 The Fittexxcoin developers

project(bench_fittexxcoin)

add_custom_command(
  OUTPUT
    ${CMAKE_CURRENT_BINARY_DIR}/data/block413567.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/data/block556034.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/data/coins_spent_413567.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/data/coins_spent_556034.cpp
  COMMAND
    ${CMAKE_CURRENT_SOURCE_DIR}/data/convert-raw-files.py ${CMAKE_CURRENT_SOURCE_DIR}/data ${CMAKE_CURRENT_BINARY_DIR}/data
  DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/data/block413567.raw.bz2
    ${CMAKE_CURRENT_SOURCE_DIR}/data/block556034.raw.bz2
    ${CMAKE_CURRENT_SOURCE_DIR}/data/coins_spent_413567.raw.bz2
    ${CMAKE_CURRENT_SOURCE_DIR}/data/coins_spent_556034.raw.bz2
    ${CMAKE_CURRENT_SOURCE_DIR}/data/convert-raw-files.py
)

add_executable(bench_fittexxcoin
	EXCLUDE_FROM_ALL
	banman.cpp
	addrman.cpp
	base58.cpp
	bench.cpp
	bench_fittexxcoin.cpp
	block_assemble.cpp
	cashaddr.cpp
	ccoins_caching.cpp
	chained_tx.cpp
	checkblock.cpp
	checkqueue.cpp
	crypto_aes.cpp
	crypto_hash.cpp
	${CMAKE_CURRENT_BINARY_DIR}/data/block413567.cpp
	${CMAKE_CURRENT_BINARY_DIR}/data/block556034.cpp
	${CMAKE_CURRENT_BINARY_DIR}/data/coins_spent_413567.cpp
	${CMAKE_CURRENT_BINARY_DIR}/data/coins_spent_556034.cpp
	dsproof.cpp
	duplicate_inputs.cpp
	examples.cpp
	gcs_filter.cpp
	lockedpool.cpp
	mempool_eviction.cpp
	merkle_root.cpp
	prevector.cpp
	removeforblock.cpp
	rollingbloom.cpp
	rpc_blockchain.cpp
	rpc_mempool.cpp
	json.cpp
	util_string.cpp
	util_time.cpp
	verify_script.cpp

	# TODO: make a test library
	../test/setup_common.cpp
	../test/util.cpp
)

target_link_libraries(bench_fittexxcoin common fittexxcoinconsensus server)

if(BUILD_FITTEXXCOIN_WALLET)
	target_sources(bench_fittexxcoin PRIVATE coin_selection.cpp keypool.cpp)
	target_link_libraries(bench_fittexxcoin wallet)
endif()

add_custom_target(bench-fittexxcoin COMMAND bench_fittexxcoin USES_TERMINAL)
add_custom_target(fittexxcoin-bench DEPENDS bench_fittexxcoin)
