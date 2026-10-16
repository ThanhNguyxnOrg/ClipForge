#include <QGuiApplication>
#include <QQmlApplicationEngine>

using namespace Qt::StringLiterals;

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;
    engine.load(u"qrc:/Main.qml"_qs);
    return engine.rootObjects().isEmpty() ? 1 : app.exec();
}
