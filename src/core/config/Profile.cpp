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

#include "core/config/NonDefault.h"
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

const std::string &Profile::stored(const Options::Option &option) const {
    static const std::string set = IniValue::ON;
    static const std::string unset;

    if (option.kind == Options::Kind::number) {
        return number(option.feature);
    }

    return optionOn(option.feature) ? set : unset;
}

void Profile::store(const Options::Option &option, std::string text) {
    if (option.kind == Options::Kind::number) {
        setNumber(option.feature, std::move(text));
    } else {
        setOption(option.feature, text == IniValue::ON);
    }
}

void Profile::clearSettings() {
    iwad.clear();
    engine.clear();
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
    options = {};
    numbers.clear();
    skill = 0;
    monsters = 0;
    complevel = -1;
    switches = DEFAULT_SWITCHES;
    display = Display::Port;
    screenshots = Screenshots::Port;
}

namespace {

static_assert(static_cast<std::size_t>(Switch::captureOutput) < 32, "one bit each in 32");

// The option keys as views, so a compare starts with the length.
constexpr auto OPTION_KEYS = [] {
    std::array<std::string_view, Options::ALL.size()> keys{};

    std::ranges::transform(Options::ALL, keys.begin(), &Options::Option::key);

    return keys;
}();

// The option a file keeps under the key in this place. Null for none.
const Options::Option *optionAt(const Options::Place place, const std::string_view key) {
    for (std::size_t at = 0; at < OPTION_KEYS.size(); ++at) {
        if (Options::ALL[at].place == place && key == OPTION_KEYS[at]) {
            return &Options::ALL[at];
        }
    }

    return nullptr;
}

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
                entry.enabled = Json::as_bool(val, entry.enabled);
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
        if (const Options::Option *option = optionAt(Options::Place::rules, key)) {
            profile.store(*option, Json::as_string(val));
        }
    });
}

yyjson_mut_val *writeRules(const Json::Builder &out, const Profile &profile) {
    yyjson_mut_val *obj = out.new_object();

    for (const Options::Option &option : Options::ALL) {
        if (option.place == Options::Place::rules && !profile.stored(option).empty()) {
            out.add_string(obj, option.key, profile.stored(option));
        }
    }

    return obj;
}

void readMultiplayer(yyjson_val *obj, MultiplayerSettings &m) {
    Json::each_field(obj, [&m](const std::string_view key, const yyjson_val *val) {
        if (key == ProfileKey::GAME_TYPE) {
            m.gameType = gameTypeOf(Json::as_int(val));
        } else if (key == ProfileKey::PLAYERS) {
            m.players = Json::as_int(val);
        } else if (key == ProfileKey::EXTRATIC) {
            m.extratic = Json::as_int(val) != 0;
        } else if (key == ProfileKey::NETMODE) {
            m.netmode = Json::as_int(val, m.netmode);
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
        } else if (key == ProfileKey::SAVEGAME) {
            m.savegame = Json::as_string(val);
        } else if (key == ProfileKey::LISTED) {
            m.listed = Json::as_bool(val);
        } else if (key == ProfileKey::PLAYER_NAME) {
            m.playerName = Json::as_string(val);
        } else if (key == ProfileKey::PLAYER_COLOR) {
            m.playerColor = Json::as_string(val);
        } else if (key == ProfileKey::TEAM) {
            m.team = Json::as_int(val, m.team);
        }
    });
}

