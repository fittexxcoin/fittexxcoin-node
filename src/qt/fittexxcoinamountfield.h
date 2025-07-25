// Copyright (c) 2011-2015 The Fittexxcoin Core developers
// Copyright (c) 2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <amount.h>

#include <QWidget>

class AmountSpinBox;

QT_BEGIN_NAMESPACE
class QValueComboBox;
QT_END_NAMESPACE

/** Widget for entering fittexxcoin amounts.
 */
class FittexxcoinAmountField : public QWidget {
    Q_OBJECT

    Q_PROPERTY(
        Amount value READ value WRITE setValue NOTIFY valueChanged USER true)

public:
    explicit FittexxcoinAmountField(QWidget *parent = nullptr);

    Amount value(bool *valid = nullptr) const;
    void setValue(const Amount value);

    /** Set single step in satoshis **/
    void setSingleStep(const Amount step);

    /** Make read-only **/
    void setReadOnly(bool fReadOnly);

    /** Mark current value as invalid in UI. */
    void setValid(bool valid);
    /** Perform input validation, mark field as invalid if entered value is not
     * valid. */
    bool validate();

    /** Change unit used to display amount. */
    void setDisplayUnit(int unit);

    /** Make field empty and ready for new input. */
    void clear();

    /** Enable/Disable. */
    void setEnabled(bool fEnabled);

    /** Qt messes up the tab chain by default in some cases (issue
       https://bugreports.qt-project.org/browse/QTBUG-10907),
        in these cases we have to set it up manually.
    */
    QWidget *setupTafxxain(QWidget *prev);

Q_SIGNALS:
    void valueChanged();

protected:
    /** Intercept focus-in event and decimal separator key presses */
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    AmountSpinBox *amount;
    QValueComboBox *unit;

private Q_SLOTS:
    void unitChanged(int idx);
};
