/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
 *
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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

#include "core/Catalog.h"
#include "core/Paths.h"
#include "core/Text.h"

namespace {

#ifdef _WIN32
constexpr bool WINDOWS = true;
#else
constexpr bool WINDOWS = false;
#endif

}

namespace {

// The ports themselves. A build is asked for by the name it carries rather
// than by a link, so a release later this year is found by the same entry.
constexpr std::array KNOWN = std::to_array<Catalog::Port>({
    {
        .id = "uzdoom",
        .name = "UZDoom",
        .blurb = "GZDoom carried on. The ZDoom family's current port, and what most of what "
                 "is being written today expects.",
        .homepage = "https://zdoom.org",
        .repository = "UZDoom/UZDoom",
        .file = "",
        .version = "",
        .windowsBuild = "windows+.zip",
        .linuxBuild = "linux+.appimage",
        .program = "uzdoom",
        .dos = false,
    },
    {
        .id = "gzdoom",
        .name = "GZDoom",
        .blurb = "Hardware rendering and ZScript. The port a decade of mods was written for.",
        .homepage = "https://zdoom.org",
        .repository = "ZDoom/gzdoom",
        .file = "",
        .version = "",
        .windowsBuild = "windows.zip",
        .linuxBuild = "",
        .program = "gzdoom",
        .dos = false,
    },
    {
        .id = "zdoom",
        .name = "ZDoom",
        .blurb = "The port the whole family grew out of. Discontinued in 2017 and kept here "
                 "for what was written for it.",
        .homepage = "https://zdoom.org/downloads",
        .repository = "",
        .file = "https://zdoom.org/files/zdoom/2.8/zdoom-2.8.1.zip",
        .version = "2.8.1",
        .windowsBuild = "zdoom-2.8.1.zip",
        .linuxBuild = "",
        .program = "zdoom",
        .dos = false,
    },
    {
        .id = "zandronum",
        .name = "Zandronum",
        .blurb = "ZDoom for multiplayer: client and server, bots, and a browser full of games "
                 "to join.",
        .homepage = "https://zandronum.com/download",
        .repository = "",
        .file = "https://zandronum.com/downloads/zandronum3.2.1-win64-base.zip",
        .version = "3.2.1",
        .windowsBuild = "zandronum3.2.1-win64-base.zip",
        // What the project puts out for Linux is a tarball against system
        // libraries rather than anything that runs where it is unpacked.
        .linuxBuild = "",
        .program = "zandronum",
        .dos = false,
    },
    {
        .id = "dsda-doom",
        .name = "DSDA-Doom",
        .blurb = "Boom and MBF21, and what demos are recorded and played back in.",
        .homepage = "https://github.com/kraflab/dsda-doom",
        .repository = "kraflab/dsda-doom",
        .file = "",
        .version = "",
        .windowsBuild = "win-x64.zip",
        .linuxBuild = "linux-x86_64.appimage",
        .program = "dsda-doom",
        .dos = false,
    },
    {
        .id = "helion",
        .name = "Helion",
        .blurb = "A modern Doom engine written from the ground up with a focus on performance.",
        .homepage = "https://github.com/Helion-Engine/Helion",
        .repository = "Helion-Engine/Helion",
        .file = "",
        .version = "",
        .windowsBuild = "win-x64_AOT.zip",
        .linuxBuild = "linux-x64_AOT.zip",
        .program = "helion",
        .dos = false,
    },
    {
        .id = "woof",
        .name = "Woof!",
        .blurb = "MBF21 in a modern shape, with the feel of the original kept as it was.",
        .homepage = "https://github.com/fabiangreffrath/woof",
        .repository = "fabiangreffrath/woof",
        .file = "",
        .version = "",
        .windowsBuild = "win64.zip",
        .linuxBuild = "linux.appimage",
        .program = "woof",
        .dos = false,
    },
    {
        .id = "nugget-doom",
        .name = "Nugget Doom",
        .blurb = "Woof! with a long list of extras on top of it.",
        .homepage = "https://github.com/MrAlaux/Nugget-Doom",
        .repository = "MrAlaux/Nugget-Doom",
        .file = "",
        .version = "",
        .windowsBuild = "win64.zip",
        .linuxBuild = "linux.appimage",
        .program = "nugget-doom",
        .dos = false,
    },
    {
        .id = "chocolate-doom",
        .name = "Chocolate Doom",
        .blurb = "Doom as it played in 1993, limits and bugs included.",
        .homepage = "https://www.chocolate-doom.org",
        .repository = "chocolate-doom/chocolate-doom",
        .file = "",
        .version = "",
        .windowsBuild = "chocolate-doom-+win64.zip",
        .linuxBuild = "",
        .program = "chocolate-doom",
        .dos = false,
    },
    {
        .id = "crispy-doom",
        .name = "Crispy Doom",
        .blurb = "Chocolate Doom with the limits lifted and a taller picture.",
        .homepage = "https://github.com/fabiangreffrath/crispy-doom",
        .repository = "fabiangreffrath/crispy-doom",
        .file = "",
        .version = "",
        .windowsBuild = "crispy-doom-+win64.zip",
        .linuxBuild = "",
        .program = "crispy-doom",
        .dos = false,
    },
    {
        .id = "mbf",
        .name = "MBF",
        .blurb = "Marine's Best Friend, Lee Killough's last DOS port and what Woof! and DSDA- "
                 "Doom are named after.",
        .homepage = "https://doomwiki.org/wiki/MBF",
        .repository = "",
        .file = "https://www.gamers.org/pub/idgames/source/mbf.zip",
        .version = "2.04",
        .windowsBuild = "mbf.zip",
        .linuxBuild = "mbf.zip",
        .program = "mbf",
        .dos = true,
    },
    {
        .id = "doomlegacy",
        .name = "Doom Legacy",
        .blurb = "The 1998 DOS release: high resolutions, free look and split screen, years "
                 "before anyone else had them.",
        .homepage = "https://doomwiki.org/wiki/Doom_Legacy",
        .repository = "",
        .file = "https://www.gamers.org/pub/idgames/source/legacy120.zip",
        .version = "1.2",
        .windowsBuild = "legacy120.zip",
        .linuxBuild = "legacy120.zip",
        .program = "doom3",
        .dos = true,
    },
});

}

