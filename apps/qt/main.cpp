#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QtQml/QQmlExtensionPlugin>

#include "BatchQueueModel.hpp"
#include "EngineBridge.hpp"

using namespace Qt::StringLiterals;

Q_IMPORT_QML_PLUGIN(ClipForgePlugin)

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(u":/qt/qml/ClipForge/assets/clipforge-logo.svg"_s));
    // Fusion allows full background/contentItem customization;
    // the native Windows style rejects it with runtime warnings.
    QQuickStyle::setStyle(QStringLiteral("Fusion"));
    QQmlApplicationEngine engine;

    BatchQueueModel model;
    EngineBridge bridge;
    bridge.setQueueModel(&model);
    engine.rootContext()->setContextProperty(u"batchModel"_qs, &model);
    engine.rootContext()->setContextProperty(u"bridge"_qs, &bridge);

    engine.loadFromModule(u"ClipForge"_s, u"Main"_s);
    return engine.rootObjects().isEmpty() ? 1 : app.exec();
}
