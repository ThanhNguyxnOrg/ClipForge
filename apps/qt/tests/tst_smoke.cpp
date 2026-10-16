#include <QtTest>

#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtQml/QQmlExtensionPlugin>

#include "BatchQueueModel.hpp"
#include "EngineBridge.hpp"

#include "clipforge/audio/audio_buffer.hpp"
#include "clipforge/audio/wav_writer.hpp"

using namespace Qt::StringLiterals;

Q_IMPORT_QML_PLUGIN(ClipForgePlugin)

class TstSmoke : public QObject {
    Q_OBJECT

private slots:
    void model_handles_500_rows() {
        BatchQueueModel model;
        QStringList files;
        for (int i = 0; i < 500; ++i) {
            files.append(QStringLiteral("C:/audio/%1.wav").arg(i, 3, 10, QChar(u'0')));
        }
        model.setFiles(files);
        QCOMPARE(model.rowCount(), 500);
        model.setRowStatus(0, u"active"_s);
        model.updateProgress(0, 0.5);
        QCOMPARE(model.statusAt(0), u"active"_s);
        QCOMPARE(model.errorAt(1), QString());
        model.setRowError(1, u"boom"_s);
        QCOMPARE(model.statusAt(1), u"failed"_s);
        QCOMPARE(model.errorAt(1), u"boom"_s);
        model.clear();
        QCOMPARE(model.rowCount(), 0);
    }

    void engine_loads_window() {
        BatchQueueModel model;
        QStringList files;
        for (int i = 0; i < 500; ++i) {
            files.append(QStringLiteral("C:/audio/%1.wav").arg(i, 3, 10, QChar(u'0')));
        }
        model.setFiles(files);

        EngineBridge bridge;
        bridge.setQueueModel(&model);
        QQmlApplicationEngine engine;
        QStringList warnings;
        QObject::connect(
            &engine, &QQmlEngine::warnings, &engine,
            [&](const QList<QQmlError> &list) {
                for (const QQmlError &w : list) {
                    warnings.append(w.toString());
                }
            });
        engine.rootContext()->setContextProperty(u"batchModel"_s, &model);
        engine.rootContext()->setContextProperty(u"bridge"_s, &bridge);
        engine.loadFromModule(u"ClipForge"_s, u"Main"_s);
        QVERIFY(!engine.rootObjects().isEmpty());
        QVERIFY(warnings.isEmpty());
    }

    void start_button_drives_batch() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        {
            clipforge::audio::WavFormatInfo fmt{};
            fmt.sample_format = clipforge::audio::SampleFormat::Float32;
            fmt.num_channels = 1;
            fmt.sample_rate = 1000;
            fmt.bits_per_sample = 32;
            fmt.block_align = 4;
            fmt.byte_rate = 4000;
            const auto st = clipforge::audio::WavWriter::write_file(
                std::filesystem::path((dir.path() + u"/a.wav"_s).toStdWString()),
                clipforge::audio::AudioBuffer(fmt, std::vector<float>(100, 0.25f)));
            QVERIFY(st.has_value());
        }

        BatchQueueModel model;
        EngineBridge bridge;
        bridge.setQueueModel(&model);
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(u"batchModel"_s, &model);
        engine.rootContext()->setContextProperty(u"bridge"_s, &bridge);
        engine.loadFromModule(u"ClipForge"_s, u"Main"_s);
        QVERIFY(!engine.rootObjects().isEmpty());

        auto *win = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(win != nullptr);
        auto *inputField = win->findChild<QObject *>(u"inputDirField"_s);
        auto *outputField = win->findChild<QObject *>(u"outputDirField"_s);
        auto *startButton = qobject_cast<QQuickItem *>(win->findChild<QObject *>(u"startButton"_s));
        QVERIFY(inputField != nullptr);
        QVERIFY(outputField != nullptr);
        QVERIFY(startButton != nullptr);
        QVERIFY(inputField->setProperty("text", dir.path()));
        QVERIFY(outputField->setProperty("text", dir.path() + u"/out"_s));

        QSignalSpy finished(&bridge, &EngineBridge::batchFinished);
        const QPoint pt = startButton
                              ->mapToScene(QPointF(startButton->width() / 2.0,
                                                   startButton->height() / 2.0))
                              .toPoint();
        QTest::mouseClick(win, Qt::LeftButton, {}, pt);
        for (int waited = 0; waited < 60000 && finished.isEmpty(); waited += 100) {
            QTest::qWait(100);
        }
        QVERIFY(!finished.isEmpty());
        QCOMPARE(finished.at(0).at(0).toBool(), true);
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.statusAt(0), u"done"_s);
    }
};

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    TstSmoke tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_smoke.moc"
