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

class FileList : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Reached through App.config.files")

    Q_PROPERTY(int count READ rowCount NOTIFY changed)

    Q_PROPERTY(int enabledCount READ enabledCount NOTIFY changed)

public:
    enum Role : std::uint16_t {
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

    void reload();

    Q_INVOKABLE void add(const QStringList &paths);

    Q_INVOKABLE void remove(int row);

    Q_INVOKABLE void clear();

    // Takes the row out and puts it back at this one.
    Q_INVOKABLE void moveTo(int from, int to);

    Q_INVOKABLE void setEnabled(int row, bool enabled);

signals:
    void changed();

private:
    [[nodiscard]] static std::vector<FileEntry> &entries();

    [[nodiscard]] static int enabledCount() ;
};
