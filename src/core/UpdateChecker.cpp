#include "UpdateChecker.h"

#include <QCoreApplication>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDesktopServices>
#include <QUrl>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QTimer>
#include <QtConcurrent>
#include <QFutureWatcher>

namespace {
const char kApiUrl[] =
    "https://api.github.com/repos/YakutsukuriYuu/dsh-plugin-manager-qt/releases/latest";
const char kAppName[] = "DSH Plugin Manager";

// 当前运行中的应用 .app 路径（macOS）：从可执行文件路径向上找 .app
QString currentAppBundlePath()
{
    const QString exePath = QCoreApplication::applicationFilePath();
    const int idx = exePath.lastIndexOf(QStringLiteral(".app/Contents/MacOS"));
    if (idx > 0)
        return exePath.left(idx + 4);  // 含 ".app"
    return exePath;
}
}

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    m_currentVersion = QCoreApplication::applicationVersion();
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

void UpdateChecker::setDownloading(bool v)
{
    if (m_downloading != v) {
        m_downloading = v;
        emit downloadingChanged();
    }
}

void UpdateChecker::setDownloadProgress(int v)
{
    if (m_downloadProgress != v) {
        m_downloadProgress = v;
        emit downloadProgressChanged();
    }
}

void UpdateChecker::setInstalling(bool v)
{
    if (m_installing != v) {
        m_installing = v;
        emit installingChanged();
    }
}

void UpdateChecker::appendLog(const QString &line)
{
    m_installLog += line + "\n";
    emit installLogChanged();
}

QString UpdateChecker::findDmgUrl(const QJsonObject &releaseObj) const
{
    const QJsonArray assets = releaseObj.value("assets").toArray();
    for (const auto &a : assets) {
        const QJsonObject asset = a.toObject();
        if (asset.value("name").toString().endsWith(".dmg"))
            return asset.value("browser_download_url").toString();
    }
    return QString();
}

