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
#include <QtQml/qqmlregistration.h>

#include "core/Config.h"

/**
 * A collection kept once and picked from by every profile: the IWADs on this
 * machine, or the source ports installed on it. Both are the same shape -- a
 * name and the file it stands for -- so both are this.
 *
 * A profile refers to an entry by its name, which is how .zdl files exchanged
 * with other Doom tools name them, so renaming one has to carry every profile
 * that pointed at it along with it.
 */
class NameList : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Reached through App.config.iwads and App.config.ports")

    Q_PROPERTY(int count READ rowCount NOTIFY changed)

    /** Every name in the list, for the pickers that choose one of them. */
    Q_PROPERTY(QStringList names READ names NOTIFY changed)

public:
    enum Role : int {
        NameRole = Qt::UserRole + 1,
        FileRole,
        DirectoryRole,
        MissingRole,
    };

    /** Which of the two collections this list is looking at. */
    enum class Kind : std::uint8_t {
        Iwads,
        Ports,
    };

    explicit NameList(Kind kind, QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;

    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;

    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] QStringList names() const;

    void reload();

    /**
     * Adds one, working out what to call it from the file itself when no name
     * is given. Returns the name it ended up with, which is unique in the list.
     */
    Q_INVOKABLE QString add(const QString &file, const QString &name = {});

    /** Adds several at once, each named after its own file. */
    Q_INVOKABLE void addAll(const QStringList &files);

    Q_INVOKABLE void update(int row, const QString &name, const QString &file);

    Q_INVOKABLE void remove(int row);

    Q_INVOKABLE void move(int row, int by);

    Q_INVOKABLE [[nodiscard]] QVariantMap at(int row) const;

    /** Where a name sits in the list, or -1 when nothing is called that. */
    Q_INVOKABLE [[nodiscard]] int indexOfName(const QString &name) const;

    /** What a file would be called if it were added, before it is. */
    Q_INVOKABLE [[nodiscard]] QString describe(const QString &file) const;

signals:
    void changed();

    /** An entry was renamed, and every profile naming it has to follow. */
    void renamed(const QString &before, const QString &after);

private:
    [[nodiscard]] std::vector<NameEntry> &entries() const;

    /** Appends " (2)", " (3)"... until the name is free, ignoring one row. */
    [[nodiscard]] std::string uniqueName(const std::string &base, int ignoring = -1) const;

    Kind _kind;
};
