#include "RemoteDshManager.h"

#include "PluginManager.h"
#include "SshBackend.h"

#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QThread>

RemoteDshManager::RemoteDshManager(PluginManager *pluginManager, QObject *parent)
    : QObject(parent), m_pluginManager(pluginManager)
{
}

SshBackend *RemoteDshManager::sshBackend() const
{
    PluginBackend *b = m_pluginManager->backend();
    if (!b || !b->isRemote())
        return nullptr;
    return dynamic_cast<SshBackend *>(b);
}

bool RemoteDshManager::active() const
{
    return sshBackend() != nullptr;
}

void RemoteDshManager::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void RemoteDshManager::appendLog(const QString &line)
{
    m_log += line + QLatin1Char('\n');
    emit logChanged();
}

void RemoteDshManager::clear()
{
    m_dshInstalled = false;
    m_dshVersion.clear();
    m_packageName.clear();
    m_npmPath.clear();
    m_latestVersion.clear();
    m_processPid.clear();
    m_processCwd.clear();
    m_processArgs.clear();
    m_upgradeAvailable = false;
    m_runMode.clear();
    m_runModeText.clear();
    m_log.clear();
    emit stateChanged();
    emit logChanged();
    emit activeChanged();
}

// 版本号比较：x.y.z 逐位数字比较，latest > current 返回 true
static bool versionNewer(const QString &latest, const QString &current)
{
    auto parse = [](const QString &ver) {
        QString v = ver;
        if (v.startsWith('v') || v.startsWith('V'))
            v = v.mid(1);
        const int dash = v.indexOf('-');
        if (dash > 0)
            v = v.left(dash);
        QList<int> parts;
        for (const QString &s : v.split('.'))
            parts << s.toInt();
        return parts;
    };
    const QList<int> a = parse(latest), b = parse(current);
    for (int i = 0; i < qMax(a.size(), b.size()); ++i) {
        const int x = i < a.size() ? a[i] : 0;
        const int y = i < b.size() ? b[i] : 0;
        if (x != y)
            return x > y;
    }
    return false;
}

void RemoteDshManager::refresh()
{
    SshBackend *backend = sshBackend();
    if (!backend || m_busy)
        return;
    setBusy(true);
    appendLog(QStringLiteral("• 检测服务器 DSH 状态…"));

    auto future = QtConcurrent::run([this, backend]() { doRefresh(backend); });
    auto *watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher]() {
        watcher->deleteLater();
        setBusy(false);
        emit stateChanged();
    });
    watcher->setFuture(future);
}

