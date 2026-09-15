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
    Json::eachItem(arr, [&files](yyjson_val *item) {
        // A bare string is accepted for hand edited configs.
        if (yyjson_is_str(item)) {
            files.push_back(FileEntry{.file = Json::asString(item), .enabled = true});

            return;
        }

        FileEntry entry;

        Json::eachField(item, [&entry](const std::string_view key, const yyjson_val *val) {
            if (key == ProfileKey::FILE) {
                entry.file = Json::asString(val);
            } else if (key == ProfileKey::ENABLED) {
                entry.enabled = Json::asBool(val, true);
            }
        });

        if (!entry.file.empty()) {
            files.push_back(std::move(entry));
        }
    });
}

void readMultiplayer(yyjson_val *obj, MultiplayerSettings &m) {
    Json::eachField(obj, [&m](const std::string_view key, const yyjson_val *val) {
        if (key == ProfileKey::GAME_TYPE) {
            m.gameType = gameTypeOf(Json::asInt(val));
        } else if (key == ProfileKey::PLAYERS) {
            m.players = Json::asInt(val);
        } else if (key == ProfileKey::EXTRATIC) {
            m.extratic = Json::asInt(val) != 0;
        } else if (key == ProfileKey::NETMODE) {
            m.netmode = Json::asInt(val, -1);
        } else if (key == ProfileKey::DUP) {
            m.dup = Json::asInt(val);
        } else if (key == ProfileKey::HOST) {
            m.host = Json::asString(val);
        } else if (key == ProfileKey::PORT) {
            m.port = Json::asString(val);
        } else if (key == ProfileKey::FRAG_LIMIT) {
            m.fragLimit = Json::asString(val);
        } else if (key == ProfileKey::TIME_LIMIT) {
            m.timeLimit = Json::asString(val);
        } else if (key == ProfileKey::DMFLAGS) {
            m.dmflags = Json::asString(val);
        } else if (key == ProfileKey::DMFLAGS2) {
            m.dmflags2 = Json::asString(val);
        } else if (key == ProfileKey::SAVEGAME) {
            m.savegame = Json::asString(val);
        } else if (key == ProfileKey::LISTED) {
            m.listed = Json::asBool(val);
        }
    });
}

void readReplay(yyjson_val *obj, ReplaySettings &r) {
    Json::eachField(obj, [&r](const std::string_view key, const yyjson_val *val) {
        if (key == ProfileKey::MODE) {
            r.mode = replayModeOf(Json::asInt(val));
        } else if (key == ProfileKey::FILE) {
            r.file = Json::asString(val);
        } else if (key == ProfileKey::PLAYBACK) {
            r.playback = playbackOf(Json::asInt(val));
        } else if (key == ProfileKey::COMPATIBILITY) {
            r.compatibility = Json::asInt(val, -1);
        } else if (key == ProfileKey::LONGTICS) {
            r.longtics = Json::asBool(val);
        } else if (key == ProfileKey::SOLO_NET) {
            r.soloNet = Json::asBool(val);
        }
    });
}

void readSave(yyjson_val *obj, SaveSettings &save) {
    Json::eachField(obj, [&save](const std::string_view key, const yyjson_val *val) {
        if (key == ProfileKey::ENABLED) {
            save.enabled = Json::asBool(val);
        } else if (key == ProfileKey::FILE) {
            save.file = Json::asString(val);
        }
    });
}

}

