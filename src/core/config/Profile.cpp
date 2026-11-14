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
#include <random>
#include <string_view>
#include <utility>

#include "core/config/Profile.h"
#include "core/config/Schema.h"

using namespace ttk;

namespace {

std::string uuid() {
    static std::random_device seed;
    static std::mt19937_64 engine(seed());
    std::uniform_int_distribution<uint64_t> draw;

    std::array<unsigned char, 16> bytes{};
    uint64_t const high = draw(engine);
    uint64_t const low = draw(engine);

    for (size_t index = 0; index < 8; ++index) {
        bytes[index] = static_cast<unsigned char>(high >> (index * 8));
        bytes[index + 8] = static_cast<unsigned char>(low >> (index * 8));
    }

    bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0fU) | 0x40U);
    bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3fU) | 0x80U);

    static constexpr char DIGITS[] = "0123456789abcdef";
    std::string out;
    out.reserve(36);

    for (size_t index = 0; index < bytes.size(); ++index) {
        if (index == 4 || index == 6 || index == 8 || index == 10) {
            out.push_back('-');
        }

        out.push_back(DIGITS[bytes[index] >> 4U]);
        out.push_back(DIGITS[bytes[index] & 0x0fU]);
    }

    return out;
}

}

std::string Profile::newId() {
    return uuid();
}

const std::string &Profile::flag(const std::string_view word) const {
    static const std::string none;

    for (const FlagValue &each : flags) {
        if (each.word == word) {
            return each.value;
        }
    }

    return none;
}

void Profile::setFlag(const std::string_view word, std::string value) {
    const auto found = std::ranges::find(flags, word, &FlagValue::word);

    if (value.empty()) {
        if (found != flags.end()) {
            flags.erase(found);
        }

        return;
    }

    if (found != flags.end()) {
        found->value = std::move(value);

        return;
    }

    flags.push_back(FlagValue{.word = std::string(word), .value = std::move(value)});
}

void Profile::clearSettings() {
    iwad.clear();
    port.clear();
    files.clear();
    skill = 0;
    monsters = 0;
    warp.clear();
    extra.clear();
    dialogOpen = false;
    generalOpen = false;
    gameplayOpen = false;
    replayOpen = false;
    saveOpen = false;
    sharedConfig = false;
    customCommand = false;
    command.clear();
    prefix.clear();
    environment.clear();
    dosFullscreen = true;
    dosExit = true;
    captureOutput = false;
    levelstat = false;
    pistolStart = false;
    cheats = false;
    complevel = -1;
    flags.clear();
    skipAutoload = false;
    music = true;
    soundEffects = true;
    display = Display::Port;
    screenshots = Screenshots::Port;
    screenshotFolder.clear();
    multiplayer = MultiplayerSettings();
    replay = ReplaySettings();
    save = SaveSettings();
}

