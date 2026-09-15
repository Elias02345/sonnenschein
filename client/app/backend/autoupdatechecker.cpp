#include "autoupdatechecker.h"

#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

AutoUpdateChecker::AutoUpdateChecker(QObject *parent) :
    QObject(parent)
{
    m_Nam = new QNetworkAccessManager(this);

    // Never communicate over HTTP
    m_Nam->setStrictTransportSecurityEnabled(true);

    // Allow HTTP redirects
    m_Nam->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);

    connect(m_Nam, &QNetworkAccessManager::finished,
            this, &AutoUpdateChecker::handleUpdateCheckRequestFinished);

    QString currentVersion(VERSION_STR);
    qDebug() << "Current Moonlight version:" << currentVersion;
    parseStringToVersionQuad(currentVersion, m_CurrentVersionQuad);

    // Should at least have a 1.0-style version number
    Q_ASSERT(m_CurrentVersionQuad.count() > 1);
}

void AutoUpdateChecker::start()
{
    if (!m_Nam) {
        Q_ASSERT(m_Nam);
        return;
    }

#if defined(Q_OS_WIN32) || defined(Q_OS_DARWIN) || defined(STEAM_LINK) || defined(APP_IMAGE) // Only run update checker on platforms without auto-update
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0) && QT_VERSION < QT_VERSION_CHECK(5, 15, 1) && !defined(QT_NO_BEARERMANAGEMENT)
    // HACK: Set network accessibility to work around QTBUG-80947 (introduced in Qt 5.14.0 and fixed in Qt 5.15.1)
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    m_Nam->setNetworkAccessible(QNetworkAccessManager::Accessible);
    QT_WARNING_POP
#endif

    // Sonnenschein: check our GitHub releases for the latest client build.
    QUrl url("https://api.github.com/repos/Elias02345/sonnenschein/releases/latest");
    QNetworkRequest request(url);
    // GitHub's API rejects requests without a User-Agent
    request.setHeader(QNetworkRequest::UserAgentHeader, "sonnenschein-client");
    request.setRawHeader("Accept", "application/vnd.github+json");
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
#else
    request.setAttribute(QNetworkRequest::HTTP2AllowedAttribute, true);
#endif
    m_Nam->get(request);
#endif
}

void AutoUpdateChecker::parseStringToVersionQuad(QString& string, QVector<int>& version)
{
    QStringList list = string.split('.');
    for (const QString& component : std::as_const(list)) {
        // Tolerate suffixed components like "3-test" (tag v0.0.3-test)
        int value = 0;
        for (const QChar& c : component) {
            if (!c.isDigit()) {
                break;
            }
            value = value * 10 + c.digitValue();
        }
        version.append(value);
    }
}

QString AutoUpdateChecker::getPlatform()
{
#if defined(STEAM_LINK)
    return QStringLiteral("steamlink");
#elif defined(APP_IMAGE)
    return QStringLiteral("appimage");
#elif defined(Q_OS_DARWIN) && QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Qt 6 changed this from 'osx' to 'macos'. Use the old one
    // to be consistent (and not require another entry in the manifest).
    return QStringLiteral("osx");
#else
    return QSysInfo::productType();
#endif
}

int AutoUpdateChecker::compareVersion(QVector<int>& version1, QVector<int>& version2) {
    for (int i = 0;; i++) {
        int v1Val = 0;
        int v2Val = 0;

        // Treat missing decimal places as 0
        if (i < version1.count()) {
            v1Val = version1[i];
        }
        if (i < version2.count()) {
            v2Val = version2[i];
        }
        if (i >= version1.count() && i >= version2.count()) {
            // Equal versions
            return 0;
        }

        if (v1Val < v2Val) {
            return -1;
        }
        else if (v1Val > v2Val) {
            return 1;
        }
    }
}

void AutoUpdateChecker::handleUpdateCheckRequestFinished(QNetworkReply* reply)
{
    Q_ASSERT(reply->isFinished());

    // Delete the QNetworkAccessManager to free resources and
    // prevent the bearer plugin from polling in the background.
    m_Nam->deleteLater();
    m_Nam = nullptr;

    if (reply->error() == QNetworkReply::NoError) {
        QTextStream stream(reply);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        stream.setEncoding(QStringConverter::Utf8);
#else
        stream.setCodec("UTF-8");
#endif

        // Read all data and queue the reply for deletion
        QString jsonString = stream.readAll();
        reply->deleteLater();

        QJsonParseError error;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonString.toUtf8(), &error);
        if (jsonDoc.isNull() || !jsonDoc.isObject()) {
            qWarning() << "GitHub release response malformed:" << error.errorString();
            return;
        }

        QJsonObject release = jsonDoc.object();
        if (!release.contains("tag_name") || !release["tag_name"].isString()) {
            qWarning() << "GitHub release response missing tag_name";
            return;
        }

        // Tag scheme: vX.Y.Z or vX.Y.Z-suffix
        QString latestVersion = release["tag_name"].toString();
        if (latestVersion.startsWith('v')) {
            latestVersion.remove(0, 1);
        }

        QVector<int> latestVersionQuad;
        parseStringToVersionQuad(latestVersion, latestVersionQuad);

        if (compareVersion(m_CurrentVersionQuad, latestVersionQuad) >= 0) {
            qDebug() << "Client is up to date (latest release:" << latestVersion << ")";
            return;
        }

        // Pick the direct download for this platform so the update button
        // fetches the right asset immediately (Windows: the installer).
        QString platform = getPlatform();
        QString downloadUrl = release["html_url"].toString();
        const QJsonArray assets = release["assets"].toArray();
        for (const auto& assetEntry : assets) {
            QJsonObject asset = assetEntry.toObject();
            QString name = asset["name"].toString();
            QString url = asset["browser_download_url"].toString();
            if (url.isEmpty()) {
                continue;
            }

            if ((platform == QLatin1String("windows") && name.endsWith(".exe") && name.contains("Setup")) ||
                    (platform == QLatin1String("appimage") && name.endsWith(".AppImage")) ||
                    (platform == QLatin1String("osx") && name.endsWith(".dmg"))) {
                downloadUrl = url;
                break;
            }
        }

        qDebug() << "Update available:" << latestVersion << downloadUrl;
        emit onUpdateAvailable(latestVersion, downloadUrl);
    }
    else {
        qWarning() << "Update checking failed with error:" << reply->error();
        reply->deleteLater();
    }
}
