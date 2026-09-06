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

#include <algorithm>

#include "core/Session.h"
#include "gui/ConfigBridge.h"
#include "gui/ProfileList.h"

namespace {

std::vector<Profile> &profiles() {
    return Session::get().config().profiles;
}

}

ProfileList::ProfileList(QObject *parent) : QAbstractListModel(parent) {}

int ProfileList::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(profiles().size());
}

QVariant ProfileList::data(const QModelIndex &index, const int role) const {
    if (!index.isValid() || role != ProfileRole
        || std::cmp_greater_equal(index.row(), profiles().size())) {
        return {};
    }

    return ConfigBridge::profileCard(index.row());
}

QHash<int, QByteArray> ProfileList::roleNames() const {
    return {{ProfileRole, "profile"}};
}

void ProfileList::reload() {
    beginResetModel();
    endResetModel();

    emit changed();
}

void ProfileList::refresh() {
    if (const int rows = rowCount(); rows > 0) {
        emit dataChanged(index(0), index(rows - 1), {ProfileRole});
    }
}

void ProfileList::moveTo(const int from, const int to) {
    std::vector<Profile> &list = profiles();

    if (from == to || from < 0 || std::cmp_greater_equal(from, list.size())
        || to < 0 || std::cmp_greater_equal(to, list.size())) {
        return;
    }

    // beginMoveRows is written in terms of where the row lands before it has
    // been taken out, which is one further along when it is moving down.
    beginMoveRows({}, from, from, {}, to > from ? to + 1 : to);

    const auto first = list.begin();
    const auto at = first + from;
    const auto onto = first + to;

    if (to > from) {
        std::rotate(at, at + 1, onto + 1);
    } else {
        std::rotate(onto, at, at + 1);
    }

    endMoveRows();

    emit changed();
}
