#pragma once

#include <QObject>
#include <QVariantList>
#include <QString>

class PluginManager;

/**
 * 远程服务器管理器。
 *
 * 服务器记录字段（QSettings 持久化）：
 *   name      备注名（唯一键）
 *   target    user@host 或 ssh config 别名
 *   port      端口（0 = 默认 22）
 *   authType  "key"（默认）或 "password"
 *   password  Base64 编码的密码（仅当用户勾选「记住密码」时保存）
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

    // 解析地址输入：支持 "user@host"、"user@host -p 6005"、"ssh -p 6005 user@host" 等。
    // 返回 { target, port }；UI 可用来把粘贴的完整命令拆到两个输入框。
    Q_INVOKABLE QVariantMap parseTarget(const QString &input) const;

    Q_INVOKABLE void addServer(const QString &name, const QString &target,
                               int port, const QString &authType,
                               const QString &password, bool rememberPassword);
    Q_INVOKABLE void editServer(const QString &name, const QString &newName,
                                const QString &newTarget, int newPort,
                                const QString &authType,
                                const QString &password, bool rememberPassword);
    Q_INVOKABLE void removeServer(const QString &name);

    // 该服务器是否需要密码才能连接（authType=password 且无已保存密码）
    Q_INVOKABLE bool needsPassword(const QString &name) const;

    // password 为空时使用已保存的密码；密钥认证服务器忽略 password 参数
    Q_INVOKABLE void connectToServer(const QString &name, const QString &password = QString());
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
    QVariantMap findServer(const QString &name) const;

    PluginManager *m_pluginManager;
    QVariantList m_servers;
    bool m_connecting = false;
};
