#include "BatchQueueModel.hpp"

BatchQueueModel::BatchQueueModel(QObject *parent)
    : QAbstractListModel(parent) {}

int BatchQueueModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : rows_.size();
}

QVariant BatchQueueModel::data(const QModelIndex &index, int role) const {
    const int row = index.row();
    if (!index.isValid() || row < 0 || row >= rows_.size()) {
        return {};
    }
    const Row &r = rows_.at(row);
    switch (role) {
        case FileNameRole: return r.filePath;
        case StatusRole:   return r.status;
        case ProgressRole: return r.progress;
        case ErrorRole:    return r.error;
        default:           return {};
    }
}

QHash<int, QByteArray> BatchQueueModel::roleNames() const {
    return {
        {FileNameRole, "fileName"},
        {StatusRole,   "status"},
        {ProgressRole, "progress"},
        {ErrorRole,    "errorMessage"},
    };
}

void BatchQueueModel::setFiles(const QStringList &paths) {
    beginResetModel();
    rows_.clear();
    rows_.reserve(paths.size());
    for (const QString &p : paths) {
        rows_.append(Row{p});
    }
    endResetModel();
}

void BatchQueueModel::clear() {
    beginResetModel();
    rows_.clear();
    endResetModel();
}

QString BatchQueueModel::fileAt(int row) const {
    return rows_.value(row).filePath;
}

QString BatchQueueModel::statusAt(int row) const {
    return rows_.value(row).status;
}

QString BatchQueueModel::errorAt(int row) const {
    return rows_.value(row).error;
}

void BatchQueueModel::setRowStatus(int row, const QString &status) {
    if (row < 0 || row >= rows_.size()) {
        return;
    }
    rows_[row].status = status;
    emit dataChanged(index(row), index(row), {StatusRole});
}

void BatchQueueModel::updateProgress(int row, double progress) {
    if (row < 0 || row >= rows_.size()) {
        return;
    }
    rows_[row].progress = progress;
    if (progress >= 1.0) {
        rows_[row].status = QStringLiteral("done");
        emit dataChanged(index(row), index(row), {ProgressRole, StatusRole});
    } else {
        emit dataChanged(index(row), index(row), {ProgressRole});
    }
}

void BatchQueueModel::setRowError(int row, const QString &message) {
    if (row < 0 || row >= rows_.size()) {
        return;
    }
    rows_[row].status = QStringLiteral("failed");
    rows_[row].error = message;
    emit dataChanged(index(row), index(row), {StatusRole, ErrorRole});
}