namespace {

void readFiles(yyjson_val *arr, std::vector<FileEntry> &files) {
    files.reserve(yyjson_arr_size(arr));

    Json::each_item(arr, [&files](yyjson_val *item) {
        // A bare string is accepted for hand edited configs.
        if (yyjson_is_str(item)) {
            files.push_back(FileEntry{.file = Json::as_string(item), .enabled = true});

            return;
        }

        FileEntry entry;

        Json::each_field(item, [&entry](const std::string_view key, const yyjson_val *val) {
            if (key == ProfileKey::FILE) {
                entry.file = Json::as_string(val);
            } else if (key == ProfileKey::ENABLED) {
                entry.enabled = Json::as_bool(val, true);
            }
        });

        if (!entry.file.empty()) {
            files.push_back(std::move(entry));
        }
    });
}

void readEnvironment(yyjson_val *arr, std::vector<EnvVariable> &environment) {
    environment.reserve(yyjson_arr_size(arr));

    Json::each_item(arr, [&environment](yyjson_val *item) {
        EnvVariable variable;

        Json::each_field(item, [&variable](const std::string_view key, const yyjson_val *val) {
            if (key == ProfileKey::NAME) {
                variable.name = Json::as_string(val);
            } else if (key == ProfileKey::VALUE) {
                variable.value = Json::as_string(val);
            }
        });

        if (!variable.name.empty()) {
            environment.push_back(std::move(variable));
        }
    });
}

// Used only when the profile has no flags object or no complevel key.
struct Moved {
    std::string dmflags;
    std::string dmflags2;
    int complevel{-1};
};

// Only the known words, the ones toJson writes back.
void readFlags(yyjson_val *obj, Profile &profile) {
    Json::each_field(obj, [&profile](const std::string_view key, const yyjson_val *val) {
        if (std::ranges::find(FlagWord::ALL, key) != FlagWord::ALL.end()) {
            profile.setFlag(key, Json::as_string(val));
        }
    });
}

void readMultiplayer(yyjson_val *obj, MultiplayerSettings &m, Moved &moved) {
    Json::each_field(obj, [&m, &moved](const std::string_view key, const yyjson_val *val) {
        if (key == ProfileKey::GAME_TYPE) {
            m.gameType = gameTypeOf(Json::as_int(val));
        } else if (key == ProfileKey::PLAYERS) {
            m.players = Json::as_int(val);
        } else if (key == ProfileKey::EXTRATIC) {
            m.extratic = Json::as_int(val) != 0;
        } else if (key == ProfileKey::NETMODE) {
            m.netmode = Json::as_int(val, -1);
        } else if (key == ProfileKey::DUP) {
            m.dup = Json::as_int(val);
        } else if (key == ProfileKey::HOST) {
            m.host = Json::as_string(val);
        } else if (key == ProfileKey::PORT) {
            m.port = Json::as_string(val);
        } else if (key == ProfileKey::FRAG_LIMIT) {
            m.fragLimit = Json::as_string(val);
        } else if (key == ProfileKey::TIME_LIMIT) {
            m.timeLimit = Json::as_string(val);
        } else if (key == ProfileKey::DMFLAGS) {
            moved.dmflags = Json::as_string(val);
        } else if (key == ProfileKey::DMFLAGS2) {
            moved.dmflags2 = Json::as_string(val);
        } else if (key == ProfileKey::SAVEGAME) {
            m.savegame = Json::as_string(val);
        } else if (key == ProfileKey::LISTED) {
            m.listed = Json::as_bool(val);
        } else if (key == ProfileKey::PLAYER_NAME) {
            m.playerName = Json::as_string(val);
        } else if (key == ProfileKey::PLAYER_COLOR) {
            m.playerColor = Json::as_string(val);
        } else if (key == ProfileKey::TEAM) {
            m.team = Json::as_int(val, -1);
        }
    });
}

void readReplay(yyjson_val *obj, ReplaySettings &r, Moved &moved) {
    Json::each_field(obj, [&r, &moved](const std::string_view key, const yyjson_val *val) {
        if (key == ProfileKey::MODE) {
            r.mode = replayModeOf(Json::as_int(val));
        } else if (key == ProfileKey::FILE) {
            r.file = Json::as_string(val);
        } else if (key == ProfileKey::SOURCE) {
            r.source = Json::as_string(val);
        } else if (key == ProfileKey::PLAYBACK) {
            r.playback = playbackOf(Json::as_int(val));
        } else if (key == ProfileKey::COMPATIBILITY) {
            moved.complevel = Json::as_int(val, -1);
        } else if (key == ProfileKey::LONGTICS) {
            r.longtics = Json::as_bool(val);
        } else if (key == ProfileKey::SOLO_NET) {
            r.soloNet = Json::as_bool(val);
        }
    });
}

void readSave(yyjson_val *obj, SaveSettings &save) {
    Json::each_field(obj, [&save](const std::string_view key, const yyjson_val *val) {
        if (key == ProfileKey::ENABLED) {
            save.enabled = Json::as_bool(val);
        } else if (key == ProfileKey::FILE) {
            save.file = Json::as_string(val);
        }
    });
}

}

