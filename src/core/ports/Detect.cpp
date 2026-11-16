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

#include <algorithm>
#include <array>
#include <cctype>
#include <utility>

#include "ttk/system/Env.h"
#include "ttk/system/Paths.h"
#include "ttk/system/Text.h"

#include "core/launch/Program.h"
#include "core/ports/Catalog.h"
#include "core/ports/Detect.h"

using namespace ttk;

namespace {
#ifdef _WIN32
constexpr bool WINDOWS = true;
constexpr char SEPARATOR = ';';
constexpr std::array SUFFIXES = {"", ".exe", ".com", ".bat", ".cmd"};
#else
constexpr bool WINDOWS = false;
constexpr char SEPARATOR = ':';
constexpr std::array SUFFIXES = {""};
#endif

// Directory levels searched under a marked directory.
constexpr int DEPTH = 1;

struct Want {
    std::string mark;
    std::span<const std::string_view> names;
    bool dos{false};
};

struct Shelved {
    std::filesystem::path best;

    // An AppImage matched by directory name only.
    std::filesystem::path loose;
};

// Lowercase alphanumerics only, so different spellings of a name compare equal.
std::string squash(const std::string_view value) {
    std::string out;

    for (const char letter : value) {
        if (std::isalnum(static_cast<unsigned char>(letter)) != 0) {
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(letter))));
        }
    }

    return out;
}

bool under(const std::filesystem::path &root, const std::filesystem::path &file) {
    if (root.empty()) {
        return false;
    }

    const std::string top = Text::lower(root.generic_string());

    return Text::lower(file.generic_string()).starts_with(top + "/");
}

std::filesystem::path inside(const std::filesystem::path &where,
                             const std::span<const std::string_view> names,
                             const bool dos,
                             const int depth) {
    std::error_code code;
    std::vector<std::filesystem::path> deeper;
    std::filesystem::path best;
    std::filesystem::path loose;
    size_t rank = names.size();

    for (std::filesystem::directory_iterator walk(where, code), end; walk != end && !code; walk.increment(code)) {
        std::error_code asked;

        if (walk->is_directory(asked)) {
            if (depth > 0) {
                deeper.push_back(walk->path());
            }

            continue;
        }

        if (!walk->is_regular_file(asked) || !Catalog::runnable(walk->path(), dos)) {
            continue;
        }

        const std::string stem = Text::lower(walk->path().stem().string());

        bool named = false;

        for (size_t index = 0; index < rank; index++) {
            if (stem == names[index]) {
                best = walk->path();
                rank = index;
                named = true;

                break;
            }
        }

        // An AppImage is named for its version. The directory already said which port it is.
        if (!named && loose.empty() && Text::iequals(walk->path().extension().string(), ".appimage")) {
            loose = walk->path();
        }
    }

    if (!best.empty()) {
        return best;
    }

    if (!loose.empty()) {
        return loose;
    }

    for (const std::filesystem::path &down : deeper) {
        if (std::filesystem::path found = inside(down, names, dos, depth - 1); !found.empty()) {
            return found;
        }
    }

    return {};
}

// The Flathub ports that start the game, with the program each runs. The GZDoom Flatpak only shows
// a notice and the Zandronum one opens Doomseeker.
using Flatpak = std::pair<std::string_view, std::string_view>;

constexpr std::array FLATPAKS = {
        Flatpak{"org.zdoom.UZDoom", "uzdoom"},
        Flatpak{"org.chocolate_doom.ChocolateDoom", "chocolate-doom"},
        Flatpak{"io.github.fabiangreffrath.Doom", "crispy-doom"},
};

#ifndef _WIN32
std::filesystem::path inFlatpak(const std::string_view mark) {
    const std::filesystem::path home = Paths::home_directory();
    std::vector<std::filesystem::path> exports = {"/var/lib/flatpak/exports/bin"};

    if (!home.empty()) {
        exports.push_back(home / ".local" / "share" / "flatpak" / "exports" / "bin");
    }

    for (const std::filesystem::path &where : exports) {
        std::error_code code;

        for (std::filesystem::directory_iterator walk(where, code), end; walk != end && !code; walk.increment(code)) {
            const std::string name = walk->path().filename().string();
            const size_t dot = name.find_last_of('.');
            const std::string_view runs = Detect::flatpakProgram(name);
            std::error_code asked;

            // A listed app goes by the program it runs, any other by the last part of its ID.
            if (dot != std::string::npos && squash(runs.empty() ? std::string_view(name).substr(dot + 1) : runs) == mark
                && walk->is_regular_file(asked)) {
                return walk->path();
            }
        }
    }

    return {};
}
#endif

