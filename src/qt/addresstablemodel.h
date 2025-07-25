// Copyright (c) 2011-2015 The Fittexxcoin Core developers
// Copyright (c) 2021 The Fittexxcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <QAbstractTableModel>
#include <QStringList>

enum class OutputType;

class AddressTablePriv;
class WalletModel;

namespace interfaces {
class Wallet;
}

/**
 * Qt model of the address book in the core. This allows views to access and
 * modify the address book.
 */
class AddressTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit AddressTableModel(WalletModel *parent = nullptr);
    ~AddressTableModel();

    enum ColumnIndex {
        /** User specified label */
        Label = 0,
        /** Fittexxcoin address */
        Address = 1
    };

    enum RoleIndex {
        /** Type of address (#Send or #Receive) */
        TypeRole = Qt::UserRole
    };

    /** Return status of edit/insert operation */
    enum EditStatus {
        /** Everything ok */
        OK,
        /** No changes were made during edit operation */
        NO_CHANGES,
        /** Unparseable address */
        INVALID_ADDRESS,
        /** Address already in address book */
        DUPLICATE_ADDRESS,
        /** Wallet could not be unlocked to create new receiving address */
        WALLET_UNLOCK_FAILURE,
        /** Generating a new public key for a receiving address failed */
        KEY_GENERATION_FAILURE
    };

    /** Specifies send address */
    static const QString Send;
    /** Specifies receive address */
    static const QString Receive;

    /** @name Methods overridden from QAbstractTableModel
        @{*/
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value,
                 int role) override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role) const override;
    QModelIndex index(int row, int column,
                      const QModelIndex &parent) const override;
    bool removeRows(int row, int count,
                    const QModelIndex &parent = QModelIndex()) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    /*@}*/

    /* Add an address to the model.
       Returns the added address on success, and an empty string otherwise.
     */
    QString addRow(const QString &type, const QString &label,
                   const QString &address, const OutputType address_type);

    /**
     * Look up label for address in address book, if not found return empty
     * string.
     */
    QString labelForAddress(const QString &address) const;

    /**
     * Look up purpose for address in address book, if not found return empty
     * string.
     */
    QString purposeForAddress(const QString &address) const;

    /* Look up row index of an address in the model.
       Return -1 if not found.
     */
    int lookupAddress(const QString &address) const;

    EditStatus getEditStatus() const { return editStatus; }

    OutputType GetDefaultAddressType() const;

private:
    WalletModel *const walletModel;
    AddressTablePriv *priv = nullptr;
    QStringList columns;
    EditStatus editStatus = OK;

    /** Look up address book data given an address string. */
    bool getAddressData(const QString &address, std::string *name,
                        std::string *purpose) const;

    /** Notify listeners that data changed. */
    void emitDataChanged(int index);

public Q_SLOTS:
    /* Update address list from core.
     */
    void updateEntry(const QString &address, const QString &label, bool isMine,
                     const QString &purpose, int status);

    friend class AddressTablePriv;
};
