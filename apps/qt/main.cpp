#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQml/QQmlExtensionPlugin>

#include "BatchQueueModel.hpp"
#include "EngineBridge.hpp"

using namespace Qt::StringLiterals;

Q_IMPORT_QML_PLUGIN(ClipForgePlugin)

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;

    BatchQueueModel model;
    EngineBridge bridge;
    bridge.setQueueModel(&model);
    engine.rootContext()->setContextProperty(u"batchModel"_qs, &model);
    engine.rootContext()->setContextProperty(u"bridge"_qs, &bridge);

    engine.loadFromModule(u"ClipForge"_s, u"Main"_s);
    return engine.rootObjects().isEmpty() ? 1 : app.exec();
}
