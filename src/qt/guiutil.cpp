// Copyright (c) 2011-2016 The Fittexxcoin Core developers
// Copyright (c) 2020-2022 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/guiutil.h>

#include <cashaddrenc.h>
#include <chainparams.h>
#include <fs.h>
#include <interfaces/node.h>
#include <key_io.h>
#include <policy/policy.h>
#include <primitives/transaction.h>
#include <protocol.h>
#include <qt/fittexxcoinaddressvalidator.h>
#include <qt/fittexxcoinunits.h>
#include <qt/qvalidatedlineedit.h>
#include <qt/walletmodel.h>
#include <script/script.h>
#include <script/standard.h>
#include <util/strencodings.h>
#include <util/system.h>

#ifdef WIN32
#ifdef _WIN32_IE
#undef _WIN32_IE
#endif
#define _WIN32_IE 0x0501
#define WIN32_LEAN_AND_MEAN 1
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#endif

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDoubleValidator>
#include <QFileDialog>
#include <QFont>
#include <QKeyEvent>
#include <QLineEdit>
#include <QLocale>
#include <QMouseEvent>
#include <QSettings>
#include <QTextDocument> // for Qt::mightBeRichText
#include <QThread>
#include <QUrlQuery>

#if QT_VERSION >= 0x50200
#include <QFontDatabase>
#endif

#if defined(Q_OS_MAC)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include <CoreServices/CoreServices.h>

void ForceActivation();
#endif

