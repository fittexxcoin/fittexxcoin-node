// Copyright (c) 2011-2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <amount.h>
#include <fs.h>

#include <QEvent>
#include <QHeaderView>
#include <QItemDelegate>
#include <QLabel>
#include <QMessageBox>
#include <QObject>
#include <QProgressBar>
#include <QString>
#include <QTableView>

class QValidatedLineEdit;
class SendCoinsRecipient;
class CChainParams;
class Config;

namespace interfaces {
class Node;
}

QT_BEGIN_NAMESPACE
class QAbstractItemView;
class QDateTime;
class QFont;
class QLineEdit;
class QUrl;
class QWidget;
QT_END_NAMESPACE

/**
 * Utility functions used by the Fittexxcoin Qt UI.
 */
namespace GUIUtil {

// Create short human-readable string from a QDateTime, e.g.: 8/1/17 13:00
QString dateTimeStr(const QDateTime &datetime);
// Like the above but takes a time value as a time_t equivalent (seconds since epoch)
QString dateTimeStr(qint64 nTime);

// Create a longer human-readable string from a QDateTime, e.g.: Thu, January 21, 2021 9:36:29 AM EST
QString dateTimeStrLong(const QDateTime &dateTime);
// Like the above but takes a time value as a time_t equivalent (seconds since epoch)
QString dateTimeStrLong(qint64 nTime);

// Returns a QDateTime object given a time_t equivalent (seconds sinch epoch)
QDateTime dateTimeFromTime(qint64 nTime);

// Return a monospace font
QFont fixedPitchFont();

// Generate an invalid, but convincing address.
std::string DummyAddress(const CChainParams &params);

// Convert any address into cashaddr
QString convertToCashAddr(const CChainParams &params, const QString &addr);

// Set up widget for address
void setupAddressWidget(QValidatedLineEdit *widget, QWidget *parent);

// Parse "fittexxcoin:" URI into recipient object, return true on successful
// parsing
bool parseFittexxcoinURI(const QString &scheme, const QUrl &uri,
                     SendCoinsRecipient *out);
bool parseFittexxcoinURI(const QString &scheme, QString uri,
                     SendCoinsRecipient *out);
QString formatFittexxcoinURI(const SendCoinsRecipient &info);
QString formatFittexxcoinURI(const CChainParams &params,
                         const SendCoinsRecipient &info);

// Returns true if given address+amount meets "dust" definition
bool isDust(interfaces::Node &node, const QString &address, const Amount amount,
            const CChainParams &chainParams);

// HTML escaping for rich text controls
QString HtmlEscape(const QString &str, bool fMultiLine = false);
QString HtmlEscape(const std::string &str, bool fMultiLine = false);

/** Copy a field of the currently selected entry of a view to the clipboard.
   Does nothing if nothing
    is selected.
   @param[in] column  Data column to extract from the model
   @param[in] role    Data role to extract from the model
   @see  TransactionView::copyLabel, TransactionView::copyAmount,
   TransactionView::copyAddress
 */
void copyEntryData(QAbstractItemView *view, int column,
                   int role = Qt::EditRole);

/** Return a field of the currently selected entry as a QString. Does nothing if
   nothing
    is selected.
   @param[in] column  Data column to extract from the model
   @see  TransactionView::copyLabel, TransactionView::copyAmount,
   TransactionView::copyAddress
 */
QList<QModelIndex> getEntryData(QAbstractItemView *view, int column);

void setClipboard(const QString &str);

/** Get save filename, mimics QFileDialog::getSaveFileName, except that it
  appends a default suffix
    when no suffix is provided by the user.

  @param[in] parent  Parent window (or 0)
  @param[in] caption Window caption (or empty, for default)
  @param[in] dir     Starting directory (or empty, to default to documents
  directory)
  @param[in] filter  Filter specification such as "Comma Separated Files
  (*.csv)"
  @param[out] selectedSuffixOut  Pointer to return the suffix (file type) that
  was selected (or 0).
              Can be useful when choosing the save file format based on suffix.
 */
QString getSaveFileName(QWidget *parent, const QString &caption,
                        const QString &dir, const QString &filter,
                        QString *selectedSuffixOut);

/** Get open filename, convenience wrapper for QFileDialog::getOpenFileName.

  @param[in] parent  Parent window (or 0)
  @param[in] caption Window caption (or empty, for default)
  @param[in] dir     Starting directory (or empty, to default to documents
  directory)
  @param[in] filter  Filter specification such as "Comma Separated Files
  (*.csv)"
  @param[out] selectedSuffixOut  Pointer to return the suffix (file type) that
  was selected (or 0).
              Can be useful when choosing the save file format based on suffix.
 */
QString getOpenFileName(QWidget *parent, const QString &caption,
                        const QString &dir, const QString &filter,
                        QString *selectedSuffixOut);

/** Get connection type to call object slot in GUI thread with invokeMethod. The
   call will be blocking.

   @returns If called from the GUI thread, return a Qt::DirectConnection.
            If called from another thread, return a
   Qt::BlockingQueuedConnection.
*/
Qt::ConnectionType blockingGUIThreadConnection();

// Determine whether a widget is hidden behind other windows
bool isObscured(QWidget *w);

// Activate, show and raise the widget
void bringToFront(QWidget *w);

// Open debug.log
void openDebugLogfile();

// Open the config file
bool openFittexxcoinConf();

// Split a QString using given separator, skipping the empty parts
QStringList splitSkipEmptyParts(const QString &s, const QString &separator);

/** Qt event filter that intercepts ToolTipChange events, and replaces the
 * tooltip with a rich text representation if needed.  This assures that Qt can
 * word-wrap long tooltip messages. Tooltips longer than the provided size
 * threshold (in characters) are wrapped.
 */
class ToolTipToRichTextFilter : public QObject {
    Q_OBJECT

public:
    explicit ToolTipToRichTextFilter(int size_threshold, QObject *parent = 0);

protected:
    bool eventFilter(QObject *obj, QEvent *evt) override;

private:
    int size_threshold;
};

/**
 * Makes a QTableView last column feel as if it was being resized from its left
 * border.
 * Also makes sure the column widths are never larger than the table's viewport.
 * In Qt, all columns are resizable from the right, but it's not intuitive
 * resizing the last column from the right.
 * Usually our second to last columns behave as if stretched, and when on
 * stretch mode, columns aren't resizable interactively or programmatically.
 *
 * This helper object takes care of this issue.
 *
 */
class TableViewLastColumnResizingFixer : public QObject {
    Q_OBJECT

public:
    TableViewLastColumnResizingFixer(QTableView *table, int lastColMinimumWidth,
                                     int allColsMinimumWidth, QObject *parent);
    void stretchColumnWidth(int column);

private:
    QTableView *tableView;
    int lastColumnMinimumWidth;
    int allColumnsMinimumWidth;
    int lastColumnIndex;
    int columnCount;
    int secondToLastColumnIndex;

