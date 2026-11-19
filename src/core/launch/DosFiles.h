/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2018-2019  Lcferrum
 * Copyright (C) 2023-2026  spacebub
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

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "core/config/Config.h"

namespace DosFiles {

// DOSBox mangles names longer than 8.3, so the port would not find them. Vanilla splits a response file on
// { | } ~, and DJGPP ports read ' as a quote.
[[nodiscard]] bool spellable(const std::string &name);

// The folder below the profile folder that a known DOS port runs from. Empty when the port runs from its own folder.
[[nodiscard]] std::filesystem::path runFolder(const Config &config, const Profile &profile);

// The config the port reads where it runs. Empty when it runs from its own folder.
[[nodiscard]] std::filesystem::path configFile(const Config &config, const Profile &profile);

// A batch file, which DOSBox must call.
[[nodiscard]] bool isBatch(const std::filesystem::path &file);

// The file of that name in the folder, in whatever case it has there.
[[nodiscard]] std::filesystem::path inFolder(const std::filesystem::path &folder, const std::string &name);

struct Copy {
    std::filesystem::path from;
    std::filesystem::path to;
};

// How a port with no -iwad is to find the game: where it already looks, through
// $DOOMWADDIR at the game's own directory, or at a copy under the name it searches for.
enum class Reach : std::uint8_t {
    beside,
    pointed,
    staged,
};

[[nodiscard]] Reach
reach(const std::filesystem::path &iwad, const std::filesystem::path &portDirectory, bool recognised);

struct Directories {
    // Where a game staged for the port goes. The run folder when the port runs there, else the profile folder.
    std::filesystem::path instance;

    // Copies of what DOS cannot name, kept out of the directory the port searches.
    std::filesystem::path files;

    // False when there is no profile folder and the port's own directory stands in for it.
    bool profileOwned{true};

    // The port runs from the instance folder, with a copy of its program there.
    bool runsHere{false};
};

[[nodiscard]] Directories directories(const Config &config, const std::filesystem::path &file);

// A wad this launch did not stage would answer for the game, and the file directory is
// ZDL's own. Everything the port writes for itself stays.
void prune(const Directories &directories, const std::vector<Copy> &planned);

class Staging {
public:
    explicit Staging(Directories directories);

    [[nodiscard]] std::filesystem::path spellableName(const std::filesystem::path &file);

    // The IWAD under the name the port searches for.
    void game(const std::filesystem::path &iwad, bool doomu);

    // A copy under its own name beside the game.
    void beside(const std::filesystem::path &file);

    // A copy of the demo beside the game, and the bare name that plays it.
    [[nodiscard]] std::string demo(const std::filesystem::path &file);

    [[nodiscard]] const std::vector<Copy> &planned() const;

private:
    std::filesystem::path
    keep(const std::filesystem::path &file, const std::filesystem::path &directory, const std::string &name);

    std::string shorten(const std::filesystem::path &file, const std::filesystem::path &directory);

    Directories _where;
    std::vector<Copy> _planned;
};

}
