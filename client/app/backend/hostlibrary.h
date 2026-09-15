#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QNetworkAccessManager>

// Sonnenschein host-library client.
//
// Talks to a Sonnenschein host's Web API (default port 47990) to fetch the
// host's Steam library and per-game artwork, which the client renders so the
// user can pick a host-installed game to stream. This is the client side of
// Client-Track C2 (the Steam-Deck Game-Mode library). Endpoints:
//   POST /api/login                     -> session cookie (username/password)
//   GET  /api/library                   -> [{appid, name, installed}, ...]
//   GET  /api/library/artwork/<appid>   -> portrait cover (JPEG/PNG) or 404
//
// NOTE (auth): today the library lives behind the Web UI's password auth, so
// the client needs the host's Web-UI credentials. A future host-side change
// should let a *paired* client (streaming cert) query the library without a
// separate password — tracked as an open C2 decision in the host STATUS.md.
class HostLibrary : public QObject
{
    Q_OBJECT

public:
    struct Game
    {
        QString appid;
        QString name;
        bool installed = false;
    };

    explicit HostLibrary(const QString& host, quint16 port = 47990, QObject* parent = nullptr);

    // Blocking. Authenticate against the host Web UI; returns true on success.
    bool login(const QString& username, const QString& password);

    // Blocking. Fetch the host's Steam library. `ok` reports transport success;
    // check steamFound() for whether a Steam install was detected on the host.
    QList<Game> fetchLibrary(bool* ok = nullptr);

    // Blocking. Fetch a game's portrait cover; empty on 404/error.
    QByteArray fetchArtwork(const QString& appid, QString* contentType = nullptr);

    bool steamFound() const { return m_SteamFound; }
    QString lastError() const { return m_LastError; }

private:
    // Blocking GET/POST helpers (self-signed host cert is accepted).
    QByteArray request(const QString& path, const QByteArray& postJson,
                       int* httpStatus, QString* contentType);

    QString m_Host;
    quint16 m_Port;
    QNetworkAccessManager m_Nam;
    bool m_SteamFound = false;
    QString m_LastError;
};
