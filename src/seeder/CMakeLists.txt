# Copyright (c) 2017-2019 The Fittexxcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

project(fittexxcoin-seeder)

add_library(seeder fittexxcoin.cpp db.cpp dns.cpp)
target_link_libraries(seeder common fittexxcoinconsensus)

add_executable(fittexxcoin-seeder main.cpp)
target_link_libraries(fittexxcoin-seeder common fittexxcoinconsensus seeder)

include(BinaryTest)
add_to_symbols_check(fittexxcoin-seeder)
add_to_security_check(fittexxcoin-seeder)

include(InstallationHelper)
install_target(fittexxcoin-seeder)

add_subdirectory(test)
