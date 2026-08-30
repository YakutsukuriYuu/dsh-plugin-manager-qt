#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTimer>

#include "core/PluginManager.h"
#include "core/TmuxManager.h"
#include "core/UpdateChecker.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("DSH Plugin Manager");
    app.setApplicationVersion(QStringLiteral(APP_VERSION));
    app.setOrganizationName("DSH");

    QQuickStyle::setStyle("Material");

    PluginManager pluginManager;
    TmuxManager tmuxManager;
    UpdateChecker updateChecker;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("pluginManager", &pluginManager);
    engine.rootContext()->setContextProperty("tmuxManager", &tmuxManager);
    engine.rootContext()->setContextProperty("updateChecker", &updateChecker);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [](QObject *obj, const QUrl &) {
        if (!obj)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    engine.loadFromModule("DshPluginManager", "Main");

    if (engine.rootObjects().isEmpty())
        return -1;

    // 启动 2 秒后静默检查更新（失败不打扰用户）
    QTimer::singleShot(2000, &updateChecker, [&updateChecker]() {
        updateChecker.check(true);
    });

    return app.exec();
}