void RemoteDshManager::doRefresh(SshBackend *backend)
{
    auto post = [this](const QString &line) {
        QMetaObject::invokeMethod(this, [this, line]() { appendLog(line); },
                                  Qt::QueuedConnection);
    };
    auto setState = [this](bool installed, const QString &version, const QString &pkg,
                           const QString &latest, const QString &mode, const QString &modeText) {
        QMetaObject::invokeMethod(this, [=]() {
            m_dshInstalled = installed;
            m_dshVersion = version;
            m_packageName = pkg;
            m_latestVersion = latest;
            m_upgradeAvailable = installed && !latest.isEmpty() && !version.isEmpty()
                                 && versionNewer(latest, version);
            m_runMode = mode;
            m_runModeText = modeText;
            emit stateChanged();
        }, Qt::QueuedConnection);
    };

    // ===== 1. 探测 dsh 命令与包信息（一次往返）=====
    // dsh 是 npm 全局 bin 的符号链接，readlink -f 顺藤摸瓜找到包目录，
    // 从包 package.json 同时拿到包名和已装版本（不硬编码包名）。
    // 登录 shell 找不到时（典型：nvm 装了但没写进 .zshrc）搜索常见安装位置，
    // 并记录 bin 目录供后续找到同目录的 npm。
    QString out;
    const QString probeScript = QStringLiteral(
        "shopt -s nullglob 2>/dev/null; setopt null_glob 2>/dev/null; "
        "B=$(zsh -lc 'command -v dsh' 2>/dev/null || bash -lc 'command -v dsh' 2>/dev/null); "
        "if [ -z \"$B\" ]; then "
        "  for c in \"$HOME\"/.nvm/versions/node/*/bin/dsh "
        "           /usr/local/bin/dsh /opt/homebrew/bin/dsh "
        "           \"$HOME/.npm-global/bin/dsh\" /usr/bin/dsh \"$HOME/.local/bin/dsh\"; do "
        "    [ -e \"$c\" ] && B=\"$c\" && break; "
        "  done; "
        "fi; "
        "[ -n \"$B\" ] || exit 3; "
        "echo \"@@@BIN|$B\"; "
        "NPM=\"$(dirname \"$B\")/npm\"; [ -x \"$NPM\" ] && echo \"@@@NPM|$NPM\"; "
        "R=$(readlink -f \"$B\" 2>/dev/null || realpath \"$B\" 2>/dev/null || echo \"$B\"); "
        "PJ=$(dirname \"$R\")/../package.json; "
        "[ -f \"$PJ\" ] && cat \"$PJ\"");
    if (!backend->execRemote(probeScript, &out, 20000)) {
        setState(false, {}, {}, {}, QStringLiteral("unknown"),
                 QStringLiteral("未检测到 DSH"));
        post(QStringLiteral("  服务器上未找到 dsh 命令"));
        return;
    }

    QString npmPath;
    const int npmIdx = out.indexOf(QStringLiteral("@@@NPM|"));
    if (npmIdx >= 0) {
        npmPath = out.mid(npmIdx + 7);
        npmPath = npmPath.left(npmPath.indexOf('\n')).trimmed();
        QMetaObject::invokeMethod(this, [this, npmPath]() { m_npmPath = npmPath; },
                                  Qt::QueuedConnection);
    }

    const int sep = out.indexOf(QStringLiteral("@@@BIN|"));
    const int jsonStart = out.indexOf(QLatin1Char('{'), sep);
    QString pkgName, pkgVersion;
    if (jsonStart >= 0) {
        const QJsonObject obj = QJsonDocument::fromJson(out.mid(jsonStart).toUtf8()).object();
        pkgName = obj.value(QStringLiteral("name")).toString();
        pkgVersion = obj.value(QStringLiteral("version")).toString();
    }
    post(QStringLiteral("  ✓ DSH 已安装: %1 v%2").arg(pkgName, pkgVersion));

    // ===== 2. 探测运行方式（一次往返）=====
    // 先找 dsh 进程（特征：可执行文件路径以 /node 结尾 + 参数含 dsh；
    // 探测 shell 自身的 comm 不是 node，不会误匹配）。
    // 找到后再看它的父进程是不是 tmux 窗格 shell（区分 tmux 托管 vs 裸进程）；
    // 没找到进程再看 systemd 用户服务，最后判定未运行。
    QString runOut;
    backend->execRemote(QStringLiteral(
        "DLINE=$(ps -eo pid,args 2>/dev/null | grep -E '/node |/node$' | grep -i dsh | grep -v grep | head -1); "
        "if [ -n \"$DLINE\" ]; then "
        "  DPID=$(echo \"$DLINE\" | awk '{print $1}'); "
        "  TSESSION=\"\"; "
        "  if command -v tmux >/dev/null 2>&1; then "
        "    DPPID=$(ps -o ppid= -p \"$DPID\" 2>/dev/null | tr -d ' '); "
        "    TSESSION=$(tmux list-panes -a -F '#{session_name} #{pane_pid}' 2>/dev/null "
        "               | awk -v p=\"$DPPID\" '$2==p {print $1}' | head -1); "
        "  fi; "
        "  if [ -n \"$TSESSION\" ]; then "
        "    echo \"MODE|tmux|$TSESSION\"; "
        "  else "
        "    CWD=$(readlink /proc/$DPID/cwd 2>/dev/null); "
        "    echo \"MODE|process|$DPID|$CWD|$(echo \"$DLINE\" | cut -d' ' -f2-)\"; "
        "  fi; "
        "else "
        "  SVC=$(systemctl --user list-units --no-legend --type=service 2>/dev/null "
        "        | grep -i dsh | awk '{print $1}' | head -1); "
        "  if [ -n \"$SVC\" ]; then echo \"MODE|systemd|$SVC\"; else echo \"MODE|stopped|\"; fi; "
        "fi"),
        &runOut, 15000);

    QString mode = QStringLiteral("stopped");
    QString modeText = QStringLiteral("未在运行");
    QString procPid, procCwd, procArgs;

    const QString modeLine = runOut.split('\n', Qt::SkipEmptyParts)
                                 .filter(QStringLiteral("MODE|")).value(0);
    const QStringList f = modeLine.split('|');
    if (f.size() >= 3 && f[1] == QLatin1String("tmux")) {
        mode = QStringLiteral("tmux:") + f[2];
        modeText = QStringLiteral("tmux 会话: ") + f[2];
    } else if (f.size() >= 5 && f[1] == QLatin1String("process")) {
        procPid = f[2];
        procCwd = f[3];
        procArgs = f.mid(4).join('|');   // args 里可能含 |，拼回来
        mode = QStringLiteral("process:") + procPid;
        modeText = QStringLiteral("后台进程 (PID %1)").arg(procPid);
    } else if (f.size() >= 3 && f[1] == QLatin1String("systemd")) {
        mode = QStringLiteral("systemd:") + f[2];
        modeText = QStringLiteral("systemd 服务: ") + f[2];
    }
    QMetaObject::invokeMethod(this, [this, procPid, procCwd, procArgs]() {
        m_processPid = procPid;
        m_processCwd = procCwd;
        m_processArgs = procArgs;
    }, Qt::QueuedConnection);
    post(QStringLiteral("  运行状态: ") + modeText);

    // ===== 3. 查询 npm 最新版本（独立往返，失败降级）=====
    // 优先用与 dsh 同目录的 npm（登录 shell 可能没加载 nvm，npm 不在 PATH）
    QString latest;
    if (!pkgName.isEmpty()) {
        QString npmOut;
        bool ok = false;
        if (!npmPath.isEmpty())
            ok = backend->execRemote(
                QStringLiteral("'%1' view '%2' version 2>/dev/null").arg(npmPath, pkgName),
                &npmOut, 60000);
        if (!ok)
            ok = backend->execRemoteLoginShell(
                QStringLiteral("npm view '%1' version 2>/dev/null").arg(pkgName),
                &npmOut, 60000);
        if (ok) {
            // 过滤 npm notice 等杂讯，取最后一个非空行
            const QStringList lines = npmOut.split('\n', Qt::SkipEmptyParts);
            for (int i = lines.size() - 1; i >= 0; --i) {
                const QString line = lines[i].trimmed();
                if (!line.startsWith(QLatin1String("npm notice"))) {
                    latest = line;
                    break;
                }
            }
            post(QStringLiteral("  npm 最新版本: v") + latest);
        } else {
            post(QStringLiteral("  ⚠ 无法查询 npm 最新版本（网络/registry 不可达），仅显示已装版本"));
        }
    }

    QString cleanMode = mode;
    const int colon = mode.indexOf(':');
    if (colon > 0)
        cleanMode = mode.left(colon);
    setState(true, pkgVersion, pkgName, latest, mode, modeText);
}

