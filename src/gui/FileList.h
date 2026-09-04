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
 * The external files the active profile loads, in the order the source port
 * will be handed them. Order is the whole point of this list, so it is a
 * model that can be reordered rather than a plain array of paths.
 *
 * It does not own what it shows: the entries live in the profile, and this
 * writes straight into them.
 */
class FileList : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Reached through App.config.files")

    Q_PROPERTY(int count READ rowCount NOTIFY changed)

    /** How many of them are actually switched on, for the panel's heading. */
    Q_PROPERTY(int enabledCount READ enabledCount NOTIFY changed)

public:
    enum Role : int {
        FileRole = Qt::UserRole + 1,
        NameRole,
        DirectoryRole,
        EnabledRole,
        MissingRole,
    };

    explicit FileList(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;

    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;

    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    /** Re-reads the profile, for when a different one has become the active one. */
    void reload();

    Q_INVOKABLE void add(const QStringList &paths);

    Q_INVOKABLE void remove(int row);

    Q_INVOKABLE void clear();

    /** Moves one entry one place towards the front or the back. */
    Q_INVOKABLE void move(int row, int by);

    Q_INVOKABLE void setEnabled(int row, bool enabled);

signals:
    /** Anything that changes what the port would be handed. */
    void changed();

private:
    [[nodiscard]] static std::vector<FileEntry> &entries();

    [[nodiscard]] int enabledCount() const;
};
