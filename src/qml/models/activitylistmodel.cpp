// Copyright (c) 2024 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qml/models/activitylistmodel.h>

#include <qml/models/walletqmlmodel.h>

ActivityListModel::ActivityListModel(WalletQmlModel *parent)
    : QAbstractListModel(parent)
    , m_wallet_model(parent)
{
    if (m_wallet_model != nullptr) {
        refreshWallet();
    }
}

int ActivityListModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_transactions.size();
}

void ActivityListModel::updateTransactionStatus(QSharedPointer<Transaction> tx) const
{
    if (m_wallet_model == nullptr) {
        return;
    }
    interfaces::WalletTxStatus wtx;
    int num_blocks;
    int64_t block_time;
    if (m_wallet_model->tryGetTxStatus(tx->hash, wtx, num_blocks, block_time)) {
        tx->updateStatus(wtx, num_blocks, block_time);
    } else {
        tx->status = Transaction::Status::NotAccepted;
    }
}

QVariant ActivityListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_transactions.size())
        return QVariant();

    QSharedPointer<Transaction> tx = m_transactions.at(index.row());
    updateTransactionStatus(tx);

    switch (role) {
    case AmountRole:
        return tx->prettyAmount();
    case AddressRole:
        return tx->address;
    case LabelRole:
        return tx->label;
    case DateTimeRole:
        return tx->dateTimeString();
    case StatusRole:
        return tx->status;
    case TypeRole:
        return tx->type;
    case DepthRole:
        return tx->depth;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> ActivityListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[AmountRole] = "amount";
    roles[AddressRole] = "address";
    roles[LabelRole] = "label";
    roles[DateTimeRole] = "date";
    roles[StatusRole] = "status";
    roles[TypeRole] = "type";
    roles[DepthRole] = "depth";
    return roles;
}

void ActivityListModel::refreshWallet()
{
    if (m_wallet_model == nullptr) {
        return;
    }
    for (const auto &tx : m_wallet_model->getWalletTxs()) {
        auto transactions = Transaction::fromWalletTx(tx);
        m_transactions.append(transactions);
        for (const auto &transaction : transactions) {
            updateTransactionStatus(transaction);
        }
    }
    std::sort(m_transactions.begin(), m_transactions.end(),
              [](const QSharedPointer<Transaction> &a, const QSharedPointer<Transaction> &b) {
                  return a->depth < b->depth;
              });
}

void ActivityListModel::updateTransaction(const uint256& hash, const interfaces::WalletTxStatus& wtx, int num_blocks, int64_t block_time)
{
    int index = findTransactionIndex(hash);
    if (index != -1) {
        QSharedPointer<Transaction> tx = m_transactions.at(index);
        tx->updateStatus(wtx, num_blocks, block_time);
        Q_EMIT dataChanged(this->index(index), this->index(index));
    }
}


int ActivityListModel::findTransactionIndex(const uint256& hash) const
{
    auto it = std::find_if(m_transactions.begin(), m_transactions.end(),
                           [&hash](const QSharedPointer<Transaction>& tx) {
                               return tx->hash == hash;
                           });
    if (it != m_transactions.end()) {
        return std::distance(m_transactions.begin(), it);
    }
    return -1;
}

void ActivityListModel::subsctribeToCoreSignals()
{
    // Connect signals to wallet
    m_handler_transaction_changed = m_wallet_model->handleTransactionChanged([this](const uint256& hash, ChangeType status) {
        interfaces::WalletTxStatus wtx;
        int num_blocks;
        int64_t block_time;
        if (m_wallet_model->tryGetTxStatus(hash, wtx, num_blocks, block_time)) {
            updateTransaction(hash, wtx, num_blocks, block_time);
        }
    });
}

void ActivityListModel::unsubscribeFromCoreSignals()
{
    // Disconnect signals from wallet
    if (m_handler_transaction_changed) {
        m_handler_transaction_changed->disconnect();
    }
}