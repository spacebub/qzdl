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

#include <span>
#include <string>
#include <vector>

#include "core/ports/Detect.h"
#include "gui/model/Bridge.h"

// The active profile's files, and the IWAD and engine lists.
class ListsBridge : public Bridge {
public:
    using Bridge::Bridge;

    void push() const;

    // Returns the name the engine was given, which most callers do not want.
    // NOLINTNEXTLINE(modernize-use-nodiscard)
    std::string addEngine(const Engine &engine) const;

    // Everything but managed, which only the downloads service sets. Another file drops the detected mark.
    void updateEngine(int row, const Engine &edited) const;

    // Saves only when a stored version changed.
    void storeVersions(std::span<const Detect::Installed> read) const;
    void removeEngine(int row) const;
    void moveEngine(int from, int to) const;

    // Empty for a row out of range.
    [[nodiscard]] static Engine engine(int row);

    [[nodiscard]] static State::NameRow rowOf(const std::vector<GameEntry> &list, int index);
    [[nodiscard]] static State::NameRow rowOf(const std::vector<Engine> &list, int index);

    // Marks the ticked rows whose file is among the patches the launch leaves out. True when a mark moved.
    static bool markLeftOut(std::vector<State::FileRow> &rows, const std::vector<std::string> &leftOut);

    void addFiles(const std::vector<std::string> &paths) const;

    // In front of the row at `at`. Past the last row appends.
    void insertFiles(const std::vector<std::string> &paths, int at) const;
    void removeFile(int row) const;
    void clearFiles() const;
    void moveFile(int from, int to) const;
    void setFileEnabled(int row, bool enabled) const;

    void addIwads(const std::vector<std::string> &paths) const;
    void updateIwad(int row, const std::string &name, const std::string &file) const;
    void removeIwad(int row) const;
    void moveIwad(int from, int to) const;

private:
    // Profiles reference IWADs and engines by name.
    void renamedIwad(const std::string &before, const std::string &after) const;
    void renamedEngine(const std::string &before, const std::string &after) const;
};
