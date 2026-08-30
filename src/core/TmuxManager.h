#pragma once

#include <QObject>
#include <QVariantList>
#include <QString>

/**
 * tmux 会话管理器。
 *
 * 通过 tmux CLI 管理会话：
 *  - list-sessions / list-panes  读取会话与窗格状态
 *  - capture-pane                抓取会话输出
 *  - send-keys                   发送按键（重启进程）
 *  - new-session / kill-session  创建与关闭会话
 *
 * DSH 会话识别：窗格当前命令为 node 且工作目录包含 ".dsh"。
 * 附加终端：通过 osascript 让 Terminal.app 执行 tmux attach。
 */
class TmuxManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList sessions READ sessions NOTIFY sessionsChanged)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString dshSession READ dshSession NOTIFY sessionsChanged)

public:
    explicit TmuxManager(QObject *parent = nullptr);

    QVariantList sessions() const { return m_sessions; }
    bool available() const { return m_available; }
    bool loading() const { return m_loading; }
    QString dshSession() const { return m_dshSession; }

    // QML 可调用的操作
    Q_INVOKABLE void refresh();
    Q_INVOKABLE QString sessionOutput(const QString &name, int lines = 100);
    Q_INVOKABLE void attachSession(const QString &name);
    Q_INVOKABLE void killSession(const QString &name);
    Q_INVOKABLE void createSession(const QString &name, const QString &command, const QString &workdir);
    Q_INVOKABLE void restartSession(const QString &name, const QString &command);

signals:
    void sessionsChanged();
    void availableChanged();
    void loadingChanged();
    void errorOccurred(const QString &message);
    void operationSucceeded(const QString &message);

private:
    bool runTmux(const QStringList &args, QString *output = nullptr);
    QString findTmux() const;
    void setLoading(bool loading);

    QString m_tmuxPath;
    QVariantList m_sessions;
    QString m_dshSession;
    bool m_available = false;
    bool m_loading = false;
};