std::span<const Catalog::Port> Catalog::ports() {
    return KNOWN;
}

const Catalog::Port *Catalog::find(const std::string_view id) {
    for (const Port &port : ports()) {
        if (port.id == id) {
            return &port;
        }
    }

    return nullptr;
}

std::string_view Catalog::pattern(const Port &port) {
#ifdef _WIN32
    return port.windowsBuild;
#else
    return port.linuxBuild;
#endif
}

bool Catalog::matches(const std::string_view name, const std::string_view pattern) {
    if (pattern.empty()) {
        return false;
    }

    const std::string lowered = Text::lower(name);
    const std::vector<std::string> tokens = Text::split(pattern, '+');

    for (size_t index = 0; index + 1 < tokens.size(); index++) {
        if (!lowered.contains(Text::lower(tokens[index]))) {
            return false;
        }
    }

    return Text::iendsWith(lowered, tokens.back());
}

std::filesystem::path Catalog::directory() {
    const std::filesystem::path data = Paths::dataDirectory();

    return data.empty() ? std::filesystem::path() : data / "source_ports";
}

std::filesystem::path Catalog::directory(const Port &port) {
    const std::filesystem::path root = directory();

    return root.empty() ? std::filesystem::path() : root / port.id;
}

std::filesystem::path Catalog::downloads() {
    const std::filesystem::path data = Paths::dataDirectory();

    return data.empty() ? std::filesystem::path() : data / "downloads";
}

std::filesystem::path Catalog::program(const std::filesystem::path &directory,
                                       const std::string_view name, const bool dos) {
    std::error_code code;
    const std::string wanted = Text::lower(name);

    // A port fetched again over an older one leaves a build named after the
    // version it was, so of two that fit it is the newer that is meant.
    std::filesystem::path best;
    std::filesystem::file_time_type when{};
    std::filesystem::path fallback;

    for (std::filesystem::recursive_directory_iterator walk(directory, code), end;
         walk != end && !code; walk.increment(code)) {
        std::error_code asked;

        if (!walk->is_regular_file(asked)) {
            continue;
        }

        const std::filesystem::path &file = walk->path();
        const std::string stem = Text::lower(file.stem().string());
        const std::string extension = Text::lower(file.extension().string());

        bool named = false;

        // ReSharper disable once CppRedundantBooleanExpressionArgument
        if (dos || WINDOWS) { // Defined per platform
            if (extension != ".exe") {
                continue;
            }

            named = stem == wanted;
        } else {
            // An AppImage is the whole port in one file and is never
            // mistakable for anything else in there.
            named = extension == ".appimage" || (stem == wanted && extension.empty());

            if (!named && !extension.empty()) {
                continue;
            }
        }

        if (named) {
            if (const std::filesystem::file_time_type stamp = walk->last_write_time(asked);
                best.empty() || stamp > when) {
                best = file;
                when = stamp;
            }

            continue;
        }

        // Failing a name, something that carries the name and can be run.
        if (fallback.empty() && stem.contains(wanted)
            && (dos || WINDOWS
                || (walk->status(asked).permissions() & std::filesystem::perms::owner_exec)
                   != std::filesystem::perms::none)) {
            fallback = file;
        }
    }

    return best.empty() ? fallback : best;
}
