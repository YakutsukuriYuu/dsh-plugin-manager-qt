#pragma once

#include "PluginBackend.h"

#include <QProcess>
#include <QClipboard>
#include <QGuiApplication>

/**
 * SSH 远程后端：通过 ssh user@host "命令" 管理远程服务器上的 DSH 插件。
 *
 * 关键设计：
 *  - 连接参数：BatchMode（免密，仅密钥认证）+ 连接超时 8s + 自动接受新主机指纹
 *  - 扫描合并为单次往返：一条 shell 脚本输出所有包的 package.json，
 *    用 @@@ENTRY| 分隔符切分，避免逐文件 SSH 往返（100+ 次 × 200ms 不可接受）
 *  - dsh 命令路径在连接时通过登录 shell（zsh -lc / bash -lc）探测并缓存
 *  - 文件写入走 stdin 管道：ssh host 'cat > path'
 */
class SshBackend : public PluginBackend
{
public:
    /**
     * @param target  ssh 目标：user@host 或 ~/.ssh/config 中的别名
     * @param label   显示名称（服务器备注名）
     */
    explicit SshBackend(const QString &target, const QString &label)
        : m_target(target), m_label(label)
    {
    }

    // 连接初始化：探测 dshHome 与 dsh 路径。成功返回 true，失败填 errorMessage。
    bool connectInit(QString *errorMessage)
    {
        // 1. 基本连通性 + 远程 HOME
        QString out;
        if (!ssh(QStringLiteral("echo $HOME"), &out)) {
            if (errorMessage)
                *errorMessage = "SSH 连接失败（" + m_target + "）。\n"
                                "请确认：网络可达、密钥认证已配置（BatchMode 不支持密码输入）。\n" + out;
            return false;
        }
        m_dshHome = out.trimmed() + "/.dsh";

        // 2. 探测 dsh 命令（登录 shell 才能拿到完整 PATH）
        QString dshPath;
        ssh(QStringLiteral("zsh -lc 'command -v dsh' 2>/dev/null || "
                           "bash -lc 'command -v dsh' 2>/dev/null || true"), &dshPath);
        m_dshPath = dshPath.trimmed();
        // 找不到不视为连接失败（只影响安装/卸载功能）
        return true;
    }

    QString displayName() const override { return m_label + " (" + m_target + ")"; }
    bool isRemote() const override { return true; }
    QString dshHome() const override { return m_dshHome; }
    QString dshPath() const { return m_dshPath; }

    QStringList listProfiles() override
    {
        QStringList found;
        QString out;
        const QString script = QStringLiteral(
            "for d in '%1'/profiles/*/; do "
            "[ -f \"$d/package.json\" ] && basename \"$d\"; done").arg(m_dshHome);
        if (ssh(script, &out))
            found = out.split('\n', Qt::SkipEmptyParts);
        return found;
    }

    QString readFile(const QString &path) override
    {
        QString out;
        if (ssh(QStringLiteral("cat -- '%1'").arg(escape(path)), &out))
            return out;
        return QString();
    }

    bool writeFile(const QString &path, const QString &content) override
    {
        return ssh(QStringLiteral("cat > '%1'").arg(escape(path)),
                   nullptr, content.toUtf8());
    }

    QList<PackageEntry> listPackages(const QString &profile) override
    {
        QList<PackageEntry> entries;
        const QString nm = m_dshHome + "/profiles/" + profile + "/node_modules";

        // 单次往返抓取全部包的 package.json：
        // 每个包输出 "@@@ENTRY|<入口路径>|<解析后路径>" 后跟文件原文。
        // nullglob/null_glob：目录不存在或无匹配时安全返回空（zsh 默认会对未命中通配符报错）
        const QString script = QStringLiteral(
            "[ -d '%1' ] || exit 0; cd '%1' || exit 0; "
            "shopt -s nullglob 2>/dev/null; setopt null_glob 2>/dev/null; "
            "for entry in */ @*/*/; do "
            "  entry=\"${entry%/}\"; "
            "  [ -f \"$entry/package.json\" ] || continue; "
            "  real=$(readlink \"$entry\" 2>/dev/null); "
            "  echo \"@@@ENTRY|$PWD/$entry|${real:-$PWD/$entry}\"; "
            "  cat \"$entry/package.json\"; "
            "done").arg(nm);

        QString out;
        if (!ssh(script, &out, {}, 60000))
            return entries;

        // 按分隔符切分
        const QStringList chunks = out.split(QStringLiteral("@@@ENTRY|"));
        for (const QString &chunk : chunks) {
            if (chunk.trimmed().isEmpty())
                continue;
            const int nl = chunk.indexOf('\n');
            if (nl < 0)
                continue;
            const QString header = chunk.left(nl);
            const QString body = chunk.mid(nl + 1);
            const QStringList parts = header.split('|');
            if (parts.size() < 2)
                continue;

            PackageEntry e;
            e.entryPath = parts[0];
            e.resolvedPath = parts.size() > 1 && !parts[1].isEmpty() ? parts[1] : parts[0];
            e.packageJson = body.toUtf8();
            entries.append(e);
        }
        return entries;
    }

    bool removeEntry(const QString &path, QString *errorMessage) override
    {
        QString out;
        if (ssh(QStringLiteral("rm -rf -- '%1'").arg(escape(path)), &out))
            return true;
        if (errorMessage) *errorMessage = "远程删除失败: " + out;
        return false;
    }

    bool runDsh(const QStringList &args, QString *output) override
    {
        if (m_dshPath.isEmpty()) {
            if (output)
                *output = "远程服务器上未找到 dsh 命令（已尝试登录 shell 探测 PATH）";
            return false;
        }
        // 参数逐个单引号转义
        QStringList quoted;
        for (const QString &a : args)
            quoted << "'" + escape(a) + "'";
        return ssh(QStringLiteral("'%1' %2").arg(m_dshPath, quoted.join(' ')),
                   output, {}, 180000);  // 安装可能较慢
    }

    void openDirectory(const QString &path) override
    {
        // 远程无法打开 Finder，复制路径到剪贴板
        QGuiApplication::clipboard()->setText(path);
    }

private:
    // shell 单引号转义：' → '\''
    static QString escape(const QString &s)
    {
        QString r = s;
        r.replace('\'', QStringLiteral("'\\''"));
        return r;
    }

    // 执行远程命令。返回是否成功（退出码 0）。
    bool ssh(const QString &remoteCommand, QString *output,
             const QByteArray &stdinData = {}, int timeoutMs = 30000)
    {
        QProcess process;
        QStringList args = {
            "-o", "BatchMode=yes",
            "-o", "ConnectTimeout=8",
            "-o", "StrictHostKeyChecking=accept-new",
            m_target,
            remoteCommand
        };
        process.start("ssh", args);
        if (!stdinData.isEmpty()) {
            process.write(stdinData);
            process.closeWriteChannel();
        }
        if (!process.waitForFinished(timeoutMs)) {
            process.kill();
            if (output) *output = "SSH 命令超时";
            return false;
        }
        const QString out = QString::fromUtf8(process.readAllStandardOutput());
        const QString err = QString::fromUtf8(process.readAllStandardError());
        if (output) *output = process.exitCode() == 0 ? out : (out + "\n" + err).trimmed();
        return process.exitCode() == 0;
    }

    QString m_target;
    QString m_label;
    QString m_dshHome;
    QString m_dshPath;
};
