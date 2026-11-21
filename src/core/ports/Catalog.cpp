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

#include "ttk/system/Text.h"

#include "core/config/ConfigPaths.h"
#include "core/launch/KnownPorts.h"
#include "core/ports/Catalog.h"

using namespace ttk;

namespace {

#ifdef _WIN32
constexpr bool WINDOWS = true;
#else
constexpr bool WINDOWS = false;
#endif

}

namespace {

constexpr std::array KNOWN = std::to_array<Catalog::Download>({
        {
                .port = Port::Port::uzdoom,
                .blurb = "GZDoom carried on. The ZDoom family's current port, and what most of what "
                         "is being written today expects.",
                .homepage = "https://zdoom.org",
                .repository = "UZDoom/UZDoom",
                .file = "",
                .version = "",
                .sha256 = "",
                .windowsBuild = "windows+.zip",
                .linuxBuild = "linux+.appimage",
                .executable = "uzdoom",
                .dos = false,
        },
        {
                .port = Port::Port::gzdoom,
                .blurb = "Hardware rendering and ZScript. The port a decade of mods was written for.",
                .homepage = "https://zdoom.org",
                .repository = "ZDoom/gzdoom",
                .file = "",
                .version = "",
                .sha256 = "",
                .windowsBuild = "windows.zip",
                .linuxBuild = "",
                .executable = "gzdoom",
                .dos = false,
        },
        {
                .port = Port::Port::zdoom,
                .blurb = "The port the whole family grew out of. Discontinued in 2017 and kept here "
                         "for what was written for it.",
                .homepage = "https://zdoom.org/downloads",
                .repository = "",
                .file = "https://zdoom.org/files/zdoom/2.8/zdoom-2.8.1.zip",
                .version = "2.8.1",
                .sha256 = "e2bdb7555273bcea636e64eaf202835e747dfbed4d2e83521a7b209a4627d77a",
                .windowsBuild = "zdoom-2.8.1.zip",
                .linuxBuild = "",
                .executable = "zdoom",
                .dos = false,
        },
        {
                .port = Port::Port::zandronum,
                .blurb = "ZDoom for multiplayer: client and server, bots, and a browser full of games "
                         "to join.",
                .homepage = "https://zandronum.com/download",
                .repository = "",
                .file = "https://zandronum.com/downloads/zandronum3.2.1-win64-base.zip",
                .version = "3.2.1",
                .sha256 = "4f367e7c0e0af02d0a7d88fd4bf1d401787d093444fcfb766bf01c20ece5ed78",
                .windowsBuild = "zandronum3.2.1-win64-base.zip",
                // The Linux release is a tarball against system libraries.
                .linuxBuild = "",
                .executable = "zandronum",
                .dos = false,
        },
        {
                .port = Port::Port::dsda,
                .blurb = "Boom and MBF21, and what demos are recorded and played back in.",
                .homepage = "https://github.com/kraflab/dsda-doom",
                .repository = "kraflab/dsda-doom",
                .file = "",
                .version = "",
                .sha256 = "",
                .windowsBuild = "win-x64.zip",
                .linuxBuild = "linux-x86_64.appimage",
                .executable = "dsda-doom",
                .dos = false,
        },
        {
                .port = Port::Port::prboomPlus,
                .blurb = "The port compatibility levels came from, and what DSDA-Doom was forked out of.",
                .homepage = "https://github.com/coelckers/prboom-plus",
                .repository = "coelckers/prboom-plus",
                .file = "",
                .version = "",
                .sha256 = "",
                .windowsBuild = "prboom-plus+.zip",
                // No Linux build is put out.
                .linuxBuild = "",
                .executable = "prboom-plus",
                .dos = false,
        },
        {
                .port = Port::Port::helion,
                .blurb = "A modern Doom engine written from the ground up with a focus on performance.",
                .homepage = "https://github.com/Helion-Engine/Helion",
                .repository = "Helion-Engine/Helion",
                .file = "",
                .version = "",
                .sha256 = "",
                .windowsBuild = "win-x64_AOT.zip",
                .linuxBuild = "linux-x64_AOT.zip",
                .executable = "helion",
                .dos = false,
        },
        {
                .port = Port::Port::eternity,
                .blurb = "Team Eternity's port of SMMU, with its own EDF and ExtraData formats for "
                         "defining things, levels and effects.",
                .homepage = "https://eternity.youfailit.net",
                .repository = "team-eternity/eternity",
                .file = "",
                .version = "",
                .sha256 = "",
                .windowsBuild = "win64.zip",
                // No Linux build is put out.
                .linuxBuild = "",
                .executable = "eternity",
                .dos = false,
        },
        {
                .port = Port::Port::woof,
                .blurb = "MBF21 in a modern shape, with the feel of the original kept as it was.",
                .homepage = "https://github.com/fabiangreffrath/woof",
                .repository = "fabiangreffrath/woof",
                .file = "",
                .version = "",
                .sha256 = "",
                .windowsBuild = "win64.zip",
                .linuxBuild = "linux.appimage",
                .executable = "woof",
                .dos = false,
        },
        {
                .port = Port::Port::nugget,
                .blurb = "Woof! with a long list of extras on top of it.",
                .homepage = "https://github.com/MrAlaux/Nugget-Doom",
                .repository = "MrAlaux/Nugget-Doom",
                .file = "",
                .version = "",
                .sha256 = "",
                .windowsBuild = "win64.zip",
                .linuxBuild = "linux.appimage",
                .executable = "nugget-doom",
                .dos = false,
        },
        {
                .port = Port::Port::chocolate,
                .blurb = "Doom as it played in 1993, limits and bugs included.",
                .homepage = "https://www.chocolate-doom.org",
                .repository = "chocolate-doom/chocolate-doom",
                .file = "",
                .version = "",
                .sha256 = "",
                .windowsBuild = "chocolate-doom-+win64.zip",
                .linuxBuild = "",
                .executable = "chocolate-doom",
                .dos = false,
        },
        {
                .port = Port::Port::crispy,
                .blurb = "Chocolate Doom with the limits lifted and a taller picture.",
                .homepage = "https://github.com/fabiangreffrath/crispy-doom",
                .repository = "fabiangreffrath/crispy-doom",
                .file = "",
                .version = "",
                .sha256 = "",
                .windowsBuild = "crispy-doom-+win64.zip",
                .linuxBuild = "",
                .executable = "crispy-doom",
                .dos = false,
        },
        {
                .port = Port::Port::boom,
                .blurb = "TeamTNT's 1998 DOS release. Lifted the engine's limits and wrote the line "
                         "and sector types every port since has kept.",
                .homepage = "https://doomwiki.org/wiki/Boom",
                .repository = "",
                .file = "https://www.gamers.org/pub/idgames/themes/TeamTNT/boom/boom202.zip",
                .version = "2.02",
                .sha256 = "076d0a8ca988d1c9aa06373154ba0844630bd6e0e5b21db751f2bbcfadc6334d",
                .windowsBuild = "boom202.zip",
                .linuxBuild = "boom202.zip",
                .executable = "boom",
                .dos = true,
        },
        {
                .port = Port::Port::mbf,
                .blurb = "Marine's Best Friend, Lee Killough's last DOS port and what Woof! and DSDA- "
                         "Doom are named after.",
                .homepage = "https://doomwiki.org/wiki/MBF",
                .repository = "",
                .file = "https://www.gamers.org/pub/idgames/source/mbf.zip",
                .version = "2.04",
                .sha256 = "19f1e48c3726449ef33e24c5e18041ccabbe08e8192361ca77a6c77187ebd083",
                .windowsBuild = "mbf.zip",
                .linuxBuild = "mbf.zip",
                .executable = "mbf",
                .dos = true,
        },
        {
                .port = Port::Port::legacy,
                .blurb = "The 1998 DOS release: high resolutions, free look and split screen, years "
                         "before anyone else had them.",
                .homepage = "https://doomwiki.org/wiki/Doom_Legacy",
                .repository = "",
                .file = "https://www.gamers.org/pub/idgames/source/legacy120.zip",
                .version = "1.2",
                .sha256 = "76c2545513d9a895c5fcf48e080613a713226a3834955940f683b2b677f601bf",
                .windowsBuild = "legacy120.zip",
                .linuxBuild = "legacy120.zip",
                .executable = "doom3",
                .dos = true,
        },
});

static_assert(std::ranges::all_of(KNOWN,
                                  [](const Catalog::Download &download) {
                                      return std::ranges::count(KNOWN, download.port, &Catalog::Download::port) == 1;
                                  }),
              "the downloads service finds the engine of a download by its port, so no two share one");

}

