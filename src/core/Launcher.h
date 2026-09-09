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
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/Config.h"
#include "core/Process.h"
#include "core/Profile.h"

namespace Launcher {

// Which -complevel numbers a port takes. Not one list: what Woof reads is a
// part of what the PrBoom line does, and it rejects the rest.
enum class Complevels : std::uint8_t { none, woof, prboom };

// The numbers offered, in order. -1 is the port's own.
[[nodiscard]] std::vector<int> complevels(Complevels which);

// What a source port can be told about demos, which is not the same everywhere:
// the ZDoom family has no complevel, Helion has no -timedemo, and nothing that
// predates Boom has heard of -longtics or -solo-net.
struct DemoSupport {
    bool records{false};
    bool timed{false};
    bool fast{false};
    Complevels complevel{Complevels::none};
    bool longtics{false};
    bool soloNet{false};
};

// How -loadgame names a save: the Boom and vanilla lines load the slot it sits
// in, GZDoom a name inside its save folder, ZDoom 2.8 and Helion a path.
enum class SaveNames : std::uint8_t { none, slot, name, path };

struct SaveSupport {
    SaveNames names{SaveNames::none};

    // Whether it takes a save folder at all. A port without one keeps its
    // saves wherever it likes, and nothing here can list them.
    bool folder{false};
};

[[nodiscard]] std::filesystem::path executable(const Config &config);

[[nodiscard]] std::vector<std::string> arguments(const Config &config);

[[nodiscard]] std::filesystem::path getConfigPath(const Profile &profile);
[[nodiscard]] std::filesystem::path getConfigPath(const Config &config);
[[nodiscard]] std::filesystem::path getSavePath(const Config &config);

/*
Where the port is told to keep this profile's saves: the folder above, unless
the port has no switch for one. Empty is a profile whose saves are the port's
own business.
*/
[[nodiscard]] std::filesystem::path saveFolder(const Config &config);

// The saves in it, newest first. Names, not paths.
[[nodiscard]] std::vector<std::string> saves(const Config &config);

[[nodiscard]] std::filesystem::path saveFile(const Config &config);

// The slot a save loads as, taken off the end of its name. -1 is a name with
// no number in it, which a port that loads by slot cannot be handed.
[[nodiscard]] int saveSlot(const std::string &name);

[[nodiscard]] SaveSupport saveSupport(const Config &config);

// What stands between this profile and the save it names. Empty is nothing.
[[nodiscard]] std::string saveTrouble(const Config &config);

/*
Where a profile's demos are kept, whether or not the port shares a config: a
"replays" folder beside the profile's own settings. Empty is a profile with no
folder of its own to put one in.
*/
[[nodiscard]] std::filesystem::path getReplayPath(const Profile &profile);
[[nodiscard]] std::filesystem::path getReplayPath(const Config &config);

// The demos in it, newest first. Names, not paths.
[[nodiscard]] std::vector<std::string> replays(const Config &config);

// The file a replay names, which is a name in the folder above unless the
// profile spelled out a path of its own. Empty is nothing named.
[[nodiscard]] std::filesystem::path replayFile(const Config &config);

// What the profile's source port does about demos.
[[nodiscard]] DemoSupport demoSupport(const Config &config);

// What stands between this profile and the demo it is set to record or play
// back, said before the launch rather than by it. Empty is nothing in the way.
[[nodiscard]] std::string replayTrouble(const Config &config);

[[nodiscard]] std::string commandLine(const Config &config);

/*
A profile that writes its own command line, as the tokens it comes out as:
the program first and its arguments after it, with {source_port}, {game},
{addon_1} upwards, {profile}, {cfgdir}, {extracfg}, {savedir}, {savefile} and
{replaydir} filled in.
Empty is one that cannot be run.
*/
[[nodiscard]] std::vector<std::string> customCommand(const Config &config,
                                                     std::string *error = nullptr);

// What is wrong with the command the profile writes itself, or nothing at all.
[[nodiscard]] std::string commandTrouble(const Config &config);

/*
What this profile would have been launched with, written the way a custom
command is written: the port, the game and the add-ons put back as the words
that stand for them. It is what taking the command over starts from.
*/
[[nodiscard]] std::string commandTemplate(const Config &config);

// A port marked as a DOS program is not started itself: DOSBox is, with the
// directories the launch names mounted as drives and the port run off C:.
[[nodiscard]] bool isDosPort(const Config &config);

/*
The DOSBox a config launches with: the one it names, or failing that whatever
this machine already has. Empty is a machine with none and a config that has
not been pointed at one.
*/
[[nodiscard]] std::filesystem::path dosbox(const Config &config);

// The id is the hold on the game that comes back, for asking later whether it
// is still up; the stream is its output, and is only piped when asked for.
bool launch(const Config &config,
            Process::Id *id = nullptr,
            Process::Stream *output = nullptr,
            std::string *error = nullptr);

[[nodiscard]] std::vector<std::string> maps(const Config &config);

}
