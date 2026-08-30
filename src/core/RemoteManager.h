#pragma once

#include <QObject>
#include <QVariantList>
#include <QString>

class PluginManager;

/**
 * 远程服务器管理器。
 *
 * 服务器列表持久化在 QSettings（DSH/dsh-plugin-manager → remoteServers），
 * 每条记录：{ name, target }（target 为 user@host 或 ~/.ssh/config 别名）。
 *
 * connectToServer() 让 PluginManager 切换到 SshBackend；
 * disconnectRemote() 切回 LocalBackend。
 */
class RemoteManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList servers READ servers NOTIFY serversChanged)
    Q_PROPERTY(bool connecting READ connecting NOTIFY connectingChanged)

public:
    explicit RemoteManager(PluginManager *pluginManager, QObject *parent = nullptr);

    QVariantList servers() const { return m_servers; }
    bool connecting() const { return m_connecting; }

    Q_INVOKABLE void addServer(const QString &name, const QString &target);
    Q_INVOKABLE void removeServer(const QString &name);
    Q_INVOKABLE void connectToServer(const QString &name);
    Q_INVOKABLE void disconnectRemote();

signals:
    void serversChanged();
    void connectingChanged();
    void errorOccurred(const QString &message);
    void operationSucceeded(const QString &message);

private:
    void loadServers();
    void saveServers();
    void setConnecting(bool connecting);

    PluginManager *m_pluginManager;
    QVariantList m_servers;
    bool m_connecting = false;
};
