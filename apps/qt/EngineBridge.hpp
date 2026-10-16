#pragma once

#include <atomic>

#include <QFutureWatcher>
#include <QObject>
#include <QString>

#include "clipforge/core/result.hpp"
#include "clipforge/services/engine_facade.hpp"

#include "BatchQueueModel.hpp"

class EngineBridge : public QObject {
    Q_OBJECT

public:
    explicit EngineBridge(QObject *parent = nullptr);
    ~EngineBridge() override;

    void setQueueModel(BatchQueueModel *model);
    [[nodiscard]] BatchQueueModel *queueModel() const noexcept { return model_; }

    Q_INVOKABLE void startBatch(
        const QString &inputDir,
        const QString &outputDir,
        const QString &operation,
        double targetDbfs,
        double thresholdDbfs,
        bool recursive,
        bool dryRun);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE QStringList scanWavFiles(const QString &inputDir, bool recursive) const;

signals:
    void fileProgress(int completed, int total, const QString &currentFile);
    void totalProgress(int completed, int total);
    void batchFinished(bool ok, const QString &message);
    void batchError(const QString &message);

private slots:
    void onWorkerProgress(std::size_t completed, std::size_t total, const QString &currentFile);
    void onWorkerFinished();

private:
    [[nodiscard]] static std::filesystem::path toFsPath(const QString &s);
    [[nodiscard]] static QString fromFsPath(const std::filesystem::path &p);
    [[nodiscard]] static QString normalize(const QString &p);

    BatchQueueModel *model_ = nullptr;
    QFutureWatcher<clipforge::Result<clipforge::services::BatchSummary>> watcher_;
    std::atomic<bool> cancelFlag_{false};
    QHash<QString, int> rowByPath_;
    int activeRow_ = -1;
    bool running_ = false;
};
