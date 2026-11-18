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

#include <filesystem>
#include <initializer_list>
#include <system_error>

#include "ttk/util/Format.h"

#include "gui/services/Filters.h"

namespace {

std::vector<std::string> listOf(const std::initializer_list<const char *> names) {
    return {names.begin(), names.end()};
}

}

namespace Filters {

const std::vector<std::string> &game() {
    static const std::vector<std::string> held = listOf({
            "*.wad",
            "*.iwad",
            "*.pk3",
            "*.pk7",
            "*.pkz",
            "*.pke",
            "*.ipk3",
            "*.ipk7",
            "*.zip",
            "*.7z",
    });

    return held;
}

const std::vector<std::string> &addon() {
    static const std::vector<std::string> held = [] {
        std::vector<std::string> made = game();

        for (const char *more : {"*.pwad", "*.deh", "*.bex", "*.hhe", "*.seh", "*.lmp", "*.cfg"}) {
            made.emplace_back(more);
        }

        return made;
    }();

    return held;
}

const std::vector<std::string> &port() {
#ifdef _WIN32
    static const std::vector<std::string> held = listOf({"*.exe", "*.bat"});
#else
    static const std::vector<std::string> held = listOf({"*"});
#endif

    return held;
}

const std::vector<std::string> &zdl() {
    static const std::vector<std::string> held = listOf({"*.zdl"});

    return held;
}

const std::vector<std::string> &config() {
    static const std::vector<std::string> held = listOf({"*.json", "*.ini"});

    return held;
}

std::vector<std::string> save(const std::string_view saveExt) {
    return {"*" + std::string(saveExt)};
}

const std::vector<std::string> &replay(const bool clientDemos) {
    static const std::vector<std::string> demos = listOf({"*.lmp"});
    static const std::vector<std::string> withClientDemos = listOf({"*.lmp", "*.cld"});

    return clientDemos ? withClientDemos : demos;
}

Dropped split(const Names &paths, const Names &patterns, const bool folders) {
    Dropped out;

    for (const std::string &path : paths) {
        // A folder may come with a slash at the end, which would leave it no name.
        const std::filesystem::path file = std::filesystem::path(path).lexically_normal();
        const std::string name = (file.has_filename() ? file : file.parent_path()).filename().string();
        std::error_code code;
        const bool folder = std::filesystem::is_directory(file, code);

        if (folder ? folders : ttk::FilePicker::passes(patterns, name)) {
            out.taken.push_back(ttk::Format::from_path(file));
        } else {
            out.left.push_back(name);
        }
    }

    return out;
}

std::string listed(const Names &names) {
    constexpr size_t SHOWN = 3;

    const size_t named = names.size() > SHOWN + 1 ? SHOWN : names.size();
    std::string list;

    for (size_t at = 0; at < named; ++at) {
        if (at > 0) {
            list += at + 1 == names.size() ? " and " : ", ";
        }

        list += names[at];
    }

    if (named < names.size()) {
        list += " and " + std::to_string(names.size() - named) + " more";
    }

    return list;
}

std::string leftOut(const Names &names, const std::string &one, const std::string &many) {
    const std::string list = listed(names);

    return names.size() == 1 ? list + " is not " + one + ", so it was left out."
                             : list + " are not " + many + ", so they were left out.";
}
}