std::filesystem::path anyOnPath(const std::span<const std::string_view> names) {
    for (const std::string_view name : names) {
        if (std::filesystem::path found = Detect::onPath(name); !found.empty()) {
            return found;
        }
    }

    return {};
}

const std::vector<std::filesystem::path> &shelves() {
    static const std::vector<std::filesystem::path> found = [] {
        std::vector<std::filesystem::path> out;
        const std::filesystem::path home = Paths::home_directory();

#ifdef _WIN32
        for (const char *variable : {"ProgramFiles", "ProgramFiles(x86)", "ProgramW6432"}) {
            if (const std::string root = Env::get(variable); !root.empty()) {
                out.emplace_back(root);
            }
        }

        if (const std::string local = Env::get("LOCALAPPDATA"); !local.empty()) {
            out.push_back(std::filesystem::path(local) / "Programs");
            out.emplace_back(local);
        }

        if (!home.empty()) {
            out.push_back(home / "scoop" / "apps");
            out.push_back(home / "Games");
        }
#else
        for (const char *root : {"/opt", "/usr/games", "/usr/local/games", "/usr/lib/games"}) {
            out.emplace_back(root);
        }

        if (!home.empty()) {
            out.push_back(home / "Applications");
            out.push_back(home / ".local" / "bin");
            out.push_back(home / "Games");
        }
#endif

        std::error_code code;

        for (size_t index = out.size(); index > 0; index--) {
            const std::filesystem::path &shelf = out[index - 1];
            const auto at = static_cast<long long>(index) - 1;
            const bool twice =
                    std::ranges::any_of(out.begin(), out.begin() + at, [&shelf](const std::filesystem::path &had) {
                        return Detect::same(had, shelf);
                    });

            if (twice || !std::filesystem::is_directory(shelf, code)) {
                out.erase(out.begin() + at);
            }
        }

        return out;
    }();

    return found;
}

std::vector<Shelved> onShelves(const std::span<const Want> wants) {
    std::vector<Shelved> out(wants.size());

    for (const std::filesystem::path &shelf : shelves()) {
        std::error_code code;

        for (std::filesystem::directory_iterator walk(shelf, code), end; walk != end && !code; walk.increment(code)) {
            std::error_code asked;
            const std::filesystem::path &entry = walk->path();
            const bool directory = walk->is_directory(asked);

            if (!directory && !walk->is_regular_file(asked)) {
                continue;
            }

            const std::string squashed = squash(directory ? entry.filename().string() : entry.stem().string());
            const std::string stem = directory ? std::string() : Text::lower(entry.stem().string());

            for (size_t index = 0; index < wants.size(); index++) {
                const Want &want = wants[index];
                Shelved &held = out[index];

                if (!held.best.empty()) {
                    continue;
                }

                if (directory) {
                    if (squashed.starts_with(want.mark)) {
                        held.best = inside(entry, want.names, want.dos, DEPTH);
                    }

                    continue;
                }

                if (!Catalog::runnable(entry, want.dos)) {
                    continue;
                }

                if (std::ranges::find(want.names, stem) != want.names.end()) {
                    held.best = entry;

                    continue;
                }

                if (!want.dos && !WINDOWS && held.loose.empty() && squashed.starts_with(want.mark)
                    && Text::iequals(entry.extension().string(), ".appimage")) {
                    held.loose = entry;
                }
            }
        }
    }

    return out;
}

std::filesystem::path settled(const Want &want, const Shelved &shelved) {
    if (!want.dos) {
        if (std::filesystem::path found = anyOnPath(want.names); !found.empty()) {
            return found;
        }
    }

    if (!shelved.best.empty()) {
        return shelved.best;
    }

#ifndef _WIN32
    if (!want.dos) {
        if (std::filesystem::path found = inFlatpak(want.mark); !found.empty()) {
            return found;
        }
    }
#endif

    return shelved.loose;
}

