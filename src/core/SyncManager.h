#pragma once

#include <QObject>
#include <QVariantList>
#include <QStringList>
#include <QMap>
#include <QString>

class PluginManager;
class PluginBackend;

/**
 * 本地 → 远程 插件同步管理器。
 *
 * 工作流程：
 *  1. buildPlan()：用独立 LocalBackend 扫描本地插件，与当前远程
 *     （PluginManager 的远程后端）对比版本，生成同步计划
 *  2. startSync(names)：在工作线程中逐个执行：
 *     - tar 打包本地插件目录（-h 解引用符号链接，排除 .git/.DS_Store）
 *       → 通过管道喂给 ssh 'tar xzf -' 解压到远程 node_modules
 *     - 更新远程 profile package.json：
 *       dependencies[name] = 本地版本（本地为主，远程更新也覆盖）
 *       bundles 与本地启用状态对齐
 *  3. 完成后远程插件列表自动刷新
 *
 * 版本策略：本地为主（forceOverwrite），远程较新也会被本地覆盖。
 */
class SyncManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool syncing READ syncing NOTIFY syncingChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString currentPlugin READ currentPlugin NOTIFY currentPluginChanged)
    Q_PROPERTY(QVariantList plan READ plan NOTIFY planChanged)

public:
    explicit SyncManager(PluginManager *pluginManager, QObject *parent = nullptr);

    bool syncing() const { return m_syncing; }
    int progress() const { return m_progress; }
    QString currentPlugin() const { return m_currentPlugin; }
    QVariantList plan() const { return m_plan; }

    // 构建同步计划（需远程已连接；本地扫描快，远程数据取当前列表）
    // 成功返回空字符串，失败返回错误信息
    Q_INVOKABLE QString buildPlan();
    // 开始同步选中的插件
    Q_INVOKABLE void startSync(const QStringList &pluginNames);

signals:
    void syncingChanged();
    void progressChanged();
    void currentPluginChanged();
    void planChanged();
    void syncLog(const QString &line);
    void syncFinished(bool ok, const QString &summary);

private:
    void setSyncing(bool v);
    void setProgress(int v);
    void setCurrentPlugin(const QString &v);
    void postLog(const QString &line);          // 线程安全地发日志

    PluginManager *m_pluginManager;
    QVariantList m_plan;
    QMap<QString, QStringList> m_localDepsByName;  // 插件名 → 本地依赖名列表（闭包展开用）
    bool m_syncing = false;
    int m_progress = 0;
    QString m_currentPlugin;
};