namespace GUIUtil {

QString dateTimeStr(const QDateTime &date) {
    return date.date().toString(QLocale().dateFormat(QLocale::ShortFormat)) +
           QString(" ") + date.toString("hh:mm");
}

QString dateTimeStr(qint64 nTime) {
    return dateTimeStr(dateTimeFromTime(nTime));
}

QString dateTimeStrLong(const QDateTime &dateTime) {
    const QLocale loc;
    QString format = loc.dateTimeFormat(QLocale::FormatType::LongFormat);
    // To save a bit of space, prefer shorter day names for our long string format, e.g.:
    // "Thursday, January 21, 2021 9:36:29 AM EST" -> "Thu, January 21, 2021 9:36:29 AM EST"
    format.replace("dddd", "ddd");
    return loc.toString(dateTime, format);
}

QString dateTimeStrLong(qint64 nTime) {
    return dateTimeStrLong(dateTimeFromTime(nTime));
}

QDateTime dateTimeFromTime(qint64 nTime) {
    return QDateTime::fromMSecsSinceEpoch(nTime * 1000);
}

QFont fixedPitchFont() {
#if QT_VERSION >= 0x50200
    return QFontDatabase::systemFont(QFontDatabase::FixedFont);
#else
    QFont font("Monospace");
    font.setStyleHint(QFont::Monospace);
    return font;
#endif
}

static std::string MakeAddrInvalid(std::string addr,
                                   const CChainParams &params) {
    if (addr.size() < 2) {
        return "";
    }

    // Checksum is at the end of the address. Swapping chars to make it invalid.
    std::swap(addr[addr.size() - 1], addr[addr.size() - 2]);
    if (!IsValidDestinationString(addr, params)) {
        return addr;
    }

    return "";
}

std::string DummyAddress(const CChainParams &params) {
    // Just some dummy data to generate a convincing random-looking (but
    // consistent) address
    static const std::vector<uint8_t> dummydata = {
        0xeb, 0x15, 0x23, 0x1d, 0xfc, 0xeb, 0x60, 0x92, 0x58, 0x86,
        0xb6, 0x7d, 0x06, 0x52, 0x99, 0x92, 0x59, 0x15, 0xae, 0xb1};

    const CTxDestination dstKey = CKeyID(uint160(dummydata));
    return MakeAddrInvalid(EncodeCashAddr(dstKey, params), params);
}

// Addresses are stored in the database with the encoding that the client was
// configured with at the time of creation.
//
// This converts to cashaddr.
QString convertToCashAddr(const CChainParams &params, const QString &addr) {
    if (!IsValidDestinationString(addr.toStdString(), params)) {
        // We have something sketchy as input. Do not try to convert.
        return addr;
    }
    bool tokenAddr{};
    CTxDestination dst = DecodeDestination(addr.toStdString(), params, &tokenAddr);
    return QString::fromStdString(EncodeCashAddr(dst, params, tokenAddr));
}

void setupAddressWidget(QValidatedLineEdit *widget, QWidget *parent) {
    parent->setFocusProxy(widget);

    widget->setFont(fixedPitchFont());
    // We don't want translators to use own addresses in translations
    // and this is the only place, where this address is supplied.
    widget->setPlaceholderText(
        QObject::tr("Enter a Fittexxcoin address (e.g. %1)").arg(QString::fromStdString(DummyAddress(Params()))));
    widget->setValidator(new FittexxcoinAddressEntryValidator(parent));
    widget->setCheckValidator(new FittexxcoinAddressCheckValidator(parent));
}

bool parseFittexxcoinURI(const QString &scheme, const QUrl &uri,
                     SendCoinsRecipient *out) {
    // return if URI has wrong scheme.
    if (!uri.isValid() || uri.scheme() != scheme) {
        return false;
    }

    SendCoinsRecipient rv;
    rv.address = uri.scheme() + ":" + uri.path();

    // Trim any following forward slash which may have been added by the OS
    if (rv.address.endsWith("/")) {
        rv.address.truncate(rv.address.length() - 1);
    }
    rv.amount = Amount::zero();

    const QUrlQuery uriQuery(uri);
    for (auto [key, value] : uriQuery.queryItems()) {
        bool required = false;
        if (key.startsWith("req-")) {
            key.remove(0, 4);
            required = true;
        }

        if (key == "label") {
            rv.label = value;
        } else if (key == "message") {
            rv.message = value;
        } else if (key == "amount") {
            if (!value.isEmpty()) {
                const auto amount = FittexxcoinUnits::parse(FittexxcoinUnits::fxx, false, value);
                if (!amount) {
                    return false;
                }
                rv.amount = *amount;
            }
        } else if (required) {
            return false;
        }
    }
    if (out) {
        *out = rv;
    }
    return true;
}

bool parseFittexxcoinURI(const QString &scheme, QString uri,
                     SendCoinsRecipient *out) {
    //
    //    Cannot handle this later, because fittexxcoin://
    //    will cause Qt to see the part after // as host,
    //    which will lower-case it (and thus invalidate the address).
    if (uri.startsWith(scheme + "://", Qt::CaseInsensitive)) {
        uri.replace(0, scheme.length() + 3, scheme + ":");
    }
    QUrl uriInstance(uri);
    return parseFittexxcoinURI(scheme, uriInstance, out);
}

QString formatFittexxcoinURI(const SendCoinsRecipient &info) {
    return formatFittexxcoinURI(Params(), info);
}

QString formatFittexxcoinURI(const CChainParams &params,
                         const SendCoinsRecipient &info) {
    QString ret = convertToCashAddr(params, info.address);
    int paramCount = 0;

    if (info.amount != Amount::zero()) {
        ret +=
            QString("?amount=%1")
                .arg(FittexxcoinUnits::format(FittexxcoinUnits::fxx, info.amount, false,
                                          FittexxcoinUnits::separatorNever));
        paramCount++;
    }

    if (!info.label.isEmpty()) {
        QString lbl(QUrl::toPercentEncoding(info.label));
        ret += QString("%1label=%2").arg(paramCount == 0 ? "?" : "&").arg(lbl);
        paramCount++;
    }

    if (!info.message.isEmpty()) {
        QString msg(QUrl::toPercentEncoding(info.message));
        ret +=
            QString("%1message=%2").arg(paramCount == 0 ? "?" : "&").arg(msg);
        paramCount++;
    }

    return ret;
}

bool isDust(interfaces::Node &node, const QString &address, const Amount amount,
            const CChainParams &chainParams) {
    CTxDestination dest = DecodeDestination(address.toStdString(), chainParams);
    CScript script = GetScriptForDestination(dest);
    CTxOut txOut(amount, script);
    return IsDust(txOut, node.getDustRelayFee());
}

QString HtmlEscape(const QString &str, bool fMultiLine) {
    QString escaped = str.toHtmlEscaped();
    if (fMultiLine) {
        escaped = escaped.replace("\n", "<br>\n");
    }
    return escaped;
}

QString HtmlEscape(const std::string &str, bool fMultiLine) {
    return HtmlEscape(QString::fromStdString(str), fMultiLine);
}

void copyEntryData(QAbstractItemView *view, int column, int role) {
    if (!view || !view->selectionModel()) {
        return;
    }
    QModelIndexList selection = view->selectionModel()->selectedRows(column);

    if (!selection.isEmpty()) {
        // Copy first item
        setClipboard(selection.at(0).data(role).toString());
    }
}

QList<QModelIndex> getEntryData(QAbstractItemView *view, int column) {
    if (!view || !view->selectionModel()) {
        return QList<QModelIndex>();
    }
    return view->selectionModel()->selectedRows(column);
}

QString getSaveFileName(QWidget *parent, const QString &caption,
                        const QString &dir, const QString &filter,
                        QString *selectedSuffixOut) {
    QString selectedFilter;
    QString myDir;
    // Default to user documents location
    if (dir.isEmpty()) {
        myDir =
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    } else {
        myDir = dir;
    }
    /* Directly convert path to native OS path separators */
    QString result = QDir::toNativeSeparators(QFileDialog::getSaveFileName(
        parent, caption, myDir, filter, &selectedFilter));

    /* Extract first suffix from filter pattern "Description (*.foo)" or
     * "Description (*.foo *.bar ...) */
    QRegExp filter_re(".* \\(\\*\\.(.*)[ \\)]");
    QString selectedSuffix;
    if (filter_re.exactMatch(selectedFilter)) {
        selectedSuffix = filter_re.cap(1);
    }

    /* Add suffix if needed */
    QFileInfo info(result);
    if (!result.isEmpty()) {
        if (info.suffix().isEmpty() && !selectedSuffix.isEmpty()) {
            /* No suffix specified, add selected suffix */
            if (!result.endsWith(".")) {
                result.append(".");
            }
            result.append(selectedSuffix);
        }
    }

    /* Return selected suffix if asked to */
    if (selectedSuffixOut) {
        *selectedSuffixOut = selectedSuffix;
    }
    return result;
}

QString getOpenFileName(QWidget *parent, const QString &caption,
                        const QString &dir, const QString &filter,
                        QString *selectedSuffixOut) {
    QString selectedFilter;
    QString myDir;
    // Default to user documents location
    if (dir.isEmpty()) {
        myDir =
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    } else {
        myDir = dir;
    }
    /* Directly convert path to native OS path separators */
    QString result = QDir::toNativeSeparators(QFileDialog::getOpenFileName(
        parent, caption, myDir, filter, &selectedFilter));

    if (selectedSuffixOut) {
        /* Extract first suffix from filter pattern "Description (*.foo)" or
         * "Description (*.foo *.bar ...) */
        QRegExp filter_re(".* \\(\\*\\.(.*)[ \\)]");
        QString selectedSuffix;
        if (filter_re.exactMatch(selectedFilter)) {
            selectedSuffix = filter_re.cap(1);
        }
        *selectedSuffixOut = selectedSuffix;
    }
    return result;
}

Qt::ConnectionType blockingGUIThreadConnection() {
    if (QThread::currentThread() != qApp->thread()) {
        return Qt::BlockingQueuedConnection;
    } else {
        return Qt::DirectConnection;
    }
}

bool checkPoint(const QPoint &p, const QWidget *w) {
    QWidget *atW = QApplication::widgetAt(w->mapToGlobal(p));
    if (!atW) {
        return false;
    }
    return atW->topLevelWidget() == w;
}

bool isObscured(QWidget *w) {
    return !(checkPoint(QPoint(0, 0), w) &&
             checkPoint(QPoint(w->width() - 1, 0), w) &&
             checkPoint(QPoint(0, w->height() - 1), w) &&
             checkPoint(QPoint(w->width() - 1, w->height() - 1), w) &&
             checkPoint(QPoint(w->width() / 2, w->height() / 2), w));
}

void bringToFront(QWidget *w) {
#ifdef Q_OS_MAC
    ForceActivation();
#endif

    if (w) {
        // activateWindow() (sometimes) helps with keyboard focus on Windows
        if (w->isMinimized()) {
            w->showNormal();
        } else {
            w->show();
        }
        w->activateWindow();
        w->raise();
    }
}

void openDebugLogfile() {
    fs::path pathDebug = GetDataDir() / "debug.log";

    /* Open debug.log with the associated application */
    if (fs::exists(pathDebug)) {
        QDesktopServices::openUrl(
            QUrl::fromLocalFile(boostPathToQString(pathDebug)));
    }
}

bool openFittexxcoinConf() {
    fs::path pathConfig =
        GetConfigFile(gArgs.GetArg("-conf", FITTEXXCOIN_CONF_FILENAME));

    /* Create the file */
    fs::ofstream configFile(pathConfig, std::ios_base::app);

    if (!configFile.good()) {
        return false;
    }

    configFile.close();

    /* Open fittexxcoin.conf with the associated application */
    return QDesktopServices::openUrl(
        QUrl::fromLocalFile(boostPathToQString(pathConfig)));
}

QStringList splitSkipEmptyParts(const QString &s, const QString &separator) {
    return s.split(separator,
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
                   Qt::SkipEmptyParts
#else
                   QString::SkipEmptyParts
#endif
    );
}

ToolTipToRichTextFilter::ToolTipToRichTextFilter(int _size_threshold,
                                                 QObject *parent)
    : QObject(parent), size_threshold(_size_threshold) {}

bool ToolTipToRichTextFilter::eventFilter(QObject *obj, QEvent *evt) {
    if (evt->type() == QEvent::ToolTipChange) {
        QWidget *widget = static_cast<QWidget *>(obj);
        QString tooltip = widget->toolTip();
        if (tooltip.size() > size_threshold && !tooltip.startsWith("<qt") &&
            !Qt::mightBeRichText(tooltip)) {
            // Envelop with <qt></qt> to make sure Qt detects this as rich text
            // Escape the current message as HTML and replace \n by <br>
            tooltip = "<qt>" + HtmlEscape(tooltip, true) + "</qt>";
            widget->setToolTip(tooltip);
            return true;
        }
    }
    return QObject::eventFilter(obj, evt);
}

void TableViewLastColumnResizingFixer::connectViewHeadersSignals() {
    connect(tableView->horizontalHeader(), &QHeaderView::sectionResized, this,
            &TableViewLastColumnResizingFixer::on_sectionResized);
    connect(tableView->horizontalHeader(), &QHeaderView::geometriesChanged,
            this, &TableViewLastColumnResizingFixer::on_geometriesChanged);
}

// We need to disconnect these while handling the resize events, otherwise we
// can enter infinite loops.
void TableViewLastColumnResizingFixer::disconnectViewHeadersSignals() {
    disconnect(tableView->horizontalHeader(), &QHeaderView::sectionResized,
               this, &TableViewLastColumnResizingFixer::on_sectionResized);
    disconnect(tableView->horizontalHeader(), &QHeaderView::geometriesChanged,
               this, &TableViewLastColumnResizingFixer::on_geometriesChanged);
} // namespace GUIUtil

// Setup the resize mode, handles compatibility for Qt5 and below as the method
// signatures changed.
// Refactored here for readability.
void TableViewLastColumnResizingFixer::setViewHeaderResizeMode(
    int logicalIndex, QHeaderView::ResizeMode resizeMode) {
    tableView->horizontalHeader()->setSectionResizeMode(logicalIndex,
                                                        resizeMode);
}

void TableViewLastColumnResizingFixer::resizeColumn(int nColumnIndex,
                                                    int width) {
    tableView->setColumnWidth(nColumnIndex, width);
    tableView->horizontalHeader()->resizeSection(nColumnIndex, width);
}

int TableViewLastColumnResizingFixer::getColumnsWidth() {
    int nColumnsWidthSum = 0;
    for (int i = 0; i < columnCount; i++) {
        nColumnsWidthSum += tableView->horizontalHeader()->sectionSize(i);
    }
    return nColumnsWidthSum;
}

int TableViewLastColumnResizingFixer::getAvailableWidthForColumn(int column) {
    int nResult = lastColumnMinimumWidth;
    int nTableWidth = tableView->horizontalHeader()->width();

    if (nTableWidth > 0) {
        int nOtherColsWidth =
            getColumnsWidth() -
            tableView->horizontalHeader()->sectionSize(column);
        nResult = std::max(nResult, nTableWidth - nOtherColsWidth);
    }

    return nResult;
}

// Make sure we don't make the columns wider than the table's viewport width.
void TableViewLastColumnResizingFixer::adjustTableColumnsWidth() {
    disconnectViewHeadersSignals();
    resizeColumn(lastColumnIndex, getAvailableWidthForColumn(lastColumnIndex));
    connectViewHeadersSignals();

    int nTableWidth = tableView->horizontalHeader()->width();
    int nColsWidth = getColumnsWidth();
    if (nColsWidth > nTableWidth) {
        resizeColumn(secondToLastColumnIndex,
                     getAvailableWidthForColumn(secondToLastColumnIndex));
    }
}

// Make column use all the space available, useful during window resizing.
void TableViewLastColumnResizingFixer::stretchColumnWidth(int column) {
    disconnectViewHeadersSignals();
    resizeColumn(column, getAvailableWidthForColumn(column));
    connectViewHeadersSignals();
}

// When a section is resized this is a slot-proxy for ajustAmountColumnWidth().
void TableViewLastColumnResizingFixer::on_sectionResized(int logicalIndex,
                                                         int oldSize,
                                                         int newSize) {
    adjustTableColumnsWidth();
    int remainingWidth = getAvailableWidthForColumn(logicalIndex);
    if (newSize > remainingWidth) {
        resizeColumn(logicalIndex, remainingWidth);
    }
}

// When the table's geometry is ready, we manually perform the stretch of the
// "Message" column,
// as the "Stretch" resize mode does not allow for interactive resizing.
void TableViewLastColumnResizingFixer::on_geometriesChanged() {
    if ((getColumnsWidth() - this->tableView->horizontalHeader()->width()) !=
        0) {
        disconnectViewHeadersSignals();
        resizeColumn(secondToLastColumnIndex,
                     getAvailableWidthForColumn(secondToLastColumnIndex));
        connectViewHeadersSignals();
    }
}

/**
 * Initializes all internal variables and prepares the
 * the resize modes of the last 2 columns of the table and
 */
TableViewLastColumnResizingFixer::TableViewLastColumnResizingFixer(
    QTableView *table, int lastColMinimumWidth, int allColsMinimumWidth,
    QObject *parent)
    : QObject(parent), tableView(table),
      lastColumnMinimumWidth(lastColMinimumWidth),
      allColumnsMinimumWidth(allColsMinimumWidth) {
    columnCount = tableView->horizontalHeader()->count();
    lastColumnIndex = columnCount - 1;
    secondToLastColumnIndex = columnCount - 2;
    tableView->horizontalHeader()->setMinimumSectionSize(
        allColumnsMinimumWidth);
    setViewHeaderResizeMode(secondToLastColumnIndex, QHeaderView::Interactive);
    setViewHeaderResizeMode(lastColumnIndex, QHeaderView::Interactive);
}
#ifdef WIN32
static fs::path StartupShortcutPath() {
    /* Note that in order for the uninstaller to remove these on Windows,
       the *.lnk files mentioned here need to match exactly the filenames
       in the uninstaller NSIS script (see: cmake/modules/NSIS.template.in) */
    std::string chain = gArgs.GetChainName();
    if (chain == CBaseChainParams::MAIN) {
        return GetSpecialFolderPath(CSIDL_STARTUP) / "Fittexxcoin Node.lnk";
    }
    if (chain == CBaseChainParams::TESTNET) {
        return GetSpecialFolderPath(CSIDL_STARTUP) / "Fittexxcoin Node (testnet).lnk";
    }
    if (chain == CBaseChainParams::TESTNET4) {
        return GetSpecialFolderPath(CSIDL_STARTUP) / "Fittexxcoin Node (testnet4).lnk";
    }
    if (chain == CBaseChainParams::SCALENET) {
        return GetSpecialFolderPath(CSIDL_STARTUP) / "Fittexxcoin Node (scalenet).lnk";
    }
    if (chain == CBaseChainParams::CHIPNET) {
        return GetSpecialFolderPath(CSIDL_STARTUP) / "Fittexxcoin Node (chipnet).lnk";
    }
    return GetSpecialFolderPath(CSIDL_STARTUP) /
           strprintf("Fittexxcoin Node (%s).lnk", chain); // If we get here: "regtest"
}

bool GetStartOnSystemStartup() {
    // check for Fittexxcoin*.lnk
    return fs::exists(StartupShortcutPath());
}

bool SetStartOnSystemStartup(bool fAutoStart) {
    // If the shortcut exists already, remove it for updating
    fs::remove(StartupShortcutPath());

    if (fAutoStart) {
        CoInitialize(nullptr);

        // Get a pointer to the IShellLink interface.
        IShellLinkW *psl = nullptr;
        HRESULT hres =
            CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                             IID_IShellLinkW, reinterpret_cast<void **>(&psl));

        if (SUCCEEDED(hres)) {
            // Get the current executable path
            WCHAR pszExePath[MAX_PATH];
            GetModuleFileNameW(nullptr, pszExePath, ARRAYSIZE(pszExePath));

            // Start client minimized
            QString strArgs = "-min";
            // Set -testnet /-regtest options
            strArgs += QString::fromStdString(strprintf(
                " -testnet=%d -regtest=%d", gArgs.GetBoolArg("-testnet", false),
                gArgs.GetBoolArg("-regtest", false)));

            // Set the path to the shortcut target
            psl->SetPath(pszExePath);
            PathRemoveFileSpecW(pszExePath);
            psl->SetWorkingDirectory(pszExePath);
            psl->SetShowCmd(SW_SHOWMINNOACTIVE);
            psl->SetArguments(strArgs.toStdWString().c_str());

            // Query IShellLink for the IPersistFile interface for
            // saving the shortcut in persistent storage.
            IPersistFile *ppf = nullptr;
            hres = psl->QueryInterface(IID_IPersistFile,
                                       reinterpret_cast<void **>(&ppf));
            if (SUCCEEDED(hres)) {
                // Save the link by calling IPersistFile::Save.
                hres = ppf->Save(StartupShortcutPath().wstring().c_str(), TRUE);
                ppf->Release();
                psl->Release();
                CoUninitialize();
                return true;
            }
            psl->Release();
        }
        CoUninitialize();
        return false;
    }
    return true;
}
#elif defined(Q_OS_LINUX)

