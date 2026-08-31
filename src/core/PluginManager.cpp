#include "PluginManager.h"
#include "LocalBackend.h"
#include "SshBackend.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSet>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <algorithm>
#include <memory>

namespace {

// ===== 与 PluginManager 解耦的扫描实现（可在工作线程运行）=====

QStringList readEnabledBundlesImpl(PluginBackend *backend, const QString &profileDir)
{
    QStringList bundles;
    const QString content = backend->readFile(profileDir + "/package.json");
    if (content.isEmpty())
        return bundles;

    const QJsonArray arr = QJsonDocument::fromJson(content.toUtf8()).object()
                               .value("dsh").toObject()
                               .value("profile").toObject()
                               .value("bundles").toArray();
    for (const auto &v : arr)
        bundles << v.toString();
    return bundles;
}

QVariantMap analyzePluginImpl(const QString &resolvedPath, const QByteArray &packageJson,
                              const QStringList &enabledBundles, const QString &profile)
{
    QVariantMap result;
    const QJsonDocument doc = QJsonDocument::fromJson(packageJson);
    if (doc.isNull() || !doc.isObject())
        return result;

    const QJsonObject packageJsonObj = doc.object();
    const QJsonObject dshConfig = packageJsonObj.value("dsh").toObject();
    if (dshConfig.isEmpty())
        return result; // 不是 DSH 插件

    const QString name = packageJsonObj.value("name").toString();
    const QJsonObject bundleConfig = dshConfig.value("bundle").toObject();

    result["id"] = name;
    result["name"] = name;
    result["version"] = packageJsonObj.value("version").toString();
    result["description"] = packageJsonObj.value("description").toString();
    result["installed"] = true;
    result["enabled"] = enabledBundles.contains(name);
    result["hasBundlePatch"] = !bundleConfig.value("patch").toString().isEmpty();
    result["path"] = resolvedPath;
    result["profile"] = profile;
    return result;
}

QVariantList scanPluginsImpl(PluginBackend *backend, const QString &profile)
{
    QVariantList found;
    const QString profileDir = backend->dshHome() + "/profiles/" + profile;

    const QStringList enabledBundles = readEnabledBundlesImpl(backend, profileDir);

    // Profile 直接声明的依赖（dsh plugin add 安装的包）
    QSet<QString> profileDeps;
    {
        const QString content = backend->readFile(profileDir + "/package.json");
        const QJsonObject deps = QJsonDocument::fromJson(content.toUtf8())
                                     .object().value("dependencies").toObject();
        for (auto it = deps.begin(); it != deps.end(); ++it)
            profileDeps.insert(it.key());
    }

    const auto packageEntries = backend->listPackages(profile);

    // 第一遍：收集所有包的 dependencies，构建「被其他包依赖」集合
    QSet<QString> transitiveDeps;
    for (const auto &entry : packageEntries) {
        const QJsonDocument doc = QJsonDocument::fromJson(entry.packageJson);
        if (doc.isNull() || !doc.isObject())
            continue;
        const QJsonObject deps = doc.object().value("dependencies").toObject();
        for (auto it = deps.begin(); it != deps.end(); ++it)
            transitiveDeps.insert(it.key());
    }

    // 第二遍：筛出 DSH 插件，并标记「直接安装」
    for (const auto &entry : packageEntries) {
        QVariantMap plugin = analyzePluginImpl(entry.resolvedPath, entry.packageJson,
                                               enabledBundles, profile);
        if (plugin.isEmpty())
            continue;

        const QString name = plugin.value("name").toString();
        // 直接安装 = 在 profile dependencies 中，或不被任何其他包依赖
        const bool direct = profileDeps.contains(name) || !transitiveDeps.contains(name);
        plugin["direct"] = direct;
        plugin["entryPath"] = entry.entryPath;
        plugin["inProfileDeps"] = profileDeps.contains(name);
        found << plugin;
    }

    // 排序：直接安装的在前，名称升序。
    // 注意：不按启用状态排序，避免切换启用时列表位置跳动。
    std::sort(found.begin(), found.end(), [](const QVariant &a, const QVariant &b) {
        const QVariantMap pa = a.toMap();
        const QVariantMap pb = b.toMap();
        const bool da = pa.value("direct").toBool();
        const bool db = pb.value("direct").toBool();
        if (da != db) return da > db;
        return pa.value("name").toString() < pb.value("name").toString();
    });

    return found;
}

} // namespace

