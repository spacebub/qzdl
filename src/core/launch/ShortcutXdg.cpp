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
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

#include "ttk/system/Env.h"
#include "ttk/system/Paths.h"

#include "core/config/Session.h"
#include "core/launch/Shortcut.h"

namespace fs = std::filesystem;

namespace {

// Named by the .zdl, which is named by the profile id, so a second make writes over the first.
fs::path entryOf(const fs::path &zdl) {
    const fs::path data = ttk::Paths::data_directory();

    return data.empty() ? fs::path()
                        : data.parent_path() / "applications" / ("zdl4-" + zdl.stem().string() + ".desktop");
}

std::string escaped(const std::string_view value) {
    std::string out;

    for (const char each : value) {
        out += each == '\\' ? "\\\\" : std::string(1, each);
    }

    return out;
}

// An Exec argument: quoted, then escaped again as a string value, so a backslash ends up as four.
std::string argument(const fs::path &path) {
    std::string quoted = "\"";

    for (const char each : path.string()) {
        if (each == '"' || each == '`' || each == '$' || each == '\\') {
            quoted += '\\';
        }

        quoted += each == '%' ? std::string("%%") : std::string(1, each);
    }

    return escaped(quoted + "\"");
}

}

namespace Shortcut {

fs::path program() {
    // Inside an AppImage the program sits in a mount that changes with every start.
    if (std::string image = ttk::Env::get("APPIMAGE"); !image.empty()) {
        return image;
    }

    std::error_code code;

    return fs::read_symlink("/proc/self/exe", code);
}

fs::path make(const std::string &stem,
              const fs::path &zdl,
              const fs::path &config,
              const std::string_view iconPng,
              std::string *error) {
    fs::path entry = entryOf(zdl);
    const fs::path exe = program();
    const fs::path icon = zdl.parent_path() / "zdl4.png";
    std::string why;
    std::error_code code;

    if (entry.empty()) {
        why = "the applications folder could not be found";
    } else if (exe.empty()) {
        why = "the path of ZDL4 could not be found";
    } else if (fs::create_directories(entry.parent_path(), code); code) {
        why = "could not make " + entry.parent_path().string();
    } else {
        std::ofstream(icon, std::ios::binary | std::ios::trunc)
                .write(iconPng.data(), static_cast<std::streamsize>(iconPng.size()));

        std::ofstream out(entry, std::ios::trunc);

        out << "[Desktop Entry]\nType=Application\nName=" << escaped(stem) << "\nComment=Launch " << escaped(stem)
            << " with ZDL4\nExec=" << argument(exe) << ' ' << Session::LAUNCH << ' ' << argument(zdl)
            << (config.empty() ? std::string() : ' ' + argument(config)) << "\nIcon=" << escaped(icon.string())
            << "\nTerminal=false\nCategories=Game;\n";

        if (out.flush()) {
            return entry;
        }

        why = "could not write " + entry.string();
    }

    if (error != nullptr) {
        *error = why;
    }

    return {};
}

}