// Follow the Desktop Application Autostart Spec:
// http://standards.freedesktop.org/autostart-spec/autostart-spec-latest.html

static fs::path GetAutostartDir() {
    char *pszConfigHome = getenv("XDG_CONFIG_HOME");
    if (pszConfigHome) {
        return fs::path(pszConfigHome) / "autostart";
    }
    char *pszHome = getenv("HOME");
    if (pszHome) {
        return fs::path(pszHome) / ".config" / "autostart";
    }
    return fs::path();
}

static fs::path GetAutostartFilePath() {
    std::string chain = gArgs.GetChainName();
    if (chain == CBaseChainParams::MAIN) {
        return GetAutostartDir() / "fittexxcoin.desktop";
    }
    return GetAutostartDir() / strprintf("fittexxcoin-%s.lnk", chain);
}

bool GetStartOnSystemStartup() {
    fs::ifstream optionFile(GetAutostartFilePath());
    if (!optionFile.good()) {
        return false;
    }
    // Scan through file for "Hidden=true":
    std::string line;
    while (!optionFile.eof()) {
        getline(optionFile, line);
        if (line.find("Hidden") != std::string::npos &&
            line.find("true") != std::string::npos) {
            return false;
        }
    }
    optionFile.close();

    return true;
}

bool SetStartOnSystemStartup(bool fAutoStart) {
    if (!fAutoStart) {
        fs::remove(GetAutostartFilePath());
    } else {
        char pszExePath[MAX_PATH + 1];
        ssize_t r =
            readlink("/proc/self/exe", pszExePath, sizeof(pszExePath) - 1);
        if (r == -1) {
            return false;
        }
        pszExePath[r] = '\0';

        fs::create_directories(GetAutostartDir());

        fs::ofstream optionFile(GetAutostartFilePath(),
                                std::ios_base::out | std::ios_base::trunc);
        if (!optionFile.good()) {
            return false;
        }
        std::string chain = gArgs.GetChainName();
        // Write a fittexxcoin.desktop file to the autostart directory:
        optionFile << "[Desktop Entry]\n";
        optionFile << "Type=Application\n";
        if (chain == CBaseChainParams::MAIN) {
            optionFile << "Name=Fittexxcoin\n";
        } else {
            optionFile << strprintf("Name=Fittexxcoin (%s)\n", chain);
        }
        optionFile << "Exec=" << pszExePath
                   << strprintf(" -min -testnet=%d -regtest=%d\n",
                                gArgs.GetBoolArg("-testnet", false),
                                gArgs.GetBoolArg("-regtest", false));
        optionFile << "Terminal=false\n";
        optionFile << "Hidden=false\n";
        optionFile.close();
    }
    return true;
}

