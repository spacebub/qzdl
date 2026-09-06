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

/*
Every profile as the library draws it. A model rather than the plain list the
rest of the interface reads, because a shelf that is reordered by dragging one
card onto another needs the card it is dragging to survive the move, and a list
that is handed over again makes every card afresh.
*/
class ProfileList : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Reached through App.config.profiles")

    Q_PROPERTY(int count READ rowCount NOTIFY changed)

public:
    enum Role : std::uint16_t {
        ProfileRole = Qt::UserRole + 1,
    };

    explicit ProfileList(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;

    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;

    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    // There are other profiles now, or the same ones in another config.
    void reload();

    // The same profiles, drawn again.
    void refresh();

    // Takes the row out and puts it back at this one.
    Q_INVOKABLE void moveTo(int from, int to);

signals:
    // The list itself changed: another order, or another set of profiles. What
    // is drawn on a card changing is not this.
    void changed();
};
