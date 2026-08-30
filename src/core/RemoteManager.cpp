#include "RemoteManager.h"
#include "PluginManager.h"

#include <QSettings>
#include <QRegularExpression>

RemoteManager::RemoteManager(PluginManager *pluginManager, QObject *parent)
    : QObject(parent)
    , m_pluginManager(pluginManager)
{
    loadServers();
}

QVariantMap RemoteManager::parseTarget(const QString &input) const
{
    QVariantMap result;
    QString t = input.trimmed();

    // 去掉误复制的 ssh 前缀
    if (t.startsWith(QStringLiteral("ssh ")))
        t = t.mid(4).trimmed();

    // 拆词，提取 -p/--port 端口，其余第一个非选项词为目标
    const QStringList parts = t.split(QRegularExpression(QStringLiteral("\\s+")),
                                      Qt::SkipEmptyParts);
    QString target;
    int port = 0;
    for (int i = 0; i < parts.size(); ++i) {
        const QString &p = parts[i];
        if ((p == "-p" || p == "--port") && i + 1 < parts.size()) {
            port = parts[++i].toInt();
        } else if (p.startsWith("-p") && p.length() > 2) {
            port = p.mid(2).toInt();
        } else if (p.startsWith("--port=")) {
            port = p.mid(7).toInt();
        } else if (!p.startsWith("-") && target.isEmpty()) {
            target = p;
        }
    }

    result["target"] = target;
    result["port"] = port;
    return result;
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
        server["port"] = settings.value("port", 0).toInt();
        server["authType"] = settings.value("authType", "key").toString();
        const QString pwd = settings.value("password").toString();
        if (!pwd.isEmpty())
            server["password"] = pwd;  // Base64
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
        settings.setValue("port", s.value("port", 0).toInt());
        settings.setValue("authType", s.value("authType", "key").toString());
        if (s.contains("password"))
            settings.setValue("password", s.value("password").toString());
        else
            settings.remove("password");
    }
    settings.endArray();
}

QVariantMap RemoteManager::findServer(const QString &name) const
{
    for (const QVariant &v : m_servers) {
        const QVariantMap s = v.toMap();
        if (s.value("name").toString() == name)
            return s;
    }
    return QVariantMap();
}

void RemoteManager::addServer(const QString &name, const QString &target,
                              int port, const QString &authType,
                              const QString &password, bool rememberPassword)
{
    const QString n = name.trimmed();
    const QVariantMap parsed = parseTarget(target);
    const QString t = parsed.value("target").toString();
    const int p = port > 0 ? port : parsed.value("port").toInt();

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
    server["port"] = p;
    server["authType"] = authType == "password" ? "password" : "key";
    if (authType == "password" && rememberPassword && !password.isEmpty())
        server["password"] = QString::fromUtf8(password.toUtf8().toBase64());

    m_servers << server;
    saveServers();
    emit serversChanged();
    emit operationSucceeded("已添加服务器: " + n);
}

void RemoteManager::editServer(const QString &name, const QString &newName,
                               const QString &newTarget, int newPort,
                               const QString &authType,
                               const QString &password, bool rememberPassword)
{
    const QString n = newName.trimmed();
    const QVariantMap parsed = parseTarget(newTarget);
    const QString t = parsed.value("target").toString();
    const int p = newPort > 0 ? newPort : parsed.value("port").toInt();

    if (n.isEmpty() || t.isEmpty()) {
        emit errorOccurred("服务器名称和地址不能为空");
        return;
    }

    for (int i = 0; i < m_servers.size(); ++i) {
        if (m_servers[i].toMap().value("name").toString() == name) {
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
            server["port"] = p;
            server["authType"] = authType == "password" ? "password" : "key";
            // 密码：记住则更新/保留，不记住则清除
            if (authType == "password" && rememberPassword) {
                if (!password.isEmpty())
                    server["password"] = QString::fromUtf8(password.toUtf8().toBase64());
                else if (m_servers[i].toMap().contains("password"))
                    server["password"] = m_servers[i].toMap().value("password").toString();
            }
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

bool RemoteManager::needsPassword(const QString &name) const
{
    const QVariantMap s = findServer(name);
    return s.value("authType").toString() == "password"
           && !s.contains("password");
}

void RemoteManager::connectToServer(const QString &name, const QString &password)
{
    const QVariantMap server = findServer(name);
    if (server.isEmpty()) {
        emit errorOccurred("找不到服务器: " + name);
        return;
    }

    // 决定实际使用的密码
    QString pwd = password;
    if (pwd.isEmpty() && server.contains("password"))
        pwd = QString::fromUtf8(QByteArray::fromBase64(
            server.value("password").toString().toUtf8()));

    if (server.value("authType").toString() == "password" && pwd.isEmpty()) {
        emit errorOccurred("该服务器使用密码认证，请输入密码");
        return;
    }

    setConnecting(true);

    // 异步连接：结果经 remoteConnectFinished 信号返回
    QObject::connect(m_pluginManager, &PluginManager::remoteConnectFinished,
                     this, [this, name](bool ok, const QString &error) {
        setConnecting(false);
        if (ok) {
            emit operationSucceeded("已连接到 " + name);
        } else {
            emit errorOccurred(error.isEmpty() ? "连接失败" : error);
        }
    }, Qt::SingleShotConnection);

    m_pluginManager->useRemoteBackendAsync(
        server.value("target").toString(),
        name,
        server.value("port", 0).toInt(),
        server.value("authType").toString() == "password" ? pwd : QString());
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
