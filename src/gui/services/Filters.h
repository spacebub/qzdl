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
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "ttk/dialogs/FilePicker.h"

namespace Filters {

// What a source port takes as the game.
const std::vector<std::string> &game();

// Every game kind, and the patches, demos and configs the add-on list takes as well.
const std::vector<std::string> &addon();

const std::vector<std::string> &port();
const std::vector<std::string> &zdl();
const std::vector<std::string> &config();

// The saves of a port that names them with saveExt, such as .zds.
[[nodiscard]] std::vector<std::string> save(std::string_view saveExt);

const std::vector<std::string> &replay();

using Names = std::vector<std::string>;

// Files dropped on a target, split into what it takes and the file names it leaves out.
struct Dropped {
    Names taken;
    Names left;
};

// Takes the files that pass `patterns`, and folders as well when `folders`. Taken paths come in
// the form the pickers give.
[[nodiscard]] Dropped split(const Names &paths, const Names &patterns, bool folders);

// "a, b and c", and past three names the rest are counted, as in "a, b, c and 2 more".
[[nodiscard]] std::string listed(const Names &names);

// Names what a drop left out, as "a.txt is not a game" or "a.txt and b.png are not games".
[[nodiscard]] std::string leftOut(const Names &names, const std::string &one, const std::string &many);
}

// Which of the config's last directories a picker starts in and remembers.
namespace LastDir {

// Anything without a slot of its own.
inline constexpr const char *GENERAL = "general";
inline constexpr const char *WAD = "wad";
inline constexpr const char *SRC = "src";
inline constexpr const char *SAVE = "save";
inline constexpr const char *ZDL = "zdl";
inline constexpr const char *CONFIG = "config";
inline constexpr const char *REPLAY = "replay";

}

// A picker callback that wants one path: called with the first chosen, never with none.
namespace Picked {

template <class F>
ttk::FilePicker::Chosen first(F picked) {
    return [then = std::move(picked)](const std::vector<std::string> &paths, [[maybe_unused]] const bool option) {
        if (paths.empty()) {
            return;
        }

        if constexpr (std::is_invocable_v<F &, const std::string &, bool>) {
            then(paths.front(), option);
        } else {
            then(paths.front());
        }
    };
}

}