std::filesystem::path
program(const std::span<const std::string_view> names, const std::string_view mark, const bool dos = false) {
    if (names.empty() || mark.empty()) {
        return {};
    }

    if (!dos) {
        if (std::filesystem::path found = anyOnPath(names); !found.empty()) {
            return found;
        }
    }

    const Want want{.mark = std::string(mark), .names = names, .dos = dos};

    return settled(want, onShelves(std::span(&want, 1)).front());
}
}

bool Detect::same(const std::filesystem::path &left, const std::filesystem::path &right) {
    if (left.empty() || right.empty()) {
        return false;
    }

    if (std::error_code code; std::filesystem::equivalent(left, right, code)) {
        return true;
    }

    if constexpr (WINDOWS) {
        return Text::iequals(left.generic_string(), right.generic_string());
    } else {
        return left.generic_string() == right.generic_string();
    }
}

std::filesystem::path Detect::onPath(const std::string_view name) {
    const std::string path = Env::get("PATH");

    if (name.empty() || path.empty()) {
        return {};
    }

    std::error_code code;

    for (const std::string &directory : Text::split(path, SEPARATOR)) {
        if (directory.empty()) {
            continue;
        }

        for (const char *suffix : SUFFIXES) {
            if (std::filesystem::path candidate = std::filesystem::path(directory) / (std::string(name) + suffix);
                std::filesystem::is_regular_file(candidate, code)) {
                return candidate;
            }
        }
    }

    return {};
}

const std::filesystem::path &Detect::dosbox() {
    static constexpr std::array<std::string_view, 3> NAMES = {"dosbox", "dosbox-x", "dosbox-staging"};

    static const std::filesystem::path found = program(NAMES, "dosbox");

    return found;
}

std::span<const Detect::Found> Detect::ports() {
    static const std::vector<Found> found = [] {
        const std::span<const Catalog::Port> catalog = Catalog::ports();
        std::vector<std::string_view> executables;
        std::vector<Want> wants;

        executables.reserve(catalog.size());
        wants.reserve(catalog.size());

        for (const Catalog::Port &port : catalog) {
            executables.push_back(port.executable);
        }

        // Directories are named for the id: the executable of Doom Legacy is doom3.
        for (size_t index = 0; index < catalog.size(); index++) {
            wants.push_back(Want{
                    .mark = squash(Program::known(catalog[index].program).word),
                    .names = std::span(&executables[index], 1),
                    .dos = catalog[index].dos,
            });
        }

        const std::vector<Shelved> shelved = onShelves(wants);
        const std::filesystem::path mine = Catalog::directory();
        std::vector<Found> out;

        for (size_t index = 0; index < catalog.size(); index++) {
            std::filesystem::path where = settled(wants[index], shelved[index]);

            if (where.empty() || under(mine, where)) {
                continue;
            }

            const Program::Known &known = Program::known(catalog[index].program);

            out.push_back(Found{
                    .portId = std::string(known.word),
                    .name = std::string(known.name),
                    .program = std::move(where),
                    .dos = catalog[index].dos,
            });
        }

        return out;
    }();

    return found;
}

const Detect::Found *Detect::of(const std::filesystem::path &file) {
    if (file.empty()) {
        return nullptr;
    }

    for (const Found &found : ports()) {
        if (same(found.program, file)) {
            return &found;
        }
    }

    return nullptr;
}

bool Detect::flatpakExport(const std::filesystem::path &program) {
    const std::filesystem::path folder = program.parent_path();

    return folder.filename() == "bin" && folder.parent_path().filename() == "exports"
           && folder.parent_path().parent_path().filename() == "flatpak";
}

std::string_view Detect::flatpakProgram(const std::string_view app) {
    // NOLINTNEXTLINE(readability-qualified-auto): the MSVC array iterator is a class, not a pointer.
    const auto found = std::ranges::find(FLATPAKS, app, &Flatpak::first);

    return found == FLATPAKS.end() ? std::string_view() : found->second;
}
