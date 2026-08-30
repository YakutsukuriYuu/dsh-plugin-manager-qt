#pragma once

#include "PluginBackend.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSet>

/**
 * 本机后端：原 PluginManager 的直接文件操作逻辑，原样搬入。
 */
class LocalBackend : public PluginBackend
{
public:
    LocalBackend()
    {
        m_dshHome = QDir::homePath() + "/.dsh";
    }

    QString displayName() const override { return QStringLiteral("本机"); }
    bool isRemote() const override { return false; }
    QString dshHome() const override { return m_dshHome; }

    QStringList listProfiles() override
    {
        QStringList found;
        QDir profilesDir(m_dshHome + "/profiles");
        if (!profilesDir.exists())
            return found;
        const auto entries = profilesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &name : entries) {
            if (QFile::exists(profilesDir.absoluteFilePath(name + "/package.json")))
                found << name;
        }
        return found;
    }

    QString readFile(const QString &path) override
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return QString();
        return QString::fromUtf8(file.readAll());
    }

    bool writeFile(const QString &path, const QString &content) override
    {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return false;
        file.write(content.toUtf8());
        return true;
    }

    QList<PackageEntry> listPackages(const QString &profile) override
    {
        QList<PackageEntry> entries;
        const QString nmPath = m_dshHome + "/profiles/" + profile + "/node_modules";
        QDir nodeModulesDir(nmPath);
        if (!nodeModulesDir.exists())
            return entries;

        auto addEntry = [&entries](const QString &entryPath) {
            QString resolved = entryPath;
            QFileInfo fi(entryPath);
            if (fi.isSymLink())
                resolved = fi.symLinkTarget();
            QFile pkg(resolved + "/package.json");
            if (!pkg.open(QIODevice::ReadOnly))
                return;
            PackageEntry e;
            e.entryPath = entryPath;
            e.resolvedPath = resolved;
            e.packageJson = pkg.readAll();
            entries.append(e);
        };

        const auto modules = nodeModulesDir.entryList(
            QDir::Dirs | QDir::NoDotAndDotDot | QDir::System);
        for (const QString &module : modules) {
            if (module.startsWith("@")) {
                QDir scopedDir(nodeModulesDir.absoluteFilePath(module));
                const auto scoped = scopedDir.entryList(
                    QDir::Dirs | QDir::NoDotAndDotDot | QDir::System);
                for (const QString &sub : scoped)
                    addEntry(scopedDir.absoluteFilePath(sub));
                continue;
            }
            addEntry(nodeModulesDir.absoluteFilePath(module));
        }
        return entries;
    }

    bool removeEntry(const QString &path, QString *errorMessage) override
    {
        QFileInfo fi(path);
        if (fi.isSymLink()) {
            if (QFile::remove(path))
                return true;
            if (errorMessage) *errorMessage = "删除链接失败: " + path;
            return false;
        }
        QDir dir(path);
        if (dir.removeRecursively())
            return true;
        if (errorMessage) *errorMessage = "删除目录失败: " + path;
        return false;
    }

    bool uploadDirectory(const QString &localPath, const QString &remotePath,
                         QString *errorMessage) override
    {
        // 本机后端基本用不到（同步是本地→远程），简单递归复制
        QDir().mkpath(remotePath);
        QDirIterator it(localPath, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            const QString rel = QDir(localPath).relativeFilePath(it.filePath());
            if (rel == ".git" || rel.startsWith(".git/") || rel.endsWith(".DS_Store"))
                continue;
            const QString dest = remotePath + "/" + rel;
            if (it.fileInfo().isDir()) {
                QDir().mkpath(dest);
            } else {
                QDir().mkpath(QFileInfo(dest).absolutePath());
                QFile::remove(dest);
                if (!QFile::copy(it.filePath(), dest)) {
                    if (errorMessage) *errorMessage = "复制失败: " + it.filePath();
                    return false;
                }
            }
        }
        return true;
    }

    bool runDsh(const QStringList &args, QString *output) override
    {
        if (m_dshExecutable.isEmpty()) {
            if (output) *output = "找不到 dsh 命令。请在设置页配置 dsh 可执行文件路径。";
            return false;
        }
        QProcess process;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        const QString home = QDir::homePath();
        env.insert("PATH", env.value("PATH") + ":/opt/homebrew/bin:/usr/local/bin:"
                   + home + "/.npm/_npx/1e7f6d9597241db0/node_modules/.bin");
        process.setProcessEnvironment(env);

        process.start(m_dshExecutable, args);
        if (!process.waitForFinished(120000)) {
            if (output) *output = "命令执行超时";
            return false;
        }
        const QString out = QString::fromUtf8(process.readAllStandardOutput());
        const QString err = QString::fromUtf8(process.readAllStandardError());
        if (output) *output = (out + "\n" + err).trimmed();
        return process.exitCode() == 0;
    }

    void openDirectory(const QString &path) override
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }

    // ---- 本机特有：dsh 路径管理（设置页使用）----
    QString dshExecutable() const { return m_dshExecutable; }
    void setDshExecutable(const QString &path) { m_dshExecutable = path; }
    QString findDshExecutable() const
    {
        const QString found = QStandardPaths::findExecutable("dsh");
        if (!found.isEmpty())
            return found;
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

private:
    QString m_dshHome;
    QString m_dshExecutable;
};