#elif defined(Q_OS_MAC)
// based on:
// https://github.com/Mozketo/LaunchAtLoginController/blob/master/LaunchAtLoginController.m

// NB: caller must release returned ref if it's not NULL
LSSharedFileListItemRef findStartupItemInList(LSSharedFileListRef list,
                                              CFURLRef findUrl);
LSSharedFileListItemRef findStartupItemInList(LSSharedFileListRef list,
                                              CFURLRef findUrl) {
    LSSharedFileListItemRef foundItem = nullptr;
    // loop through the list of startup items and try to find the fittexxcoin app
    CFArrayRef listSnapshot = LSSharedFileListCopySnapshot(list, nullptr);
    for (int i = 0; !foundItem && i < CFArrayGetCount(listSnapshot); ++i) {
        LSSharedFileListItemRef item =
            (LSSharedFileListItemRef)CFArrayGetValueAtIndex(listSnapshot, i);
        UInt32 resolutionFlags = kLSSharedFileListNoUserInteraction |
                                 kLSSharedFileListDoNotMountVolumes;
        CFURLRef currentItemURL = nullptr;

#if defined(MAC_OS_X_VERSION_MAX_ALLOWED) &&                                   \
    MAC_OS_X_VERSION_MAX_ALLOWED >= 10100
        if (&LSSharedFileListItemCopyResolvedURL) {
            currentItemURL = LSSharedFileListItemCopyResolvedURL(
                item, resolutionFlags, nullptr);
        }
#if defined(MAC_OS_X_VERSION_MIN_REQUIRED) &&                                  \
    MAC_OS_X_VERSION_MIN_REQUIRED < 10100
        else {
            LSSharedFileListItemResolve(item, resolutionFlags, &currentItemURL,
                                        nullptr);
        }
#endif
#else
        LSSharedFileListItemResolve(item, resolutionFlags, &currentItemURL,
                                    nullptr);
#endif

        if (currentItemURL && CFEqual(currentItemURL, findUrl)) {
            // found
            CFRetain(foundItem = item);
        }
        if (currentItemURL) {
            CFRelease(currentItemURL);
        }
    }
    CFRelease(listSnapshot);
    return foundItem;
}

