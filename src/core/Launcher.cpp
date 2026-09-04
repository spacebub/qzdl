/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2018-2019  Lcferrum
 * Copyright (C) 2023-2026  spacebub
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

#include <algorithm>
#include <regex>

#include "core/Launcher.h"
#include "core/MapFile.h"
#include "core/Paths.h"
#include "core/Process.h"
#include "core/Text.h"

namespace {

struct ClassifiedFiles {
    std::vector<std::string> pwads;
    std::vector<std::string> dehs;
    std::vector<std::string> bexs;
    std::vector<std::string> autoexecs;
    std::vector<std::string> lumps;

    // Which of -deh and -bex goes last, decided by whichever kind appeared
    // last in the list. The source port applies the later one on top.
    char dehLast{1};
};

ClassifiedFiles classifyFiles(const std::vector<FileEntry> &files) {
    ClassifiedFiles out;

    for (const FileEntry &entry : files) {
        // Disabled entries stay in the list but off the command line.
        if (!entry.enabled) {
            continue;
        }

        if (Text::iendsWith(entry.file, ".bex")) {
            out.dehLast = 0;
            out.bexs.push_back(entry.file);
        } else if (Text::iendsWith(entry.file, ".deh")) {
            out.dehLast = 1;
            out.dehs.push_back(entry.file);
        } else if (Text::iendsWith(entry.file, ".cfg")) {
            out.autoexecs.push_back(entry.file);
        } else if (Text::iendsWith(entry.file, ".lmp")) {
            out.lumps.push_back(entry.file);
        } else {
            out.pwads.push_back(entry.file);
        }
    }

    return out;
}

std::string iwadPath(const Config &config, const Profile &profile) {
    const NameEntry *iwad = config.findIwad(profile.iwad);

    return iwad != nullptr ? iwad->file : std::string();
}

/*
Older ports only understand -warp, and what it takes depends on how the IWAD
names its maps: two numbers for ExMy, one for MAPxx. A name that fits neither
is passed as +map, which every modern port understands.
*/
std::vector<std::string> warpArguments(const std::string &iwad, const std::string &map) {
    if (iwad.empty()) {
        return {};
    }

    bool mapxx = false;

    if (const std::unique_ptr<MapFile> file = MapFile::open(iwad)) {
        mapxx = file->isMapXX();
    }

    std::smatch match;

    if (mapxx) {
        static const std::regex pattern("^MAP(\\d\\d)$", std::regex::icase);

        if (std::regex_match(map, match, pattern)) {
            return {"-warp", match[1].str()};
        }

        return {};
    }

    static const std::regex pattern("^E(\\d)M([1-9])$", std::regex::icase);

    if (std::regex_match(map, match, pattern)) {
        return {"-warp", match[1].str(), match[2].str()};
    }

    return {};
}

void append(std::vector<std::string> &into, const std::vector<std::string> &what) {
    into.insert(into.end(), what.begin(), what.end());
}

/*
Chocolate Doom keeps its settings in two files rather than one: the vanilla
half that -config names, and everything the port added on top of vanilla,
which is -extraconfig.
*/
bool splitsConfig(const std::filesystem::path &port) {
    const std::string name = Text::lower(port.stem().string());

    return name.find("chocolate") != std::string::npos || name.find("crispy") != std::string::npos;
}

// The second of those two files, named after the first.
std::filesystem::path extraConfigFile(const std::filesystem::path &config) {
    return config.parent_path() / (config.stem().string() + "-extra" + config.extension().string());
}

}

