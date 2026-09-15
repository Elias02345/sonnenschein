#include "hostlibrary.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSslConfiguration>

HostLibrary::HostLibrary(const QString& host, quint16 port, QObject* parent)
    : QObject(parent), m_Host(host), m_Port(port)
{
    // The host serves the Web UI over HTTPS with a self-signed certificate;
    // QNetworkAccessManager keeps a cookie jar by default so the session
    // cookie from /api/login is replayed on subsequent requests.
}

QByteArray HostLibrary::request(const QString& path, const QByteArray& postJson,
                                int* httpStatus, QString* contentType)
{
    QUrl url(QStringLiteral("https://%1:%2%3").arg(m_Host).arg(m_Port).arg(path));
    QNetworkRequest req(url);

    // Accept the host's self-signed certificate (the user's own machine on the
    // LAN). TODO: pin the cert exchanged during pairing once the library moves
    // to paired-client (streaming cert) auth.
    QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
    ssl.setPeerVerifyMode(QSslSocket::VerifyNone);
    req.setSslConfiguration(ssl);

    QNetworkReply* reply;
    if (postJson.isNull()) {
        reply = m_Nam.get(req);
    } else {
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        reply = m_Nam.post(req, postJson);
    }

    // Block until the reply finishes (with a hard timeout).
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::sslErrors, reply,
                     qOverload<const QList<QSslError>&>(&QNetworkReply::ignoreSslErrors));
    QTimer::singleShot(15000, &loop, &QEventLoop::quit);
    loop.exec();

    QByteArray body;
    if (httpStatus) {
        *httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    }
    if (contentType) {
        *contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    }
    if (reply->error() == QNetworkReply::NoError) {
        body = reply->readAll();
    } else {
        m_LastError = reply->errorString();
    }
    reply->deleteLater();
    return body;
}

bool HostLibrary::login(const QString& username, const QString& password)
{
    QJsonObject creds;
    creds["username"] = username;
    creds["password"] = password;

    int status = 0;
    request(QStringLiteral("/api/login"), QJsonDocument(creds).toJson(QJsonDocument::Compact),
            &status, nullptr);
    if (status != 200) {
        if (m_LastError.isEmpty()) {
            m_LastError = QStringLiteral("login failed (HTTP %1)").arg(status);
        }
        return false;
    }
    return true;
}

QList<HostLibrary::Game> HostLibrary::fetchLibrary(bool* ok)
{
    QList<Game> games;
    int status = 0;
    QByteArray body = request(QStringLiteral("/api/library"), QByteArray(), &status, nullptr);
    if (status != 200) {
        if (ok) *ok = false;
        if (m_LastError.isEmpty()) {
            m_LastError = QStringLiteral("library request failed (HTTP %1)").arg(status);
        }
        return games;
    }

    QJsonObject root = QJsonDocument::fromJson(body).object();
    m_SteamFound = root.value("steam_found").toBool();
    for (const QJsonValue& v : root.value("games").toArray()) {
        QJsonObject g = v.toObject();
        Game game;
        game.appid = g.value("appid").toString();
        game.name = g.value("name").toString();
        game.installed = g.value("installed").toBool();
        games.append(game);
    }
    if (ok) *ok = true;
    return games;
}

QByteArray HostLibrary::fetchArtwork(const QString& appid, QString* contentType)
{
    int status = 0;
    QByteArray body = request(QStringLiteral("/api/library/artwork/%1").arg(appid),
                              QByteArray(), &status, contentType);
    if (status != 200) {
        return QByteArray();
    }
    return body;
}
