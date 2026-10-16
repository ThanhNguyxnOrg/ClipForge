#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QStringList>

class BatchQueueModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        FileNameRole = Qt::UserRole + 1,
        StatusRole,
        ProgressRole,
        ErrorRole
    };

    explicit BatchQueueModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setFiles(const QStringList &paths);
    void clear();

    [[nodiscard]] QString fileAt(int row) const;
    [[nodiscard]] QString statusAt(int row) const;
    [[nodiscard]] QString errorAt(int row) const;

public slots:
    void setRowStatus(int row, const QString &status);
    void updateProgress(int row, double progress);
    void setRowError(int row, const QString &message);

private:
    struct Row {
        QString filePath;
        QString status = QStringLiteral("pending");
        double progress = 0.0;
        QString error;
    };

    QList<Row> rows_;
};
