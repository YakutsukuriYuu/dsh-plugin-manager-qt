#include "TmuxManager.h"

#include <QDateTime>
#include <QProcess>
#include <QStandardPaths>
#include <QFile>
#include <QDir>
#include <QRegularExpression>
#include <QTimer>

TmuxManager::TmuxManager(QObject *parent)
    : QObject(parent)
{
    m_tmuxPath = findTmux();
    m_available = !m_tmuxPath.isEmpty();
    emit availableChanged();
    refresh();
}

QString TmuxManager::findTmux() const
{
    const QString found = QStandardPaths::findExecutable("tmux");
    if (!found.isEmpty())
        return found;
    // GUI 应用 PATH 较窄，检查常见位置
    for (const QString &path : {"/opt/homebrew/bin/tmux", "/usr/local/bin/tmux"}) {
        if (QFile::exists(path))
            return path;
    }
    return QString();
}

bool TmuxManager::runTmux(const QStringList &args, QString *output)
{
    if (!m_available) {
        if (output) *output = "找不到 tmux 命令";
        return false;
    }

    QProcess process;
    process.start(m_tmuxPath, args);
    if (!process.waitForFinished(15000)) {
        if (output) *output = "命令执行超时";
        return false;
    }

    const QString out = QString::fromUtf8(process.readAllStandardOutput());
    const QString err = QString::fromUtf8(process.readAllStandardError());
    if (output)
        *output = out.isEmpty() ? err.trimmed() : out;
    return process.exitCode() == 0;
}

void TmuxManager::refresh()
{
    if (!m_available)
        return;

    setLoading(true);

    QString output;
    QVariantList found;
    QString dshSession;

    // 格式：name|windows|created_epoch|attached
    if (runTmux({"list-sessions", "-F",
                 "#{session_name}|#{session_windows}|#{session_created}|#{session_attached}"},
                &output)) {
        const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            const QStringList parts = line.split('|');
            if (parts.size() < 4)
                continue;

            QVariantMap session;
            const QString name = parts[0];
            session["name"] = name;
            session["windows"] = parts[1].toInt();
            session["created"] = QDateTime::fromSecsSinceEpoch(parts[2].toLongLong())
                                     .toString("MM-dd HH:mm");
            session["attached"] = parts[3] == "1";

            // 读取第一个窗格的命令与工作目录，用于识别 DSH 会话
            QString paneInfo;
            QString command, workdir;
            if (runTmux({"list-panes", "-t", name, "-F",
                         "#{pane_current_command}|#{pane_current_path}"},
                        &paneInfo)) {
                const QStringList paneLines = paneInfo.split('\n', Qt::SkipEmptyParts);
                if (!paneLines.isEmpty()) {
                    const QStringList pp = paneLines.first().split('|');
                    if (pp.size() >= 2) {
                        command = pp[0];
                        workdir = pp[1];
                    }
                }
            }
            session["command"] = command;
            session["workdir"] = workdir;

            const bool isDsh = (command == "node" && workdir.contains(".dsh"));
            session["isDsh"] = isDsh;
            if (isDsh && dshSession.isEmpty())
                dshSession = name;

            found << session;
        }
    }

    m_sessions = found;
    m_dshSession = dshSession;
    setLoading(false);
    emit sessionsChanged();
}

QString TmuxManager::sessionOutput(const QString &name, int lines)
{
    QString output;
    runTmux({"capture-pane", "-t", name, "-p", "-S", QString::number(-lines)}, &output);
    return output;
}

void TmuxManager::attachSession(const QString &name)
{
    // 让 Terminal.app 新开窗口执行 tmux attach
    const QString script1 = QStringLiteral(
        "tell application \"Terminal\" to do script \"%1 attach -t %2\"")
        .arg(m_tmuxPath, name);
    QProcess::startDetached("osascript", {"-e", script1});
    QProcess::startDetached("osascript",
                            {"-e", "tell application \"Terminal\" to activate"});
}

void TmuxManager::killSession(const QString &name)
{
    QString output;
    if (runTmux({"kill-session", "-t", name}, &output)) {
        emit operationSucceeded("已关闭会话: " + name);
        refresh();
    } else {
        emit errorOccurred("关闭会话失败: " + name + "\n" + output);
    }
}

void TmuxManager::createSession(const QString &name, const QString &command, const QString &workdir)
{
    // tmux 会话名不允许空格和点号
    QString safeName = name.trimmed();
    safeName.replace(QRegularExpression("[\\s.]"), "-");
    if (safeName.isEmpty()) {
        emit errorOccurred("会话名不能为空");
        return;
    }

    QStringList args = {"new-session", "-d", "-s", safeName};
    if (!workdir.isEmpty())
        args << "-c" << workdir;
    if (!command.isEmpty())
        args << command;

    QString output;
    if (runTmux(args, &output)) {
        emit operationSucceeded("已创建会话: " + safeName);
        refresh();
    } else {
        emit errorOccurred("创建会话失败: " + safeName + "\n" + output);
    }
}

void TmuxManager::restartSession(const QString &name, const QString &command)
{
    // 先 Ctrl-C 终止当前进程，稍后重新发送启动命令
    runTmux({"send-keys", "-t", name, "C-c"});

    QTimer::singleShot(600, this, [this, name, command]() {
        runTmux({"send-keys", "-t", name, command, "Enter"});
        emit operationSucceeded("已重启会话: " + name);
        // 等进程起来后再刷新状态
        QTimer::singleShot(1500, this, &TmuxManager::refresh);
    });
}

void TmuxManager::setLoading(bool loading)
{
    if (m_loading != loading) {
        m_loading = loading;
        emit loadingChanged();
    }
}
