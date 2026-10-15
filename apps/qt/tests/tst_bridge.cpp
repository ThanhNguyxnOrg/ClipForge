#include <QtTest>

#include <QSignalSpy>
#include <QTemporaryDir>

#include <QFile>

#include "BatchQueueModel.h"
#include "EngineBridge.h"

#include "clipforge/audio/audio_buffer.hpp"
#include "clipforge/audio/wav_writer.hpp"

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
    QVERIFY(status.has_value());
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

    void non_wav_ignored() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile txt(dir.path() + u"/notes.txt"_s);
        QVERIFY(txt.open(QIODevice::WriteOnly));
        txt.write("not audio");
        txt.close();
        const QString wav = writeGoodWav(dir, u"a.wav"_s);

        BatchQueueModel model;
        EngineBridge bridge;
        bridge.setQueueModel(&model);
        QSignalSpy finished(&bridge, &EngineBridge::batchFinished);
        bridge.startBatch(dir.path(), dir.path() + u"/out"_s, u"normalize"_s, -1.0, -45.0, false, true);
        QVERIFY(finished.wait(30000));
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.fileAt(0), wav);
    }

    void failed_file_continues() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        writeGoodWav(dir, u"good1.wav"_s);
        QFile bad(dir.path() + u"/bad.wav"_s);
        QVERIFY(bad.open(QIODevice::WriteOnly));
        bad.write("RIFFxxxxNOTAWAVEFILE________________");
        bad.close();
        writeGoodWav(dir, u"good2.wav"_s);

        BatchQueueModel model;
        EngineBridge bridge;
        bridge.setQueueModel(&model);
        QSignalSpy finished(&bridge, &EngineBridge::batchFinished);
        bridge.startBatch(dir.path(), dir.path() + u"/out"_s, u"normalize"_s, -1.0, -45.0, false, true);
        QVERIFY(finished.wait(30000));
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
            writeGoodWav(dir, QStringLiteral("f%1.wav").arg(i, 3, 10, QChar(u'0')));
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
        QVERIFY(finished.wait(60000));
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

QTEST_MAIN(TstBridge)
#include "tst_bridge.moc"
