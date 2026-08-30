#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "core/PluginManager.h"
#include "core/TmuxManager.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("DSH Plugin Manager");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("DSH");

    QQuickStyle::setStyle("Material");

    PluginManager pluginManager;
    TmuxManager tmuxManager;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("pluginManager", &pluginManager);
    engine.rootContext()->setContextProperty("tmuxManager", &tmuxManager);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [](QObject *obj, const QUrl &) {
        if (!obj)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    engine.loadFromModule("DshPluginManager", "Main");

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
