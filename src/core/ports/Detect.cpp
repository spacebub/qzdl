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
#include "ttk/util/Format.h"

#include "core/launch/Dialect.h"
#include "core/ports/Catalog.h"
#include "core/ports/Detect.h"
#include "core/ports/Flatpak.h"

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

std::filesystem::path anyOnPath(const std::span<const std::string_view> names) {
    for (const std::string_view name : names) {
        if (std::filesystem::path found = Detect::onPath(name); !found.empty()) {
            return found;
        }
    }

    return {};
}

std::vector<std::filesystem::path> shelves() {
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

std::filesystem::path settled(const Want &want, const Shelved &shelved, const std::span<const Flatpak::Export> apps) {
    if (!want.dos) {
        if (std::filesystem::path found = anyOnPath(want.names); !found.empty()) {
            return found;
        }
    }

    if (!shelved.best.empty()) {
        return shelved.best;
    }

    if (!want.dos) {
        for (const Flatpak::Export &app : apps) {
            if (squash(app.name) == want.mark) {
                return app.file;
            }
        }
    }

    return shelved.loose;
}

std::filesystem::path
executableFile(const std::span<const std::string_view> names, const std::string_view mark, const bool dos = false) {
    if (names.empty() || mark.empty()) {
        return {};
    }

    if (!dos) {
        if (std::filesystem::path found = anyOnPath(names); !found.empty()) {
            return found;
        }
    }

    const Want want{.mark = std::string(mark), .names = names, .dos = dos};

    return settled(want, onShelves(std::span(&want, 1)).front(), Flatpak::exports());
}

bool followsInstall(const Engine &engine) {
    return !engine.managed && !engine.dosbox && Flatpak::exported(std::string_view(engine.file));
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

    // A Flatpak DOSBox goes by its app name, such as dosbox-staging or DOSBox-X.
    static const std::filesystem::path found = [] {
        if (std::filesystem::path installed = executableFile(NAMES, "dosbox"); !installed.empty()) {
            return installed;
        }

        for (const Flatpak::Export &app : Flatpak::exports()) {
            if (std::ranges::any_of(NAMES,
                                    [&app](const std::string_view name) { return squash(app.name) == squash(name); })) {
                return app.file;
            }
        }

        return std::filesystem::path();
    }();

    return found;
}

std::vector<Detect::Found> Detect::ports() {
    const std::span<const Catalog::Download> catalog = Catalog::downloads();
    std::vector<std::string_view> executables;
    std::vector<Want> wants;

    executables.reserve(catalog.size());
    wants.reserve(catalog.size());

    for (const Catalog::Download &download : catalog) {
        executables.push_back(download.executable);
    }

    // Directories are named for the id: the executable of Doom Legacy is doom3.
    for (size_t index = 0; index < catalog.size(); index++) {
        wants.push_back(Want{
                .mark = squash(Catalog::id(catalog[index])),
                .names = std::span(&executables[index], 1),
                .dos = catalog[index].dos,
        });
    }

    const std::vector<Shelved> shelved = onShelves(wants);
    const std::vector<Flatpak::Export> apps = Flatpak::exports();
    const std::filesystem::path mine = Catalog::directory();
    std::vector<Found> out;

    for (size_t index = 0; index < catalog.size(); index++) {
        std::filesystem::path where = settled(wants[index], shelved[index], apps);

        if (where.empty() || under(mine, where)) {
            continue;
        }

        out.push_back(Found{
                .port = catalog[index].port,
                .name = std::string(Catalog::name(catalog[index])),
                .file = std::move(where),
                .dos = catalog[index].dos,
        });
    }

    return out;
}

Engine Detect::engine(const Found &found) {
    Engine out{
            .name = found.name,
            .file = Format::from_path(found.file),
            .dosbox = found.dos,
            .port = found.port,
            .managed = false,
            .detected = true,
            .version = {},
    };

    out.version = installedVersion(out);

    return out;
}

std::vector<Engine> Detect::unlisted(const std::span<const Engine> listed, const std::span<const Found> found) {
    std::vector<Engine> out;

    for (const Found &each : found) {
        const auto holds = [&each](const Engine &engine) {
            return same(std::filesystem::path(engine.file), each.file);
        };

        if (std::ranges::none_of(listed, holds) && std::ranges::none_of(out, holds)) {
            out.push_back(engine(each));
        }
    }

    return out;
}

std::vector<Engine> Detect::stillUnlisted(const std::span<const Engine> listed,
                                          const std::span<const Engine> compared,
                                          const std::span<const Engine> fresh) {
    std::vector<Engine> out;

    for (const Engine &each : fresh) {
        const auto holds = [&each, compared](const Engine &engine) {
            if (engine.file == each.file) {
                return true;
            }

            // Unlisted already asked the disk about every file that compared names.
            return !std::ranges::contains(compared, engine.file, &Engine::file)
                   && same(std::filesystem::path(engine.file), std::filesystem::path(each.file));
        };

        if (std::ranges::none_of(listed, holds)) {
            out.push_back(each);
        }
    }

    return out;
}

std::string Detect::installedVersion(const Engine &engine) {
    if (!followsInstall(engine)) {
        return {};
    }

    const std::string text = Flatpak::version(std::filesystem::path(engine.file));

    return Dialect::leadingRelease(text).empty() ? std::string() : text;
}

std::vector<Detect::Installed> Detect::installedVersions(const std::span<const Engine> engines) {
    std::vector<Installed> out;

    for (const Engine &engine : engines) {
        if (std::ranges::contains(out, engine.file, &Installed::file)) {
            continue;
        }

        if (std::string version = installedVersion(engine); !version.empty()) {
            out.push_back({.file = engine.file, .version = std::move(version)});
        }
    }

    return out;
}

bool Detect::storeVersions(const std::span<Engine> engines, const std::span<const Installed> read) {
    bool changed = false;

    for (Engine &engine : engines) {
        const auto found = std::ranges::find(read, engine.file, &Installed::file);

        if (found != read.end() && engine.version != found->version && !Dialect::leadingRelease(found->version).empty()
            && followsInstall(engine)) {
            engine.version = found->version;
            changed = true;
        }
    }

    return changed;
}
