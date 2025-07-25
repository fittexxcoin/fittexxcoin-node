// Copyright (c) 2011-2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <qt/paymentrequest.pb.h>
#pragma GCC diagnostic pop

#include <amount.h>
#include <script/script.h>

#include <openssl/x509.h>

#include <QByteArray>
#include <QList>
#include <QString>

/** Default for -allowselfsignedrootcertificates */
static constexpr bool DEFAULT_SELFSIGNED_ROOTCERTS = false;

//
// Wraps dumb protocol buffer paymentRequest with extra methods
//

class PaymentRequestPlus {
public:
    PaymentRequestPlus() {}

    bool parse(const QByteArray &data);
    bool SerializeToString(std::string *output) const;

    bool IsInitialized() const;
    // Returns true if merchant's identity is authenticated, and returns
    // human-readable merchant identity in merchant
    bool getMerchant(X509_STORE *certStore, QString &merchant) const;

    // Returns list of outputs, amount
    QList<std::pair<CScript, Amount>> getPayTo() const;

    const payments::PaymentDetails &getDetails() const { return details; }

private:
    payments::PaymentRequest paymentRequest;
    payments::PaymentDetails details;
};