namespace Launcher {

std::filesystem::path executable(const Config &config) {
    const NameEntry *port = config.findPort(config.activeProfile().port);

    return port != nullptr ? std::filesystem::path(port->file) : std::filesystem::path();
}

std::filesystem::path getConfigPath(const Profile &profile) {
    if (profile.config.empty()) {
        return {};
    }

    // A hand edited config can name a file anywhere; anything else is a name
    // inside the directory ZDL keeps these in.
    std::filesystem::path named(profile.config);

    if (named.is_absolute()) {
        return named;
    }

    const std::filesystem::path directory = Paths::dataDirectory();

    return directory.empty() ? std::filesystem::path() : directory / named;
}

std::filesystem::path getConfigPath(const Config &config) {
    const Profile &profile = config.activeProfile();

    if (!config.general.profileConfigs || profile.sharedConfig) {
        return {};
    }

    return getConfigPath(profile);
}

std::vector<std::string> arguments(const Config &config) {
    std::vector<std::string> args;
    const Profile &profile = config.activeProfile();
    const std::string iwad = iwadPath(config, profile);

    if (const std::filesystem::path own = getConfigPath(config); !own.empty()) {
        args.emplace_back("-config");
        args.push_back(own.string());

        if (splitsConfig(executable(config))) {
            args.emplace_back("-extraconfig");
            args.push_back(extraConfigFile(own).string());
        }
    }

    if (!iwad.empty()) {
        args.emplace_back("-iwad");
        args.push_back(iwad);
    }

    if (profile.monsters > 0) {
        if (profile.monsters == 1) {
            args.emplace_back("-nomonsters");
        } else {
            if (profile.monsters % 2 == 0) {
                args.emplace_back("-fast");
            }

            if (profile.monsters >= 3) {
                args.emplace_back("-respawn");
            }
        }
    }

    if (profile.skill > 0) {
        args.emplace_back("-skill");
        args.push_back(std::to_string(profile.skill));
    }

    if (!profile.warp.empty()) {
        if (std::vector<std::string> const warp = warpArguments(iwad, profile.warp); !warp.empty()) {
            append(args, warp);
        } else {
            args.emplace_back("+map");
            args.push_back(profile.warp);
        }
    }

    const ClassifiedFiles files = classifyFiles(profile.files);

    if (!files.pwads.empty()) {
        args.emplace_back("-file");
        append(args, files.pwads);
    }

    // Both kinds of patch go on, and the one that appeared last in the list goes
    // on last, since that is the one the port applies over the other.
    char dehLast = files.dehLast;

    do {
        if (dehLast % 2 != 0) {
            for (const std::string &file : files.bexs) {
                args.emplace_back("-bex");
                args.push_back(file);
            }
        } else {
            for (const std::string &file : files.dehs) {
                args.emplace_back("-deh");
                args.push_back(file);
            }
        }

        dehLast += 3;
    } while (dehLast <= 4);

    for (const std::string &file : files.autoexecs) {
        args.emplace_back("+exec");
        args.push_back(file);
    }

    for (const std::string &file : files.lumps) {
        args.emplace_back("-playdemo");
        args.push_back(file);
    }

    const MultiplayerSettings &mp = profile.multiplayer;

    if (mp.gameType != 0) {
        if (!mp.dmflags.empty()) {
            args.emplace_back("+set");
            args.emplace_back("dmflags");
            args.push_back(mp.dmflags);
        }

        if (!mp.dmflags2.empty()) {
            args.emplace_back("+set");
            args.emplace_back("dmflags2");
            args.push_back(mp.dmflags2);
        }

        if (mp.gameType == 2) {
            args.emplace_back("-deathmatch");
        } else if (mp.gameType == 3) {
            args.emplace_back("-altdeath");
        }

        if (mp.players > 0) {
            args.emplace_back("-host");
            args.push_back(std::to_string(mp.players));

            if (!mp.port.empty()) {
                args.emplace_back("-port");
                args.push_back(mp.port);
            }
        } else if (mp.players == 0 && !mp.host.empty()) {
            args.emplace_back("-join");

            if (!mp.port.empty()) {
                // A port typed into the address itself is replaced by the one
                // in the field beside it rather than left on the end.
                static const std::regex trailing(":\\d*\\s*$");

                args.push_back(std::regex_replace(mp.host, trailing, "") + ":" + mp.port);
            } else {
                args.push_back(mp.host);
            }
        }

        if (!mp.fragLimit.empty()) {
            args.emplace_back("+set");
            args.emplace_back("fraglimit");
            args.push_back(mp.fragLimit);
        }

        if (!mp.timeLimit.empty()) {
            args.emplace_back("+set");
            args.emplace_back("timelimit");
            args.push_back(mp.timeLimit);
        }

        if (mp.extratic == 1) {
            args.emplace_back("-extratic");
        }

        if (mp.netmode != -1) {
            args.emplace_back("-netmode");
            args.push_back(std::to_string(mp.netmode));
        }

        if (mp.dup != 0) {
            args.emplace_back("-dup");
            args.push_back(std::to_string(mp.dup));
        }

        if (!mp.savegame.empty()) {
            args.emplace_back("-loadgame");
            args.push_back(mp.savegame);
        }
    }

    if (!config.general.alwaysAdd.empty()) {
        append(args, Text::parseArguments(config.general.alwaysAdd));
    }

    if (!profile.extra.empty()) {
        append(args, Text::parseArguments(profile.extra));
    }

    return args;
}

std::string commandLine(const Config &config) {
    std::vector<std::string> parts;
    const std::filesystem::path port = executable(config);

    if (!port.empty()) {
        parts.push_back(Text::quoteArgument(port.string()));
    }

    for (const std::string &argument : arguments(config)) {
        parts.push_back(Text::quoteArgument(argument));
    }

    return Text::join(parts, " ");
}

bool launch(const Config &config, std::string *error) {
    const std::filesystem::path port = executable(config);

    if (port.empty()) {
        if (error != nullptr) {
            *error = "No source port is selected.";
        }

        return false;
    }

    /*
    The port writes its config itself, but only if it has somewhere to write
    it. A directory that cannot be made is not worth failing the launch over.
    The port then says so in its own words, having been started either way.
    */
    if (const std::filesystem::path own = getConfigPath(config); !own.empty()) {
        std::error_code made;
        std::filesystem::create_directories(own.parent_path(), made);
    }

    std::error_code code;
    std::filesystem::path resolved = std::filesystem::absolute(port, code);

    if (code) {
        resolved = port;
    }

    return Process::startDetached(resolved, arguments(config), resolved.parent_path(), error);
}

std::vector<std::string> maps(const Config &config) {
    std::vector<std::string> names;
    const Profile &profile = config.activeProfile();

    if (const std::string iwad = iwadPath(config, profile); !iwad.empty()) {
        if (const std::unique_ptr<MapFile> file = MapFile::open(iwad)) {
            append(names, file->mapNames());
        }
    }

    for (const FileEntry &entry : profile.files) {
        // Disabled files aren't loaded, so their maps aren't reachable either.
        if (!entry.enabled) {
            continue;
        }

        if (const std::unique_ptr<MapFile> file = MapFile::open(entry.file)) {
            append(names, file->mapNames());
        }
    }

    std::ranges::sort(names, Text::naturalLess);
    names.erase(std::ranges::unique(names).begin(), names.end());

    return names;
}

}