PluginManager::PluginManager(QObject *parent)
    : QObject(parent)
{
    auto local = std::make_unique<LocalBackend>();

    // dsh 路径：优先用户设置，否则自动检测（仅本机模式有意义）
    QSettings settings("DSH", "dsh-plugin-manager");
    QString saved = settings.value("dshExecutable").toString();
    local->setDshExecutable(saved.isEmpty() ? local->findDshExecutable() : saved);

    m_backend = std::move(local);
    scanProfiles();
    if (!m_profiles.isEmpty())
        m_currentProfile = m_profiles.first();
    scanPlugins();
}

QString PluginManager::dshHome() const
{
    return m_backend ? m_backend->dshHome() : QString();
}

QString PluginManager::dshExecutable() const
{
    auto *local = dynamic_cast<LocalBackend*>(m_backend.get());
    return local ? local->dshExecutable() : QString();
}

bool PluginManager::remoteActive() const
{
    return m_backend && m_backend->isRemote();
}

QString PluginManager::backendName() const
{
    return m_backend ? m_backend->displayName() : QString();
}

void PluginManager::useLocalBackend()
{
    if (!remoteActive())
        return;
    auto local = std::make_unique<LocalBackend>();
    QSettings settings("DSH", "dsh-plugin-manager");
    QString saved = settings.value("dshExecutable").toString();
    local->setDshExecutable(saved.isEmpty() ? local->findDshExecutable() : saved);
    m_backend = std::move(local);
    m_currentProfile.clear();
    emit backendChanged();
    refresh();
}

bool PluginManager::useRemoteBackend(const QString &target, const QString &label,
                                     int port, const QString &password, QString *errorMessage)
{
    auto remote = std::make_unique<SshBackend>(target, label, port);
    if (!password.isEmpty())
        remote->setPassword(password);

    setLoading(true);
    const bool ok = remote->connectInit(errorMessage);
    setLoading(false);

    if (!ok)
        return false;

    m_backend = std::move(remote);
    m_currentProfile.clear();
    emit backendChanged();
    refresh();
    return true;
}

// 异步远程连接：connectInit + 首次扫描全部在工作线程执行，
// 主线程不被阻塞（UI 可显示等待动画）。
void PluginManager::useRemoteBackendAsync(const QString &target, const QString &label,
                                          int port, const QString &password)
{
    setLoading(true);

    // 结果载体：backend 在工作线程构建，完成后转移到主线程
    struct Result {
        std::unique_ptr<SshBackend> backend;
        bool ok = false;
        QString error;
        QStringList profiles;
        QString currentProfile;
        QVariantList plugins;
    };

    QFuture<std::shared_ptr<Result>> future = QtConcurrent::run(
        [target, label, port, password]() -> std::shared_ptr<Result> {
            auto res = std::make_shared<Result>();
            auto remote = std::make_unique<SshBackend>(target, label, port);
            if (!password.isEmpty())
                remote->setPassword(password);

            if (!remote->connectInit(&res->error))
                return res;

            // 连接成功后立即在同一工作线程完成首次扫描
            res->profiles = remote->listProfiles();
            if (!res->profiles.isEmpty()) {
                res->currentProfile = res->profiles.first();
                res->plugins = scanPluginsImpl(remote.get(), res->currentProfile);
            }
            res->backend = std::move(remote);
            res->ok = true;
            return res;
        });

    auto *watcher = new QFutureWatcher<std::shared_ptr<Result>>(this);
    connect(watcher, &QFutureWatcher<std::shared_ptr<Result>>::finished,
            this, [this, watcher]() {
        auto res = watcher->result();
        watcher->deleteLater();

        if (!res->ok) {
            setLoading(false);
            emit remoteConnectFinished(false, res->error);
            return;
        }

        // 主线程接管后端与扫描结果
        m_backend = std::move(res->backend);
        m_currentProfile.clear();
        emit backendChanged();

        m_profiles = res->profiles;
        emit profilesChanged();

        m_currentProfile = res->currentProfile;
        emit currentProfileChanged();

        m_plugins = res->plugins;
        setLoading(false);
        emit pluginsChanged();

        emit remoteConnectFinished(true, QString());
    });
    watcher->setFuture(future);
}

void PluginManager::setCurrentProfile(const QString &profile)
{
    if (m_currentProfile != profile && m_profiles.contains(profile)) {
        m_currentProfile = profile;
        emit currentProfileChanged();
        scanPlugins();
    }
}

