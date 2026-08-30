#pragma once

#include <QObject>
#include <QVariantList>
#include <QStringList>
#include <memory>

#include "PluginBackend.h"

/**
 * DSH 插件管理器核心类。
 *
 * 所有文件/命令操作通过 PluginBackend 完成：
 *  - 本机：LocalBackend（QFile/QDir）
 *  - 远程：SshBackend（ssh 命令）
 * useRemoteBackend() / useLocalBackend() 切换目标后立即重新扫描。
 *
 * 模型：
 *  - Profile：<dshHome>/profiles/<name>/，含 package.json
 *  - 插件：Profile 的 node_modules/ 下 package.json 含 "dsh" 字段的包
 *  - 已启用：包名在 profile package.json 的 dsh.profile.bundles 数组中
 *  - 安装/卸载：dsh plugin --profile <name> add/remove <pkg>
 *  - 启用/禁用：编辑 profile package.json 的 bundles 数组
 */
class PluginManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList plugins READ plugins NOTIFY pluginsChanged)
    Q_PROPERTY(QStringList profiles READ profiles NOTIFY profilesChanged)
    Q_PROPERTY(QString currentProfile READ currentProfile WRITE setCurrentProfile NOTIFY currentProfileChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString dshHome READ dshHome NOTIFY backendChanged)
    Q_PROPERTY(QString dshExecutable READ dshExecutable NOTIFY dshExecutableChanged)
    Q_PROPERTY(QString lastOutput READ lastOutput NOTIFY lastOutputChanged)
    Q_PROPERTY(bool remoteActive READ remoteActive NOTIFY backendChanged)
    Q_PROPERTY(QString backendName READ backendName NOTIFY backendChanged)

public:
    explicit PluginManager(QObject *parent = nullptr);

    QVariantList plugins() const { return m_plugins; }
    QStringList profiles() const { return m_profiles; }
    QString currentProfile() const { return m_currentProfile; }
    bool loading() const { return m_loading; }
    QString dshHome() const;
    QString dshExecutable() const;
    QString lastOutput() const { return m_lastOutput; }
    bool remoteActive() const;
    QString backendName() const;

    void setCurrentProfile(const QString &profile);

    // QML 可调用的操作
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void installPlugin(const QString &packageName);
    Q_INVOKABLE void uninstallPlugin(const QString &pluginId);
    Q_INVOKABLE void togglePlugin(const QString &pluginId, bool enabled);
    Q_INVOKABLE void openPluginDirectory(const QString &pluginId);
    Q_INVOKABLE void openProfileDirectory();
    Q_INVOKABLE void setDshExecutable(const QString &path);   // 仅本机模式有效
    Q_INVOKABLE void useLocalBackend();
    Q_INVOKABLE bool useRemoteBackend(const QString &target, const QString &label, QString *errorMessage = nullptr);

signals:
    void pluginsChanged();
    void profilesChanged();
    void currentProfileChanged();
    void loadingChanged();
    void dshExecutableChanged();
    void lastOutputChanged();
    void backendChanged();
    void errorOccurred(const QString &message);
    void operationSucceeded(const QString &message);

private:
    void scanProfiles();
    void scanPlugins();
    QVariantMap analyzePlugin(const QString &resolvedPath, const QByteArray &packageJson,
                              const QStringList &enabledBundles) const;
    QStringList readEnabledBundles() const;
    bool writeEnabledBundles(const QStringList &bundles);
    QString profileDir() const;
    void setLoading(bool loading);
    void setLastOutput(const QString &output);

    std::unique_ptr<PluginBackend> m_backend;
    QString m_currentProfile;
    QVariantList m_plugins;
    QStringList m_profiles;
    bool m_loading = false;
    QString m_lastOutput;
};
