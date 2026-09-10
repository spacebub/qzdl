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
#include <cctype>
#include <ranges>

#include "core/launch/Arguments.h"
#include "core/launch/Dialect.h"
#include "core/launch/Netgame.h"
#include "core/launch/Storage.h"
#include "core/util/Text.h"
#include "core/wad/MapFile.h"

namespace Arguments {

namespace {

struct ClassifiedFiles {
    std::vector<std::string> pwads;
    std::vector<std::string> dehs;
    std::vector<std::string> bexs;
    std::vector<std::string> autoexecs;
    std::vector<std::string> lumps;

    // The kind that appeared last in the list is applied last.
    bool dehLast{true};
};

ClassifiedFiles classifyFiles(const std::vector<FileEntry> &files) {
    ClassifiedFiles out;

    for (const FileEntry &entry : files) {
        if (!entry.enabled) {
            continue;
        }

        if (Text::iendsWith(entry.file, ".bex")) {
            out.dehLast = false;
            out.bexs.push_back(entry.file);
        } else if (Text::iendsWith(entry.file, ".deh")) {
            out.dehLast = true;
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

bool digit(const char letter) {
    return letter >= '0' && letter <= '9';
}

bool letterIs(const char letter, const char wanted) {
    return std::tolower(static_cast<unsigned char>(letter)) == wanted;
}

std::vector<std::string> warpArguments(const std::string &iwad, const std::string &map) {
    if (iwad.empty()) {
        return {};
    }

    if (MapFile::maps(iwad).mapxx) {
        // MAPxx
        if (map.size() == 5 && letterIs(map[0], 'm') && letterIs(map[1], 'a')
            && letterIs(map[2], 'p') && digit(map[3]) && digit(map[4])) {
            return {"-warp", map.substr(3, 2)};
        }

        return {};
    }

    // ExMy
    if (map.size() == 4 && letterIs(map[0], 'e') && digit(map[1]) && letterIs(map[2], 'm')
        && map[3] >= '1' && map[3] <= '9') {
        return {"-warp", map.substr(1, 1), map.substr(3, 1)};
    }

    return {};
}

void append(std::vector<std::string> &into, const std::vector<std::string> &what) {
    into.insert(into.end(), what.begin(), what.end());
}

void addSettings(std::vector<std::string> &args, const Config &config,
                 const Dialect::Port &speaks) {
    if (const std::filesystem::path own = Storage::configFile(config); !own.empty()) {
        args.emplace_back("-config");
        args.push_back(own.string());

        if (speaks.extraConfig) {
            args.emplace_back("-extraconfig");
            args.push_back(Storage::extraConfigFile(own).string());
        }

        if (const std::filesystem::path saves = Storage::saveDirectory(config);
            !saves.empty() && !speaks.save.empty()) {
            args.emplace_back(speaks.save);
            args.push_back(saves.string());
        }
    }
}

void addGame(std::vector<std::string> &args, const Profile &profile,
             const Dialect::Port &speaks, const std::string &iwad) {
    if (!iwad.empty() && speaks.iwad) {
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

            if (profile.monsters >= 3 && speaks.respawn) {
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
        } else if (speaks.map) {
            args.emplace_back("+map");
            args.push_back(profile.warp);
        }
    }
}

void addFiles(std::vector<std::string> &args, const Profile &profile,
              const Dialect::Port &speaks, const std::filesystem::path &demo) {
    ClassifiedFiles files = classifyFiles(profile.files);

    // A port only plays the first demo it is handed, so the profile's own wins.
    if (!demo.empty()) {
        files.lumps.clear();
    }

    if (speaks.bex.empty()) {
        files.bexs.clear();
    }

    if (speaks.deh.empty()) {
        files.dehs.clear();
    }

    if (!speaks.exec) {
        files.autoexecs.clear();
    }

    if (!files.pwads.empty()) {
        args.emplace_back("-file");
        append(args, files.pwads);
    }

    // Each switch appears once with every file behind it; ports stop at the first -deh.
    const bool bexFirst = files.dehLast;
    const std::vector<std::string> &first = bexFirst ? files.bexs : files.dehs;
    const std::vector<std::string> &second = bexFirst ? files.dehs : files.bexs;

    if (!first.empty()) {
        args.emplace_back(bexFirst ? speaks.bex : speaks.deh);
        append(args, first);
    }

    if (!second.empty()) {
        if (first.empty() || speaks.deh != speaks.bex) {
            args.emplace_back(bexFirst ? speaks.deh : speaks.bex);
        }

        append(args, second);
    }

    for (const std::string &file : files.autoexecs) {
        args.emplace_back("+exec");
        args.push_back(file);
    }

    for (const std::string &file : files.lumps) {
        args.emplace_back("-playdemo");
        args.push_back(file);
    }
}

void addDemo(std::vector<std::string> &args, const Profile &profile,
             const Dialect::Port &speaks, const std::filesystem::path &demo) {
    const ReplaySettings &replay = profile.replay;

    if (!demo.empty()) {
        if (replay.mode == 1) {
            // These must precede -record; they decide the demo header.
            const std::vector<int> reads = Dialect::complevels(speaks.complevel);

            // Woof rejects a complevel outside its table.
            if (replay.compatibility >= 0
                && std::ranges::find(reads, replay.compatibility) != reads.end()) {
                args.emplace_back("-complevel");
                args.push_back(std::to_string(replay.compatibility));
            }

            if (speaks.longtics && replay.longtics) {
                args.emplace_back("-longtics");
            }

            if (speaks.soloNet && replay.soloNet) {
                args.emplace_back("-solo-net");
            }

            args.emplace_back("-record");

            // Without extension: the vanilla line appends .lmp regardless.
            args.push_back((demo.parent_path() / demo.stem()).string());
        } else {
            std::string_view how = "-playdemo";

            if (replay.playback == 1 && speaks.timedemo) {
                how = "-timedemo";
            } else if (replay.playback == 2 && speaks.fastdemo) {
                how = "-fastdemo";
            }

            args.emplace_back(how);
            args.push_back(demo.string());
        }
    }
}

std::filesystem::path addSave(std::vector<std::string> &args, const Config &config,
                              const Dialect::Port &speaks, const std::filesystem::path &demo) {
    const std::filesystem::path save = config.activeProfile().save.enabled && demo.empty()
        ? Storage::saveFile(config)
        : std::filesystem::path();

    if (!save.empty()) {
        std::string named;

        switch (speaks.loads) {
            case Dialect::SaveNames::slot:
                if (const int slot = Storage::saveSlot(save.filename().string()); slot >= 0) {
                    named = std::to_string(slot);
                }

                break;
            case Dialect::SaveNames::name:
                named = save.filename().string();

                break;
            case Dialect::SaveNames::path:
                named = save.string();

                break;
            case Dialect::SaveNames::none:
                break;
        }

        if (!named.empty()) {
            args.emplace_back("-loadgame");
            args.push_back(named);
        }
    }

    return save;
}

}

std::vector<std::string> of(const Config &config) {
    std::vector<std::string> args;
    const Profile &profile = config.activeProfile();
    const std::string iwad = config.activeIwadFile();
    const Dialect::Port speaks = Dialect::of(config);

    const std::filesystem::path demo = profile.replay.mode != 0 && speaks.demos
        ? Storage::replayFile(config)
        : std::filesystem::path();

    addSettings(args, config, speaks);
    addGame(args, profile, speaks, iwad);
    addFiles(args, profile, speaks, demo);
    addDemo(args, profile, speaks, demo);

    if (speaks.levelstat && profile.levelstat) {
        args.emplace_back("-levelstat");
    }

    const std::filesystem::path save = addSave(args, config, speaks, demo);

    Netgame::arguments(args, profile, speaks, save);

    if (!config.general.alwaysAdd.empty()) {
        append(args, Text::parseArguments(config.general.alwaysAdd));
    }

    if (!profile.extra.empty()) {
        append(args, Text::parseArguments(profile.extra));
    }

    return args;
}

std::vector<std::string> maps(const Config &config) {
    std::vector<std::string> names;
    const Profile &profile = config.activeProfile();

    if (const std::string iwad = config.activeIwadFile(); !iwad.empty()) {
        append(names, MapFile::maps(iwad).names);
    }

    for (const FileEntry &entry : profile.files) {
        if (!entry.enabled) {
            continue;
        }

        append(names, MapFile::maps(entry.file).names);
    }

    std::ranges::sort(names, Text::naturalLess);
    names.erase(std::ranges::unique(names).begin(), names.end());

    return names;
}

}
