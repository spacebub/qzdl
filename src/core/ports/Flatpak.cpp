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
#include <fstream>
#include <iterator>
#include <map>
#include <mutex>
#include <span>
#include <system_error>

#include "ttk/system/Paths.h"
#include "ttk/system/Text.h"

#include "core/ports/Flatpak.h"

namespace Flatpak {

namespace {

// The Flathub apps ZDL4 knows. Each runs with --command, since the default command of an app need not start the
// game. The GZDoom one only shows a notice and holds no game.
struct App {
    std::string_view id;
    std::string_view command;
    std::string_view refused;
};

constexpr std::array FLATPAKS = {
        App{.id = "org.zdoom.UZDoom", .command = "uzdoom", .refused = ""},
        App{.id = "org.chocolate_doom.ChocolateDoom", .command = "chocolate-doom", .refused = ""},
        App{.id = "io.github.fabiangreffrath.Doom", .command = "crispy-doom", .refused = ""},
        App{.id = "com.zandronum.Zandronum", .command = "zandronum", .refused = ""},
        App{
                .id = "org.zdoom.GZDoom",
                .command = "",
                .refused = "This Flatpak only shows a notice. Install UZDoom instead.",
        },
};

struct Installed {
    Installation installation;
    std::filesystem::path exports;
};

std::vector<Installed> installations() {
    std::vector<Installed> out = {{.installation = Installation::system, .exports = "/var/lib/flatpak/exports/bin"}};

    if (const std::filesystem::path home = ttk::Paths::home_directory(); !home.empty()) {
        out.push_back({
                .installation = Installation::user,
                .exports = (home / ".local" / "share" / "flatpak" / "exports" / "bin").lexically_normal(),
        });
    }

    return out;
}

const App *listed(const std::string_view app) {
    const std::span<const App> all(FLATPAKS);
    const auto found = std::ranges::find(all, app, &App::id);

    return found == all.end() ? nullptr : &*found;
}

bool blank(const char letter) {
    return std::isspace(static_cast<unsigned char>(letter)) != 0;
}

std::string attribute(const std::string_view tag, const std::string_view name) {
    for (size_t at = tag.find(name); at != std::string_view::npos; at = tag.find(name, at + 1)) {
        size_t after = at + name.size();

        while (after < tag.size() && blank(tag[after])) {
            ++after;
        }

        if (at == 0 || !blank(tag[at - 1]) || after >= tag.size() || tag[after] != '=') {
            continue;
        }

        ++after;

        while (after < tag.size() && blank(tag[after])) {
            ++after;
        }

        if (after < tag.size() && (tag[after] == '"' || tag[after] == '\'')) {
            const size_t end = tag.find(tag[after], after + 1);

            return end == std::string_view::npos ? std::string()
                                                 : ttk::Text::trim(tag.substr(after + 1, end - after - 1));
        }
    }

    return {};
}

std::filesystem::path commitOf(const std::filesystem::path &file) {
    std::error_code code;
    const std::filesystem::path real = std::filesystem::canonical(file, code);
    const std::filesystem::path commit = real.parent_path().parent_path().parent_path();
    const std::filesystem::path app = commit.parent_path().parent_path().parent_path();
    const bool laidOut = !code && real.filename() == file.filename() && real.parent_path().filename() == "bin"
                         && real.parent_path().parent_path().filename() == "export" && app.filename() == file.filename()
                         && app.parent_path().filename() == "app";

    return laidOut ? commit : std::filesystem::path();
}

std::string releaseIn(const std::filesystem::path &commit, const std::string &app) {
    const std::filesystem::path share = commit / "files" / "share";
    const std::array candidates = {
            share / "metainfo" / (app + ".metainfo.xml"),
            share / "metainfo" / (app + ".appdata.xml"),
            share / "appdata" / (app + ".appdata.xml"),
    };

    for (const std::filesystem::path &candidate : candidates) {
        if (std::ifstream in(candidate, std::ios::binary); in) {
            return firstRelease(std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()));
        }
    }

    return {};
}

}

bool exported(const std::filesystem::path &file) {
    const std::filesystem::path folder = file.parent_path();

    return folder.filename() == "bin" && folder.parent_path().filename() == "exports"
           && folder.parent_path().parent_path().filename() == "flatpak";
}

std::string_view executable(const std::string_view app) {
    const App *found = listed(app);

    return found == nullptr ? std::string_view() : found->command;
}

std::string_view refusal(const std::string_view app) {
    const App *found = listed(app);

    return found == nullptr ? std::string_view() : found->refused;
}

std::vector<Export> exports() {
    std::vector<Export> out;

    for (const Installed &installed : installations()) {
        std::error_code code;

        for (std::filesystem::directory_iterator walk(installed.exports, code), end; walk != end && !code;
             walk.increment(code)) {
            const std::string name = walk->path().filename().string();
            const size_t dot = name.find_last_of('.');
            const std::string_view runs = executable(name);
            std::error_code asked;

            if (dot != std::string::npos && refusal(name).empty() && walk->is_regular_file(asked)) {
                out.push_back({.file = walk->path(), .name = runs.empty() ? name.substr(dot + 1) : std::string(runs)});
            }
        }
    }

    return out;
}

std::filesystem::path home(const std::filesystem::path &file) {
    return exported(file) ? ttk::Paths::home_directory() / ".var" / "app" / file.filename() : std::filesystem::path();
}

Deploy deploy(const std::filesystem::path &file) {
    const std::filesystem::path folder = file.parent_path().lexically_normal();
    Deploy out{.installation = Installation::other, .folder = commitOf(file), .arch = {}, .branch = {}};

    for (const Installed &installed : installations()) {
        if (folder == installed.exports) {
            out.installation = installed.installation;
        }
    }

    if (!out.folder.empty()) {
        out.arch = out.folder.parent_path().parent_path().filename().string();
        out.branch = out.folder.parent_path().filename().string();
    }

    return out;
}

std::string version(const std::filesystem::path &file) {
    const std::filesystem::path commit = commitOf(file);

    if (commit.empty()) {
        return {};
    }

    // An update deploys a new commit folder, so what one folder holds never changes.
    static std::mutex guard;
    static std::map<std::filesystem::path, std::string> read;
    const std::scoped_lock held(guard);
    auto found = read.find(commit);

    if (found == read.end()) {
        found = read.emplace(commit, releaseIn(commit, file.filename().string())).first;
    }

    return found->second;
}

std::string firstRelease(const std::string_view metainfo) {
    static constexpr std::string_view TAG = "<release";

    for (size_t at = metainfo.find('<'); at != std::string_view::npos; at = metainfo.find('<', at + 1)) {
        const std::string_view rest = metainfo.substr(at);

        if (rest.starts_with("<!--")) {
            at = metainfo.find("-->", at);

            if (at == std::string_view::npos) {
                return {};
            }

            continue;
        }

        const bool named = rest.starts_with(TAG) && rest.size() > TAG.size()
                           && (blank(rest[TAG.size()]) || rest[TAG.size()] == '>' || rest[TAG.size()] == '/');

        if (named) {
            return attribute(rest.substr(TAG.size(), rest.find('>') - TAG.size()), "version");
        }
    }

    return {};
}

}