    void adjustTableColumnsWidth();
    int getAvailableWidthForColumn(int column);
    int getColumnsWidth();
    void connectViewHeadersSignals();
    void disconnectViewHeadersSignals();
    void setViewHeaderResizeMode(int logicalIndex,
                                 QHeaderView::ResizeMode resizeMode);
    void resizeColumn(int nColumnIndex, int width);

private Q_SLOTS:
    void on_sectionResized(int logicalIndex, int oldSize, int newSize);
    void on_geometriesChanged();
};

bool GetStartOnSystemStartup();
bool SetStartOnSystemStartup(bool fAutoStart);

/* Convert QString to OS specific boost path through UTF-8 */
fs::path qstringToBoostPath(const QString &path);

/* Convert OS specific boost path to QString through UTF-8 */
QString boostPathToQString(const fs::path &path);

/* Convert seconds into a QString with days, hours, mins, secs */
QString formatDurationStr(int secs);

/* Format CNodeStats.nServices bitmask into a user-readable string */
QString formatServicesStr(quint64 mask);

/* Format a CNodeCombinedStats.dPingTime into a user-readable string or display
 * N/A, if 0*/
QString formatPingTime(double dPingTime);

/* Format a CNodeCombinedStats.nTimeOffset into a user-readable string. */
QString formatTimeOffset(int64_t nTimeOffset);

QString formatNiceTimeOffset(qint64 secs);

QString formatBytes(uint64_t bytes);

class ClickableLabel : public QLabel {
    Q_OBJECT

public:
    bool hasPixmap() const;

Q_SIGNALS:
    /** Emitted when the label is clicked. The relative mouse coordinates of the
     * click are passed to the signal.
     */
    void clicked(const QPoint &point);

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;
};

class ClickableProgressBar : public QProgressBar {
    Q_OBJECT

Q_SIGNALS:
    /** Emitted when the progressbar is clicked. The relative mouse coordinates
     * of the click are passed to the signal.
     */
    void clicked(const QPoint &point);

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;
};

typedef ClickableProgressBar ProgressBar;

class ItemDelegate : public QItemDelegate {
    Q_OBJECT
public:
    ItemDelegate(QObject *parent) : QItemDelegate(parent) {}

Q_SIGNALS:
    void keyEscapePressed();

private:
    bool eventFilter(QObject *object, QEvent *event);
};

/**
 * Returns the distance in pixels appropriate for drawing a subsequent character
 * after text.
 *
 * Before Qt 5.11, QFontMetrics::width() is used (but it is deprecated
 * since Qt 5.13.0). In Qt >= 5.11 QFontMetrics::horizontalAdvance() is used. 
 */
int TextWidth(const QFontMetrics &fm, const QString &text);
} // namespace GUIUtil
