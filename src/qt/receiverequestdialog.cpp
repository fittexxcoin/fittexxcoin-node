// Copyright (c) 2011-2016 The Fittexxcoin Core developers
// Copyright (c) 2017-2020 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/forms/ui_receiverequestdialog.h>
#include <qt/receiverequestdialog.h>

#include <qt/fittexxcoinunits.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/walletmodel.h>

#include <QClipboard>
#include <QDrag>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPixmap>

#if defined(HAVE_CONFIG_H)
#include <config/fittexxcoin-config.h> /* for USE_QRCODE */
#endif

#ifdef USE_QRCODE
#include <qrencode.h>
#endif

QRImageWidget::QRImageWidget(QWidget *parent)
    : QLabel(parent), contextMenu(nullptr) {
    contextMenu = new QMenu(this);
    QAction *saveImageAction = new QAction(tr("&Save Image..."), this);
    connect(saveImageAction, &QAction::triggered, this,
            &QRImageWidget::saveImage);
    contextMenu->addAction(saveImageAction);
    QAction *copyImageAction = new QAction(tr("&Copy Image"), this);
    connect(copyImageAction, &QAction::triggered, this,
            &QRImageWidget::copyImage);
    contextMenu->addAction(copyImageAction);
}

bool QRImageWidget::hasPixmap() const {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    return !pixmap(Qt::ReturnByValue).isNull();
#else
    return pixmap() != nullptr;
#endif
}

QImage QRImageWidget::exportImage() {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    return pixmap(Qt::ReturnByValue).toImage();
#else
    return hasPixmap() ? pixmap()->toImage() : QImage();
#endif
}

void QRImageWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && hasPixmap()) {
        event->accept();
        QMimeData *mimeData = new QMimeData;
        mimeData->setImageData(exportImage());

        QDrag *drag = new QDrag(this);
        drag->setMimeData(mimeData);
        drag->exec();
    } else {
        QLabel::mousePressEvent(event);
    }
}

void QRImageWidget::saveImage() {
    if (!hasPixmap()) {
        return;
    }
    QString fn = GUIUtil::getSaveFileName(this, tr("Save QR Code"), QString(),
                                          tr("PNG Image (*.png)"), nullptr);
    if (!fn.isEmpty()) {
        exportImage().save(fn);
    }
}

void QRImageWidget::copyImage() {
    if (!hasPixmap()) {
        return;
    }
    QApplication::clipboard()->setImage(exportImage());
}

void QRImageWidget::contextMenuEvent(QContextMenuEvent *event) {
    if (!hasPixmap()) {
        return;
    }
    contextMenu->exec(event->globalPos());
}

ReceiveRequestDialog::ReceiveRequestDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::ReceiveRequestDialog), model(nullptr) {
    ui->setupUi(this);

#ifndef USE_QRCODE
    ui->btnSaveAs->setVisible(false);
    ui->lblQRCode->setVisible(false);
#endif

    connect(ui->btnSaveAs, &QPushButton::clicked, ui->lblQRCode,
            &QRImageWidget::saveImage);
}

ReceiveRequestDialog::~ReceiveRequestDialog() {
    delete ui;
}

void ReceiveRequestDialog::setModel(WalletModel *_model) {
    this->model = _model;

    if (_model) {
        connect(_model->getOptionsModel(), &OptionsModel::displayUnitChanged,
                this, &ReceiveRequestDialog::update);
    }

    // update the display unit if necessary
    update();
}

void ReceiveRequestDialog::setInfo(const SendCoinsRecipient &_info) {
    this->info = _info;
    update();
}

void ReceiveRequestDialog::update() {
    if (!model) {
        return;
    }
    QString target = info.label;
    if (target.isEmpty()) {
        target = info.address;
    }
    setWindowTitle(tr("Request payment to %1").arg(target));

    QString uri = GUIUtil::formatFittexxcoinURI(info);
    ui->btnSaveAs->setEnabled(false);
    QString html;
    html += "<html><font face='verdana, arial, helvetica, sans-serif'>";
    html += "<b>" + tr("Payment information") + "</b><br>";
    html += "<b>" + tr("URI") + "</b>: ";
    html += "<a href=\"" + uri + "\">" + GUIUtil::HtmlEscape(uri) + "</a><br>";
    html += "<b>" + tr("Address") +
            "</b>: " + GUIUtil::HtmlEscape(info.address) + "<br>";
    if (info.amount != Amount::zero()) {
        html += "<b>" + tr("Amount") + "</b>: " +
                FittexxcoinUnits::formatHtmlWithUnit(
                    model->getOptionsModel()->getDisplayUnit(), info.amount) +
                "<br>";
    }
    if (!info.label.isEmpty()) {
        html += "<b>" + tr("Label") +
                "</b>: " + GUIUtil::HtmlEscape(info.label) + "<br>";
    }
    if (!info.message.isEmpty()) {
        html += "<b>" + tr("Message") +
                "</b>: " + GUIUtil::HtmlEscape(info.message) + "<br>";
    }
    if (model->isMultiwallet()) {
        html += "<b>" + tr("Wallet") +
                "</b>: " + GUIUtil::HtmlEscape(model->getWalletName()) + "<br>";
    }
    ui->outUri->setText(html);

#ifdef USE_QRCODE
    int fontSize = 10;

    ui->lblQRCode->setText("");
    if (!uri.isEmpty()) {
        // limit URI length
        if (uri.length() > MAX_URI_LENGTH) {
            ui->lblQRCode->setText(tr("Resulting URI too long, try to reduce "
                                      "the text for label / message."));
        } else {
            QRcode *code = QRcode_encodeString(uri.toUtf8().constData(), 0,
                                               QR_ECLEVEL_L, QR_MODE_8, 1);
            if (!code) {
                ui->lblQRCode->setText(tr("Error encoding URI into QR Code."));
                return;
            }
            QImage qrImage =
                QImage(code->width + 8, code->width + 8, QImage::Format_RGB32);
            qrImage.fill(0xffffff);
            uint8_t *p = code->data;
            for (int y = 0; y < code->width; y++) {
                for (int x = 0; x < code->width; x++) {
                    qrImage.setPixel(x + 4, y + 4, ((*p & 1) ? 0x0 : 0xffffff));
                    p++;
                }
            }
            QRcode_free(code);

            QImage qrAddrImage =
                QImage(QR_IMAGE_SIZE, QR_IMAGE_SIZE + 20, QImage::Format_RGB32);
            qrAddrImage.fill(0xffffff);
            QPainter painter(&qrAddrImage);
            painter.drawImage(0, 0,
                              qrImage.scaled(QR_IMAGE_SIZE, QR_IMAGE_SIZE));
            QFont font = GUIUtil::fixedPitchFont();
            font.setPixelSize(fontSize);
            painter.setFont(font);
            QRect paddedRect = qrAddrImage.rect();
            paddedRect.setHeight(QR_IMAGE_SIZE + 12);
            painter.drawText(paddedRect, Qt::AlignBottom | Qt::AlignCenter,
                             info.address);
            painter.end();

            ui->lblQRCode->setPixmap(QPixmap::fromImage(qrAddrImage));
            ui->btnSaveAs->setEnabled(true);
        }
    }
#endif
}

void ReceiveRequestDialog::on_btnCopyURI_clicked() {
    GUIUtil::setClipboard(GUIUtil::formatFittexxcoinURI(info));
}

void ReceiveRequestDialog::on_btnCopyAddress_clicked() {
    GUIUtil::setClipboard(info.address);
}