QString PluginManager::profileDir() const
{
    return dshHome() + "/profiles/" + m_currentProfile;
}

void PluginManager::refresh()
{
    scanProfiles();
    scanPlugins();
}

void PluginManager::scanProfiles()
{
    const QStringList found = m_backend->listProfiles();
    if (found != m_profiles) {
        m_profiles = found;
        emit profilesChanged();
    }
    if (m_currentProfile.isEmpty() && !m_profiles.isEmpty()) {
        m_currentProfile = m_profiles.first();
        emit currentProfileChanged();
    }
    // 当前 profile 在目标上不存在时，回退到第一个
    if (!m_currentProfile.isEmpty() && !m_profiles.isEmpty()
        && !m_profiles.contains(m_currentProfile)) {
        m_currentProfile = m_profiles.first();
        emit currentProfileChanged();
    }
}

QStringList PluginManager::readEnabledBundles() const
{
    return readEnabledBundlesImpl(m_backend.get(), profileDir());
}

bool PluginManager::writeEnabledBundles(const QStringList &bundles)
{
    const QString path = profileDir() + "/package.json";
    const QString content = m_backend->readFile(path);
    if (content.isEmpty()) {
        emit errorOccurred("无法读取 profile package.json: " + path);
        return false;
    }

    QJsonObject root = QJsonDocument::fromJson(content.toUtf8()).object();
    QJsonObject dsh = root.value("dsh").toObject();
    QJsonObject profile = dsh.value("profile").toObject();
    profile["bundles"] = QJsonArray::fromStringList(bundles);
    dsh["profile"] = profile;
    root["dsh"] = dsh;

    if (!m_backend->writeFile(path, QString::fromUtf8(
                QJsonDocument(root).toJson(QJsonDocument::Indented)))) {
        emit errorOccurred("无法写入 profile package.json: " + path);
        return false;
    }
    return true;
}

QVariantMap PluginManager::analyzePlugin(const QString &resolvedPath, const QByteArray &packageJson,
                                         const QStringList &enabledBundles) const
{
    return analyzePluginImpl(resolvedPath, packageJson, enabledBundles, m_currentProfile);
}

void PluginManager::scanPlugins()
{
    setLoading(true);
    m_plugins = m_currentProfile.isEmpty()
                ? QVariantList()
                : scanPluginsImpl(m_backend.get(), m_currentProfile);
    setLoading(false);
    emit pluginsChanged();
}

void PluginManager::installPlugin(const QString &packageName)
{
    if (m_currentProfile.isEmpty()) {
        emit errorOccurred("请先选择 Profile");
        return;
    }
    if (packageName.trimmed().isEmpty()) {
        emit errorOccurred("包名不能为空");
        return;
    }

    setLoading(true);
    QString output;
    const bool ok = m_backend->runDsh({"plugin", "--profile", m_currentProfile, "add", packageName}, &output);
    setLoading(false);
    setLastOutput(output);

    if (ok) {
        emit operationSucceeded("安装成功: " + packageName);
        scanPlugins();
    } else {
        emit errorOccurred("安装失败: " + packageName + "\n" + output);
    }
}

