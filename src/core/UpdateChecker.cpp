#include "UpdateChecker.h"

#include <QCoreApplication>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDesktopServices>
#include <QUrl>

namespace {
const char kApiUrl[] =
    "https://api.github.com/repos/YakutsukuriYuu/dsh-plugin-manager-qt/releases/latest";
}

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    m_currentVersion = QCoreApplication::applicationVersion();
}

void UpdateChecker::check(bool silent)
{
    if (m_checking)
        return;

    if (!silent)
        setStatusText("正在检查更新…");
    setChecking(true);

    QNetworkRequest request(QUrl(QString::fromLatin1(kApiUrl)));
    // GitHub API 要求 User-Agent
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("dsh-plugin-manager/%1").arg(m_currentVersion));
    request.setRawHeader("Accept", "application/vnd.github+json");

    m_reply = m_nam->get(request);
    connect(m_reply, &QNetworkReply::finished, this, [this, silent]() {
        m_reply->deleteLater();

        if (m_reply->error() != QNetworkReply::NoError) {
            setChecking(false);
            if (!silent)
                setStatusText("检查失败：" + m_reply->errorString());
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(m_reply->readAll());
        const QJsonObject obj = doc.object();

        // 没有任何 Release 时 GitHub 返回 404 + {"message":"Not Found"}
        if (obj.contains("message")) {
            setChecking(false);
            if (!silent)
                setStatusText("暂未发布任何版本");
            return;
        }

        m_latestVersion = obj.value("tag_name").toString();
        m_releaseUrl = obj.value("html_url").toString();
        m_releaseNotes = obj.value("body").toString();
        emit latestVersionChanged();
        emit releaseUrlChanged();
        emit releaseNotesChanged();

        const bool newer = isNewer(m_latestVersion, m_currentVersion);
        if (newer != m_updateAvailable) {
            m_updateAvailable = newer;
            emit updateAvailableChanged();
        }

        setChecking(false);
        if (newer)
            setStatusText("发现新版本 " + m_latestVersion);
        else if (!silent)
            setStatusText("当前已是最新版本");
    });
}

void UpdateChecker::openReleasePage()
{
    if (!m_releaseUrl.isEmpty())
        QDesktopServices::openUrl(QUrl(m_releaseUrl));
}

bool UpdateChecker::isNewer(const QString &latest, const QString &current)
{
    // 去掉 v/V 前缀，按 . 拆分数字段逐位比较
    auto parse = [](const QString &ver) {
        QString v = ver;
        if (v.startsWith('v') || v.startsWith('V'))
            v = v.mid(1);
        // 去掉预发布后缀（如 0.1.1-rc.1 → 0.1.1）
        const int dash = v.indexOf('-');
        if (dash > 0)
            v = v.left(dash);
        QList<int> parts;
        for (const QString &p : v.split('.'))
            parts << p.toInt();
        return parts;
    };

    const QList<int> a = parse(latest);
    const QList<int> b = parse(current);
    const int n = qMax(a.size(), b.size());
    for (int i = 0; i < n; ++i) {
        const int av = i < a.size() ? a[i] : 0;
        const int bv = i < b.size() ? b[i] : 0;
        if (av != bv)
            return av > bv;
    }
    return false;
}

void UpdateChecker::setChecking(bool checking)
{
    if (m_checking != checking) {
        m_checking = checking;
        emit checkingChanged();
    }
}

void UpdateChecker::setStatusText(const QString &text)
{
    if (m_statusText != text) {
        m_statusText = text;
        emit statusTextChanged();
    }
}
