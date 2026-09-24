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

    bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0f) | 0x40);
    bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3f) | 0x80);

    static constexpr char DIGITS[] = "0123456789abcdef";
    std::string out;
    out.reserve(36);

    for (size_t index = 0; index < bytes.size(); ++index) {
        if (index == 4 || index == 6 || index == 8 || index == 10) {
            out.push_back('-');
        }

        out.push_back(DIGITS[bytes[index] >> 4]);
        out.push_back(DIGITS[bytes[index] & 0x0f]);
    }

    return out;
}

}

std::string Profile::newId() {
    return uuid();
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
    replayOpen = false;
    saveOpen = false;
    sharedConfig = false;
    customCommand = false;
    command.clear();
    dosFullscreen = true;
    dosExit = true;
    captureOutput = false;
    levelstat = false;
    multiplayer = MultiplayerSettings();
    replay = ReplaySettings();
    save = SaveSettings();
}

namespace {

void readFiles(yyjson_val *arr, std::vector<FileEntry> &files) {
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

void readMultiplayer(yyjson_val *obj, MultiplayerSettings &m) {
    Json::each_field(obj, [&m](const std::string_view key, const yyjson_val *val) {
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
            m.dmflags = Json::as_string(val);
        } else if (key == ProfileKey::DMFLAGS2) {
            m.dmflags2 = Json::as_string(val);
        } else if (key == ProfileKey::SAVEGAME) {
            m.savegame = Json::as_string(val);
        } else if (key == ProfileKey::LISTED) {
            m.listed = Json::as_bool(val);
        }
    });
}

void readReplay(yyjson_val *obj, ReplaySettings &r) {
    Json::each_field(obj, [&r](const std::string_view key, const yyjson_val *val) {
        if (key == ProfileKey::MODE) {
            r.mode = replayModeOf(Json::as_int(val));
        } else if (key == ProfileKey::FILE) {
            r.file = Json::as_string(val);
        } else if (key == ProfileKey::PLAYBACK) {
            r.playback = playbackOf(Json::as_int(val));
        } else if (key == ProfileKey::COMPATIBILITY) {
            r.compatibility = Json::as_int(val, -1);
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
        } else if (key == ProfileKey::DOS_FULLSCREEN) {
            profile.dosFullscreen = Json::as_bool(val, true);
        } else if (key == ProfileKey::DOS_EXIT) {
            profile.dosExit = Json::as_bool(val, true);
        } else if (key == ProfileKey::CAPTURE_OUTPUT) {
            profile.captureOutput = Json::as_bool(val);
        } else if (key == ProfileKey::LEVELSTAT) {
            profile.levelstat = Json::as_bool(val);
        } else if (key == ProfileKey::MULTIPLAYER) {
            readMultiplayer(val, profile.multiplayer);
        } else if (key == ProfileKey::REPLAY) {
            readReplay(val, profile.replay);
        } else if (key == ProfileKey::SAVE) {
            readSave(val, profile.save);
        }
    });

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
    builder.add_bool(obj, ProfileKey::REPLAY_OPEN, replayOpen);
    builder.add_bool(obj, ProfileKey::SAVE_OPEN, saveOpen);
    builder.add_string(obj, ProfileKey::CONFIG, config);
    builder.add_bool(obj, ProfileKey::SHARED_CONFIG, sharedConfig);
    builder.add_bool(obj, ProfileKey::CUSTOM_COMMAND, customCommand);
    builder.add_string(obj, ProfileKey::COMMAND, command);
    builder.add_bool(obj, ProfileKey::DOS_FULLSCREEN, dosFullscreen);
    builder.add_bool(obj, ProfileKey::DOS_EXIT, dosExit);
    builder.add_bool(obj, ProfileKey::CAPTURE_OUTPUT, captureOutput);
    builder.add_bool(obj, ProfileKey::LEVELSTAT, levelstat);

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
    builder.add_string(mp, ProfileKey::DMFLAGS, multiplayer.dmflags);
    builder.add_string(mp, ProfileKey::DMFLAGS2, multiplayer.dmflags2);
    builder.add_string(mp, ProfileKey::SAVEGAME, multiplayer.savegame);
    builder.add_bool(mp, ProfileKey::LISTED, multiplayer.listed);
    builder.add_value(obj, ProfileKey::MULTIPLAYER, mp);

    yyjson_mut_val *demo = builder.new_object();

    builder.add_int(demo, ProfileKey::MODE, static_cast<int>(replay.mode));
    builder.add_string(demo, ProfileKey::FILE, replay.file);
    builder.add_int(demo, ProfileKey::PLAYBACK, static_cast<int>(replay.playback));
    builder.add_int(demo, ProfileKey::COMPATIBILITY, replay.compatibility);
    builder.add_bool(demo, ProfileKey::LONGTICS, replay.longtics);
    builder.add_bool(demo, ProfileKey::SOLO_NET, replay.soloNet);
    builder.add_value(obj, ProfileKey::REPLAY, demo);

    yyjson_mut_val *saved = builder.new_object();

    builder.add_bool(saved, ProfileKey::ENABLED, save.enabled);
    builder.add_string(saved, ProfileKey::FILE, save.file);
    builder.add_value(obj, ProfileKey::SAVE, saved);

    return obj;
}