std::span<const Catalog::Download> Catalog::downloads() {
    return KNOWN;
}

const Catalog::Download *Catalog::find(const std::string_view id) {
    for (const Download &download : downloads()) {
        if (Catalog::id(download) == id) {
            return &download;
        }
    }

    return nullptr;
}

std::string_view Catalog::id(const Download &download) {
    return Port::known(download.port).id;
}

std::string_view Catalog::name(const Download &download) {
    return Port::known(download.port).name;
}

std::string_view Catalog::pattern(const Download &download) {
#ifdef _WIN32
    return download.windowsBuild;
#else
    return download.linuxBuild;
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

    return Text::iends_with(lowered, tokens.back());
}

std::filesystem::path Catalog::directory() {
    const std::filesystem::path data = ConfigPaths::dataDirectory();

    return data.empty() ? std::filesystem::path() : data / "source_ports";
}

// Without an id it would be the ports root, which Downloads::erase removes.
std::filesystem::path Catalog::directory(const Download &download) {
    const std::filesystem::path root = directory();

    return root.empty() || id(download).empty() ? std::filesystem::path() : root / id(download);
}

std::filesystem::path Catalog::downloadDirectory() {
    const std::filesystem::path data = ConfigPaths::dataDirectory();

    return data.empty() ? std::filesystem::path() : data / "downloads";
}

