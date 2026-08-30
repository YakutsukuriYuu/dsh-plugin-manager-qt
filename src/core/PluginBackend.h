#pragma once

#include <QObject>
#include <QVariantList>
#include <QStringList>
#include <QPair>

/**
 * 插件管理后端抽象接口。
 *
 * 上层 PluginManager 只与该接口交互，不关心目标在本机还是远程：
 *  - LocalBackend：QFile/QDir 直接读写本机文件
 *  - SshBackend：  通过 ssh user@host "命令" 操作远程服务器
 *
 * 所有方法在主线程同步执行。SSH 单次命令约 100~300ms，
 * 扫描通过「一次性脚本抓取全部 package.json」合并往返，避免逐文件卡顿。
 */
class PluginBackend
{
public:
    virtual ~PluginBackend() = default;

    // 后端标识
    virtual QString displayName() const = 0;   // "本机" 或 "user@host"
    virtual bool isRemote() const = 0;
    virtual QString dshHome() const = 0;       // 如 ~/.dsh（远程为绝对路径）

    // Profile 列表（含 package.json 的目录名）
    virtual QStringList listProfiles() = 0;

    // 文件读写（失败返回空串 / false）
    virtual QString readFile(const QString &path) = 0;
    virtual bool writeFile(const QString &path, const QString &content) = 0;

    // 插件包条目：entryPath 为 node_modules 入口（可能是符号链接），
    // resolvedPath 为真实路径，packageJson 为 package.json 原文（避免二次读取）
    struct PackageEntry {
        QString entryPath;
        QString resolvedPath;
        QByteArray packageJson;
    };
    virtual QList<PackageEntry> listPackages(const QString &profile) = 0;

    // 删除入口（符号链接或目录）
    virtual bool removeEntry(const QString &path, QString *errorMessage) = 0;

    // 上传本地目录到远程路径（tar 管道/本地复制）。后端自行处理连接参数。
    // 用于「本地 → 远程」插件同步。
    virtual bool uploadDirectory(const QString &localPath, const QString &remotePath,
                                 QString *errorMessage) = 0;

    // 执行 dsh 命令（dsh 路径由后端自行解析）
    virtual bool runDsh(const QStringList &args, QString *output) = 0;

    // 「打开目录」：本机用 Finder，远程复制路径到剪贴板
    virtual void openDirectory(const QString &path) = 0;
};
