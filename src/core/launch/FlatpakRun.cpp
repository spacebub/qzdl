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

#include <algorithm>
#include <filesystem>
#include <string_view>
#include <system_error>
#include <utility>

#include "core/launch/Arguments.h"
#include "core/launch/FlatpakRun.h"
#include "core/ports/Detect.h"
#include "core/ports/Flatpak.h"

namespace FlatpakRun {

namespace {

using Arguments::Access;
using Arguments::Reached;

constexpr std::string_view MISSING = "The source port is a Flatpak, and flatpak is not on the PATH.";
constexpr std::string_view GONE = "The Flatpak export is gone. Reinstall the app or pick another engine.";
constexpr std::string_view WAD_FOLDER = "DOOMWADDIR";

using Variable = std::pair<std::string, std::string>;

std::filesystem::path normal(const std::filesystem::path &path) {
    const std::filesystem::path out = path.lexically_normal();

    return out.filename().empty() && out.has_relative_path() ? out.parent_path() : out;
}

bool inside(const std::filesystem::path &path, const std::filesystem::path &outer) {
    return std::ranges::mismatch(outer, path).in1 == outer.end();
}

// A read path inside a written folder goes too, since its read-only grant would cover that part of the folder.
bool holds(const Reached &outer, const Reached &inner) {
    return outer.path != inner.path && inside(inner.path, outer.path)
           && (outer.access == Access::writes || inner.access == Access::reads);
}

std::string troubleOf(const std::filesystem::path &file, const std::filesystem::path &flatpak) {
    if (std::error_code code; !std::filesystem::exists(file, code)) {
        return std::string(GONE);
    }

    if (const std::string_view refused = Flatpak::refusal(file.filename().string()); !refused.empty()) {
        return std::string(refused);
    }

    return flatpak.empty() ? std::string(MISSING) : std::string();
}

}

std::vector<Reached> grants(const std::vector<Reached> &reached, const std::filesystem::path &home) {
    const std::filesystem::path own = home.empty() ? std::filesystem::path() : normal(home);
    std::vector<Reached> merged;

    for (const Reached &each : reached) {
        std::filesystem::path path = normal(each.path);

        // flatpak refuses to grant the root or a relative path.
        if (!path.is_absolute() || !path.has_relative_path() || (!own.empty() && inside(path, own))) {
            continue;
        }

        const auto found = std::ranges::find(merged, path, &Reached::path);

        if (found == merged.end()) {
            merged.push_back({.path = std::move(path), .access = each.access});
        } else if (each.access == Access::writes) {
            found->access = Access::writes;
        }
    }

    std::vector<Reached> out;

    for (const Reached &each : merged) {
        if (std::ranges::none_of(merged, [&each](const Reached &outer) { return holds(outer, each); })) {
            out.push_back(each);
        }
    }

    return out;
}

std::string grant(const Reached &reached) {
    std::string out = "--filesystem=";

    // flatpak reads a colon as the start of :ro, so a colon in the path is escaped.
    for (const char letter : reached.path.string()) {
        if (letter == ':' || letter == '\\') {
            out.push_back('\\');
        }

        out.push_back(letter);
    }

    return reached.access == Access::reads ? out + ":ro" : out;
}

std::vector<std::string> installationArguments(const Flatpak::Deploy &deployed) {
    std::vector<std::string> out;

    if (deployed.installation == Flatpak::Installation::user) {
        out.emplace_back("--user");
    } else if (deployed.installation == Flatpak::Installation::system) {
        out.emplace_back("--system");
    }

    if (!deployed.folder.empty()) {
        out.push_back("--branch=" + deployed.branch);
        out.push_back("--arch=" + deployed.arch);
    }

    return out;
}

Command command(Launch launch, std::string *error) {
    std::filesystem::path flatpak = Detect::onPath("flatpak");

    if (std::string said = troubleOf(launch.file, flatpak); !said.empty()) {
        if (error != nullptr) {
            *error = std::move(said);
        }

        return {};
    }

    const std::string app = launch.file.filename().string();
    std::vector<Reached> &reached = launch.reached;

    // Without a grant the app starts in an empty private folder, and what it writes there is lost at exit.
    if (launch.runDirectory.is_absolute()) {
        reached.push_back({.path = launch.runDirectory, .access = Access::writes});
    }

    if (launch.gameFolder.is_absolute()) {
        reached.push_back({.path = launch.gameFolder, .access = Access::reads});

        if (!std::ranges::contains(launch.variables, WAD_FOLDER, &Variable::first)) {
            launch.variables.insert(launch.variables.begin(), {std::string(WAD_FOLDER), launch.gameFolder.string()});
        }
    }

    std::vector<std::string> arguments = installationArguments(Flatpak::deploy(launch.file));

    arguments.insert(arguments.begin(), "run");

    for (const Reached &each : grants(reached, Flatpak::home(launch.file))) {
        arguments.push_back(grant(each));
    }

    for (const auto &[name, value] : launch.variables) {
        std::string set = "--env=";

        set.append(name).append("=").append(value);
        arguments.push_back(std::move(set));
    }

    if (const std::string_view runs = Flatpak::executable(app); !runs.empty()) {
        arguments.push_back("--command=" + std::string(runs));
    }

    arguments.push_back(app);

    return {.flatpak = std::move(flatpak), .arguments = std::move(arguments)};
}

}