bool GetStartOnSystemStartup() {
    CFURLRef fittexxcoinAppUrl = CFBundleCopyBundleURL(CFBundleGetMainBundle());
    LSSharedFileListRef loginItems = LSSharedFileListCreate(
        nullptr, kLSSharedFileListSessionLoginItems, nullptr);
    LSSharedFileListItemRef foundItem =
        findStartupItemInList(loginItems, fittexxcoinAppUrl);
    // findStartupItemInList retains the item it returned, need to release
    if (foundItem) {
        CFRelease(foundItem);
    }
    CFRelease(loginItems);
    CFRelease(fittexxcoinAppUrl);
    return foundItem;
}

bool SetStartOnSystemStartup(bool fAutoStart) {
    CFURLRef fittexxcoinAppUrl = CFBundleCopyBundleURL(CFBundleGetMainBundle());
    LSSharedFileListRef loginItems = LSSharedFileListCreate(
        nullptr, kLSSharedFileListSessionLoginItems, nullptr);
    LSSharedFileListItemRef foundItem =
        findStartupItemInList(loginItems, fittexxcoinAppUrl);

    if (fAutoStart && !foundItem) {
        // add fittexxcoin app to startup item list
        LSSharedFileListInsertItemURL(loginItems,
                                      kLSSharedFileListItemBeforeFirst, nullptr,
                                      nullptr, fittexxcoinAppUrl, nullptr, nullptr);
    } else if (!fAutoStart && foundItem) {
        // remove item
        LSSharedFileListItemRemove(loginItems, foundItem);
    }
    // findStartupItemInList retains the item it returned, need to release
    if (foundItem) {
        CFRelease(foundItem);
    }
    CFRelease(loginItems);
    CFRelease(fittexxcoinAppUrl);
    return true;
}
#pragma GCC diagnostic pop
#else

