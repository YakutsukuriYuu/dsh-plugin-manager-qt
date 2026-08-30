#pragma once

#include <QObject>
#include <QVariantList>
#include <QStringList>
#include <QProcess>

/**
 * DSH 插件管理器核心类。
 *
 * 模型：
 *  - DSH 主目录：~/.dsh
 *  - Profile：~/.dsh/profiles/<name>/，含 package.json + node_modules/
 *  - 插件：node_modules 下 package.json 含 "dsh" 字段的包
 *  - 已启用：包名出现在 profile package.json 的 dsh.profile.bundles 数组中
 *  - 安装/卸载：dsh plugin --profile <name> add/remove <pkg>
 *  - 启用/禁用：编辑 profile package.json 的 bundles 数组
 *
 * 所有扫描操作在主线程同步执行（仅读取少量目录和 JSON 文件，足够快），
 * 从而彻底避免 QObject 跨线程问题。
 */
class PluginManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList plugins READ plugins NOTIFY pluginsChanged)
    Q_PROPERTY(QStringList profiles READ profiles NOTIFY profilesChanged)
    Q_PROPERTY(QString currentProfile READ currentProfile WRITE setCurrentProfile NOTIFY currentProfileChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString dshHome READ dshHome CONSTANT)
    Q_PROPERTY(QString dshExecutable READ dshExecutable NOTIFY dshExecutableChanged)
    Q_PROPERTY(QString lastOutput READ lastOutput NOTIFY lastOutputChanged)

public:
    explicit PluginManager(QObject *parent = nullptr);

    QVariantList plugins() const { return m_plugins; }
    QStringList profiles() const { return m_profiles; }
    QString currentProfile() const { return m_currentProfile; }
    bool loading() const { return m_loading; }
    QString dshHome() const { return m_dshHome; }
    QString dshExecutable() const { return m_dshExecutable; }
    QString lastOutput() const { return m_lastOutput; }

    void setCurrentProfile(const QString &profile);

    // QML 可调用的操作
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void installPlugin(const QString &packageName);
    Q_INVOKABLE void uninstallPlugin(const QString &pluginId);
    Q_INVOKABLE void togglePlugin(const QString &pluginId, bool enabled);
    Q_INVOKABLE void openPluginDirectory(const QString &pluginId);
    Q_INVOKABLE void openProfileDirectory();
    Q_INVOKABLE void setDshExecutable(const QString &path);

signals:
    void pluginsChanged();
    void profilesChanged();
    void currentProfileChanged();
    void loadingChanged();
    void dshExecutableChanged();
    void lastOutputChanged();
    void errorOccurred(const QString &message);
    void operationSucceeded(const QString &message);

private:
    void scanProfiles();
    void scanPlugins();
    QVariantMap analyzePlugin(const QString &pluginPath, const QStringList &enabledBundles) const;
    QStringList readEnabledBundles() const;
    bool writeEnabledBundles(const QStringList &bundles);
    QString profileDir() const;
    QString findDshExecutable() const;
    bool runDsh(const QStringList &args, QString *output);
    void setLoading(bool loading);
    void setLastOutput(const QString &output);

    QString m_dshHome;
    QString m_dshExecutable;
    QString m_currentProfile;
    QVariantList m_plugins;
    QStringList m_profiles;
    bool m_loading = false;
    QString m_lastOutput;
};
