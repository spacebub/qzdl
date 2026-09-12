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

#include <initializer_list>

#include "gui/app/Filters.h"

namespace {

std::vector<std::string> listOf(const std::initializer_list<const char *> names) {
    return {names.begin(), names.end()};
}

}

namespace Filters {

const std::vector<std::string> &wad() {
    static const std::vector<std::string> held = listOf({
        "*.wad", "*.pwad", "*.iwad", "*.pk3", "*.pk7", "*.pkz", "*.pke", "*.ipk3", "*.ipk7",
        "*.zip", "*.7z", "*.deh", "*.bex", "*.lmp", "*.cfg",
    });

    return held;
}

const std::vector<std::string> &port() {
#ifdef _WIN32
    static const std::vector<std::string> held = listOf({"*.exe"});
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

const std::vector<std::string> &save() {
    static const std::vector<std::string> held =
        listOf({"*.zds", "*.dsg", "*.esg", "*.sav", "*.save"});

    return held;
}

const std::vector<std::string> &replay() {
    static const std::vector<std::string> held = listOf({"*.lmp"});

    return held;
}

}