bool GetStartOnSystemStartup() {
    return false;
}
bool SetStartOnSystemStartup(bool fAutoStart) {
    return false;
}

#endif

void setClipboard(const QString &str) {
    QApplication::clipboard()->setText(str, QClipboard::Clipboard);
    QApplication::clipboard()->setText(str, QClipboard::Selection);
}

fs::path qstringToBoostPath(const QString &path) {
    return fs::path(path.toStdString());
}

QString boostPathToQString(const fs::path &path) {
    return QString::fromStdString(path.string());
}

QString formatDurationStr(int secs) {
    QStringList strList;
    int days = secs / 86400;
    int hours = (secs % 86400) / 3600;
    int mins = (secs % 3600) / 60;
    int seconds = secs % 60;

    if (days) {
        strList.append(QString(QObject::tr("%1 d")).arg(days));
    }
    if (hours) {
        strList.append(QString(QObject::tr("%1 h")).arg(hours));
    }
    if (mins) {
        strList.append(QString(QObject::tr("%1 m")).arg(mins));
    }
    if (seconds || (!days && !hours && !mins)) {
        strList.append(QString(QObject::tr("%1 s")).arg(seconds));
    }

    return strList.join(" ");
}

QString formatServicesStr(quint64 mask) {
    QStringList strList;

    // Don't display experimental service bits
    for (uint64_t check = 1; check <= NODE_LAST_NON_EXPERIMENTAL_SERVICE_BIT;
         check <<= 1) {
        if (mask & check) {
            switch (check) {
                case NODE_NETWORK:
                    strList.append("NETWORK");
                    break;
                case NODE_GETUTXO:
                    strList.append("GETUTXO");
                    break;
                case NODE_BLOOM:
                    strList.append("BLOOM");
                    break;
                case NODE_XTHIN:
                    strList.append("XTHIN");
                    break;
                case NODE_FITTEXXCOIN_CASH:
                    strList.append("CASH");
                    break;
                case NODE_GRAPHENE:
                    strList.append("GRAPHENE");
                    break;
                case NODE_CF:
                    strList.append("CF");
                    break;
                case NODE_NETWORK_LIMITED:
                    strList.append("LIMITED");
                    break;
                case NODE_EXTVERSION:
                    strList.append("EXTVERSION");
                    break;
                default:
                    strList.append(QString("%1[%2]").arg("UNKNOWN").arg(check));
            }
        }
    }

    if (strList.size()) {
        return strList.join(" & ");
    } else {
        return QObject::tr("None");
    }
}

