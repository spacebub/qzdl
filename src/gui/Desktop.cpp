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

#include <array>

#include "core/Env.h"
#include "core/Paths.h"
#include "core/Text.h"
#include "gui/Desktop.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#else
#include "core/Process.h"
#endif

namespace {

// Best first, each with its file's stem: Slint cannot say what is installed,
// so the files are looked for where a system keeps them.
struct Face {
    const char *family;
    const char *stem;
};

constexpr std::array MONOSPACE = {
    Face{.family = "JetBrains Mono", .stem = "jetbrainsmono"},
    Face{.family = "Fira Code", .stem = "firacode"},
    Face{.family = "DejaVu Sans Mono", .stem = "dejavusansmono"},
    Face{.family = "Noto Sans Mono", .stem = "notosansmono"},
#ifdef _WIN32
    Face{.family = "Consolas", .stem = "consola"},
    Face{.family = "Courier New", .stem = "cour"},
#elifdef __APPLE__
    Face{.family = "Menlo", .stem = "menlo"},
    Face{.family = "Monaco", .stem = "monaco"},
#endif
};

// One walk for all of them: these directories hold thousands of files, and this
// runs on the interface's thread before the window is up.
void look(const std::filesystem::path &root, std::array<bool, MONOSPACE.size()> &found) {
    std::error_code code;

    for (std::filesystem::recursive_directory_iterator walk(
             root, std::filesystem::directory_options::skip_permission_denied, code), end;
         walk != end && !code; walk.increment(code)) {
        if (walk.depth() > 3) {
            walk.disable_recursion_pending();
        }

        const std::string stem = Text::lower(walk->path().stem().string());

        for (size_t index = 0; index < MONOSPACE.size(); ++index) {
            found[index] = found[index] || stem.starts_with(MONOSPACE[index].stem);
        }

        if (found.front()) {
            return;
        }
    }
}

std::vector<std::filesystem::path> fontDirectories() {
    std::vector<std::filesystem::path> roots;

#ifdef _WIN32
    if (const std::string windows = Env::get("SystemRoot"); !windows.empty()) {
        roots.emplace_back(std::filesystem::path(windows) / "Fonts");
    }
#elifdef __APPLE__
    roots.emplace_back("/System/Library/Fonts");
    roots.emplace_back("/Library/Fonts");
#else
    roots.emplace_back("/usr/share/fonts");
    roots.emplace_back("/usr/local/share/fonts");
#endif

    if (const std::filesystem::path home = Paths::homeDirectory(); !home.empty()) {
#ifdef _WIN32
        roots.push_back(home / "AppData/Local/Microsoft/Windows/Fonts");
#elifdef __APPLE__
        roots.push_back(home / "Library/Fonts");
#else
        roots.push_back(home / ".local/share/fonts");
        roots.push_back(home / ".fonts");
#endif
    }

    return roots;
}

}

bool Desktop::open(const std::string &target, std::string *why) {
    if (target.empty()) {
        if (why != nullptr) {
            *why = "there is nowhere to go";
        }

        return false;
    }

#ifdef _WIN32
    const int wide = MultiByteToWideChar(CP_UTF8, 0, target.c_str(), -1, nullptr, 0);
    std::wstring wanted(static_cast<size_t>(wide), L'\0');

    MultiByteToWideChar(CP_UTF8, 0, target.c_str(), -1, wanted.data(), wide);

    // At or below 32 is one of the old API's failure codes.
    const auto answer = reinterpret_cast<INT_PTR>(
        ShellExecuteW(nullptr, L"open", wanted.c_str(), nullptr, nullptr, SW_SHOWNORMAL));

    if (answer <= 32 && why != nullptr) {
        *why = "the shell refused it (error " + std::to_string(answer) + ")";
    }

    return answer > 32;
#else
#ifdef __APPLE__
    const char *opener = "open";
#else
    const char *opener = "xdg-open";
#endif

    // Started and forgotten: the opener outlives ZDL either way.
    return Process::start(opener, {target}, std::filesystem::current_path(), {},
                          nullptr, nullptr, why);
#endif
}

std::vector<std::string> Desktop::drives() {
    std::vector<std::string> found;

#ifdef _WIN32
    const DWORD mask = GetLogicalDrives();

    for (int letter = 0; letter < 26; letter++) {
        if ((mask & (1U << letter)) != 0) {
            found.push_back(std::string(1, static_cast<char>('A' + letter)) + ":/");
        }
    }
#endif

    return found;
}

std::string Desktop::monospaceFamily() {
    std::array<bool, MONOSPACE.size()> found{};

    for (const std::filesystem::path &root : fontDirectories()) {
        look(root, found);

        if (found.front()) {
            break;
        }
    }

    for (size_t index = 0; index < MONOSPACE.size(); ++index) {
        if (found[index]) {
            return MONOSPACE[index].family;
        }
    }

    return "monospace";
}