Profile Profile::fromJson(yyjson_val *obj) {
    Profile profile = {};
    Moved moved;
    bool flagsRead = false;
    bool complevelRead = false;

    Json::each_field(obj, [&profile, &moved, &flagsRead, &complevelRead](const std::string_view key, yyjson_val *val) {
        if (key == ProfileKey::ID) {
            profile.id = Json::as_string(val);
        } else if (key == ProfileKey::NAME) {
            profile.name = Json::as_string(val);
        } else if (key == ProfileKey::IWAD) {
            profile.iwad = Json::as_string(val);
        } else if (key == ProfileKey::PORT) {
            profile.port = Json::as_string(val);
        } else if (key == ProfileKey::FILES) {
            readFiles(val, profile.files);
        } else if (key == ProfileKey::SKILL) {
            profile.skill = Json::as_int(val);
        } else if (key == ProfileKey::MONSTERS) {
            profile.monsters = Json::as_int(val);
        } else if (key == ProfileKey::WARP) {
            profile.warp = Json::as_string(val);
        } else if (key == ProfileKey::EXTRA) {
            profile.extra = Json::as_string(val);
        } else if (key == ProfileKey::DIALOG_OPEN) {
            profile.dialogOpen = Json::as_bool(val);
        } else if (key == ProfileKey::REPLAY_OPEN) {
            profile.replayOpen = Json::as_bool(val);
        } else if (key == ProfileKey::SAVE_OPEN) {
            profile.saveOpen = Json::as_bool(val);
        } else if (key == ProfileKey::CONFIG) {
            profile.config = Json::as_string(val);
        } else if (key == ProfileKey::SHARED_CONFIG) {
            profile.sharedConfig = Json::as_bool(val);
        } else if (key == ProfileKey::CUSTOM_COMMAND) {
            profile.customCommand = Json::as_bool(val);
        } else if (key == ProfileKey::COMMAND) {
            profile.command = Json::as_string(val);
        } else if (key == ProfileKey::PREFIX) {
            profile.prefix = Json::as_string(val);
        } else if (key == ProfileKey::ENVIRONMENT) {
            readEnvironment(val, profile.environment);
        } else if (key == ProfileKey::DOS_FULLSCREEN) {
            profile.dosFullscreen = Json::as_bool(val, true);
        } else if (key == ProfileKey::DOS_EXIT) {
            profile.dosExit = Json::as_bool(val, true);
        } else if (key == ProfileKey::CAPTURE_OUTPUT) {
            profile.captureOutput = Json::as_bool(val);
        } else if (key == ProfileKey::LEVELSTAT) {
            profile.levelstat = Json::as_bool(val);
        } else if (key == ProfileKey::PISTOL_START) {
            profile.pistolStart = Json::as_bool(val);
        } else if (key == ProfileKey::CHEATS) {
            profile.cheats = Json::as_bool(val);
        } else if (key == ProfileKey::SKIP_AUTOLOAD) {
            profile.skipAutoload = Json::as_bool(val);
        } else if (key == ProfileKey::MUSIC) {
            profile.music = Json::as_bool(val, true);
        } else if (key == ProfileKey::SOUND_EFFECTS) {
            profile.soundEffects = Json::as_bool(val, true);
        } else if (key == ProfileKey::DISPLAY) {
            profile.display = displayOf(Json::as_int(val));
        } else if (key == ProfileKey::SCREENSHOTS) {
            profile.screenshots = screenshotsOf(Json::as_int(val));
        } else if (key == ProfileKey::SCREENSHOT_FOLDER) {
            profile.screenshotFolder = Json::as_string(val);
        } else if (key == ProfileKey::ZDL_FILE) {
            profile.zdlFile = Json::as_string(val);
        } else if (key == ProfileKey::GENERAL_OPEN) {
            profile.generalOpen = Json::as_bool(val);
        } else if (key == ProfileKey::GAMEPLAY_OPEN) {
            profile.gameplayOpen = Json::as_bool(val);
        } else if (key == ProfileKey::COMPLEVEL) {
            profile.complevel = Json::as_int(val, -1);
            complevelRead = true;
        } else if (key == ProfileKey::FLAGS) {
            readFlags(val, profile);
            flagsRead = true;
        } else if (key == ProfileKey::MULTIPLAYER) {
            readMultiplayer(val, profile.multiplayer, moved);
        } else if (key == ProfileKey::REPLAY) {
            readReplay(val, profile.replay, moved);
        } else if (key == ProfileKey::SAVE) {
            readSave(val, profile.save);
        }
    });

    if (!flagsRead) {
        profile.setFlag(FlagWord::DMFLAGS, std::move(moved.dmflags));
        profile.setFlag(FlagWord::DMFLAGS2, std::move(moved.dmflags2));
    }

    if (!complevelRead) {
        profile.complevel = moved.complevel;
    }

    if (profile.id.empty()) {
        profile.id = newId();
    }

    return profile;
}