void RemoteDshManager::upgrade()
{
    SshBackend *backend = sshBackend();
    if (!backend || m_busy || m_packageName.isEmpty())
        return;
    setBusy(true);
    appendLog(QStringLiteral("• 升级 DSH: npm install -g %1@latest …").arg(m_packageName));

    const QString pkg = m_packageName;
    const QString npmPath = m_npmPath;   // 优先用与 dsh 同目录的 npm
    auto future = QtConcurrent::run([this, backend, pkg, npmPath]() {
        auto post = [this](const QString &line) {
            QMetaObject::invokeMethod(this, [this, line]() { appendLog(line); },
                                      Qt::QueuedConnection);
        };
        QString out;
        bool ok;
        if (!npmPath.isEmpty()) {
            ok = backend->execRemote(
                QStringLiteral("'%1' install -g '%2@latest' 2>&1").arg(npmPath, pkg),
                &out, 300000);
        } else {
            ok = backend->execRemoteLoginShell(
                QStringLiteral("npm install -g '%1@latest' 2>&1").arg(pkg), &out, 300000);
        }
        for (const QString &line : out.split('\n', Qt::SkipEmptyParts))
            post(QStringLiteral("  ") + line.trimmed());
        if (ok) {
            post(QStringLiteral("✓ 升级完成，重新检测版本…"));
            QMetaObject::invokeMethod(this, [this]() {
                emit operationSucceeded(QStringLiteral("DSH 升级完成，重启后生效"));
            }, Qt::QueuedConnection);
            doRefresh(backend);
        } else {
            if (out.contains(QLatin1String("EACCES"))) {
                post(QStringLiteral("✗ 权限不足（EACCES）。建议在服务器配置 npm 用户级目录："));
                post(QStringLiteral("    mkdir ~/.npm-global && npm config set prefix ~/.npm-global"));
                post(QStringLiteral("  或使用 nvm 管理的 Node.js。"));
            } else {
                post(QStringLiteral("✗ 升级失败"));
            }
            QMetaObject::invokeMethod(this, [this]() {
                emit errorOccurred(QStringLiteral("DSH 升级失败，详见日志"));
            }, Qt::QueuedConnection);
        }
    });
    auto *watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher]() {
        watcher->deleteLater();
        setBusy(false);
        emit stateChanged();
    });
    watcher->setFuture(future);
}

