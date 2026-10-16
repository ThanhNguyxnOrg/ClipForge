#include <QtTest>

#include <QSignalSpy>
#include <QTemporaryDir>

#include <QFile>

#include "BatchQueueModel.hpp"
#include "EngineBridge.hpp"

#include "clipforge/audio/audio_buffer.hpp"
#include "clipforge/audio/wav_writer.hpp"
#include "clipforge/services/engine_facade.hpp"

using namespace Qt::StringLiterals;

#include <cstdio>

// Bounded poll that does not depend on QSignalSpy::wait's nested loop:
// returns true as soon as the spy records the signal.
static bool waitFor(QSignalSpy &spy, int timeoutMs) {
    const int step = 100;
    int waited = 0;
    while (spy.isEmpty() && waited < timeoutMs) {
        QTest::qWait(step);
        waited += step;
    }
    return !spy.isEmpty();
}

namespace {

clipforge::audio::AudioBuffer makeToneBuffer() {
    clipforge::audio::WavFormatInfo fmt{};
    fmt.sample_format = clipforge::audio::SampleFormat::Float32;
    fmt.num_channels = 1;
    fmt.sample_rate = 1000;
    fmt.bits_per_sample = 32;
    fmt.block_align = 4;
    fmt.byte_rate = 4000;
    return clipforge::audio::AudioBuffer(fmt, std::vector<float>(100, 0.25f));
}

QString writeGoodWav(const QTemporaryDir &dir, const QString &name) {
    const QString path = dir.path() + u"/"_s + name;
    const auto status = clipforge::audio::WavWriter::write_file(
        std::filesystem::path(path.toStdWString()), makeToneBuffer());
    if (!status.has_value()) {
        return {};
    }
    return path;
}

} // namespace

class TstBridge : public QObject {
    Q_OBJECT

private slots:
    void init_empty_model() {
        BatchQueueModel model;
        QCOMPARE(model.rowCount(), 0);
    }

    void run_batch_synchronous() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(!writeGoodWav(dir, u"a.wav"_s).isEmpty());

        clipforge::services::BatchRequest req{};
        req.input_dir = std::filesystem::path(dir.path().toStdWString());
        req.output_dir = std::filesystem::path((dir.path() + u"/out"_s).toStdWString());
        req.operation = clipforge::services::BatchOperation::Normalize;
        req.dry_run = true;
        int callbacks = 0;
        auto res = clipforge::services::EngineFacade::run_batch(
            req,
            [&](std::size_t, std::size_t, std::string_view current) {
                ++callbacks;
                const QString cur = QString::fromUtf8(current.data(), static_cast<int>(current.size()));
                Q_UNUSED(cur);
            },
            nullptr);
        QVERIFY(res.has_value());
        QCOMPARE(res->succeeded_count, static_cast<std::size_t>(1));
        QVERIFY(callbacks >= 1);
    }

    void non_wav_ignored() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile txt(dir.path() + u"/notes.txt"_s);
        QVERIFY(txt.open(QIODevice::WriteOnly));
        txt.write("not audio");
        txt.close();
        const QString wav = writeGoodWav(dir, u"a.wav"_s);
        QVERIFY(!wav.isEmpty());

        BatchQueueModel model;
        EngineBridge bridge;
        bridge.setQueueModel(&model);
        QSignalSpy finished(&bridge, &EngineBridge::batchFinished);
        bridge.startBatch(dir.path(), dir.path() + u"/out"_s, u"normalize"_s, -1.0, -45.0, false, true);
        QVERIFY(waitFor(finished, 30000));
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.fileAt(0), wav);
    }

    void failed_file_continues() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(!writeGoodWav(dir, u"good1.wav"_s).isEmpty());
        QFile bad(dir.path() + u"/bad.wav"_s);
        QVERIFY(bad.open(QIODevice::WriteOnly));
        bad.write("RIFFxxxxNOTAWAVEFILE________________");
        bad.close();
        QVERIFY(!writeGoodWav(dir, u"good2.wav"_s).isEmpty());

        BatchQueueModel model;
        EngineBridge bridge;
        bridge.setQueueModel(&model);
        QSignalSpy finished(&bridge, &EngineBridge::batchFinished);
        bridge.startBatch(dir.path(), dir.path() + u"/out"_s, u"normalize"_s, -1.0, -45.0, false, true);
        QVERIFY(waitFor(finished, 30000));
        QCOMPARE(finished.at(0).at(0).toBool(), true);
        QCOMPARE(model.rowCount(), 3);
        int done = 0;
        int failed = 0;
        for (int r = 0; r < 3; ++r) {
            const QString st = model.statusAt(r);
            if (st == u"done"_s) {
                ++done;
            } else if (st == u"failed"_s) {
                ++failed;
                QVERIFY(!model.errorAt(r).isEmpty());
            }
        }
        QCOMPARE(done, 2);
        QCOMPARE(failed, 1);
    }

    void cancel_marks_rows_cancelled() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        for (int i = 0; i < 200; ++i) {
            QVERIFY(!writeGoodWav(dir, QStringLiteral("f%1.wav").arg(i, 3, 10, QChar(u'0'))).isEmpty());
        }

        BatchQueueModel model;
        EngineBridge bridge;
        bridge.setQueueModel(&model);
        QSignalSpy finished(&bridge, &EngineBridge::batchFinished);
        bool cancelled = false;
        QObject::connect(&bridge, &EngineBridge::fileProgress, &bridge,
                         [&](int, int, const QString &) {
                             if (!cancelled) {
                                 cancelled = true;
                                 bridge.cancel();
                             }
                         });
        bridge.startBatch(dir.path(), dir.path() + u"/out"_s, u"normalize"_s, -1.0, -45.0, false, true);
        QVERIFY(waitFor(finished, 60000));
        QCOMPARE(model.rowCount(), 200);
        int pending = 0;
        int active = 0;
        for (int r = 0; r < 200; ++r) {
            const QString st = model.statusAt(r);
            if (st == u"pending"_s) {
                ++pending;
            } else if (st == u"active"_s) {
                ++active;
            } else {
                QVERIFY(st == u"done"_s || st == u"cancelled"_s);
            }
        }
        QCOMPARE(pending, 0);
        QCOMPARE(active, 0);
        QVERIFY(model.statusAt(0) != u"pending"_s);
    }
};

#include <QCoreApplication>

int main(int argc, char *argv[]) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    QCoreApplication app(argc, argv);
    TstBridge tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_bridge.moc"
