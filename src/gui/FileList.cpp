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
#include "gui/FileList.h"

FileList::FileList(QObject *parent) : QAbstractListModel(parent) {
}

std::vector<FileEntry> &FileList::entries() {
    return Session::get().config().activeProfile().files;
}

int FileList::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(entries().size());
}

int FileList::enabledCount() {
    const std::vector<FileEntry> &files = entries();

    return static_cast<int>(std::ranges::count_if(files, [](const FileEntry &entry) {
        return entry.enabled;
    }));
}

QHash<int, QByteArray> FileList::roleNames() const {
    return {
        {FileRole, "file"},
        {NameRole, "name"},
        {DirectoryRole, "directory"},
        {EnabledRole, "loaded"},
        {MissingRole, "missing"},
    };
}

QVariant FileList::data(const QModelIndex &index, const int role) const {
    const std::vector<FileEntry> &files = entries();

    if (index.row() < 0 || std::cmp_greater_equal(index.row(), files.size())) {
        return {};
    }

    const FileEntry &entry = files[static_cast<size_t>(index.row())];
    const std::filesystem::path path(entry.file);

    switch (role) {
        case FileRole:
            return QString::fromStdString(entry.file);
        case NameRole:
            return QString::fromStdString(path.filename().string());
        case DirectoryRole:
            return QString::fromStdString(path.parent_path().string());
        case EnabledRole:
            return entry.enabled;
        case MissingRole: {
            std::error_code code;

            return !std::filesystem::exists(path, code);
        }
        default:
            return {};
    }
}

void FileList::reload() {
    beginResetModel();
    endResetModel();

    emit changed();
}

void FileList::add(const QStringList &paths) {
    if (paths.isEmpty()) {
        return;
    }

    std::vector<FileEntry> &files = entries();
    const int first = static_cast<int>(files.size());

    beginInsertRows({}, first, first + static_cast<int>(paths.size()) - 1);

    for (const QString &path : paths) {
        files.push_back(FileEntry{.file = path.toStdString(), .enabled = true});
    }

    endInsertRows();

    emit changed();
}

void FileList::remove(const int row) {
    std::vector<FileEntry> &files = entries();

    if (row < 0 || std::cmp_greater_equal(row, files.size())) {
        return;
    }

    beginRemoveRows({}, row, row);
    files.erase(files.begin() + row);
    endRemoveRows();

    emit changed();
}

void FileList::clear() {
    std::vector<FileEntry> &files = entries();

    if (files.empty()) {
        return;
    }

    beginResetModel();
    files.clear();
    endResetModel();

    emit changed();
}

void FileList::move(const int row, const int by) {
    std::vector<FileEntry> &files = entries();
    const int target = row + by;

    if (row < 0 || std::cmp_greater_equal(row, files.size())
        || target < 0 || std::cmp_greater_equal(target, files.size())) {
        return;
    }

    /*
    Moving down by one means landing after the row below, which is two places
    along in the terms beginMoveRows is written in: the destination is where
    the row goes before anything has been taken out.
    */
    beginMoveRows({}, row, row, {}, by > 0 ? target + 1 : target);
    std::swap(files[static_cast<size_t>(row)], files[static_cast<size_t>(target)]);
    endMoveRows();

    emit changed();
}

void FileList::setEnabled(const int row, const bool enabled) {
    std::vector<FileEntry> &files = entries();

    if (row < 0 || std::cmp_greater_equal(row, files.size())) {
        return;
    }

    files[static_cast<size_t>(row)].enabled = enabled;

    emit dataChanged(index(row), index(row), {EnabledRole});
    emit changed();
}
