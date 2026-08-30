#pragma once

#include "PluginBackend.h"

#include <QProcess>
#include <QClipboard>
#include <QGuiApplication>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>

/**
 * SSH 远程后端：通过 ssh 管理远程服务器上的 DSH 插件。
 *
 * 连接能力：
 *  - 自定义端口：构造时传入 port（0 = 默认 22）
 *  - 密钥认证（默认）：BatchMode，不交互
 *  - 密码认证：OpenSSH 官方 SSH_ASKPASS 机制——
 *    生成一个 0700 权限的临时脚本输出密码，SSH_ASKPASS_REQUIRE=force
 *    强制使用，避免依赖 sshpass。后端销毁时删除脚本。
 *
 * 其他关键点：
 *  - 扫描合并为单次往返（@@@ENTRY 分隔符），避免逐文件卡顿
 *  - nullglob/null_glob 兼容 bash/zsh 空目录
 *  - dsh 路径通过登录 shell（zsh -lc / bash -lc）探测缓存
 */
class SshBackend : public PluginBackend
{
public:
    /**
     * @param target  ssh 目标：user@host 或 ~/.ssh/config 别名
     * @param label   显示名称
     * @param port    SSH 端口（0 = 默认）
     */
    explicit SshBackend(const QString &target, const QString &label, int port = 0)
        : m_target(target), m_label(label), m_port(port)
    {
    }

    ~SshBackend() override
    {
        // 清理 askpass 临时脚本（含密码）
        m_askpassDir.remove();
    }

    // 设置密码认证（连接前调用）。设置后该后端不再使用 BatchMode。
    void setPassword(const QString &password)
    {
        if (password.isEmpty())
            return;
        m_password = password;

        // 生成 askpass 脚本：printf 输出密码（单引号转义）
        if (!m_askpassDir.isValid())
            return;
        const QString scriptPath = m_askpassDir.path() + "/askpass.sh";
        QFile f(scriptPath);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return;
        QString escaped = password;
        escaped.replace('\'', QStringLiteral("'\\''"));
        f.write("#!/bin/sh\nprintf '%s\\n' '" + escaped.toUtf8() + "'\n");
        f.close();
        f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        m_askpassPath = scriptPath;
    }

    // 连接初始化：探测 dshHome 与 dsh 路径。成功返回 true，失败填 errorMessage。
    bool connectInit(QString *errorMessage)
    {
        QString out;
        if (!ssh(QStringLiteral("echo $HOME"), &out)) {
            if (errorMessage)
                *errorMessage = "SSH 连接失败（" + m_target + "）。\n"
                                + (m_password.isEmpty()
                                   ? "当前为密钥认证：请确认已配置免密登录（ssh-copy-id）。\n"
                                   : "请确认密码正确、端口可达。\n")
                                + out;
            return false;
        }
        m_dshHome = out.trimmed() + "/.dsh";

        // 探测 dsh 命令（登录 shell 才能拿到完整 PATH）
        QString dshPath;
        ssh(QStringLiteral("zsh -lc 'command -v dsh' 2>/dev/null || "
                           "bash -lc 'command -v dsh' 2>/dev/null || true"), &dshPath);
        m_dshPath = dshPath.trimmed();
        return true;
    }

    QString displayName() const override
    {
        QString t = m_target;
        if (m_port > 0)
            t += ":" + QString::number(m_port);
        return m_label + " (" + t + ")";
    }
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
        QStringList args;

        if (m_password.isEmpty()) {
            // 密钥认证：禁用一切交互
            args << "-o" << "BatchMode=yes";
        } else {
            // 密码认证：SSH_ASKPASS 机制（不依赖 sshpass）
            args << "-o" << "NumberOfPasswordPrompts=1"
                 << "-o" << "PreferredAuthentications=password"
                 << "-o" << "PubkeyAuthentication=no";

            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert("SSH_ASKPASS", m_askpassPath);
            env.insert("SSH_ASKPASS_REQUIRE", "force");
            env.insert("DISPLAY", ":0");  // 部分 OpenSSH 版本要求
            process.setProcessEnvironment(env);
        }

        args << "-o" << "ConnectTimeout=8"
             << "-o" << "StrictHostKeyChecking=accept-new";
        if (m_port > 0)
            args << "-p" << QString::number(m_port);
        args << m_target << remoteCommand;

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
    int m_port = 0;
    QString m_dshHome;
    QString m_dshPath;
    QString m_password;
    QString m_askpassPath;
    QTemporaryDir m_askpassDir;   // 销毁时自动删除（含密码的脚本）
};
