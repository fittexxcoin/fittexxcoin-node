#!/usr/bin/env bash
#
# Copyright (c) 2018 The Fittexxcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# Check for circular dependencies

export LC_ALL=C

EXPECTED_CIRCULAR_DEPENDENCIES=(
    "index/txindex -> validation -> index/txindex"
    "policy/policy -> validation -> policy/policy"
    "qt/addresstablemodel -> qt/walletmodel -> qt/addresstablemodel"
    "qt/bantablemodel -> qt/clientmodel -> qt/bantablemodel"
    "qt/fittexxcoingui -> qt/utilitydialog -> qt/fittexxcoingui"
    "qt/fittexxcoingui -> qt/walletframe -> qt/fittexxcoingui"
    "qt/fittexxcoingui -> qt/walletview -> qt/fittexxcoingui"
    "qt/clientmodel -> qt/peertablemodel -> qt/clientmodel"
    "qt/paymentserver -> qt/walletmodel -> qt/paymentserver"
    "qt/recentrequeststablemodel -> qt/walletmodel -> qt/recentrequeststablemodel"
    "qt/transactiontablemodel -> qt/walletmodel -> qt/transactiontablemodel"
    "qt/walletmodel -> qt/walletmodeltransaction -> qt/walletmodel"
    "txmempool -> validation -> txmempool"
    "validation -> validationinterface -> validation"
    "wallet/coincontrol -> wallet/wallet -> wallet/coincontrol"
    "wallet/fees -> wallet/wallet -> wallet/fees"
    "wallet/rpcwallet -> wallet/wallet -> wallet/rpcwallet"
    "wallet/wallet -> wallet/walletdb -> wallet/wallet"
    "qt/addressbookpage -> qt/fittexxcoingui -> qt/walletview -> qt/addressbookpage"
    "txmempool -> validation -> validationinterface -> txmempool"
    "qt/addressbookpage -> qt/fittexxcoingui -> qt/walletview -> qt/receivecoinsdialog -> qt/addressbookpage"
    "qt/addressbookpage -> qt/fittexxcoingui -> qt/walletview -> qt/signverifymessagedialog -> qt/addressbookpage"
    "qt/addressbookpage -> qt/fittexxcoingui -> qt/walletview -> qt/sendcoinsdialog -> qt/sendcoinsentry -> qt/addressbookpage"
    "chainparams -> protocol -> chainparams"
    "chainparamsbase -> util/system -> chainparamsbase"
    "script/scriptcache -> validation -> script/scriptcache"
    "seeder/fittexxcoin -> seeder/db -> seeder/fittexxcoin"
    "chainparams -> protocol -> config -> chainparams"
    "config -> policy/policy -> validation -> config"
    "config -> policy/policy -> validation -> protocol -> config"
    "psbt -> script/script_execution_context -> psbt"
)

EXIT_CODE=0

CIRCULAR_DEPENDENCIES=()

IFS=$'\n'
for CIRC in $(cd src && ../contrib/devtools/circular-dependencies.py {*,*/*,*/*/*}.{h,cpp} | sed -e 's/^Circular dependency: //'); do
    CIRCULAR_DEPENDENCIES+=("$CIRC")
    IS_EXPECTED_CIRC=0
    for EXPECTED_CIRC in "${EXPECTED_CIRCULAR_DEPENDENCIES[@]}"; do
        if [[ "${CIRC}" == "${EXPECTED_CIRC}" ]]; then
            IS_EXPECTED_CIRC=1
            break
        fi
    done
    if [[ ${IS_EXPECTED_CIRC} == 0 ]]; then
        echo "A new circular dependency in the form of \"${CIRC}\" appears to have been introduced."
        echo
        EXIT_CODE=1
    fi
done

for EXPECTED_CIRC in "${EXPECTED_CIRCULAR_DEPENDENCIES[@]}"; do
    IS_PRESENT_EXPECTED_CIRC=0
    for CIRC in "${CIRCULAR_DEPENDENCIES[@]}"; do
        if [[ "${CIRC}" == "${EXPECTED_CIRC}" ]]; then
            IS_PRESENT_EXPECTED_CIRC=1
            break
        fi
    done
    if [[ ${IS_PRESENT_EXPECTED_CIRC} == 0 ]]; then
        echo "Good job! The circular dependency \"${EXPECTED_CIRC}\" is no longer present."
        echo "Please remove it from EXPECTED_CIRCULAR_DEPENDENCIES in $0"
        echo "to make sure this circular dependency is not accidentally reintroduced."
        echo
        EXIT_CODE=1
    fi
done

exit ${EXIT_CODE}