Profile Profile::fromJson(yyjson_val *obj) {
    Profile profile = {};

    Json::eachField(obj, [&profile](const std::string_view key, yyjson_val *val) {
        if (key == ProfileKey::ID) {
            profile.id = Json::asString(val);
        } else if (key == ProfileKey::NAME) {
            profile.name = Json::asString(val);
        } else if (key == ProfileKey::IWAD) {
            profile.iwad = Json::asString(val);
        } else if (key == ProfileKey::PORT) {
            profile.port = Json::asString(val);
        } else if (key == ProfileKey::FILES) {
            readFiles(val, profile.files);
        } else if (key == ProfileKey::SKILL) {
            profile.skill = Json::asInt(val);
        } else if (key == ProfileKey::MONSTERS) {
            profile.monsters = Json::asInt(val);
        } else if (key == ProfileKey::WARP) {
            profile.warp = Json::asString(val);
        } else if (key == ProfileKey::EXTRA) {
            profile.extra = Json::asString(val);
        } else if (key == ProfileKey::DIALOG_OPEN) {
            profile.dialogOpen = Json::asBool(val);
        } else if (key == ProfileKey::REPLAY_OPEN) {
            profile.replayOpen = Json::asBool(val);
        } else if (key == ProfileKey::SAVE_OPEN) {
            profile.saveOpen = Json::asBool(val);
        } else if (key == ProfileKey::CONFIG) {
            profile.config = Json::asString(val);
        } else if (key == ProfileKey::SHARED_CONFIG) {
            profile.sharedConfig = Json::asBool(val);
        } else if (key == ProfileKey::CUSTOM_COMMAND) {
            profile.customCommand = Json::asBool(val);
        } else if (key == ProfileKey::COMMAND) {
            profile.command = Json::asString(val);
        } else if (key == ProfileKey::DOS_FULLSCREEN) {
            profile.dosFullscreen = Json::asBool(val, true);
        } else if (key == ProfileKey::DOS_EXIT) {
            profile.dosExit = Json::asBool(val, true);
        } else if (key == ProfileKey::CAPTURE_OUTPUT) {
            profile.captureOutput = Json::asBool(val);
        } else if (key == ProfileKey::LEVELSTAT) {
            profile.levelstat = Json::asBool(val);
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
    yyjson_mut_val *obj = builder.newObject();

    builder.addString(obj, ProfileKey::ID, id);
    builder.addString(obj, ProfileKey::NAME, name);
    builder.addString(obj, ProfileKey::IWAD, iwad);
    builder.addString(obj, ProfileKey::PORT, port);

    yyjson_mut_val *fileArr = builder.newArray();

    for (const FileEntry &entry : files) {
        yyjson_mut_val *fileObj = builder.newObject();

        builder.addString(fileObj, ProfileKey::FILE, entry.file);
        builder.addBool(fileObj, ProfileKey::ENABLED, entry.enabled);
        Json::Builder::appendValue(fileArr, fileObj);
    }

    builder.addValue(obj, ProfileKey::FILES, fileArr);

    builder.addInt(obj, ProfileKey::SKILL, skill);
    builder.addInt(obj, ProfileKey::MONSTERS, monsters);
    builder.addString(obj, ProfileKey::WARP, warp);
    builder.addString(obj, ProfileKey::EXTRA, extra);
    builder.addBool(obj, ProfileKey::DIALOG_OPEN, dialogOpen);
    builder.addBool(obj, ProfileKey::REPLAY_OPEN, replayOpen);
    builder.addBool(obj, ProfileKey::SAVE_OPEN, saveOpen);
    builder.addString(obj, ProfileKey::CONFIG, config);
    builder.addBool(obj, ProfileKey::SHARED_CONFIG, sharedConfig);
    builder.addBool(obj, ProfileKey::CUSTOM_COMMAND, customCommand);
    builder.addString(obj, ProfileKey::COMMAND, command);
    builder.addBool(obj, ProfileKey::DOS_FULLSCREEN, dosFullscreen);
    builder.addBool(obj, ProfileKey::DOS_EXIT, dosExit);
    builder.addBool(obj, ProfileKey::CAPTURE_OUTPUT, captureOutput);
    builder.addBool(obj, ProfileKey::LEVELSTAT, levelstat);

    yyjson_mut_val *mp = builder.newObject();

    builder.addInt(mp, ProfileKey::GAME_TYPE, static_cast<int>(multiplayer.gameType));
    builder.addInt(mp, ProfileKey::PLAYERS, multiplayer.players);
    builder.addInt(mp, ProfileKey::EXTRATIC, multiplayer.extratic ? 1 : 0);
    builder.addInt(mp, ProfileKey::NETMODE, multiplayer.netmode);
    builder.addInt(mp, ProfileKey::DUP, multiplayer.dup);
    builder.addString(mp, ProfileKey::HOST, multiplayer.host);
    builder.addString(mp, ProfileKey::PORT, multiplayer.port);
    builder.addString(mp, ProfileKey::FRAG_LIMIT, multiplayer.fragLimit);
    builder.addString(mp, ProfileKey::TIME_LIMIT, multiplayer.timeLimit);
    builder.addString(mp, ProfileKey::DMFLAGS, multiplayer.dmflags);
    builder.addString(mp, ProfileKey::DMFLAGS2, multiplayer.dmflags2);
    builder.addString(mp, ProfileKey::SAVEGAME, multiplayer.savegame);
    builder.addBool(mp, ProfileKey::LISTED, multiplayer.listed);
    builder.addValue(obj, ProfileKey::MULTIPLAYER, mp);

    yyjson_mut_val *demo = builder.newObject();

    builder.addInt(demo, ProfileKey::MODE, static_cast<int>(replay.mode));
    builder.addString(demo, ProfileKey::FILE, replay.file);
    builder.addInt(demo, ProfileKey::PLAYBACK, static_cast<int>(replay.playback));
    builder.addInt(demo, ProfileKey::COMPATIBILITY, replay.compatibility);
    builder.addBool(demo, ProfileKey::LONGTICS, replay.longtics);
    builder.addBool(demo, ProfileKey::SOLO_NET, replay.soloNet);
    builder.addValue(obj, ProfileKey::REPLAY, demo);

    yyjson_mut_val *saved = builder.newObject();

    builder.addBool(saved, ProfileKey::ENABLED, save.enabled);
    builder.addString(saved, ProfileKey::FILE, save.file);
    builder.addValue(obj, ProfileKey::SAVE, saved);

    return obj;
}
