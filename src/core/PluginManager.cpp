#include "PluginManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>
#include <QProcessEnvironment>
#include <QSet>
#include <QSettings>
#include <algorithm>

PluginManager::PluginManager(QObject *parent)
    : QObject(parent)
{
    m_dshHome = QDir::homePath() + "/.dsh";

    // 优先使用用户在设置页保存的路径，否则自动检测
    QSettings settings("DSH", "dsh-plugin-manager");
    m_dshExecutable = settings.value("dshExecutable").toString();
    if (m_dshExecutable.isEmpty())
        m_dshExecutable = findDshExecutable();

    scanProfiles();
    if (!m_profiles.isEmpty()) {
        m_currentProfile = m_profiles.first();
    }
    scanPlugins();
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
    return m_dshHome + "/profiles/" + m_currentProfile;
}

void PluginManager::refresh()
{
    scanProfiles();
    scanPlugins();
}

void PluginManager::scanProfiles()
{
    QStringList found;
    QDir profilesDir(m_dshHome + "/profiles");
    if (profilesDir.exists()) {
        const auto entries = profilesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &name : entries) {
            // 只把含 package.json 的目录视为 profile
            if (QFile::exists(profilesDir.absoluteFilePath(name + "/package.json"))) {
                found << name;
            }
        }
    }
    if (found != m_profiles) {
        m_profiles = found;
        emit profilesChanged();
    }
    if (m_currentProfile.isEmpty() && !m_profiles.isEmpty()) {
        m_currentProfile = m_profiles.first();
        emit currentProfileChanged();
    }
}

QStringList PluginManager::readEnabledBundles() const
{
    QStringList bundles;
    QFile file(profileDir() + "/package.json");
    if (!file.open(QIODevice::ReadOnly))
        return bundles;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    const QJsonArray arr = doc.object()
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
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        emit errorOccurred("无法读取 profile package.json: " + path);
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    QJsonObject root = doc.object();
    QJsonObject dsh = root.value("dsh").toObject();
    QJsonObject profile = dsh.value("profile").toObject();
    profile["bundles"] = QJsonArray::fromStringList(bundles);
    dsh["profile"] = profile;
    root["dsh"] = dsh;

    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit errorOccurred("无法写入 profile package.json: " + path);
        return false;
    }
    out.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

QVariantMap PluginManager::analyzePlugin(const QString &pluginPath, const QStringList &enabledBundles) const
{
    QVariantMap result;
    QFile packageFile(pluginPath + "/package.json");
    if (!packageFile.open(QIODevice::ReadOnly))
        return result;

    const QJsonDocument doc = QJsonDocument::fromJson(packageFile.readAll());
    if (doc.isNull() || !doc.isObject())
        return result;

    const QJsonObject packageJson = doc.object();
    const QJsonObject dshConfig = packageJson.value("dsh").toObject();
    if (dshConfig.isEmpty())
        return result; // 不是 DSH 插件

    const QString name = packageJson.value("name").toString();
    const QJsonObject bundleConfig = dshConfig.value("bundle").toObject();

    result["id"] = name;
    result["name"] = name;
    result["version"] = packageJson.value("version").toString();
    result["description"] = packageJson.value("description").toString();
    result["installed"] = true;
    result["enabled"] = enabledBundles.contains(name);
    result["hasBundlePatch"] = !bundleConfig.value("patch").toString().isEmpty();
    result["path"] = pluginPath;
    result["profile"] = m_currentProfile;
    return result;
}

