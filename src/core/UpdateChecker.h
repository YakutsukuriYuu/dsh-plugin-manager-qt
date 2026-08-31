#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>

/**
 * 更新检查器 + 应用内自动更新。
 *
 * 检查：GitHub Releases API 查询最新发布版本并与当前版本比较。
 * 更新：下载 Release 中的 DMG 附件（带进度回调），然后在工作线程完成
 *       挂载 → 原地替换 .app → 卸载 → 重启，失败自动回滚旧版。
 *
 * 版本比较：按 x.y.z 数字段逐位比较（忽略 v 前缀与后缀）。
 */
class UpdateChecker : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool checking READ checking NOTIFY checkingChanged)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY updateAvailableChanged)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY latestVersionChanged)
    Q_PROPERTY(QString releaseUrl READ releaseUrl NOTIFY releaseUrlChanged)
    Q_PROPERTY(QString releaseNotes READ releaseNotes NOTIFY releaseNotesChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

    // 下载/安装状态
    Q_PROPERTY(bool downloading READ downloading NOTIFY downloadingChanged)
    Q_PROPERTY(int downloadProgress READ downloadProgress NOTIFY downloadProgressChanged)
    Q_PROPERTY(bool installing READ installing NOTIFY installingChanged)
    Q_PROPERTY(QString installLog READ installLog NOTIFY installLogChanged)

public:
    explicit UpdateChecker(QObject *parent = nullptr);

    bool checking() const { return m_checking; }
    bool updateAvailable() const { return m_updateAvailable; }
    QString currentVersion() const { return m_currentVersion; }
    QString latestVersion() const { return m_latestVersion; }
    QString releaseUrl() const { return m_releaseUrl; }
    QString releaseNotes() const { return m_releaseNotes; }
    QString statusText() const { return m_statusText; }

    bool downloading() const { return m_downloading; }
    int downloadProgress() const { return m_downloadProgress; }
    bool installing() const { return m_installing; }
    QString installLog() const { return m_installLog; }

    // silent=true 时失败不显示错误文案（用于启动静默检查）
    Q_INVOKABLE void check(bool silent = false);
    Q_INVOKABLE void openReleasePage();

    // 应用内更新：下载 DMG → 挂载 → 替换 .app → 重启
    Q_INVOKABLE void downloadAndInstall();

signals:
    void checkingChanged();
    void updateAvailableChanged();
    void latestVersionChanged();
    void releaseUrlChanged();
    void releaseNotesChanged();
    void statusTextChanged();

    void downloadingChanged();
    void downloadProgressChanged();
    void installingChanged();
    void installLogChanged();

private:
    static bool isNewer(const QString &latest, const QString &current);
    void setChecking(bool checking);
    void setStatusText(const QString &text);
    void setDownloading(bool v);
    void setDownloadProgress(int v);
    void setInstalling(bool v);
    void appendLog(const QString &line);

    // 从 Release JSON 中找到 DMG 附件的下载地址
    QString findDmgUrl(const QJsonObject &releaseObj) const;

    QNetworkAccessManager *m_nam;
    QNetworkReply *m_reply = nullptr;
    QString m_currentVersion;
    QString m_latestVersion;
    QString m_releaseUrl;
    QString m_releaseNotes;
    QString m_statusText;
    bool m_checking = false;
    bool m_updateAvailable = false;

    bool m_downloading = false;
    int m_downloadProgress = 0;
    bool m_installing = false;
    QString m_installLog;
};
