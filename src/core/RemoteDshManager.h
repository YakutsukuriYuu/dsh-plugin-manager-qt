#pragma once

#include <QObject>
#include <QString>

class PluginManager;
class SshBackend;

/**
 * 远程服务器 DSH 服务管理器。
 *
 * 对已连接的远程服务器（SshBackend）上的 DSH（npm -g 安装）提供：
 *  - 版本检查：已装版本（解析 dsh 符号链接定位全局 package.json）
 *              + npm registry 最新版本（npm view，失败降级不报错）
 *  - 升级：    npm install -g <包名>@latest（登录 shell，拿完整 PATH）
 *  - 重启：    自动探测运行方式——
 *              tmux 会话（respawn-pane -k 原地重启）>
 *              systemd 用户服务（systemctl --user restart）>
 *              裸进程（无法安全重启，仅提示）
 *
 * 所有操作在 QtConcurrent 工作线程执行，主线程不卡；
 * 操作过程输出追加到 log 属性，UI 可实时展示。
 */
class RemoteDshManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(bool dshInstalled READ dshInstalled NOTIFY stateChanged)
    Q_PROPERTY(QString dshVersion READ dshVersion NOTIFY stateChanged)
    Q_PROPERTY(QString packageName READ packageName NOTIFY stateChanged)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY stateChanged)
    Q_PROPERTY(bool upgradeAvailable READ upgradeAvailable NOTIFY stateChanged)
    Q_PROPERTY(QString runMode READ runMode NOTIFY stateChanged)
    Q_PROPERTY(QString runModeText READ runModeText NOTIFY stateChanged)
    Q_PROPERTY(bool running READ running NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString log READ log NOTIFY logChanged)

public:
    explicit RemoteDshManager(PluginManager *pluginManager, QObject *parent = nullptr);

    bool active() const;   // 当前是否处于远程连接状态
    bool dshInstalled() const { return m_dshInstalled; }
    QString dshVersion() const { return m_dshVersion; }
    QString packageName() const { return m_packageName; }
    QString latestVersion() const { return m_latestVersion; }
    bool upgradeAvailable() const { return m_upgradeAvailable; }
    QString runMode() const { return m_runMode; }     // tmux / systemd / process / stopped / unknown
    QString runModeText() const { return m_runModeText; } // 展示用描述
    bool running() const { return m_runMode != QLatin1String("stopped")
                                && m_runMode != QLatin1String("unknown")
                                && !m_runMode.isEmpty(); }
    bool busy() const { return m_busy; }
    QString log() const { return m_log; }

    // 连接成功后调用：一次往返探测版本 + 运行方式，再异步查 npm 最新版本
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void upgrade();
    Q_INVOKABLE void restart();
    // 断开远程 / 回本机时清空状态
    Q_INVOKABLE void clear();

signals:
    void activeChanged();
    void stateChanged();
    void busyChanged();
    void logChanged();
    void errorOccurred(const QString &message);
    void operationSucceeded(const QString &message);

private:
    SshBackend *sshBackend() const;   // 当前后端是 SshBackend 时返回之，否则 nullptr
    void setBusy(bool busy);
    void appendLog(const QString &line);
    // 在工作线程内刷新状态（调用方保证 busy 已置位）
    void doRefresh(SshBackend *backend);

    PluginManager *m_pluginManager;
    bool m_dshInstalled = false;
    QString m_dshVersion;
    QString m_packageName;
    QString m_npmPath;   // 与 dsh 同目录的 npm 绝对路径（登录 shell 无 npm 时用）
    QString m_latestVersion;
    // 裸进程模式下捕获的重启信息（kill + 原命令重放）
    QString m_processPid;
    QString m_processCwd;
    QString m_processArgs;
    bool m_upgradeAvailable = false;
    QString m_runMode;
    QString m_runModeText;
    bool m_busy = false;
    QString m_log;
};
