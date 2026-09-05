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

#include <QAbstractListModel>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

#include "core/Config.h"

class NameList : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Reached through App.config.iwads and App.config.ports")

    Q_PROPERTY(int count READ rowCount NOTIFY changed)

    Q_PROPERTY(QStringList names READ names NOTIFY changed)

    /** The whole list at once, for anything that filters it before drawing. */
    Q_PROPERTY(QVariantList entries READ entryList NOTIFY changed)

public:
    enum Role : std::uint16_t {
        NameRole = Qt::UserRole + 1,
        FileRole,
        DirectoryRole,
        MissingRole,
        DosboxRole,
    };

    enum class Kind : std::uint8_t {
        Iwads,
        Ports,
    };

    explicit NameList(Kind kind, QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;

    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;

    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] QStringList names() const;

    [[nodiscard]] QVariantList entryList() const;

    void reload();

    Q_INVOKABLE QString add(const QString &file, const QString &name = {}, bool dosbox = false);

    Q_INVOKABLE void addAll(const QStringList &files, bool dosbox = false);

    Q_INVOKABLE void update(int row, const QString &name, const QString &file,
                            bool dosbox = false);

    Q_INVOKABLE void remove(int row);

    /** Takes the row out and puts it back at this one. */
    Q_INVOKABLE void moveTo(int from, int to);

    Q_INVOKABLE [[nodiscard]] QVariantMap at(int row) const;

    Q_INVOKABLE [[nodiscard]] int indexOfName(const QString &name) const;

    Q_INVOKABLE [[nodiscard]] QString describe(const QString &file) const;

signals:
    void changed();

    void renamed(const QString &before, const QString &after);

private:
    [[nodiscard]] std::vector<NameEntry> &entries() const;

    [[nodiscard]] std::string uniqueName(const std::string &base, int ignoring = -1) const;

    Kind _kind;
};
