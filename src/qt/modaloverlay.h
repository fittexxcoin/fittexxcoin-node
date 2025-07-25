// Copyright (c) 2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <QDateTime>
#include <QWidget>

//! The required delta of headers to the estimated number of available headers
//! until we show the IBD progress
static constexpr int HEADER_HEIGHT_DELTA_SYNC = 24;

namespace Ui {
class ModalOverlay;
}

/** Modal overlay to display information about the chain-sync state */
class ModalOverlay : public QWidget {
    Q_OBJECT

public:
    explicit ModalOverlay(QWidget *parent);
    ~ModalOverlay();

public Q_SLOTS:
    void tipUpdate(int count, const QDateTime &blockDate,
                   double nVerificationProgress);
    void setKnownBestHeight(int count, const QDateTime &blockDate);

    void toggleVisibility();
    // will show or hide the modal layer
    void showHide(bool hide = false, bool userRequested = false);
    void closeClicked();
    bool isLayerVisible() const { return layerIsVisible; }

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;
    bool event(QEvent *ev) override;

private:
    Ui::ModalOverlay *ui;
    int bestHeaderHeight; // best known height (based on the headers)
    QDateTime bestHeaderDate;
    QVector<QPair<qint64, double>> blockProcessTime;
    bool layerIsVisible;
    bool userClosed;
};
