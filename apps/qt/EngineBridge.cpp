#include "EngineBridge.hpp"

#include <QtConcurrent>

#include <QtGlobal>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QMetaObject>

using namespace Qt::StringLiterals;

EngineBridge::EngineBridge(QObject *parent)
    : QObject(parent) {
    connect(&watcher_, &QFutureWatcher<clipforge::Result<clipforge::services::BatchSummary>>::finished,
            this, &EngineBridge::onWorkerFinished);
}

EngineBridge::~EngineBridge() {
    if (watcher_.isRunning()) {
        cancelFlag_.store(true, std::memory_order_relaxed);
        watcher_.waitForFinished();
    }
}

void EngineBridge::setQueueModel(BatchQueueModel *model) {
    model_ = model;
}

std::filesystem::path EngineBridge::toFsPath(const QString &s) {
#ifdef Q_OS_WIN
    return std::filesystem::path(s.toStdWString());
#else
    return std::filesystem::path(s.toStdString());
#endif
}

QString EngineBridge::fromFsPath(const std::filesystem::path &p) {
#ifdef Q_OS_WIN
    return QString::fromStdWString(p.native());
#else
    return QString::fromStdString(p.native());
#endif
}

QString EngineBridge::normalize(const QString &p) {
    return QDir::cleanPath(QFileInfo(p).absoluteFilePath());
}

int EngineBridge::lookupRow(const QString &path) const {
    const QString norm = normalize(path);
    int row = rowByPath_.value(norm, -1);
    if (row >= 0) {
        return row;
    }
    // Fallback for filesystem case-insensitivity and separator drift.
    return lowerRowByPath_.value(norm.toLower(), -1);
}

QStringList EngineBridge::scanWavFiles(const QString &inputDir, bool recursive) const {
    QStringList out;
    const auto flags = recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;
    QDirIterator it(inputDir, QDir::Files | QDir::NoDotAndDotDot, flags);
    while (it.hasNext()) {
        it.next();
        if (it.fileInfo().suffix().compare(u"wav"_s, Qt::CaseInsensitive) == 0) {
            out.append(normalize(it.filePath()));
        }
    }
    return out;
}

void EngineBridge::cancel() {
    cancelFlag_.store(true, std::memory_order_relaxed);
}

void EngineBridge::cancelAllPending() {
    if (model_ == nullptr) {
        return;
    }
    for (int r = 0; r < model_->rowCount(); ++r) {
        const QString st = model_->statusAt(r);
        if (st == u"pending"_s || st == u"active"_s) {
            model_->setRowStatus(r, u"cancelled"_s);
        }
    }
}

void EngineBridge::startBatch(
    const QString &inputDir,
    const QString &outputDir,
    const QString &operation,
    double targetDbfs,
    double thresholdDbfs,
    bool recursive,
    bool dryRun) {
    if (running_) {
        emit batchError(u"Batch already running"_s);
        return;
    }

    clipforge::services::BatchOperation op{};
    if (operation == u"normalize"_s) {
        op = clipforge::services::BatchOperation::Normalize;
    } else if (operation == u"trim"_s) {
        op = clipforge::services::BatchOperation::Trim;
    } else if (operation == u"both"_s || operation == u"normalize+trim"_s) {
        op = clipforge::services::BatchOperation::NormalizeAndTrim;
    } else {
        emit batchError(u"Unknown operation (expected normalize|trim|both)"_s);
        return;
    }

    const QStringList files = scanWavFiles(inputDir, recursive);
    rowByPath_.clear();
    lowerRowByPath_.clear();
    activeRow_ = -1;
    if (model_ != nullptr) {
        model_->setFiles(files);
        for (int r = 0; r < files.size(); ++r) {
            rowByPath_.insert(files.at(r), r);
            lowerRowByPath_.insert(files.at(r).toLower(), r);
        }
    }

    cancelFlag_.store(false, std::memory_order_relaxed);
    running_ = true;

    auto future = QtConcurrent::run([=, this]() -> clipforge::Result<clipforge::services::BatchSummary> {
        clipforge::services::BatchRequest req{};
        req.input_dir = toFsPath(inputDir);
        req.output_dir = toFsPath(outputDir);
        req.operation = op;
        req.recursive = recursive;
        req.dry_run = dryRun;
        req.normalize_options.target_dbfs = static_cast<float>(targetDbfs);
        req.trim_options.threshold_dbfs = static_cast<float>(thresholdDbfs);
        auto result = clipforge::services::EngineFacade::run_batch(
            req,
            [this](std::size_t completed, std::size_t total, std::string_view current) {
                const QString cur = QString::fromUtf8(current.data(), static_cast<int>(current.size()));
                QMetaObject::invokeMethod(this, [=] {
                    onWorkerProgress(completed, total, cur);
                }, Qt::QueuedConnection);
            },
            &cancelFlag_);
        return result;
    });
    watcher_.setFuture(future);
    if (future.isFinished()) {
        // setFuture on an already-finished future may not re-emit finished
        // on all Qt versions — deliver it ourselves (guarded by running_).
        QMetaObject::invokeMethod(this, &EngineBridge::onWorkerFinished, Qt::QueuedConnection);
    }
}

