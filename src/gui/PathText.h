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

#include <filesystem>

#include <QDir>
#include <QString>

// The interface writes every path with forward slashes; std::filesystem hands
// back the native form. A path that reaches the interface still in it is one
// prettyPath will not shorten and PathLabel will not elide a directory at a time.
namespace PathText {

[[nodiscard]] inline std::filesystem::path toPath(const QString &path) {
    return {path.toStdString()};
}

[[nodiscard]] inline QString fromPath(const std::filesystem::path &path) {
    return QDir::fromNativeSeparators(QString::fromStdString(path.string()));
}

}
