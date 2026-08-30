#include "PluginManager.h"
#include "LocalBackend.h"
#include "SshBackend.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <algorithm>

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
    QStringList bundles;
    const QString content = m_backend->readFile(profileDir() + "/package.json");
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
    result["profile"] = m_currentProfile;
    return result;
}

void PluginManager::scanPlugins()
{
    setLoading(true);
    QVariantList found;

    if (!m_currentProfile.isEmpty()) {
        const QStringList enabledBundles = readEnabledBundles();

        // Profile 直接声明的依赖（dsh plugin add 安装的包）
        QSet<QString> profileDeps;
        {
            const QString content = m_backend->readFile(profileDir() + "/package.json");
            const QJsonObject deps = QJsonDocument::fromJson(content.toUtf8())
                                         .object().value("dependencies").toObject();
            for (auto it = deps.begin(); it != deps.end(); ++it)
                profileDeps.insert(it.key());
        }

        const auto packageEntries = m_backend->listPackages(m_currentProfile);

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
            QVariantMap plugin = analyzePlugin(entry.resolvedPath, entry.packageJson, enabledBundles);
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
    }

    m_plugins = found;
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
    const QString entryPath = target.value("entryPath").toString();

    // ===== 情况 1：包管理器管理的包 → dsh plugin remove =====
    if (inProfileDeps) {
        setLoading(true);
        QString output;
        const bool ok = m_backend->runDsh({"plugin", "--profile", m_currentProfile, "remove", pluginId}, &output);
        setLoading(false);
        setLastOutput(output);

        if (ok) {
            emit operationSucceeded("卸载成功: " + pluginId);
            scanPlugins();
        } else {
            emit errorOccurred("卸载失败: " + pluginId + "\n" + output);
        }
        return;
    }

    // ===== 情况 2/4：符号链接或孤儿目录 → 直接删除入口 =====
    if (target.value("direct").toBool()) {
        QString error;
        if (!m_backend->removeEntry(entryPath, &error)) {
            emit errorOccurred(error);
            return;
        }
        QStringList bundles = readEnabledBundles();
        if (bundles.removeAll(pluginId) > 0)
            writeEnabledBundles(bundles);
        emit operationSucceeded(remoteActive()
                                ? "已删除远程插件入口: " + pluginId
                                : "已移除插件: " + pluginId);
        scanPlugins();
        return;
    }

    // ===== 情况 3：其他插件的子依赖 → 不允许单独卸载 =====
    emit errorOccurred(pluginId + " 是其他插件的子依赖，不能单独卸载。\n"
                       "如需移除，请卸载依赖它的插件（如 @linxin666/dsh-web-all）。");
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
