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

#include "core/FileInfo.h"
#include "core/Session.h"
#include "core/Text.h"
#include "gui/NameList.h"
#include "gui/PathText.h"

NameList::NameList(const Kind kind, QObject *parent) : QAbstractListModel(parent), _kind(kind) {
}

std::vector<NameEntry> &NameList::entries() const {
    Config &config = Session::get().config();

    return _kind == Kind::Iwads ? config.iwads : config.ports;
}

int NameList::rowCount(const QModelIndex &parent) const {
    return parent.isValid()
        ? 0
        : static_cast<int>(entries().size());
}

QHash<int, QByteArray> NameList::roleNames() const {
    return {
        {NameRole, "name"},
        {FileRole, "file"},
        {DirectoryRole, "directory"},
        {MissingRole, "missing"},
        {DosboxRole, "dosbox"},
    };
}

QVariant NameList::data(const QModelIndex &index, const int role) const {
    const std::vector<NameEntry> &list = entries();

    if (index.row() < 0 || std::cmp_greater_equal(index.row(), list.size())) {
        return {};
    }

    const NameEntry &entry = list[static_cast<size_t>(index.row())];
    const std::filesystem::path path(entry.file);

    switch (role) {
        case NameRole:
            return QString::fromStdString(entry.name);
        case FileRole:
            return QString::fromStdString(entry.file);
        case DirectoryRole:
            return PathText::fromPath(path.parent_path());
        case MissingRole: {
            std::error_code code;

            return !std::filesystem::exists(path, code);
        }
        case DosboxRole:
            return entry.dosbox;
        default:
            return {};
    }
}

QStringList NameList::names() const {
    QStringList out;

    for (const NameEntry &entry : entries()) {
        out << QString::fromStdString(entry.name);
    }

    return out;
}

QVariantList NameList::entryList() const {
    QVariantList out;
    const std::vector<NameEntry> &list = entries();

    for (size_t index = 0; index < list.size(); ++index) {
        const NameEntry &entry = list[index];
        const std::filesystem::path path(entry.file);
        std::error_code code;

        out.append(QVariantMap{
            {QStringLiteral("index"), static_cast<int>(index)},
            {QStringLiteral("name"), QString::fromStdString(entry.name)},
            {QStringLiteral("file"), QString::fromStdString(entry.file)},
            {QStringLiteral("directory"), PathText::fromPath(path.parent_path())},

            {QStringLiteral("kind"), QString::fromStdString(Text::lower(path.extension().string()))},
            {QStringLiteral("missing"), !std::filesystem::exists(path, code)},
            {QStringLiteral("dosbox"), entry.dosbox},
        });
    }

    return out;
}

int NameList::indexOfName(const QString &name) const {
    const std::vector<NameEntry> &list = entries();
    const std::string wanted = name.toStdString();

    for (size_t index = 0; index < list.size(); ++index) {
        if (list[index].name == wanted) {
            return static_cast<int>(index);
        }
    }

    return -1;
}

QVariantMap NameList::at(const int row) const {
    const std::vector<NameEntry> &list = entries();

    if (row < 0 || std::cmp_greater_equal(row, list.size())) {
        return {};
    }

    const NameEntry &entry = list[static_cast<size_t>(row)];

    return {
        {"name", QString::fromStdString(entry.name)},
        {"file", QString::fromStdString(entry.file)},
        {"dosbox", entry.dosbox},
    };
}

QString NameList::describe(const QString &file) const {
    const std::filesystem::path path(file.toStdString());

    return QString::fromStdString(_kind == Kind::Iwads
        ? FileInfo::describeIwad(path)
        : FileInfo::describePort(path));
}

std::string NameList::uniqueName(const std::string &base, const int ignoring) const {
    const std::vector<NameEntry> &list = entries();
    std::string candidate = Text::trim(base);

    if (candidate.empty()) {
        candidate = "Unnamed";
    }

    const auto taken = [&list, ignoring](const std::string &name) {
        for (size_t index = 0; index < list.size(); ++index) {
            if (std::cmp_not_equal(index, ignoring) && Text::iequals(list[index].name, name)) {
                return true;
            }
        }

        return false;
    };

    if (!taken(candidate)) {
        return candidate;
    }

    for (int suffix = 2;; suffix++) {
        std::string numbered = candidate + " (" + std::to_string(suffix) + ")";

        if (!taken(numbered)) {
            return numbered;
        }
    }
}

void NameList::reload() {
    beginResetModel();
    endResetModel();

    emit changed();
}

QString NameList::add(const QString &file, const QString &name, const bool dosbox) {
    if (file.isEmpty()) {
        return {};
    }

    std::vector<NameEntry> &list = entries();
    const std::string chosen = uniqueName(name.isEmpty()
        ? describe(file).toStdString()
        : name.toStdString());
    const int row = static_cast<int>(list.size());

    beginInsertRows({}, row, row);
    list.push_back(NameEntry{.name = chosen, .file = file.toStdString(), .dosbox = dosbox});
    endInsertRows();

    emit changed();

    return QString::fromStdString(chosen);
}

void NameList::addAll(const QStringList &files, const bool dosbox) {
    for (const QString &file : files) {
        add(file, {}, dosbox);
    }
}

void NameList::update(const int row, const QString &name, const QString &file,
                      const bool dosbox) {
    std::vector<NameEntry> &list = entries();

    if (row < 0 || std::cmp_greater_equal(row, list.size())) {
        return;
    }

    NameEntry &entry = list[static_cast<size_t>(row)];
    const std::string before = entry.name;
    const std::string after = uniqueName(name.isEmpty()
        ? describe(file).toStdString()
        : name.toStdString(), row);

    entry.name = after;
    entry.file = file.toStdString();
    entry.dosbox = dosbox;

    emit dataChanged(index(row), index(row));
    emit changed();

    // Profiles point at entries by name, so a rename has to be carried across.
    if (before != after) {
        emit renamed(QString::fromStdString(before), QString::fromStdString(after));
    }
}

void NameList::remove(const int row) {
    std::vector<NameEntry> &list = entries();

    if (row < 0 || std::cmp_greater_equal(row, list.size())) {
        return;
    }

    beginRemoveRows({}, row, row);
    list.erase(list.begin() + row);
    endRemoveRows();

    emit changed();
}

void NameList::moveTo(const int from, const int to) {
    std::vector<NameEntry> &list = entries();

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