void PluginManager::uninstallPlugin(const QString &pluginId)
{
    if (m_currentProfile.isEmpty()) {
        emit errorOccurred("请先选择 Profile");
        return;
    }

    // 找到目标插件
    QVariantMap target;
    for (const QVariant &v : m_plugins) {
        const QVariantMap p = v.toMap();
        if (p.value("id").toString() == pluginId) {
            target = p;
            break;
        }
    }
    if (target.isEmpty()) {
        emit errorOccurred("找不到插件: " + pluginId);
        return;
    }

    const bool inProfileDeps = target.value("inProfileDeps").toBool();
    const bool direct = target.value("direct").toBool();
    const QString entryPath = target.value("entryPath").toString();
    const QString profile = m_currentProfile;

    // 子依赖不允许单独卸载（纯本地判断，直接提示）
    if (!inProfileDeps && !direct) {
        emit errorOccurred(pluginId + " 是其他插件的子依赖，不能单独卸载。\n"
                           "如需移除，请卸载依赖它的插件（如 @linxin666/dsh-web-all）。");
        return;
    }

    // ===== 异步卸载：后端操作（本地文件 / 远程 SSH）在工作线程执行，
    // 主线程不被阻塞（BusyIndicator 正常转，远程慢操作也不卡 UI）=====
    setLoading(true);
    PluginBackend *backend = m_backend.get();

    QFuture<QVariantMap> future = QtConcurrent::run([=]() -> QVariantMap {
        QVariantMap result;
        result["ok"] = false;
        result["error"] = QString();

        if (inProfileDeps) {
            // 情况 1：包管理器管理的包 → dsh plugin remove
            QString output;
            const bool ok = backend->runDsh(
                {"plugin", "--profile", profile, "remove", pluginId}, &output);
            result["ok"] = ok;
            result["output"] = output;
            return result;
        }

        // 情况 2/4：符号链接或孤儿目录 → 直接删除入口 + 清理 bundles
        QString error;
        if (!backend->removeEntry(entryPath, &error)) {
            result["error"] = error;
            return result;
        }
        result["ok"] = true;
        result["removed"] = true;
        return result;
    });

    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this,
            [this, watcher, pluginId, direct]() {
        watcher->deleteLater();
        setLoading(false);

        const QVariantMap result = watcher->result();
        if (!result.value("ok").toBool()) {
            const QString output = result.value("output").toString();
            const QString error = result.value("error").toString();
            setLastOutput(output);
            emit errorOccurred("卸载失败: " + pluginId + "\n"
                               + (error.isEmpty() ? output : error));
            return;
        }

        if (result.value("removed").toBool()) {
            // 情况 2：删除入口后同步清理 profile bundles
            QStringList bundles = readEnabledBundles();
            if (bundles.removeAll(pluginId) > 0)
                writeEnabledBundles(bundles);
            emit operationSucceeded(remoteActive()
                                    ? "已删除远程插件入口: " + pluginId
                                    : "已移除插件: " + pluginId);
        } else {
            setLastOutput(result.value("output").toString());
            emit operationSucceeded("卸载成功: " + pluginId);
        }
        scanPlugins();
    });
    watcher->setFuture(future);
}

void PluginManager::uninstallPlugins(const QStringList &pluginIds)
{
    if (m_currentProfile.isEmpty()) {
        emit errorOccurred("请先选择 Profile");
        return;
    }
    if (pluginIds.isEmpty()) {
        emit errorOccurred("没有选择要卸载的插件");
        return;
    }

    // 收集目标插件信息（主线程读取 m_plugins 快照，工作线程只读）
    struct Item { QString id; bool inProfileDeps; bool direct; QString entryPath; };
    QVector<Item> items;
    for (const QString &pid : pluginIds) {
        for (const QVariant &v : m_plugins) {
            const QVariantMap p = v.toMap();
            if (p.value("id").toString() == pid) {
                Item it;
                it.id = pid;
                it.inProfileDeps = p.value("inProfileDeps").toBool();
                it.direct = p.value("direct").toBool();
                it.entryPath = p.value("entryPath").toString();
                items.append(it);
                break;
            }
        }
    }

    setLoading(true);
    PluginBackend *backend = m_backend.get();
    const QString profile = m_currentProfile;

    // ===== 批量异步卸载：全部在工作线程循环执行，主线程不卡 =====
    QFuture<QVariantMap> future = QtConcurrent::run([=]() -> QVariantMap {
        int removed = 0, skipped = 0, failed = 0;
        QStringList failedList, skippedList;

        for (const Item &it : items) {
            if (!it.inProfileDeps && !it.direct) {
                ++skipped;
                skippedList << it.id;   // 子依赖：不允许单独卸载
                continue;
            }
            if (it.inProfileDeps) {
                QString output;
                const bool ok = backend->runDsh(
                    {"plugin", "--profile", profile, "remove", it.id}, &output);
                if (ok) { ++removed; }
                else { ++failed; failedList << (it.id + " (" + output.trimmed() + ")"); }
            } else {
                QString error;
                if (backend->removeEntry(it.entryPath, &error)) { ++removed; }
                else { ++failed; failedList << (it.id + " (" + error + ")"); }
            }
        }

        QVariantMap result;
        result["removed"] = removed;
        result["skipped"] = skipped;
        result["failed"] = failed;
        result["failedList"] = failedList.join("\n");
        result["skippedList"] = skippedList.join("\n");
        return result;
    });

    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this,
            [this, watcher, items]() {
        watcher->deleteLater();
        setLoading(false);

        const QVariantMap result = watcher->result();
        const int removed = result.value("removed").toInt();
        const int skipped = result.value("skipped").toInt();
        const int failed = result.value("failed").toInt();

        // 删除入口成功后，把已卸载的包从 bundles 清理掉
        QSet<QString> failedSet;
        for (const QString &f : result.value("failedList").toString()
                                    .split('\n', Qt::SkipEmptyParts))
            failedSet.insert(f.left(f.indexOf(' ')));   // 失败项格式 "id (错误)"
        QStringList removedIds;
        for (const Item &it : items)
            if (!failedSet.contains(it.id) && (it.inProfileDeps || it.direct))
                removedIds << it.id;
        if (!removedIds.isEmpty()) {
            QStringList bundles = readEnabledBundles();
            bool changed = false;
            for (const QString &id : removedIds)
                changed = bundles.removeAll(id) > 0 || changed;
            if (changed)
                writeEnabledBundles(bundles);
        }

        QString summary = QString("已卸载 %1 个插件").arg(removed);
        if (skipped > 0)
            summary += QString("，跳过子依赖 %1 个").arg(skipped);
        if (failed > 0)
            summary += QString("，失败 %1 个").arg(failed);
        if (!result.value("failedList").toString().isEmpty())
            summary += "\n失败明细:\n" + result.value("failedList").toString();
        if (!result.value("skippedList").toString().isEmpty())
            summary += "\n跳过的子依赖:\n" + result.value("skippedList").toString();

        if (failed > 0)
            emit errorOccurred(summary);
        else
            emit operationSucceeded(summary);

        scanPlugins();
    });
    watcher->setFuture(future);
}