yyjson_mut_val *Profile::toJson(const Json::Builder &builder) const {
    yyjson_mut_val *obj = builder.new_object();

    builder.add_string(obj, ProfileKey::ID, id);
    builder.add_string(obj, ProfileKey::NAME, name);
    builder.add_string(obj, ProfileKey::IWAD, iwad);
    builder.add_string(obj, ProfileKey::PORT, port);

    yyjson_mut_val *fileArr = builder.new_array();

    for (const FileEntry &entry : files) {
        yyjson_mut_val *fileObj = builder.new_object();

        builder.add_string(fileObj, ProfileKey::FILE, entry.file);
        builder.add_bool(fileObj, ProfileKey::ENABLED, entry.enabled);
        Json::Builder::append_value(fileArr, fileObj);
    }

    builder.add_value(obj, ProfileKey::FILES, fileArr);

    builder.add_int(obj, ProfileKey::SKILL, skill);
    builder.add_int(obj, ProfileKey::MONSTERS, monsters);
    builder.add_string(obj, ProfileKey::WARP, warp);
    builder.add_string(obj, ProfileKey::EXTRA, extra);
    builder.add_bool(obj, ProfileKey::DIALOG_OPEN, dialogOpen);
    builder.add_bool(obj, ProfileKey::GENERAL_OPEN, generalOpen);
    builder.add_bool(obj, ProfileKey::GAMEPLAY_OPEN, gameplayOpen);
    builder.add_bool(obj, ProfileKey::REPLAY_OPEN, replayOpen);
    builder.add_bool(obj, ProfileKey::SAVE_OPEN, saveOpen);
    builder.add_string(obj, ProfileKey::CONFIG, config);
    builder.add_bool(obj, ProfileKey::SHARED_CONFIG, sharedConfig);
    builder.add_bool(obj, ProfileKey::CUSTOM_COMMAND, customCommand);
    builder.add_string(obj, ProfileKey::COMMAND, command);
    builder.add_string(obj, ProfileKey::PREFIX, prefix);

    yyjson_mut_val *variables = builder.new_array();

    for (const EnvVariable &variable : environment) {
        yyjson_mut_val *pair = builder.new_object();

        builder.add_string(pair, ProfileKey::NAME, variable.name);
        builder.add_string(pair, ProfileKey::VALUE, variable.value);
        Json::Builder::append_value(variables, pair);
    }

    builder.add_value(obj, ProfileKey::ENVIRONMENT, variables);
    builder.add_bool(obj, ProfileKey::DOS_FULLSCREEN, dosFullscreen);
    builder.add_bool(obj, ProfileKey::DOS_EXIT, dosExit);
    builder.add_bool(obj, ProfileKey::CAPTURE_OUTPUT, captureOutput);
    builder.add_bool(obj, ProfileKey::LEVELSTAT, levelstat);
    builder.add_bool(obj, ProfileKey::PISTOL_START, pistolStart);
    builder.add_bool(obj, ProfileKey::CHEATS, cheats);
    builder.add_int(obj, ProfileKey::COMPLEVEL, complevel);

    yyjson_mut_val *words = builder.new_object();

    for (const char *word : FlagWord::ALL) {
        if (const std::string &value = flag(word); !value.empty()) {
            builder.add_string(words, word, value);
        }
    }

    builder.add_value(obj, ProfileKey::FLAGS, words);
    builder.add_bool(obj, ProfileKey::SKIP_AUTOLOAD, skipAutoload);
    builder.add_bool(obj, ProfileKey::MUSIC, music);
    builder.add_bool(obj, ProfileKey::SOUND_EFFECTS, soundEffects);
    builder.add_int(obj, ProfileKey::DISPLAY, static_cast<int>(display));
    builder.add_int(obj, ProfileKey::SCREENSHOTS, static_cast<int>(screenshots));
    builder.add_string(obj, ProfileKey::SCREENSHOT_FOLDER, screenshotFolder);
    builder.add_string(obj, ProfileKey::ZDL_FILE, zdlFile);

    yyjson_mut_val *mp = builder.new_object();

    builder.add_int(mp, ProfileKey::GAME_TYPE, static_cast<int>(multiplayer.gameType));
    builder.add_int(mp, ProfileKey::PLAYERS, multiplayer.players);
    builder.add_int(mp, ProfileKey::EXTRATIC, multiplayer.extratic ? 1 : 0);
    builder.add_int(mp, ProfileKey::NETMODE, multiplayer.netmode);
    builder.add_int(mp, ProfileKey::DUP, multiplayer.dup);
    builder.add_string(mp, ProfileKey::HOST, multiplayer.host);
    builder.add_string(mp, ProfileKey::PORT, multiplayer.port);
    builder.add_string(mp, ProfileKey::FRAG_LIMIT, multiplayer.fragLimit);
    builder.add_string(mp, ProfileKey::TIME_LIMIT, multiplayer.timeLimit);
    builder.add_string(mp, ProfileKey::SAVEGAME, multiplayer.savegame);
    builder.add_bool(mp, ProfileKey::LISTED, multiplayer.listed);
    builder.add_string(mp, ProfileKey::PLAYER_NAME, multiplayer.playerName);
    builder.add_string(mp, ProfileKey::PLAYER_COLOR, multiplayer.playerColor);
    builder.add_int(mp, ProfileKey::TEAM, multiplayer.team);
    builder.add_value(obj, ProfileKey::MULTIPLAYER, mp);

    yyjson_mut_val *demo = builder.new_object();

    builder.add_int(demo, ProfileKey::MODE, static_cast<int>(replay.mode));
    builder.add_string(demo, ProfileKey::FILE, replay.file);
    builder.add_string(demo, ProfileKey::SOURCE, replay.source);
    builder.add_int(demo, ProfileKey::PLAYBACK, static_cast<int>(replay.playback));
    builder.add_bool(demo, ProfileKey::LONGTICS, replay.longtics);
    builder.add_bool(demo, ProfileKey::SOLO_NET, replay.soloNet);
    builder.add_value(obj, ProfileKey::REPLAY, demo);

    yyjson_mut_val *saved = builder.new_object();

    builder.add_bool(saved, ProfileKey::ENABLED, save.enabled);
    builder.add_string(saved, ProfileKey::FILE, save.file);
    builder.add_value(obj, ProfileKey::SAVE, saved);

    return obj;
}
