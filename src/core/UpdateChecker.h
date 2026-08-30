#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>

/**
 * 更新检查器。
 *
 * 通过 GitHub Releases API 查询最新发布版本并与当前版本比较：
 *   GET https://api.github.com/repos/<owner>/<repo>/releases/latest
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

public:
    explicit UpdateChecker(QObject *parent = nullptr);

    bool checking() const { return m_checking; }
    bool updateAvailable() const { return m_updateAvailable; }
    QString currentVersion() const { return m_currentVersion; }
    QString latestVersion() const { return m_latestVersion; }
    QString releaseUrl() const { return m_releaseUrl; }
    QString releaseNotes() const { return m_releaseNotes; }
    QString statusText() const { return m_statusText; }

    // silent=true 时失败不显示错误文案（用于启动静默检查）
    Q_INVOKABLE void check(bool silent = false);
    Q_INVOKABLE void openReleasePage();

signals:
    void checkingChanged();
    void updateAvailableChanged();
    void latestVersionChanged();
    void releaseUrlChanged();
    void releaseNotesChanged();
    void statusTextChanged();

private:
    static bool isNewer(const QString &latest, const QString &current);
    void setChecking(bool checking);
    void setStatusText(const QString &text);

    QNetworkAccessManager *m_nam;
    QNetworkReply *m_reply = nullptr;
    QString m_currentVersion;
    QString m_latestVersion;
    QString m_releaseUrl;
    QString m_releaseNotes;
    QString m_statusText;
    bool m_checking = false;
    bool m_updateAvailable = false;
};
