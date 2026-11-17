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

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Flatpak {

struct Export {
    std::filesystem::path file;

    // The executable a Flathub port runs, or else the last part of the app ID.
    std::string name;
};

#ifdef __linux__

enum class Installation : std::uint8_t {
    other,
    system,
    user,
};

// Where an export links to: app/<id>/<arch>/<branch>/<commit>/export/bin/<id>.
struct Deploy {
    Installation installation;

    // The <commit> folder. Empty when the export links nowhere like that, and then so are arch and branch.
    std::filesystem::path folder;

    std::string arch;
    std::string branch;
};

// A file in flatpak/exports/bin, named for its app ID.
[[nodiscard]] bool exported(const std::filesystem::path &file);

// The executable a Flathub port runs the game with. Empty for an app that starts no game and for any other app ID.
[[nodiscard]] std::string_view executable(std::string_view app);

// Why a Flathub app cannot be launched. Empty for one that starts the game and for any other app ID.
[[nodiscard]] std::string_view refusal(std::string_view app);

// Every app exported for the system and for this user, apart from the ones that start no game.
[[nodiscard]] std::vector<Export> exports();

[[nodiscard]] Deploy deploy(const std::filesystem::path &file);

// The folder an exported app keeps as its home. Empty for any other file.
[[nodiscard]] std::filesystem::path home(const std::filesystem::path &file);

// The newest release in the metainfo the app of an export installs. Empty when there is none to read. The file is
// read once for each deployed commit.
[[nodiscard]] std::string version(const std::filesystem::path &file);

// The version of the first <release> in AppStream metainfo, which lists releases newest first.
[[nodiscard]] std::string firstRelease(std::string_view metainfo);

#else

// Flatpak exists only on Linux.
[[nodiscard]] inline bool exported(const std::filesystem::path & /*file*/) {
    return false;
}

[[nodiscard]] inline std::string_view executable(std::string_view /*app*/) {
    return {};
}

[[nodiscard]] inline std::vector<Export> exports() {
    return {};
}

[[nodiscard]] inline std::filesystem::path home(const std::filesystem::path & /*file*/) {
    return {};
}

[[nodiscard]] inline std::string version(const std::filesystem::path & /*file*/) {
    return {};
}

#endif

}