void EngineBridge::onWorkerProgress(std::size_t completed, std::size_t total, const QString &currentFile) {
    emit totalProgress(static_cast<int>(completed), static_cast<int>(total));
    if (currentFile.isEmpty()) {
        return;
    }
    emit fileProgress(static_cast<int>(completed), static_cast<int>(total), currentFile);
    if (model_ == nullptr) {
        return;
    }
    const int row = lookupRow(currentFile);
    if (row < 0) {
        return;
    }
    if (model_->statusAt(row) == u"pending"_s) {
        model_->setRowStatus(row, u"active"_s);
    }
    activeRow_ = row;
}

void EngineBridge::onWorkerFinished() {
    if (!running_) {
        return;
    }
    running_ = false;
    const bool wasCancelled = cancelFlag_.load(std::memory_order_relaxed);
    clipforge::Result<clipforge::services::BatchSummary> result;
    try {
        result = watcher_.result();
    } catch (const std::exception &ex) {
        const QString msg = QString::fromUtf8(ex.what());
        if (model_ != nullptr && activeRow_ >= 0) {
            model_->setRowError(activeRow_, msg);
        }
        emit batchError(msg);
        emit batchFinished(false, msg);
        return;
    } catch (...) {
        const QString msg = u"Worker thread failed with unknown exception"_s;
        emit batchError(msg);
        emit batchFinished(false, msg);
        return;
    }

    if (!result.has_value()) {
        const QString msg = QString::fromStdString(result.error().message);
        if (result.error().code == clipforge::ErrorCode::Cancelled) {
            cancelAllPending();
            emit batchFinished(false, u"Cancelled by user"_s);
        } else {
            if (model_ != nullptr && activeRow_ >= 0) {
                model_->setRowError(activeRow_, msg);
            }
            emit batchError(msg);
            emit batchFinished(false, msg);
        }
        return;
    }

    const auto &summary = result.value();
    std::size_t okCount = 0;
    std::size_t failCount = 0;
    bool markedCancelled = false;
    if (model_ != nullptr) {
        for (const auto &rec : summary.records) {
            const int row = lookupRow(fromFsPath(rec.input_path));
            if (row < 0) {
                continue;
            }
            if (rec.succeeded) {
                ++okCount;
                model_->setRowStatus(row, u"done"_s);
                model_->updateProgress(row, 1.0);
            } else {
                ++failCount;
                model_->setRowError(row, QString::fromStdString(rec.status_message));
            }
        }
        // Reconcile rows the summary did not cover (path-spelling drift).
        for (int r = 0; r < model_->rowCount(); ++r) {
            const QString st = model_->statusAt(r);
            if (st != u"pending"_s && st != u"active"_s) {
                continue;
            }
            if (wasCancelled) {
                model_->setRowStatus(r, u"cancelled"_s);
                markedCancelled = true;
            } else if (failCount == 0) {
                model_->setRowStatus(r, u"done"_s);
                model_->updateProgress(r, 1.0);
            } else {
                model_->setRowError(r, u"Finished without a record for this file"_s);
            }
        }
    }

    // A cancel that lands after the last file still reports success:
    // nothing was actually cancelled.
    if (markedCancelled) {
        emit batchFinished(false, u"Cancelled by user"_s);
    } else {
        emit batchFinished(true, QStringLiteral("%1 succeeded, %2 failed").arg(okCount).arg(failCount));
    }
}