void PluginManager::togglePlugin(const QString &pluginId, bool enabled)
{
    QStringList bundles = readEnabledBundles();
    const bool currentlyEnabled = bundles.contains(pluginId);

    if (enabled && !currentlyEnabled) {
        bundles << pluginId;
    } else if (!enabled && currentlyEnabled) {
        bundles.removeAll(pluginId);
    } else {
        return; // 状态未变化
    }

    if (writeEnabledBundles(bundles)) {
        // 就地更新该插件的 enabled 字段，不重新扫描：
        // 1. 保持列表顺序不变（卡片不跳动）
        // 2. 不弹成功提示（开关本身已是即时反馈）
        for (int i = 0; i < m_plugins.size(); ++i) {
            QVariantMap p = m_plugins[i].toMap();
            if (p.value("id").toString() == pluginId) {
                p["enabled"] = enabled;
                m_plugins[i] = p;
                break;
            }
        }
        emit pluginsChanged();
    }
}

void PluginManager::openPluginDirectory(const QString &pluginId)
{
    for (const QVariant &v : m_plugins) {
        const QVariantMap p = v.toMap();
        if (p.value("id").toString() == pluginId) {
            m_backend->openDirectory(p.value("path").toString());
            if (remoteActive())
                emit operationSucceeded("远程路径已复制到剪贴板:\n" + p.value("path").toString());
            return;
        }
    }
    emit errorOccurred("找不到插件: " + pluginId);
}

void PluginManager::openProfileDirectory()
{
    if (m_currentProfile.isEmpty())
        return;
    m_backend->openDirectory(profileDir());
    if (remoteActive())
        emit operationSucceeded("远程路径已复制到剪贴板:\n" + profileDir());
}

void PluginManager::setDshExecutable(const QString &path)
{
    auto *local = dynamic_cast<LocalBackend*>(m_backend.get());
    if (!local)
        return; // 远程模式下 dsh 路径由 SshBackend 自动探测

    const QString trimmed = path.trimmed();
    if (local->dshExecutable() == trimmed)
        return;

    QSettings settings("DSH", "dsh-plugin-manager");
    if (trimmed.isEmpty()) {
        settings.remove("dshExecutable");
        local->setDshExecutable(local->findDshExecutable());
    } else {
        settings.setValue("dshExecutable", trimmed);
        local->setDshExecutable(trimmed);
    }
    emit dshExecutableChanged();
}

void PluginManager::setLoading(bool loading)
{
    if (m_loading != loading) {
        m_loading = loading;
        emit loadingChanged();
    }
}

void PluginManager::setLastOutput(const QString &output)
{
    if (m_lastOutput != output) {
        m_lastOutput = output;
        emit lastOutputChanged();
    }
}
