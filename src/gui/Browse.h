/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
 *
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <QHash>
#include <QtQml/qqmlregistration.h>

#include "core/Catalog.h"
#include "gui/NameList.h"
#include "gui/Notifier.h"

class QFile;
class QNetworkAccessManager;
class QNetworkReply;

/*
The source ports ZDL knows about, and the state of getting one: asking GitHub
what the latest release is, fetching it, unpacking it into the data directory
and putting it in the port list. Everything here is one row per port.
*/
class Browse : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Reached through App.browse")

    Q_PROPERTY(int count READ rowCount CONSTANT)

    // Whether anything is being asked about or fetched right now.
    Q_PROPERTY(bool checking READ checking NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)

    // Where a fetched port is unpacked, and where what was fetched is kept.
    Q_PROPERTY(QString directory READ directory CONSTANT)
    Q_PROPERTY(QString downloads READ downloads CONSTANT)

    // How much of it is being kept, in bytes.
    Q_PROPERTY(qint64 cached READ cached NOTIFY cacheChanged)

    // Why the last round of asking came back empty, if it did.
    Q_PROPERTY(QString trouble READ trouble NOTIFY changed)

public:
    enum Role : std::uint16_t {
        NameRole = Qt::UserRole + 1,
        BlurbRole,
        HomepageRole,

        // waiting | checking | ready | elsewhere | unavailable | fetching
        // | unpacking | installed | failed
        StatusRole,

        // What the project has released, and what is here already.
        VersionRole,
        HaveRole,
        SizeRole,
        ProgressRole,
        FileRole,
        ErrorRole,

        // A DOS program, which is launched inside DOSBox.
        DosRole,
    };

    explicit Browse(Notifier *notifier, NameList *ports, QObject *parent = nullptr);

    ~Browse() override;

    Browse(const Browse &) = delete;
    Browse &operator=(const Browse &) = delete;
    Browse(Browse &&) = delete;
    Browse &operator=(Browse &&) = delete;

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;

    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;

    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] bool checking() const;
    [[nodiscard]] bool busy() const;
    [[nodiscard]] static QString directory();
    [[nodiscard]] static QString downloads();
    [[nodiscard]] qint64 cached() const;
    [[nodiscard]] QString trouble() const;

    // Asks what the latest release of each port is. Only the ports nothing is
    // known about are asked after, unless everything is.
    Q_INVOKABLE void refresh(bool everything = false);

    // Fetches one, unpacks it, and adds it to the source ports.
    Q_INVOKABLE void install(int row);

    Q_INVOKABLE void cancel(int row);

    // Deletes what was unpacked and takes the port out of the list.
    Q_INVOKABLE void remove(int row);

    // The same from the other end: a row of the source port list goes, and
    // with it whatever ZDL unpacked for it.
    Q_INVOKABLE void forget(int listed);

    // Adds up what the downloads are holding, and throws it away.
    Q_INVOKABLE void measure();

    Q_INVOKABLE void clearDownloads();

signals:
    void changed();

    void cacheChanged();

private:
    struct Entry {
        QString state{QStringLiteral("waiting")};
        QString version;
        QString have;
        QString url;
        QString asset;
        QString file;
        QString error;
        qint64 size{0};
        double progress{0};

        // What is in flight for this row, while there is something.
        QNetworkReply *reply{nullptr};
        QFile *sink{nullptr};

        // Set when the answer to what the latest release is is only being
        // waited on so that it can be fetched.
        bool wanted{false};
    };

    [[nodiscard]] static const Catalog::Port &port(int row);

    // Where a row stands before anything has been asked: already unpacked, or
    // waiting to be asked about, or not fetchable here at all.
    void settle(int row);

    void check(int row);

    void fetch(int row);

    void unpack(int row, const QString &archive);

    // Puts a port that is now on disk into the source port list.
    void adopt(int row, const QString &file);

    // Throws away what was fetched for one of the known ports.
    void erase(int row);

    void touch(int row);

    void give(int row, const QString &state, const QString &error = {});

    // Drops whatever a row has in flight, leaving nothing half written behind.
    static void drop(Entry &entry, bool keepFile);

    Notifier *_notifier;
    NameList *_ports;
    QNetworkAccessManager *_network;

    std::vector<Entry> _entries;
    QString _trouble;

    // What the downloads add up to, as of the last time they were measured.
    qint64 _cached{0};
};