// 收集 node_modules 下所有顶层包的路径（含 scoped 包，解析符号链接）
static QStringList collectPackagePaths(const QString &nodeModulesPath)
{
    QStringList paths;
    QDir nodeModulesDir(nodeModulesPath);
    if (!nodeModulesDir.exists())
        return paths;

    const auto entries = nodeModulesDir.entryList(
        QDir::Dirs | QDir::NoDotAndDotDot | QDir::System);
    for (const QString &module : entries) {
        if (module.startsWith("@")) {
            // scoped 包（@scope/name）
            QDir scopedDir(nodeModulesDir.absoluteFilePath(module));
            const auto scoped = scopedDir.entryList(
                QDir::Dirs | QDir::NoDotAndDotDot | QDir::System);
            for (const QString &sub : scoped) {
                QString path = scopedDir.absoluteFilePath(sub);
                QFileInfo fi(path);
                if (fi.isSymLink())
                    path = fi.symLinkTarget();
                paths << path;
            }
            continue;
        }

        QString path = nodeModulesDir.absoluteFilePath(module);
        QFileInfo fi(path);
        if (fi.isSymLink())
            path = fi.symLinkTarget();
        paths << path;
    }
    return paths;
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
            QFile file(profileDir() + "/package.json");
            if (file.open(QIODevice::ReadOnly)) {
                const QJsonObject deps = QJsonDocument::fromJson(file.readAll())
                                             .object().value("dependencies").toObject();
                for (auto it = deps.begin(); it != deps.end(); ++it)
                    profileDeps.insert(it.key());
            }
        }

        const QStringList packagePaths =
            collectPackagePaths(profileDir() + "/node_modules");

        // 第一遍：收集所有包的 dependencies，构建「被其他包依赖」集合
        QSet<QString> transitiveDeps;
        for (const QString &path : packagePaths) {
            QFile packageFile(path + "/package.json");
            if (!packageFile.open(QIODevice::ReadOnly))
                continue;
            const QJsonDocument doc = QJsonDocument::fromJson(packageFile.readAll());
            if (doc.isNull() || !doc.isObject())
                continue;
            const QJsonObject deps = doc.object().value("dependencies").toObject();
            for (auto it = deps.begin(); it != deps.end(); ++it)
                transitiveDeps.insert(it.key());
        }

        // 第二遍：筛出 DSH 插件，并标记是否为「直接安装」
        for (const QString &path : packagePaths) {
            QVariantMap plugin = analyzePlugin(path, enabledBundles);
            if (plugin.isEmpty())
                continue;

            const QString name = plugin.value("name").toString();
            // 直接安装 = 在 profile dependencies 中，或不被任何其他包依赖
            const bool direct = profileDeps.contains(name) || !transitiveDeps.contains(name);
            plugin["direct"] = direct;
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
    const bool ok = runDsh({"plugin", "--profile", m_currentProfile, "add", packageName}, &output);
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

    setLoading(true);
    QString output;
    const bool ok = runDsh({"plugin", "--profile", m_currentProfile, "remove", pluginId}, &output);
    setLoading(false);
    setLastOutput(output);

    if (ok) {
        emit operationSucceeded("卸载成功: " + pluginId);
        scanPlugins();
    } else {
        emit errorOccurred("卸载失败: " + pluginId + "\n" + output);
    }
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
            QDesktopServices::openUrl(QUrl::fromLocalFile(p.value("path").toString()));
            return;
        }
    }
    emit errorOccurred("找不到插件: " + pluginId);
}

void PluginManager::openProfileDirectory()
{
    if (!m_currentProfile.isEmpty())
        QDesktopServices::openUrl(QUrl::fromLocalFile(profileDir()));
}

void PluginManager::setDshExecutable(const QString &path)
{
    const QString trimmed = path.trimmed();
    if (m_dshExecutable == trimmed)
        return;

    m_dshExecutable = trimmed;

    // 持久化到 QSettings（空字符串 = 清除自定义，恢复自动检测）
    QSettings settings("DSH", "dsh-plugin-manager");
    if (trimmed.isEmpty()) {
        settings.remove("dshExecutable");
        m_dshExecutable = findDshExecutable();
    } else {
        settings.setValue("dshExecutable", trimmed);
    }

    emit dshExecutableChanged();
}

QString PluginManager::findDshExecutable() const
{
    // 1. 从 PATH 查找
    const QString found = QStandardPaths::findExecutable("dsh");
    if (!found.isEmpty())
        return found;

    // 2. GUI 应用 PATH 可能不含 npm 全局 bin，检查常见位置
    const QString home = QDir::homePath();
    const QStringList candidates = {
        "/opt/homebrew/bin/dsh",
        "/usr/local/bin/dsh",
        home + "/.npm/_npx/1e7f6d9597241db0/node_modules/.bin/dsh",
    };
    for (const QString &path : candidates) {
        if (QFile::exists(path))
            return path;
    }
    return QString();
}

bool PluginManager::runDsh(const QStringList &args, QString *output)
{
    if (m_dshExecutable.isEmpty()) {
        *output = "找不到 dsh 命令。请在设置页配置 dsh 可执行文件路径。";
        return false;
    }

    QProcess process;
    // GUI 应用默认 PATH 很窄，补充常见 bin 目录以便 dsh 找到 node/npm/pnpm
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString home = QDir::homePath();
    env.insert("PATH", env.value("PATH") + ":/opt/homebrew/bin:/usr/local/bin:"
               + home + "/.npm/_npx/1e7f6d9597241db0/node_modules/.bin");
    process.setProcessEnvironment(env);

    process.start(m_dshExecutable, args);
    if (!process.waitForFinished(120000)) { // 2 分钟超时
        *output = "命令执行超时";
        return false;
    }

    const QString out = QString::fromUtf8(process.readAllStandardOutput());
    const QString err = QString::fromUtf8(process.readAllStandardError());
    *output = (out + "\n" + err).trimmed();
    return process.exitCode() == 0;
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