bool Catalog::runnable(const std::filesystem::path &file, const bool dos) {
    const std::string extension = Text::lower(file.extension().string());

    // ReSharper disable once CppRedundantBooleanExpressionArgument
    if (dos || WINDOWS) { // Defined per platform
        return extension == ".exe";
    }

    return extension.empty() || extension == ".appimage";
}

std::filesystem::path
Catalog::file(const std::filesystem::path &directory, const std::string_view name, const bool dos) {
    std::error_code code;
    const std::string wanted = Text::lower(name);

    // Newest wins when several fit.
    std::filesystem::path best;
    std::filesystem::file_time_type when{};
    std::filesystem::path fallback;

    for (std::filesystem::recursive_directory_iterator walk(directory, code), end; walk != end && !code;
         walk.increment(code)) {
        std::error_code asked;

        if (!walk->is_regular_file(asked)) {
            continue;
        }

        const std::filesystem::path &file = walk->path();

        if (!runnable(file, dos)) {
            continue;
        }

        const std::string stem = Text::lower(file.stem().string());

        const bool named =
                stem == wanted || (!dos && !WINDOWS && Text::lower(file.extension().string()) == ".appimage");

        if (named) {
            if (const std::filesystem::file_time_type stamp = walk->last_write_time(asked);
                best.empty() || stamp > when) {
                best = file;
                when = stamp;
            }

            continue;
        }

        if (fallback.empty() && stem.contains(wanted)
            && (dos || WINDOWS
                || (walk->status(asked).permissions() & std::filesystem::perms::owner_exec)
                           != std::filesystem::perms::none)) {
            fallback = file;
        }
    }

    return best.empty() ? fallback : best;
}
