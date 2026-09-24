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

#include <string>

#include "ttk/system/Env.h"

#include "support/Sandbox.h"

using namespace ttk;

namespace {

std::filesystem::path &held() {
    static std::filesystem::path path;

    return path;
}

void point(const char *name, const std::filesystem::path &at) {
    std::error_code code;

    std::filesystem::create_directories(at, code);

    Env::set(name, at.string().c_str());
}

}

namespace bench::Sandbox {

void enter() {
    if (!held().empty()) {
        return;
    }

    std::error_code code;

    const std::filesystem::path base =
        std::filesystem::temp_directory_path(code) / "qzdl-bench";

    std::filesystem::remove_all(base, code);
    std::filesystem::create_directories(base, code);

    held() = base;

    point("HOME", base / "home");
    point("XDG_CONFIG_HOME", base / "config");
    point("XDG_DATA_HOME", base / "data");
    point("XDG_CACHE_HOME", base / "cache");
    point("APPDATA", base / "appdata");
    point("USERPROFILE", base / "home");

    Env::set("XDG_CONFIG_DIRS", (base / "etc").string().c_str());
}

void leave() {
    if (held().empty()) {
        return;
    }

    std::error_code code;

    std::filesystem::remove_all(held(), code);

    held().clear();
}

const std::filesystem::path &root() {
    return held();
}

std::filesystem::path scratch(const char *name) {
    const std::filesystem::path at = held() / "scratch" / name;

    std::error_code code;

    std::filesystem::remove_all(at, code);
    std::filesystem::create_directories(at, code);

    return at;
}

}
