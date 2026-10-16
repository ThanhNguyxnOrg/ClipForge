#include <QtTest>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQml/QQmlExtensionPlugin>

#include "BatchQueueModel.hpp"
#include "EngineBridge.hpp"

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
        model.setRowError(1, u"boom"_s);
        QCOMPARE(model.statusAt(1), u"failed"_s);
        QCOMPARE(model.errorAt(1), u"boom"_s);
        model.clear();
        QCOMPARE(model.rowCount(), 0);
    }

    void engine_loads_window() {
        BatchQueueModel model;
        EngineBridge bridge;
        bridge.setQueueModel(&model);
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(u"batchModel"_s, &model);
        engine.rootContext()->setContextProperty(u"bridge"_s, &bridge);
        engine.loadFromModule(u"ClipForge"_s, u"Main"_s);
        QVERIFY(!engine.rootObjects().isEmpty());
    }
};

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    TstSmoke tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_smoke.moc"
