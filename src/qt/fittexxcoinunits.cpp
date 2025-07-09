// Copyright (c) 2011-2016 The Fittexxcoin Core developers
// Copyright (c) 2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/fittexxcoinunits.h>

#include <primitives/transaction.h>

#include <QLocale>
#include <QStringList>

FittexxcoinUnits::FittexxcoinUnits(QObject *parent)
    : QAbstractListModel(parent), unitlist(availableUnits()) {}

QList<FittexxcoinUnits::Unit> FittexxcoinUnits::availableUnits() {
    QList<FittexxcoinUnits::Unit> unitlist;
    unitlist.append(fxx);
    unitlist.append(mfxx);
    unitlist.append(ufxx);
    unitlist.append(SAT);
    return unitlist;
}

bool FittexxcoinUnits::valid(int unit) {
    switch (unit) {
        case fxx:
        case mfxx:
        case ufxx:
        case SAT:
            return true;
        default:
            return false;
    }
}

QString FittexxcoinUnits::ticker(int unit) {
    switch (unit) {
        case fxx:
            return QString("fxx");
        case mfxx:
            return QString("mfxx");
        case ufxx:
            return QString::fromUtf8("μfxx");
        case SAT:
            return QString("sat");
        default:
            return QString("???");
    }
}

QString FittexxcoinUnits::description(int unit) {
    constexpr auto thinUtf8 = FittexxcoinSpaces::thinUtf8;
    switch (unit) {
        case fxx:
            return QObject::tr("fittexxcoins");
        case mfxx:
            return QObject::tr("millifittexxcoins") + " (1 / 1" + thinUtf8 + "000)";
        case ufxx:
            return QObject::tr("microfittexxcoins/bits") + " (1 / 1" + thinUtf8 + "000" + thinUtf8 + "000)";
        case SAT:
            return QObject::tr("satoshis") + " (1 / 100" + thinUtf8 + "000" + thinUtf8 + "000)";
        default:
            return QString("???");
    }
}

qint64 FittexxcoinUnits::factor(int unit) {
    switch (unit) {
        case fxx:
            return 100000000;
        case mfxx:
            return 100000;
        case ufxx:
            return 100;
        case SAT:
            return 1;
        default:
            return 100000000;
    }
}

int FittexxcoinUnits::decimals(int unit) {
    switch (unit) {
        case fxx:
            return 8;
        case mfxx:
            return 5;
        case ufxx:
            return 2;
        case SAT:
            return 0;
        default:
            return 0;
    }
}

bool FittexxcoinUnits::decimalSeparatorIsComma() {
    // Considering that:
    // * fittexxcoin is an international currency;
    // * Fittexxcoin-Qt uses only spaces as group separator, as recommended by SI;
    // * Fittexxcoin-Qt traditionally displays amounts with the dot as decimal separator;
    // * Fittexxcoin-Qt traditionally accepts both dots and commas as decimal separators in input amounts;
    // * some locales use dots as group separator rather than decimal separator;
    // * some locales have different decimal separators for currency amounts and other numbers;
    // * one cannot retrieve the decimal separator for currency amounts from QLocale;
    // * decimal separators other than dot and comma are rare, especially on computers;
    // this function suggests the dot as decimal separator for displaying amounts,
    // with comma as the fallback if a reader could mistake the dot for a group separator.
    return QString(QLocale().groupSeparator()) == ".";
}

