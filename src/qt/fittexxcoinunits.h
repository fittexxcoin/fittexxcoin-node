// Copyright (c) 2011-2016 The Fittexxcoin Core developers
// Copyright (c) 2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <amount.h>
#include <qt/fittexxcoinspaces.h>

#include <optional>

#include <QAbstractListModel>
#include <QString>

/**
 * Fittexxcoin unit definitions. Encapsulates parsing and formatting and serves as
 * list model for drop-down selection boxes.
 */
class FittexxcoinUnits : public QAbstractListModel {
    Q_OBJECT

public:
    explicit FittexxcoinUnits(QObject *parent);

    /**
     * Fittexxcoin units (Fittexxcoin unit work the same as Bitoin).
     * @note Source: https://en.fittexxcoin.it/wiki/Units.
     * Please add only sensible ones.
     */
    enum Unit { fxx, mfxx, ufxx, SAT };

    enum SeparatorStyle { separatorNever, separatorStandard, separatorAlways };

    //! @name Static API
    //! Unit conversion and formatting
    ///@{

    //! Get list of units, for drop-down box
    static QList<Unit> availableUnits();
    //! Is unit ID valid?
    static bool valid(int unit);
    //! Name
    static QString ticker(int unit);
    //! Longer description
    static QString description(int unit);
    //! Number of Satoshis (1e-8) per unit
    static qint64 factor(int unit);
    //! Number of decimals left
    static int decimals(int unit);
    //! Returns whether the locale-dependent decimal separator for currency amounts is a comma (true) or a dot (false).
    //! Use for output formatting only - input can accept both dot and comma.
    static bool decimalSeparatorIsComma();
    //! Format as string (locale-dependent decimal separator)
    static QString format(int unit, const Amount amount, bool plussign = false,
                          SeparatorStyle separators = separatorStandard);
    //! Format as string (with unit)
    static QString
    formatWithUnit(int unit, const Amount amount, bool plussign = false,
                   SeparatorStyle separators = separatorStandard);
    //! Format as HTML string (with unit)
    static QString
    formatHtmlWithUnit(int unit, const Amount amount, bool plussign = false,
                       SeparatorStyle separators = separatorStandard);
    //! Parse string to coin amount
    static std::optional<Amount> parse(int unit, bool allowComma, const QString& value);
    //! Gets title for amount column including current display unit if
    //! optionsModel reference available */
    static QString getAmountColumnTitle(int unit);
    ///@}

    //! @name AbstractListModel implementation
    //! List model for unit drop-down selection box.
    ///@{
    enum RoleIndex {
        /** Unit identifier */
        UnitRole = Qt::UserRole
    };
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    ///@}

    [[nodiscard]]
    static QString removeSpaces(QString text) {
        text.remove(' ');
        text.remove(FittexxcoinSpaces::thin);
        if constexpr (FittexxcoinSpaces::thin != FittexxcoinSpaces::realThin) {
            text.remove(FittexxcoinSpaces::realThin);
        }
        return text;
    }

private:
    QList<FittexxcoinUnits::Unit> unitlist;
};

typedef FittexxcoinUnits::Unit FittexxcoinUnit;
