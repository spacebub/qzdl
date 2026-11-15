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
#include <cstddef>
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

const std::string &Profile::rule(const Rules::Rule which) const {
    static const std::string on = IniValue::ON;
    static const std::string off;

    if (Rules::takesNumber(which)) {
        return ruleNumbers.get(which);
    }

    return ruleOn(which) ? on : off;
}

void Profile::setRule(const Rules::Rule which, std::string value) {
    if (Rules::takesNumber(which)) {
        ruleNumbers.set(which, std::move(value));
    } else {
        setRuleOn(which, value == IniValue::ON);
    }
}

bool Profile::ruleOn(const Rules::Rule which) const {
    if (Rules::takesNumber(which)) {
        return ruleNumbers.get(which) == IniValue::ON;
    }

    return (rulesOn & (1U << Rules::index(which))) != 0;
}

void Profile::setRuleOn(const Rules::Rule which, const bool on) {
    if (Rules::takesNumber(which)) {
        ruleNumbers.set(which, on ? IniValue::ON : "");

        return;
    }

    const std::uint32_t bit = 1U << Rules::index(which);

    rulesOn = on ? rulesOn | bit : rulesOn & ~bit;
}

void Profile::clearSettings() {
    iwad.clear();
    port.clear();
    files.clear();
    warp.clear();
    extra.clear();
    multiplayer = MultiplayerSettings();
    replay = ReplaySettings();
    save = SaveSettings();
    command.clear();
    prefix.clear();
    environment.clear();
    screenshotFolder.clear();
    flags.clear();
    ruleNumbers.clear();
    skill = 0;
    monsters = 0;
    complevel = -1;
    switches = DEFAULT_SWITCHES;
    rulesOn = 0;
    display = Display::Port;
    screenshots = Screenshots::Port;
}