QString formatPingTime(double dPingTime) {
    return (dPingTime >= double(std::numeric_limits<int64_t>::max()) / 1e6 ||
            dPingTime <= 0.)
               ? QObject::tr("N/A")
               : QString("%1 ms").arg(QString::number((int)(dPingTime * 1000), 10));
}

QString formatTimeOffset(int64_t nTimeOffset) {
    return QString("%1 s").arg(QString::number((int)nTimeOffset, 10));
}

QString formatNiceTimeOffset(qint64 secs) {
    // Represent time from last generated block in human readable text
    QString timeBehindText;
    const int HOUR_IN_SECONDS = 60 * 60;
    const int DAY_IN_SECONDS = 24 * 60 * 60;
    const int WEEK_IN_SECONDS = 7 * 24 * 60 * 60;
    // Average length of year in Gregorian calendar
    const int YEAR_IN_SECONDS = 31556952;
    if (secs < 60) {
        timeBehindText = QObject::tr("%n second(s)", "", secs);
    } else if (secs < 2 * HOUR_IN_SECONDS) {
        timeBehindText = QObject::tr("%n minute(s)", "", secs / 60);
    } else if (secs < 2 * DAY_IN_SECONDS) {
        timeBehindText = QObject::tr("%n hour(s)", "", secs / HOUR_IN_SECONDS);
    } else if (secs < 2 * WEEK_IN_SECONDS) {
        timeBehindText = QObject::tr("%n day(s)", "", secs / DAY_IN_SECONDS);
    } else if (secs < YEAR_IN_SECONDS) {
        timeBehindText = QObject::tr("%n week(s)", "", secs / WEEK_IN_SECONDS);
    } else {
        qint64 years = secs / YEAR_IN_SECONDS;
        qint64 remainder = secs % YEAR_IN_SECONDS;
        timeBehindText = QObject::tr("%1 and %2")
                             .arg(QObject::tr("%n year(s)", "", years))
                             .arg(QObject::tr("%n week(s)", "",
                                              remainder / WEEK_IN_SECONDS));
    }
    return timeBehindText;
}

