#!/usr/bin/env bash
#
# Copyright (c) 2018 The Fittexxcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# Make sure all shell scripts:
# a.) explicitly opt out of locale dependence using
#     "export LC_ALL=C" or "export LC_ALL=C.UTF-8", or
# b.) explicitly opt in to locale dependence using the annotation below.

export LC_ALL=C

EXIT_CODE=0
mapfile -d '' file_list < <(git ls-files -z -- '*.sh' '*.sh.in' ':!:src/secp256k1' ':!:src/univalue')
for SHELL_SCRIPT in "${file_list[@]}"; do
    if grep -q "# This script is intentionally locale dependent by not setting \"export LC_ALL=C\"" "${SHELL_SCRIPT}"; then
        continue
    fi
    FIRST_NON_COMMENT_LINE=$(grep -vE '^(#.*)?$' "${SHELL_SCRIPT}" | head -1)
    if [[ ${FIRST_NON_COMMENT_LINE} != "export LC_ALL=C" && ${FIRST_NON_COMMENT_LINE} != "export LC_ALL=C.UTF-8" ]]; then
        echo "Missing \"export LC_ALL=C\" (to avoid locale dependence) as first non-comment non-empty line in ${SHELL_SCRIPT}"
        EXIT_CODE=1
    fi
done
exit ${EXIT_CODE}