namespace {

static_assert(Rules::RULES <= 32 && static_cast<std::size_t>(Switch::soundEffects) < 32, "one bit each in 32");

// What a profile from before the Gameplay section kept elsewhere, used only when the newer keys are missing.
struct Loaded {
    std::string dmflags;
    std::string dmflags2;
    int complevel{-1};
    bool flagsRead{false};
    bool complevelRead{false};
};

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

void readFlags(yyjson_val *obj, Profile &profile) {
    Json::each_field(obj, [&profile](const std::string_view key, const yyjson_val *val) {
        for (const Flags::Word word : Flags::ALL) {
            if (key == Flags::name(word)) {
                profile.setFlag(word, Json::as_string(val));
            }
        }
    });
}

yyjson_mut_val *writeFlags(const Json::Builder &out, const Profile &profile) {
    yyjson_mut_val *obj = out.new_object();

    for (const auto &[word, value] : profile.flags.entries()) {
        out.add_string(obj, Flags::name(word), value);
    }

    return obj;
}

void readRules(yyjson_val *obj, Profile &profile) {
    Json::each_field(obj, [&profile](const std::string_view key, const yyjson_val *val) {
        for (const Rules::Rule rule : Rules::ALL) {
            if (key == Rules::key(rule)) {
                profile.setRule(rule, Json::as_string(val));
            }
        }
    });
}

yyjson_mut_val *writeRules(const Json::Builder &out, const Profile &profile) {
    yyjson_mut_val *obj = out.new_object();

    for (const Rules::Rule rule : Rules::ALL) {
        if (const std::string &value = profile.rule(rule); !value.empty()) {
            out.add_string(obj, Rules::key(rule), value);
        }
    }

    return obj;
}

void readMultiplayer(yyjson_val *obj, MultiplayerSettings &m, Loaded &loaded) {
    Json::each_field(obj, [&m, &loaded](const std::string_view key, const yyjson_val *val) {
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
            loaded.dmflags = Json::as_string(val);
        } else if (key == ProfileKey::DMFLAGS2) {
            loaded.dmflags2 = Json::as_string(val);
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

void readReplay(yyjson_val *obj, ReplaySettings &r, Loaded &loaded) {
    Json::each_field(obj, [&r, &loaded](const std::string_view key, const yyjson_val *val) {
        if (key == ProfileKey::MODE) {
            r.mode = replayModeOf(Json::as_int(val));
        } else if (key == ProfileKey::FILE) {
            r.file = Json::as_string(val);
        } else if (key == ProfileKey::SOURCE) {
            r.source = Json::as_string(val);
        } else if (key == ProfileKey::PLAYBACK) {
            r.playback = playbackOf(Json::as_int(val));
        } else if (key == ProfileKey::COMPATIBILITY) {
            loaded.complevel = Json::as_int(val, -1);
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
    Loaded loaded;

    Json::each_field(obj, [&profile, &loaded](const std::string_view key, yyjson_val *val) {
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
            profile.set(Switch::dialogOpen, Json::as_bool(val));
        } else if (key == ProfileKey::REPLAY_OPEN) {
            profile.set(Switch::replayOpen, Json::as_bool(val));
        } else if (key == ProfileKey::SAVE_OPEN) {
            profile.set(Switch::saveOpen, Json::as_bool(val));
        } else if (key == ProfileKey::CONFIG) {
            profile.config = Json::as_string(val);
        } else if (key == ProfileKey::SHARED_CONFIG) {
            profile.set(Switch::sharedConfig, Json::as_bool(val));
        } else if (key == ProfileKey::CUSTOM_COMMAND) {
            profile.set(Switch::customCommand, Json::as_bool(val));
        } else if (key == ProfileKey::COMMAND) {
            profile.command = Json::as_string(val);
        } else if (key == ProfileKey::PREFIX) {
            profile.prefix = Json::as_string(val);
        } else if (key == ProfileKey::ENVIRONMENT) {
            readEnvironment(val, profile.environment);
        } else if (key == ProfileKey::DOS_FULLSCREEN) {
            profile.set(Switch::dosFullscreen, Json::as_bool(val, true));
        } else if (key == ProfileKey::DOS_EXIT) {
            profile.set(Switch::dosExit, Json::as_bool(val, true));
        } else if (key == ProfileKey::CAPTURE_OUTPUT) {
            profile.set(Switch::captureOutput, Json::as_bool(val));
        } else if (key == ProfileKey::LEVELSTAT) {
            profile.set(Switch::levelstat, Json::as_bool(val));
        } else if (key == ProfileKey::PISTOL_START) {
            profile.set(Switch::pistolStart, Json::as_bool(val));
        } else if (key == ProfileKey::CHEATS) {
            profile.set(Switch::cheats, Json::as_bool(val));
        } else if (key == ProfileKey::SKIP_AUTOLOAD) {
            profile.set(Switch::skipAutoload, Json::as_bool(val));
        } else if (key == ProfileKey::MUSIC) {
            profile.set(Switch::music, Json::as_bool(val, true));
        } else if (key == ProfileKey::SOUND_EFFECTS) {
            profile.set(Switch::soundEffects, Json::as_bool(val, true));
        } else if (key == ProfileKey::DISPLAY) {
            profile.display = displayOf(Json::as_int(val));
        } else if (key == ProfileKey::SCREENSHOTS) {
            profile.screenshots = screenshotsOf(Json::as_int(val));
        } else if (key == ProfileKey::SCREENSHOT_FOLDER) {
            profile.screenshotFolder = Json::as_string(val);
        } else if (key == ProfileKey::ZDL_FILE) {
            profile.zdlFile = Json::as_string(val);
        } else if (key == ProfileKey::GENERAL_OPEN) {
            profile.set(Switch::generalOpen, Json::as_bool(val));
        } else if (key == ProfileKey::GAMEPLAY_OPEN) {
            profile.set(Switch::gameplayOpen, Json::as_bool(val));
        } else if (key == ProfileKey::COMPLEVEL) {
            profile.complevel = Json::as_int(val, -1);
            loaded.complevelRead = true;
        } else if (key == ProfileKey::FLAGS) {
            readFlags(val, profile);
            loaded.flagsRead = true;
        } else if (key == ProfileKey::RULES) {
            readRules(val, profile);
        } else if (key == ProfileKey::MULTIPLAYER) {
            readMultiplayer(val, profile.multiplayer, loaded);
        } else if (key == ProfileKey::REPLAY) {
            readReplay(val, profile.replay, loaded);
        } else if (key == ProfileKey::SAVE) {
            readSave(val, profile.save);
        }
    });

    if (!loaded.flagsRead) {
        profile.setFlag(Flags::Word::dmflags, std::move(loaded.dmflags));
        profile.setFlag(Flags::Word::dmflags2, std::move(loaded.dmflags2));
    }

    if (!loaded.complevelRead) {
        profile.complevel = loaded.complevel;
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
    builder.add_bool(obj, ProfileKey::DIALOG_OPEN, on(Switch::dialogOpen));
    builder.add_bool(obj, ProfileKey::GENERAL_OPEN, on(Switch::generalOpen));
    builder.add_bool(obj, ProfileKey::GAMEPLAY_OPEN, on(Switch::gameplayOpen));
    builder.add_bool(obj, ProfileKey::REPLAY_OPEN, on(Switch::replayOpen));
    builder.add_bool(obj, ProfileKey::SAVE_OPEN, on(Switch::saveOpen));
    builder.add_string(obj, ProfileKey::CONFIG, config);
    builder.add_bool(obj, ProfileKey::SHARED_CONFIG, on(Switch::sharedConfig));
    builder.add_bool(obj, ProfileKey::CUSTOM_COMMAND, on(Switch::customCommand));
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
    builder.add_bool(obj, ProfileKey::DOS_FULLSCREEN, on(Switch::dosFullscreen));
    builder.add_bool(obj, ProfileKey::DOS_EXIT, on(Switch::dosExit));
    builder.add_bool(obj, ProfileKey::CAPTURE_OUTPUT, on(Switch::captureOutput));
    builder.add_bool(obj, ProfileKey::LEVELSTAT, on(Switch::levelstat));
    builder.add_bool(obj, ProfileKey::PISTOL_START, on(Switch::pistolStart));
    builder.add_bool(obj, ProfileKey::CHEATS, on(Switch::cheats));
    builder.add_int(obj, ProfileKey::COMPLEVEL, complevel);

    builder.add_value(obj, ProfileKey::FLAGS, writeFlags(builder, *this));
    builder.add_value(obj, ProfileKey::RULES, writeRules(builder, *this));
    builder.add_bool(obj, ProfileKey::SKIP_AUTOLOAD, on(Switch::skipAutoload));
    builder.add_bool(obj, ProfileKey::MUSIC, on(Switch::music));
    builder.add_bool(obj, ProfileKey::SOUND_EFFECTS, on(Switch::soundEffects));
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
