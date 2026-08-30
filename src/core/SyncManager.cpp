#include "SyncManager.h"
#include "PluginManager.h"
#include "PluginBackend.h"
#include "LocalBackend.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QtConcurrent>
#include <functional>

SyncManager::SyncManager(PluginManager *pluginManager, QObject *parent)
    : QObject(parent)
    , m_pluginManager(pluginManager)
{
}

void SyncManager::postLog(const QString &line)
{
    // 工作线程 → 主线程 排队发送
    QMetaObject::invokeMethod(this, [this, line]() { emit syncLog(line); },
                              Qt::QueuedConnection);
}

void SyncManager::setSyncing(bool v)
{
    if (m_syncing != v) { m_syncing = v; emit syncingChanged(); }
}

void SyncManager::setProgress(int v)
{
    if (m_progress != v) { m_progress = v; emit progressChanged(); }
}

void SyncManager::setCurrentPlugin(const QString &v)
{
    if (m_currentPlugin != v) { m_currentPlugin = v; emit currentPluginChanged(); }
}

QString SyncManager::buildPlan()
{
    if (!m_pluginManager->remoteActive())
        return "请先连接到远程服务器";

    // 本地扫描（独立 LocalBackend，不影响当前远程模式）
    LocalBackend local;
    const QString profile = m_pluginManager->currentProfile();
    const QString localProfilePkg = local.dshHome() + "/profiles/" + profile + "/package.json";
    if (!QFile::exists(localProfilePkg))
        return "本机没有名为 " + profile + " 的 Profile，无法同步";

    // 本地 profile 的 bundles（决定同步后远程的启用状态）
    QStringList localBundles;
    {
        const QJsonObject root = QJsonDocument::fromJson(
            local.readFile(localProfilePkg).toUtf8()).object();
        for (const auto &b : root.value("dsh").toObject()
                                  .value("profile").toObject()
                                  .value("bundles").toArray())
            localBundles << b.toString();
    }
    // 本地 profile 的 dependencies（决定写入远程的版本规范）
    const QJsonObject localDeps = QJsonDocument::fromJson(
        local.readFile(localProfilePkg).toUtf8()).object().value("dependencies").toObject();

    // 远程现状：name → plugin map
    QMap<QString, QVariantMap> remotePlugins;
    for (const QVariant &v : m_pluginManager->plugins()) {
        const QVariantMap p = v.toMap();
        remotePlugins[p.value("name").toString()] = p;
    }

    // 本地插件 → 计划项（只包含「直接安装」的插件；子依赖由聚合包自带，
    // 不单独同步，与插件管理页的默认视图一致）
    const auto localEntries = local.listPackages(profile);

    QSet<QString> localTransitiveDeps;
    for (const auto &entry : localEntries) {
        const QJsonDocument doc = QJsonDocument::fromJson(entry.packageJson);
        if (doc.isNull() || !doc.isObject())
            continue;
        const QJsonObject deps = doc.object().value("dependencies").toObject();
        for (auto it = deps.begin(); it != deps.end(); ++it)
            localTransitiveDeps.insert(it.key());
    }

    QVariantList planItems;
    for (const auto &entry : localEntries) {
        const QJsonDocument doc = QJsonDocument::fromJson(entry.packageJson);
        if (doc.isNull() || !doc.isObject())
            continue;
        const QJsonObject pkg = doc.object();
        if (pkg.value("dsh").toObject().isEmpty())
            continue;  // 不是 DSH 插件

        const QString name = pkg.value("name").toString();
        // 直接安装 = 在本地 profile dependencies 中，或不被任何其他包依赖
        const bool direct = localDeps.contains(name) || !localTransitiveDeps.contains(name);

        const QString localVersion = pkg.value("version").toString();

        QVariantMap item;
        item["name"] = name;
        item["localVersion"] = localVersion;
        item["sourcePath"] = entry.resolvedPath;  // 解引用后的真实路径
        item["direct"] = direct;
        item["enabledLocal"] = localBundles.contains(name);
        item["depSpec"] = localDeps.contains(name)
                              ? localDeps.value(name).toString()
                              : "^" + localVersion;

        if (remotePlugins.contains(name)) {
            const QVariantMap r = remotePlugins.value(name);
            const QString remoteVersion = r.value("version").toString();
            item["remoteVersion"] = remoteVersion;
            item["remoteEnabled"] = r.value("enabled").toBool();
            // 版本策略：本地为主——只要不一致就更新（包括远程更新的情况）
            item["action"] = remoteVersion == localVersion ? "same" : "update";
        } else {
            item["remoteVersion"] = "";
            item["remoteEnabled"] = false;
            item["action"] = "install";
        }
        planItems << item;

        // 记录该插件的 DSH 子依赖（同步时自动带上依赖闭包，
        // 否则聚合包同步过去后子插件在远程不存在）
        QStringList dshDeps;
        const QJsonObject deps = pkg.value("dependencies").toObject();
        for (auto it = deps.begin(); it != deps.end(); ++it)
            dshDeps << it.key();
        m_localDepsByName[name] = dshDeps;
    }

    m_plan = planItems;
    emit planChanged();
    return QString();
}

