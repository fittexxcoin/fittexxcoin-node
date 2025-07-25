// Copyright (c) 2011-2016 The Fittexxcoin Core developers
// Copyright (c) 2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/fittexxcoin-config.h>
#endif

#include <qt/utilitydialog.h>

#include <clientversion.h>
#include <init.h>
#include <interfaces/node.h>
#include <qt/fittexxcoingui.h>
#include <qt/clientmodel.h>
#include <qt/forms/ui_helpmessagedialog.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/intro.h>
#ifdef ENABLE_BIP70
#include <qt/paymentrequestplus.h>
#endif
#include <util/system.h>

#include <QCloseEvent>
#include <QLabel>
#include <QRegExp>
#include <QTextCursor>
#include <QTextTable>
#include <QVBoxLayout>

#include <cstdio>

QString HelpMessageDialog::versionText() {
    return QString{PACKAGE_NAME} + " " + tr("version") + " " + QString::fromStdString(FormatFullVersion())
           // Also append build platform bits: so users can distinguish between 32-bit vs. 64-bit builds
           + " " + tr("(%1-bit)").arg(sizeof(void *) * 8);
}

/** "Help message" or "About" dialog box */
HelpMessageDialog::HelpMessageDialog(interfaces::Node &node, QWidget *parent,
                                     bool about)
    : QDialog(parent), ui(new Ui::HelpMessageDialog) {
    ui->setupUi(this);

    if (about) {
        setWindowTitle(tr("About %1").arg(PACKAGE_NAME));

        /// HTML-format the license message from the core
        QString licenseInfoHTML = QString::fromStdString(LicenseInfo());
        // Make URLs clickable
        QRegExp uri("<(.*)>", Qt::CaseSensitive, QRegExp::RegExp2);
        uri.setMinimal(true); // use non-greedy matching
        licenseInfoHTML.replace(uri, "<a href=\"\\1\">\\1</a>");
        // Replace newlines with HTML breaks
        licenseInfoHTML.replace("\n", "<br>");

        ui->aboutMessage->setTextFormat(Qt::RichText);
        ui->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        ui->aboutMessage->setText(versionText() + "<br><br>" + licenseInfoHTML);
        ui->aboutMessage->setWordWrap(true);
        ui->helpMessage->setVisible(false);
    } else {
        setWindowTitle(tr("Command-line options"));
        QTextCursor cursor(ui->helpMessage->document());
        cursor.insertText(versionText());
        cursor.insertBlock();
        cursor.insertText(headerText);
        cursor.insertBlock();

        QTextTableFormat tf;
        tf.setBorderStyle(QTextFrameFormat::BorderStyle_None);
        tf.setCellPadding(2);
        QVector<QTextLength> widths;
        widths << QTextLength(QTextLength::PercentageLength, 35);
        widths << QTextLength(QTextLength::PercentageLength, 65);
        tf.setColumnWidthConstraints(widths);

        QTextCharFormat bold;
        bold.setFontWeight(QFont::Bold);

        for (const QString &line : QString::fromStdString(gArgs.GetHelpMessage()).split("\n")) {
            if (line.startsWith("  -")) {
                cursor.currentTable()->appendRows(1);
                cursor.movePosition(QTextCursor::PreviousCell);
                cursor.movePosition(QTextCursor::NextRow);
                cursor.insertText(line.trimmed());
                cursor.movePosition(QTextCursor::NextCell);
            } else if (line.startsWith("   ")) {
                cursor.insertText(line.trimmed() + ' ');
            } else if (line.size() > 0) {
                // Title of a group
                if (cursor.currentTable()) {
                    cursor.currentTable()->appendRows(1);
                }
                cursor.movePosition(QTextCursor::Down);
                cursor.insertText(line.trimmed(), bold);
                cursor.insertTable(1, 2, tf);
            }
        }

        ui->helpMessage->moveCursor(QTextCursor::Start);
        ui->scrollArea->setVisible(false);
        ui->aboutLogo->setVisible(false);
    }
}

HelpMessageDialog::~HelpMessageDialog() {
    delete ui;
}

void HelpMessageDialog::on_okButton_accepted() {
    close();
}

/** "Shutdown" window */
ShutdownWindow::ShutdownWindow(QWidget *parent) : QWidget(parent) {
    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(new QLabel(
        tr("%1 is shutting down...").arg(PACKAGE_NAME) + "<br /><br />" +
        tr("Do not shut down the computer until this window disappears.")));
    setLayout(layout);
}

QWidget *ShutdownWindow::showShutdownWindow(FittexxcoinGUI *window) {
    if (!window) {
        return nullptr;
    }

    // Show a simple window indicating shutdown status
    QWidget *shutdownWindow = new ShutdownWindow();
    shutdownWindow->setWindowTitle(window->windowTitle());

    // Center shutdown window at where main window was
    const QPoint global = window->mapToGlobal(window->rect().center());
    shutdownWindow->move(global.x() - shutdownWindow->width() / 2,
                         global.y() - shutdownWindow->height() / 2);
    shutdownWindow->show();
    return shutdownWindow;
}

void ShutdownWindow::closeEvent(QCloseEvent *event) {
    event->ignore();
}
