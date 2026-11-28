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

// The file browser under the add-ons of the active profile.
class BrowserBridge : public Bridge {
public:
    using Bridge::Bridge;

    // Lists the folder again only when the folder in effect moved. The picks are marked on every call.
    void push();

    // Unfolding lists the folder again.
    void setOpen(bool open);
    static void setFilter(const std::string &value);
    static void fold(int row, bool folded);

    // The profile lists this folder in place of the one in Settings.
    void setOwnFolder(const std::string &folder);

    // Every row that is to be in the file list, as FileBrowser::pick places them.
    void applyPicks(const std::vector<std::string> &paths) const;

private:
    [[nodiscard]] static const std::string &folderInEffect();

    void scan(const std::string &folder);
    [[nodiscard]] bool markPicks() const;
    static void match();

    std::string _scanned;

    // The fileKey of each file row, empty for a folder.
    std::vector<std::string> _keys;
};