void RemoteDshManager::restart()
{
    SshBackend *backend = sshBackend();
    if (!backend || m_busy)
        return;

    const QString mode = m_runMode;
    const QString procPid = m_processPid;
    const QString procCwd = m_processCwd;
    const QString procArgs = m_processArgs;

    if (mode.startsWith(QLatin1String("process")) && procArgs.isEmpty()) {
        emit errorOccurred(QStringLiteral("未捕获到进程启动命令，无法安全重启"));
        return;
    }
    if (!mode.startsWith(QLatin1String("tmux:"))
        && !mode.startsWith(QLatin1String("systemd:"))
        && !mode.startsWith(QLatin1String("process:"))) {
        emit errorOccurred(QStringLiteral("DSH 未在运行，或运行方式未知，无法重启"));
        return;
    }

    setBusy(true);
    appendLog(QStringLiteral("• 重启 DSH（%1）…").arg(m_runModeText));

    auto future = QtConcurrent::run([this, backend, mode, procPid, procCwd, procArgs]() {
        auto post = [this](const QString &line) {
            QMetaObject::invokeMethod(this, [this, line]() { appendLog(line); },
                                      Qt::QueuedConnection);
        };
        const QString target = mode.section(':', 1);
        QString out;
        bool ok = false;
        if (mode.startsWith(QLatin1String("tmux:"))) {
            // respawn-pane -k：杀掉窗格当前进程并用原始启动命令原地拉起
            ok = backend->execRemote(
                QStringLiteral("tmux respawn-pane -k -t '%1'").arg(target), &out, 15000);
        } else if (mode.startsWith(QLatin1String("systemd:"))) {
            ok = backend->execRemote(
                QStringLiteral("systemctl --user restart '%1'").arg(target), &out, 30000);
        } else {  // process: kill 后用捕获的原命令+原工作目录重新拉起
            const QString cdCmd = procCwd.isEmpty()
                ? QStringLiteral("cd \"$HOME\"")
                : QStringLiteral("cd '%1'").arg(procCwd);
            // 关键：必须 setsid 脱离 ssh 会话进程组——
            // 仅 nohup 时 sshd 会话关闭会向进程组发信号把新进程带走（实测踩坑）。
            // 原命令行从 ps 捕获，直接作为 shell 命令重放（dsh 的启动参数无特殊字符）
            const QString cmd = QStringLiteral(
                "kill '%1' 2>/dev/null; sleep 2; %2; "
                "if command -v setsid >/dev/null 2>&1; then "
                "  setsid nohup %3 </dev/null >/dev/null 2>&1 & "
                "else "
                "  nohup %3 </dev/null >/dev/null 2>&1 & "
                "fi; "
                "sleep 3").arg(procPid, cdCmd, procArgs);
            ok = backend->execRemote(cmd, &out, 20000);
        }
        if (!ok) {
            post(QStringLiteral("✗ 重启命令失败: ") + out.trimmed());
            QMetaObject::invokeMethod(this, [this]() {
                emit errorOccurred(QStringLiteral("重启失败，详见日志"));
            }, Qt::QueuedConnection);
            return;
        }
        // 等待后验证进程是否还活着
        post(QStringLiteral("  验证运行状态…"));
        QString verify;
        if (mode.startsWith(QLatin1String("tmux:"))) {
            ok = backend->execRemote(
                QStringLiteral("tmux list-panes -t '%1' -F '#{pane_current_command}' 2>/dev/null"
                               " | grep -qE 'node|MainThread'").arg(target),
                &verify, 10000);
        } else if (mode.startsWith(QLatin1String("systemd:"))) {
            ok = backend->execRemote(
                QStringLiteral("systemctl --user is-active --quiet '%1'").arg(target),
                &verify, 10000);
        } else {
            ok = backend->execRemote(
                QStringLiteral("ps -eo pid,args 2>/dev/null | grep -E '/node |/node$'"
                               " | grep -i dsh | grep -v grep"),
                &verify, 10000) && !verify.trimmed().isEmpty();
            if (ok) {
                // 二次确认：sshd 关闭上一个会话时可能清理进程组（约数秒后），
                // 延时再验一次，确保进程真正存活
                post(QStringLiteral("  二次确认稳定性…"));
                QThread::sleep(6);
                verify.clear();
                ok = backend->execRemote(
                    QStringLiteral("ps -eo pid,args 2>/dev/null | grep -E '/node |/node$'"
                                   " | grep -i dsh | grep -v grep"),
                    &verify, 10000) && !verify.trimmed().isEmpty();
            }
        }
        if (ok) {
            post(QStringLiteral("✓ 重启成功，DSH 已在运行"));
            QMetaObject::invokeMethod(this, [this]() {
                emit operationSucceeded(QStringLiteral("DSH 重启成功"));
            }, Qt::QueuedConnection);
            doRefresh(backend);
        } else {
            post(QStringLiteral("✗ 重启后未检测到 DSH 进程，请登录服务器查看"));
            QMetaObject::invokeMethod(this, [this]() {
                emit errorOccurred(QStringLiteral("重启后未检测到 DSH 进程"));
            }, Qt::QueuedConnection);
        }
    });
    auto *watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher]() {
        watcher->deleteLater();
        setBusy(false);
    });
    watcher->setFuture(future);
}