QString FittexxcoinUnits::format(int unit, const Amount nIn, bool fPlus,
                             SeparatorStyle separators) {
    // Note: not using straight sprintf here because we do NOT want
    // standard localized number formatting.
    if (!valid(unit)) {
        // Refuse to format invalid unit
        return QString();
    }
    qint64 n = qint64(nIn / SATOSHI);
    qint64 coin = factor(unit);
    int num_decimals = decimals(unit);
    qint64 n_abs = (n > 0 ? n : -n);
    qint64 quotient = n_abs / coin;
    QString quotient_str = QString::number(quotient);

    // Use SI-style thin space separators as these are locale independent and
    // can't be confused with the decimal marker.
    int q_size = quotient_str.size();
    if (separators == separatorAlways ||
        (separators == separatorStandard && q_size > 4)) {
        for (int i = 3; i < q_size; i += 3) {
            quotient_str.insert(q_size - i, FittexxcoinSpaces::thin);
        }
    }

    if (n < 0) {
        quotient_str.insert(0, '-');
    } else if (fPlus && n > 0) {
        quotient_str.insert(0, '+');
    }

    if (num_decimals > 0) {
        qint64 remainder = n_abs % coin;
        QString remainder_str =
            QString::number(remainder).rightJustified(num_decimals, '0');
        return quotient_str + (decimalSeparatorIsComma() ? ',' : '.') + remainder_str;
    } else {
        return quotient_str;
    }
}

// NOTE: Using formatWithUnit in an HTML context risks wrapping
// quantities at the thousands separator. More subtly, it also results
// in a standard space rather than a thin space, due to a bug in Qt's
// XML whitespace canonicalisation
//
// Please take care to use formatHtmlWithUnit instead, when
// appropriate.

QString FittexxcoinUnits::formatWithUnit(int unit, const Amount amount,
                                     bool plussign, SeparatorStyle separators) {
    return format(unit, amount, plussign, separators) + " " + ticker(unit);
}

QString FittexxcoinUnits::formatHtmlWithUnit(int unit, const Amount amount,
                                         bool plussign,
                                         SeparatorStyle separators) {
    QString str(formatWithUnit(unit, amount, plussign, separators));
    str.replace(FittexxcoinSpaces::thin, QString(FittexxcoinSpaces::thinHtml));
    return QString("<span style='white-space: nowrap;'>%1</span>").arg(str);
}

std::optional<Amount> FittexxcoinUnits::parse(int unit, bool allowComma, const QString& value) {
    if (!valid(unit) || value.isEmpty()) {
        // Refuse to parse invalid unit or empty string
        return std::nullopt;
    }
    const int num_decimals = decimals(unit);

    // Ignore spaces and thin spaces when parsing
    QString trimmed = removeSpaces(value);
    // If comma is allowed, accept both comma and dot as decimal separators
    if (allowComma) {
        trimmed.replace(',', '.');
    }
    const QStringList parts = trimmed.split('.');

    if (parts.size() > 2) {
        // More than one decimal separator
        return std::nullopt;
    }
    const QString& whole = parts[0];
    QString decimals;

    if (parts.size() > 1) {
        decimals = parts[1];
    }
    if (decimals.size() > num_decimals) {
        // Exceeds max precision
        return std::nullopt;
    }

    const QString str = whole + decimals.leftJustified(num_decimals, '0');
    if (str.size() > 18) {
        // Longer numbers will exceed 63 bits
        return std::nullopt;
    }
    bool ok = false;
    const int64_t sats = int64_t(str.toLongLong(&ok));
    if (!ok) {
        // String-to-integer conversion failed
        return std::nullopt;
    }
    return sats * SATOSHI;
}

QString FittexxcoinUnits::getAmountColumnTitle(int unit) {
    QString amountTitle = QObject::tr("Amount");
    if (FittexxcoinUnits::valid(unit)) {
        amountTitle += " (" + FittexxcoinUnits::ticker(unit) + ")";
    }
    return amountTitle;
}

int FittexxcoinUnits::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return unitlist.size();
}

QVariant FittexxcoinUnits::data(const QModelIndex &index, int role) const {
    int row = index.row();
    if (row >= 0 && row < unitlist.size()) {
        Unit unit = unitlist.at(row);
        switch (role) {
            case Qt::EditRole:
            case Qt::DisplayRole:
                return QVariant(ticker(unit));
            case Qt::ToolTipRole:
                return QVariant(description(unit));
            case UnitRole:
                return QVariant(static_cast<int>(unit));
        }
    }
    return QVariant();
}
