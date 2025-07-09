// Copyright (c) 2017 The Fittexxcoin Developers
// Copyright (c) 2017-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/fittexxcoinaddressvalidator.h>
#include <qt/test/fittexxcoinaddressvalidatortests.h>

#include <chainparams.h>

#include <QValidator>

void FittexxcoinAddressValidatorTests::inputTests() {
    const auto params = CreateChainParams(CBaseChainParams::MAIN);
    FittexxcoinAddressEntryValidator v(nullptr);

    int unused = 0;
    QString in;

    // Empty string is intermediate.
    in = "";
    QVERIFY(v.validate(in, unused) == QValidator::Intermediate);

    // invalid base58 because of I, invalid cashaddr, currently considered valid
    // anyway.
    in = "BIIC";
    QVERIFY(v.validate(in, unused) == QValidator::Acceptable);

    // invalid base58, invalid cashaddr, currently considered valid anyway.
    in = "FITTEXXCOINH";
    QVERIFY(v.validate(in, unused) == QValidator::Acceptable);

    // invalid base58 because of I, but could be a cashaddr prefix
    in = "BITC";
    QVERIFY(v.validate(in, unused) == QValidator::Acceptable);

    // invalid base58, valid cashaddr
    in = "FITTEXXCOIN:QP";
    QVERIFY(v.validate(in, unused) == QValidator::Acceptable);

    // invalid base58, valid cashaddr, lower case
    in = "fittexxcoin:qp";
    QVERIFY(v.validate(in, unused) == QValidator::Acceptable);

    // invalid base58, valid cashaddr, mixed case
    in = "bItCoInCaSh:Qp";
    QVERIFY(v.validate(in, unused) == QValidator::Acceptable);

    // valid base58, invalid cash
    in = "BBBBBBBBBBBBBB";
    QVERIFY(v.validate(in, unused) == QValidator::Acceptable);

    // Only alphanumeric chars are accepted.
    in = "%";
    QVERIFY(v.validate(in, unused) == QValidator::Invalid);
}
