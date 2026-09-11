/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
 *
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
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

#include <string>
#include <vector>

#include "gui/model/Bridge.h"

// The active profile's files, and the IWAD and port lists.
class ListsBridge : public Bridge {
public:
    using Bridge::Bridge;

    void push() const;

    // Returns the new port's id, which most callers do not want.
    // NOLINTNEXTLINE(modernize-use-nodiscard)
    std::string addPort(const std::string &file, const std::string &name, bool dosbox) const;
    void updatePort(int row, const std::string &name, const std::string &file, bool dosbox) const;
    void removePort(int row) const;
    void movePort(int from, int to) const;

    [[nodiscard]] static const std::vector<NameEntry> &ports();

    [[nodiscard]] static State::NameRow rowOf(const std::vector<NameEntry> &list, int index,
                                              bool ports);

    void addFiles(const std::vector<std::string> &paths) const;
    void removeFile(int row) const;
    void clearFiles() const;
    void moveFile(int from, int to) const;
    void setFileEnabled(int row, bool enabled) const;

    void addIwads(const std::vector<std::string> &paths) const;
    void updateIwad(int row, const std::string &name, const std::string &file) const;
    void removeIwad(int row) const;
    void moveIwad(int from, int to) const;

private:
    // Profiles reference IWADs and ports by name.
    void renamedIwad(const std::string &before, const std::string &after) const;
    void renamedPort(const std::string &before, const std::string &after) const;

    [[nodiscard]] static std::string uniqueName(const std::vector<NameEntry> &list,
                                                const std::string &base, int ignoring = -1);
};