void UpdateChecker::check(bool silent)
{
    if (m_checking)
        return;

    if (!silent)
        setStatusText("正在检查更新…");
    setChecking(true);

    QNetworkRequest request(QUrl(QString::fromLatin1(kApiUrl)));
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

// ===== 应用内更新 =====

void UpdateChecker::downloadAndInstall()
{
    if (m_downloading || m_installing || !m_updateAvailable)
        return;

    m_installLog.clear();
    emit installLogChanged();
    setDownloading(true);
    setDownloadProgress(0);
    setStatusText("正在下载更新…");

    // 1) 请求 Release JSON 获取 DMG 附件地址
    QNetworkRequest request(QUrl(QString::fromLatin1(kApiUrl)));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("dsh-plugin-manager/%1").arg(m_currentVersion));
    request.setRawHeader("Accept", "application/vnd.github+json");

    QNetworkReply *metaReply = m_nam->get(request);
    connect(metaReply, &QNetworkReply::finished, this, [this, metaReply]() {
        metaReply->deleteLater();

        if (metaReply->error() != QNetworkReply::NoError) {
            setDownloading(false);
            setStatusText("获取版本信息失败：" + metaReply->errorString());
            appendLog("✗ " + m_statusText);
            return;
        }

        const QJsonObject obj = QJsonDocument::fromJson(metaReply->readAll()).object();
        const QString dmgUrl = findDmgUrl(obj);
        if (dmgUrl.isEmpty()) {
            setDownloading(false);
            setStatusText("该版本未提供 DMG 附件");
            appendLog("✗ " + m_statusText);
            return;
        }

        // 2) 下载 DMG 到临时文件（进度回调）
        const QString dmgPath = QDir::temp().absoluteFilePath(
            QStringLiteral("dsh-pm-update-%1.dmg").arg(m_latestVersion));
        QFile::remove(dmgPath);

        QNetworkRequest req{QUrl(dmgUrl)};
        req.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("dsh-plugin-manager/%1").arg(m_currentVersion));
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

        QNetworkReply *dl = m_nam->get(req);
        auto *file = new QFile(dmgPath);
        if (!file->open(QIODevice::WriteOnly)) {
            delete file;
            setDownloading(false);
            setStatusText("无法创建临时下载文件");
            appendLog("✗ " + m_statusText);
            return;
        }

        connect(dl, &QNetworkReply::downloadProgress, this,
                [this, file, dl](qint64 received, qint64 total) {
            Q_UNUSED(received)
            file->write(dl->readAll());
            if (total > 0)
                setDownloadProgress(static_cast<int>(received * 100 / total));
        });

        connect(dl, &QNetworkReply::finished, this, [this, dl, file, dmgPath]() {
            file->write(dl->readAll());
            file->close();
            file->deleteLater();
            dl->deleteLater();

            if (dl->error() != QNetworkReply::NoError) {
                QFile::remove(dmgPath);
                setDownloading(false);
                setStatusText("下载失败：" + dl->errorString());
                appendLog("✗ " + m_statusText);
                return;
            }

            setDownloading(false);
            setInstalling(true);
            setStatusText("正在安装更新…");
            appendLog("✓ 下载完成（" +
                      QString::number(QFileInfo(dmgPath).size() / 1024 / 1024) + " MB）");

            // 3) 挂载/替换/回滚/重启 —— 工作线程执行
            const QString appPath = currentAppBundlePath();
            auto *watcher = new QFutureWatcher<bool>(this);
            connect(watcher, &QFutureWatcher<bool>::finished, this,
                    [this, watcher, dmgPath, appPath]() {
                const bool ok = watcher->result();
                watcher->deleteLater();

                if (ok) {
                    setInstalling(false);
                    setStatusText("更新完成，即将重启…");
                    appendLog("✓ 安装完成，正在重启应用…");
                    // 重启：启动新版 → 退出当前实例
                    QTimer::singleShot(1200, this, [appPath]() {
                        QProcess::startDetached("open", {appPath});
                        QCoreApplication::exit(0);
                    });
                } else {
                    setInstalling(false);
                    setStatusText("安装失败，已回滚旧版本");
                }
            });

            QFuture<bool> future = QtConcurrent::run([this, dmgPath, appPath]() -> bool {
                auto run = [](const QString &cmd, const QStringList &args,
                              int timeoutMs = 60000) -> QPair<bool, QString> {
                    QProcess p;
                    p.start(cmd, args);
                    if (!p.waitForFinished(timeoutMs)) {
                        p.kill();
                        p.waitForFinished(2000);
                        return {false, QStringLiteral("超时")};
                    }
                    const QString out = QString::fromUtf8(
                        p.readAllStandardOutput() + p.readAllStandardError());
                    return {p.exitCode() == 0, out.trimmed()};
                };
                auto post = [this](const QString &line) {
                    QMetaObject::invokeMethod(this, [this, line]() { appendLog(line); },
                                              Qt::QueuedConnection);
                };

                const QString mountPoint = QStringLiteral("/Volumes/DSH Plugin Manager");
                const QString backupPath = QDir::temp().absoluteFilePath(
                    QStringLiteral("DSH-Plugin-Manager-backup.app"));

                // 备份旧版
                post("• 备份当前版本…");
                {
                    QProcess p;
                    p.start("cp", {"-R", appPath, backupPath});
                    if (!p.waitForFinished(120000) || p.exitCode() != 0) {
                        post("  ✗ 备份失败");
                        return false;
                    }
                }

                // 挂载 DMG
                post("• 挂载 DMG…");
                {
                    auto [ok, out] = run("hdiutil", {"attach", dmgPath, "-nobrowse", "-quiet"});
                    if (!ok) {
                        post("  ✗ 挂载失败: " + out);
                        return false;
                    }
                }

                // 原地替换
                post("• 替换应用…");
                {
                    QProcess p;
                    p.start("rm", {"-rf", appPath});
                    p.waitForFinished(60000);
                    auto [ok, out] = run("cp", {"-R",
                        mountPoint + "/" + kAppName + ".app",
                        QFileInfo(appPath).absolutePath() + "/"}, 120000);
                    if (!ok) {
                        post("  ✗ 替换失败: " + out + "，已回滚旧版本");
                        QProcess::startDetached("cp",
                            {"-R", backupPath, QFileInfo(appPath).absolutePath() + "/"});
                        run("hdiutil", {"detach", mountPoint, "-quiet"});
                        return false;
                    }
                }

                // 移除隔离属性（新版可正常启动）
                run("xattr", {"-cr", appPath});

                // 卸载 DMG、清理下载文件
                run("hdiutil", {"detach", mountPoint, "-quiet"});
                QFile::remove(dmgPath);

                post("✓ 更新安装完成");
                return true;
            });
            watcher->setFuture(future);
        });
    });
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