void SyncManager::startSync(const QStringList &pluginNames)
{
    if (m_syncing || pluginNames.isEmpty())
        return;

    PluginBackend *remote = m_pluginManager->backend();
    const QString profile = m_pluginManager->currentProfile();
    const QString remoteNm = m_pluginManager->dshHome() + "/profiles/" + profile + "/node_modules";
    const QString remotePkgPath = m_pluginManager->dshHome() + "/profiles/" + profile + "/package.json";

    // 计划项按名索引
    QMap<QString, QVariantMap> planByName;
    for (const QVariant &v : m_plan)
        planByName[v.toMap().value("name").toString()] = v.toMap();

    // 依赖闭包展开：选中聚合包时自动带上它的 DSH 子依赖
    // （否则远程缺少子插件包，聚合包加载会失败）
    QStringList expanded;
    {
        QSet<QString> seen;
        std::function<void(const QString&)> dfs = [&](const QString &n) {
            if (seen.contains(n) || !planByName.contains(n))
                return;
            seen.insert(n);
            expanded << n;
            for (const QString &dep : m_localDepsByName.value(n))
                dfs(dep);
        };
        for (const QString &n : pluginNames)
            dfs(n);
    }
    if (expanded.size() > pluginNames.size())
        postLog(QString("自动带上 %1 个子依赖").arg(expanded.size() - pluginNames.size()));

    setSyncing(true);
    setProgress(0);
    setCurrentPlugin(QString());
    postLog("开始同步 " + QString::number(pluginNames.size()) + " 个插件…");

    QFuture<void> future = QtConcurrent::run([=]() {
        int succeeded = 0;
        int failed = 0;
        int done = 0;
        const int total = expanded.size();

        for (const QString &name : expanded) {
            const QVariantMap item = planByName.value(name);
            QMetaObject::invokeMethod(this, [this, name]() { setCurrentPlugin(name); },
                                      Qt::QueuedConnection);
            postLog("▶ " + name);

            const QString sourcePath = item.value("sourcePath").toString();
            const QString localVersion = item.value("localVersion").toString();
            const QString depSpec = item.value("depSpec").toString();
            const bool enabledLocal = item.value("enabledLocal").toBool();
            const QString remoteTarget = remoteNm + "/" + name;

            // 1) 删除远程旧版本（不存在也无妨）
            {
                QString err;
                remote->removeEntry(remoteTarget, &err);
            }

            // 2) 上传目录（tar 管道 / 后端自行处理连接与认证）
            {
                QString err;
                if (!remote->uploadDirectory(sourcePath, remoteTarget, &err)) {
                    ++failed;
                    postLog("  ✗ 上传失败: " + err);
                    QMetaObject::invokeMethod(this, [this, total, &done]() {
                        setProgress(static_cast<int>((++done) * 100.0 / total));
                    }, Qt::QueuedConnection);
                    continue;
                }
            }
            postLog("  ✓ 文件已上传");

            // 3) 更新远程 package.json：dependencies + bundles（本地为主）
            {
                const QString content = remote->readFile(remotePkgPath);
                QJsonObject root = QJsonDocument::fromJson(content.toUtf8()).object();

                QJsonObject deps = root.value("dependencies").toObject();
                deps[name] = depSpec;
                root["dependencies"] = deps;

                QJsonObject dsh = root.value("dsh").toObject();
                QJsonObject prof = dsh.value("profile").toObject();
                QStringList bundles;
                for (const auto &b : prof.value("bundles").toArray())
                    bundles << b.toString();
                if (enabledLocal) {
                    if (!bundles.contains(name))
                        bundles << name;
                } else {
                    bundles.removeAll(name);
                }
                prof["bundles"] = QJsonArray::fromStringList(bundles);
                dsh["profile"] = prof;
                root["dsh"] = dsh;

                if (remote->writeFile(remotePkgPath, QString::fromUtf8(
                        QJsonDocument(root).toJson(QJsonDocument::Indented)))) {
                    postLog("  ✓ 远程配置已更新（v" + localVersion + "，本地为主）");
                } else {
                    postLog("  ⚠ 远程 package.json 写入失败");
                }
            }

            ++succeeded;
            QMetaObject::invokeMethod(this, [this, total, &done]() {
                setProgress(static_cast<int>((++done) * 100.0 / total));
            }, Qt::QueuedConnection);
        }

        QMetaObject::invokeMethod(this, [this, succeeded, failed]() {
            setSyncing(false);
            setCurrentPlugin(QString());
            emit syncFinished(failed == 0,
                              QString("同步完成：成功 %1 个，失败 %2 个")
                                  .arg(succeeded).arg(failed));
        }, Qt::QueuedConnection);
    });
}
