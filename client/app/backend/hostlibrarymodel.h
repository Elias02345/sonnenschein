#pragma once

#include <QAbstractListModel>
#include <QUrl>
#include "hostlibrary.h"

// QML list model over a Sonnenschein host's Steam library. Fetches the game
// list from the host Web API and caches each portrait cover to a file, exposed
// as a file:// URL (mirrors how BoxArtManager feeds the app grid). Backs
// LibraryView.qml — the client's host-library view.
class HostLibraryModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY statusChanged)
    Q_PROPERTY(bool steamFound READ steamFound NOTIFY statusChanged)
    Q_PROPERTY(QString error READ error NOTIFY statusChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        AppIdRole,
        InstalledRole,
        CoverRole,
    };

    explicit HostLibraryModel(QObject* parent = nullptr);

    // Blocking fetch of the library + covers for the given host.
    Q_INVOKABLE void load(const QString& host, int port, const QString& username, const QString& password);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool busy() const { return m_Busy; }
    bool steamFound() const { return m_SteamFound; }
    QString error() const { return m_Error; }

signals:
    void statusChanged();

private:
    struct Row {
        HostLibrary::Game game;
        QUrl cover;  // file:// to the cached cover, or empty
    };

    QList<Row> m_Rows;
    bool m_Busy = false;
    bool m_SteamFound = false;
    QString m_Error;
};
