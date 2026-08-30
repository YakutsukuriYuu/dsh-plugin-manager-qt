#include "RemoteManager.h"
#include "PluginManager.h"

#include <QSettings>

RemoteManager::RemoteManager(PluginManager *pluginManager, QObject *parent)
    : QObject(parent)
    , m_pluginManager(pluginManager)
{
    loadServers();
}

void RemoteManager::loadServers()
{
    m_servers.clear();
    QSettings settings("DSH", "dsh-plugin-manager");
    const int count = settings.beginReadArray("remoteServers");
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        QVariantMap server;
        server["name"] = settings.value("name").toString();
        server["target"] = settings.value("target").toString();
        m_servers << server;
    }
    settings.endArray();
}

void RemoteManager::saveServers()
{
    QSettings settings("DSH", "dsh-plugin-manager");
    settings.beginWriteArray("remoteServers");
    for (int i = 0; i < m_servers.size(); ++i) {
        settings.setArrayIndex(i);
        const QVariantMap s = m_servers[i].toMap();
        settings.setValue("name", s.value("name").toString());
        settings.setValue("target", s.value("target").toString());
    }
    settings.endArray();
}

void RemoteManager::addServer(const QString &name, const QString &target)
{
    const QString n = name.trimmed();
    const QString t = target.trimmed();
    if (n.isEmpty() || t.isEmpty()) {
        emit errorOccurred("服务器名称和地址不能为空");
        return;
    }
    for (const QVariant &v : m_servers) {
        if (v.toMap().value("name").toString() == n) {
            emit errorOccurred("服务器名称已存在: " + n);
            return;
        }
    }

    QVariantMap server;
    server["name"] = n;
    server["target"] = t;
    m_servers << server;
    saveServers();
    emit serversChanged();
    emit operationSucceeded("已添加服务器: " + n);
}

void RemoteManager::editServer(const QString &name, const QString &newName, const QString &newTarget)
{
    const QString n = newName.trimmed();
    const QString t = newTarget.trimmed();
    if (n.isEmpty() || t.isEmpty()) {
        emit errorOccurred("服务器名称和地址不能为空");
        return;
    }

    for (int i = 0; i < m_servers.size(); ++i) {
        if (m_servers[i].toMap().value("name").toString() == name) {
            // 改名时检查新名字是否与其他服务器冲突
            if (n != name) {
                for (const QVariant &v : m_servers) {
                    if (v.toMap().value("name").toString() == n) {
                        emit errorOccurred("服务器名称已存在: " + n);
                        return;
                    }
                }
            }
            QVariantMap server;
            server["name"] = n;
            server["target"] = t;
            m_servers[i] = server;
            saveServers();
            emit serversChanged();
            emit operationSucceeded("已保存服务器: " + n);
            return;
        }
    }
    emit errorOccurred("找不到服务器: " + name);
}

void RemoteManager::removeServer(const QString &name)
{
    for (int i = 0; i < m_servers.size(); ++i) {
        if (m_servers[i].toMap().value("name").toString() == name) {
            m_servers.removeAt(i);
            saveServers();
            emit serversChanged();
            emit operationSucceeded("已删除服务器: " + name);
            return;
        }
    }
}

void RemoteManager::connectToServer(const QString &name)
{
    QString target;
    for (const QVariant &v : m_servers) {
        if (v.toMap().value("name").toString() == name) {
            target = v.toMap().value("target").toString();
            break;
        }
    }
    if (target.isEmpty()) {
        emit errorOccurred("找不到服务器: " + name);
        return;
    }

    setConnecting(true);
    QString error;
    const bool ok = m_pluginManager->useRemoteBackend(target, name, &error);
    setConnecting(false);

    if (ok) {
        emit operationSucceeded("已连接到 " + name + " (" + target + ")");
    } else {
        emit errorOccurred(error.isEmpty() ? "连接失败" : error);
    }
}

void RemoteManager::disconnectRemote()
{
    m_pluginManager->useLocalBackend();
    emit operationSucceeded("已切换回本机");
}

void RemoteManager::setConnecting(bool connecting)
{
    if (m_connecting != connecting) {
        m_connecting = connecting;
        emit connectingChanged();
    }
}