void readReplay(yyjson_val *obj, ReplaySettings &r) {
    Json::each_field(obj, [&r](const std::string_view key, const yyjson_val *val) {
        if (key == ProfileKey::MODE) {
            r.mode = replayModeOf(Json::as_int(val));
        } else if (key == ProfileKey::FILE) {
            r.file = Json::as_string(val);
        } else if (key == ProfileKey::SOURCE) {
            r.source = Json::as_string(val);
        } else if (key == ProfileKey::PLAYBACK) {
            r.playback = playbackOf(Json::as_int(val));
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

    Json::each_field(obj, [&profile](const std::string_view key, yyjson_val *val) {
        if (key == ProfileKey::ID) {
            profile.id = Json::as_string(val);
        } else if (key == ProfileKey::NAME) {
            profile.name = Json::as_string(val);
        } else if (key == ProfileKey::IWAD) {
            profile.iwad = Json::as_string(val);
        } else if (key == ProfileKey::PORT) {
            profile.engine = Json::as_string(val);
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
            profile.set(Switch::dosFullscreen, Json::as_bool(val, profile.on(Switch::dosFullscreen)));
        } else if (key == ProfileKey::DOS_EXIT) {
            profile.set(Switch::dosExit, Json::as_bool(val, profile.on(Switch::dosExit)));
        } else if (key == ProfileKey::CAPTURE_OUTPUT) {
            profile.set(Switch::captureOutput, Json::as_bool(val));
        } else if (key == ProfileKey::DISPLAY) {
            profile.display = displayOf(Json::as_int(val));
        } else if (key == ProfileKey::SCREENSHOTS) {
            profile.screenshots = screenshotsOf(Json::as_int(val));
        } else if (key == ProfileKey::SCREENSHOT_FOLDER) {
            profile.screenshotFolder = Json::as_string(val);
        } else if (key == ProfileKey::ZDL_FILE) {
            profile.zdlFile = Json::as_string(val);
        } else if (key == ProfileKey::SHORTCUT) {
            profile.shortcut = Json::as_string(val);
        } else if (key == ProfileKey::GENERAL_OPEN) {
            profile.set(Switch::generalOpen, Json::as_bool(val));
        } else if (key == ProfileKey::GAMEPLAY_OPEN) {
            profile.set(Switch::gameplayOpen, Json::as_bool(val));
        } else if (key == ProfileKey::COMPLEVEL) {
            profile.complevel = Json::as_int(val, profile.complevel);
        } else if (key == ProfileKey::FLAGS) {
            readFlags(val, profile);
        } else if (key == ProfileKey::RULES) {
            readRules(val, profile);
        } else if (key == ProfileKey::MULTIPLAYER) {
            readMultiplayer(val, profile.multiplayer);
        } else if (key == ProfileKey::REPLAY) {
            readReplay(val, profile.replay);
        } else if (key == ProfileKey::SAVE) {
            readSave(val, profile.save);
        } else if (const Options::Option *option = optionAt(Options::Place::profile, key)) {
            profile.setChecked(*option, Json::as_bool(val, profile.checked(*option)));
        }
    });

    if (profile.id.empty()) {
        profile.id = newId();
    }

    return profile;
}

yyjson_mut_val *Profile::toJson(const Json::Builder &builder) const {
    static const Profile fresh;
    const NonDefault put(builder);
    const auto putSwitch = [&](yyjson_mut_val *obj, const char *key, const Switch which) {
        put.field(obj, key, on(which), fresh.on(which));
    };
    yyjson_mut_val *obj = builder.new_object();

    put.field(obj, ProfileKey::ID, id, fresh.id);
    put.field(obj, ProfileKey::NAME, name, fresh.name);
    put.field(obj, ProfileKey::IWAD, iwad, fresh.iwad);
    put.field(obj, ProfileKey::PORT, engine, fresh.engine);

    yyjson_mut_val *fileArr = builder.new_array();

    for (const FileEntry &entry : files) {
        yyjson_mut_val *fileObj = builder.new_object();

        put.field(fileObj, ProfileKey::FILE, entry.file, FileEntry{}.file);
        put.field(fileObj, ProfileKey::ENABLED, entry.enabled, FileEntry{}.enabled);
        Json::Builder::append_value(fileArr, fileObj);
    }

    put.filled(obj, ProfileKey::FILES, fileArr);

    put.field(obj, ProfileKey::SKILL, skill, fresh.skill);
    put.field(obj, ProfileKey::MONSTERS, monsters, fresh.monsters);
    put.field(obj, ProfileKey::WARP, warp, fresh.warp);
    put.field(obj, ProfileKey::EXTRA, extra, fresh.extra);
    putSwitch(obj, ProfileKey::DIALOG_OPEN, Switch::dialogOpen);
    putSwitch(obj, ProfileKey::GENERAL_OPEN, Switch::generalOpen);
    putSwitch(obj, ProfileKey::GAMEPLAY_OPEN, Switch::gameplayOpen);
    putSwitch(obj, ProfileKey::REPLAY_OPEN, Switch::replayOpen);
    putSwitch(obj, ProfileKey::SAVE_OPEN, Switch::saveOpen);
    put.field(obj, ProfileKey::CONFIG, config, fresh.config);
    putSwitch(obj, ProfileKey::SHARED_CONFIG, Switch::sharedConfig);
    putSwitch(obj, ProfileKey::CUSTOM_COMMAND, Switch::customCommand);
    put.field(obj, ProfileKey::COMMAND, command, fresh.command);
    put.field(obj, ProfileKey::PREFIX, prefix, fresh.prefix);

    yyjson_mut_val *variables = builder.new_array();

    for (const EnvVariable &variable : environment) {
        yyjson_mut_val *pair = builder.new_object();

        put.field(pair, ProfileKey::NAME, variable.name, EnvVariable{}.name);
        put.field(pair, ProfileKey::VALUE, variable.value, EnvVariable{}.value);
        Json::Builder::append_value(variables, pair);
    }

    put.filled(obj, ProfileKey::ENVIRONMENT, variables);
    putSwitch(obj, ProfileKey::DOS_FULLSCREEN, Switch::dosFullscreen);
    putSwitch(obj, ProfileKey::DOS_EXIT, Switch::dosExit);
    putSwitch(obj, ProfileKey::CAPTURE_OUTPUT, Switch::captureOutput);

    for (const Options::Option &option : Options::ALL) {
        if (option.place == Options::Place::profile) {
            put.field(obj, option.key, checked(option), fresh.checked(option));
        }
    }

    put.field(obj, ProfileKey::COMPLEVEL, complevel, fresh.complevel);

    put.filled(obj, ProfileKey::FLAGS, writeFlags(builder, *this));
    put.filled(obj, ProfileKey::RULES, writeRules(builder, *this));
    put.field(obj, ProfileKey::DISPLAY, static_cast<int>(display), static_cast<int>(fresh.display));
    put.field(obj, ProfileKey::SCREENSHOTS, static_cast<int>(screenshots), static_cast<int>(fresh.screenshots));
    put.field(obj, ProfileKey::SCREENSHOT_FOLDER, screenshotFolder, fresh.screenshotFolder);
    put.field(obj, ProfileKey::ZDL_FILE, zdlFile, fresh.zdlFile);
    put.field(obj, ProfileKey::SHORTCUT, shortcut, fresh.shortcut);

    yyjson_mut_val *mp = builder.new_object();

    put.field(mp,
              ProfileKey::GAME_TYPE,
              static_cast<int>(multiplayer.gameType),
              static_cast<int>(fresh.multiplayer.gameType));
    put.field(mp, ProfileKey::PLAYERS, multiplayer.players, fresh.multiplayer.players);
    put.field(mp, ProfileKey::EXTRATIC, multiplayer.extratic ? 1 : 0, fresh.multiplayer.extratic ? 1 : 0);
    put.field(mp, ProfileKey::NETMODE, multiplayer.netmode, fresh.multiplayer.netmode);
    put.field(mp, ProfileKey::DUP, multiplayer.dup, fresh.multiplayer.dup);
    put.field(mp, ProfileKey::HOST, multiplayer.host, fresh.multiplayer.host);
    put.field(mp, ProfileKey::PORT, multiplayer.port, fresh.multiplayer.port);
    put.field(mp, ProfileKey::FRAG_LIMIT, multiplayer.fragLimit, fresh.multiplayer.fragLimit);
    put.field(mp, ProfileKey::TIME_LIMIT, multiplayer.timeLimit, fresh.multiplayer.timeLimit);
    put.field(mp, ProfileKey::SAVEGAME, multiplayer.savegame, fresh.multiplayer.savegame);
    put.field(mp, ProfileKey::LISTED, multiplayer.listed, fresh.multiplayer.listed);
    put.field(mp, ProfileKey::PLAYER_NAME, multiplayer.playerName, fresh.multiplayer.playerName);
    put.field(mp, ProfileKey::PLAYER_COLOR, multiplayer.playerColor, fresh.multiplayer.playerColor);
    put.field(mp, ProfileKey::TEAM, multiplayer.team, fresh.multiplayer.team);
    put.filled(obj, ProfileKey::MULTIPLAYER, mp);

    yyjson_mut_val *demo = builder.new_object();

    put.field(demo, ProfileKey::MODE, static_cast<int>(replay.mode), static_cast<int>(fresh.replay.mode));
    put.field(demo, ProfileKey::FILE, replay.file, fresh.replay.file);
    put.field(demo, ProfileKey::SOURCE, replay.source, fresh.replay.source);
    put.field(demo, ProfileKey::PLAYBACK, static_cast<int>(replay.playback), static_cast<int>(fresh.replay.playback));
    put.field(demo, ProfileKey::LONGTICS, replay.longtics, fresh.replay.longtics);
    put.field(demo, ProfileKey::SOLO_NET, replay.soloNet, fresh.replay.soloNet);
    put.filled(obj, ProfileKey::REPLAY, demo);

    yyjson_mut_val *saved = builder.new_object();

    put.field(saved, ProfileKey::ENABLED, save.enabled, fresh.save.enabled);
    put.field(saved, ProfileKey::FILE, save.file, fresh.save.file);
    put.filled(obj, ProfileKey::SAVE, saved);

    return obj;
}
