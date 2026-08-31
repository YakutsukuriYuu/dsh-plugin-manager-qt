#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTimer>

#include "core/PluginManager.h"
#include "core/TmuxManager.h"
#include "core/UpdateChecker.h"
#include "core/RemoteManager.h"
#include "core/RemoteDshManager.h"

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
    RemoteManager remoteManager(&pluginManager);
    RemoteDshManager remoteDshManager(&pluginManager);

    // 远程连接成功后自动探测服务器 DSH 状态；切回本机时清空
    QObject::connect(&pluginManager, &PluginManager::remoteConnectFinished,
                     &remoteDshManager, [&remoteDshManager](bool ok, const QString &) {
        if (ok)
            remoteDshManager.refresh();
    });
    QObject::connect(&pluginManager, &PluginManager::backendChanged,
                     &remoteDshManager, [&pluginManager, &remoteDshManager]() {
        emit remoteDshManager.activeChanged();
        if (!pluginManager.remoteActive())
            remoteDshManager.clear();
    });

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("pluginManager", &pluginManager);
    engine.rootContext()->setContextProperty("tmuxManager", &tmuxManager);
    engine.rootContext()->setContextProperty("updateChecker", &updateChecker);
    engine.rootContext()->setContextProperty("remoteManager", &remoteManager);
    engine.rootContext()->setContextProperty("remoteDshManager", &remoteDshManager);

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
