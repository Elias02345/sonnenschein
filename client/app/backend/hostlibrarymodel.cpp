#include "hostlibrarymodel.h"

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QCryptographicHash>

HostLibraryModel::HostLibraryModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void HostLibraryModel::load(const QString& host, int port, const QString& username, const QString& password)
{
    beginResetModel();
    m_Rows.clear();
    m_Error.clear();
    m_Busy = true;
    endResetModel();
    emit statusChanged();

    HostLibrary lib(host, static_cast<quint16>(port));
    if (!username.isEmpty() && !lib.login(username, password)) {
        m_Error = lib.lastError();
        m_Busy = false;
        emit statusChanged();
        return;
    }

    bool ok = false;
    QList<HostLibrary::Game> games = lib.fetchLibrary(&ok);
    if (!ok) {
        m_Error = lib.lastError();
        m_Busy = false;
        emit statusChanged();
        return;
    }
    m_SteamFound = lib.steamFound();

    // Cache dir for covers.
    QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(cacheRoot + "/covers");

    QList<Row> rows;
    for (const HostLibrary::Game& g : games) {
        Row r;
        r.game = g;

        // Fetch + cache the portrait cover; leave empty on 404 so the delegate
        // can show a name-only placeholder (SteamGridDB fallback comes later).
        QString ct;
        QByteArray bytes = lib.fetchArtwork(g.appid, &ct);
        if (!bytes.isEmpty()) {
            QString ext = ct.contains("png") ? "png" : "jpg";
            QString key = QCryptographicHash::hash((host + "_" + g.appid).toUtf8(),
                                                   QCryptographicHash::Md5).toHex();
            QString path = QStringLiteral("%1/covers/%2.%3").arg(cacheRoot, key, ext);
            QFile f(path);
            if (f.open(QIODevice::WriteOnly)) {
                f.write(bytes);
                f.close();
                r.cover = QUrl::fromLocalFile(path);
            }
        }
        rows.append(r);
    }

    beginResetModel();
    m_Rows = rows;
    m_Busy = false;
    endResetModel();
    emit statusChanged();
}

int HostLibraryModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_Rows.size();
}

QVariant HostLibraryModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_Rows.size()) {
        return QVariant();
    }
    const Row& r = m_Rows.at(index.row());
    switch (role) {
    case NameRole:      return r.game.name;
    case AppIdRole:     return r.game.appid;
    case InstalledRole: return r.game.installed;
    case CoverRole:     return r.cover;
    default:            return QVariant();
    }
}

QHash<int, QByteArray> HostLibraryModel::roleNames() const
{
    return {
        { NameRole, "name" },
        { AppIdRole, "appid" },
        { InstalledRole, "installed" },
        { CoverRole, "cover" },
    };
}