QString formatBytes(uint64_t bytes) {
    if (bytes < 1'000) {
        return QString::number(bytes) + " B";
    }
    if (bytes < 1'000'000) {
        return QString::number(bytes / 1'000) + " kB";
    }
    if (bytes < 1'000'000'000) {
        return QString::number(bytes / 1'000'000) + QLocale().decimalPoint() + QString::number(bytes % 1'000'000 / 100'000) + " MB";
    }
    QString decimals = QString::number(bytes % 1'000'000'000 / 10'000'000);
    if (decimals.length() < 2) {
        decimals = QString("0") + decimals;
    }
    return QString::number(bytes / 1'000'000'000) + QLocale().decimalPoint() + decimals + " GB";
}

bool ClickableLabel::hasPixmap() const {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    return !pixmap(Qt::ReturnByValue).isNull();
#else
    return pixmap() != nullptr;
#endif
}

void ClickableLabel::mouseReleaseEvent(QMouseEvent *event) {
    Q_EMIT clicked(event->pos());
}

void ClickableProgressBar::mouseReleaseEvent(QMouseEvent *event) {
    Q_EMIT clicked(event->pos());
}

bool ItemDelegate::eventFilter(QObject *object, QEvent *event) {
    if (event->type() == QEvent::KeyPress) {
        if (static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
            Q_EMIT keyEscapePressed();
        }
    }
    return QItemDelegate::eventFilter(object, event);
}

int TextWidth(const QFontMetrics &fm, const QString &text) {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
    return fm.horizontalAdvance(text);
#else
    return fm.width(text);
#endif
}

} // namespace GUIUtil
