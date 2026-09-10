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
    captureOutput = false;
    levelstat = false;
    multiplayer = MultiplayerSettings();
    replay = ReplaySettings();
    save = SaveSettings();
}

Profile Profile::fromJson(yyjson_val *obj) {
    Profile profile = {};

    if (obj == nullptr) {
        return profile;
    }

    profile.id = Json::objGetString(obj, ProfileKey::ID);

    if (profile.id.empty()) {
        profile.id = newId();
    }

    profile.name = Json::objGetString(obj, ProfileKey::NAME);
    profile.iwad = Json::objGetString(obj, ProfileKey::IWAD);
    profile.port = Json::objGetString(obj, ProfileKey::PORT);

    yyjson_val *fileArr = Json::objGet(obj, ProfileKey::FILES);

    if (fileArr != nullptr && yyjson_is_arr(fileArr)) {
        size_t idx = 0;
        size_t max = 0;
        yyjson_val *item = nullptr;

        yyjson_arr_foreach(fileArr, idx, max, item) {
            // A bare string is accepted for hand edited configs.
            if (yyjson_is_str(item)) {
                profile.files.push_back(FileEntry{
                    .file = std::string(yyjson_get_str(item), yyjson_get_len(item)),
                    .enabled = true,
                });
            } else if (yyjson_is_obj(item)) {
                const std::string file = Json::objGetString(item, ProfileKey::FILE);

                if (!file.empty()) {
                    profile.files.push_back(FileEntry{
                        .file = file,
                        .enabled = Json::objGetBool(item, ProfileKey::ENABLED, true),
                    });
                }
            }
        }
    }

    profile.skill = Json::objGetInt(obj, ProfileKey::SKILL);
    profile.monsters = Json::objGetInt(obj, ProfileKey::MONSTERS);
    profile.warp = Json::objGetString(obj, ProfileKey::WARP);
    profile.extra = Json::objGetString(obj, ProfileKey::EXTRA);
    profile.dialogOpen = Json::objGetBool(obj, ProfileKey::DIALOG_OPEN);
    profile.replayOpen = Json::objGetBool(obj, ProfileKey::REPLAY_OPEN);
    profile.saveOpen = Json::objGetBool(obj, ProfileKey::SAVE_OPEN);
    profile.config = Json::objGetString(obj, ProfileKey::CONFIG);
    profile.sharedConfig = Json::objGetBool(obj, ProfileKey::SHARED_CONFIG);
    profile.customCommand = Json::objGetBool(obj, ProfileKey::CUSTOM_COMMAND);
    profile.command = Json::objGetString(obj, ProfileKey::COMMAND);
    profile.dosFullscreen = Json::objGetBool(obj, ProfileKey::DOS_FULLSCREEN, true);
    profile.captureOutput = Json::objGetBool(obj, ProfileKey::CAPTURE_OUTPUT, false);
    profile.levelstat = Json::objGetBool(obj, ProfileKey::LEVELSTAT, false);

    if (yyjson_val *mp = Json::objGet(obj, ProfileKey::MULTIPLAYER)) {
        MultiplayerSettings &m = profile.multiplayer;

        m.gameType = Json::objGetInt(mp, ProfileKey::GAME_TYPE);
        m.players = Json::objGetInt(mp, ProfileKey::PLAYERS);
        m.extratic = Json::objGetInt(mp, ProfileKey::EXTRATIC);
        m.netmode = Json::objGetInt(mp, ProfileKey::NETMODE, -1);
        m.dup = Json::objGetInt(mp, ProfileKey::DUP);
        m.host = Json::objGetString(mp, ProfileKey::HOST);
        m.port = Json::objGetString(mp, ProfileKey::PORT);
        m.fragLimit = Json::objGetString(mp, ProfileKey::FRAG_LIMIT);
        m.timeLimit = Json::objGetString(mp, ProfileKey::TIME_LIMIT);
        m.dmflags = Json::objGetString(mp, ProfileKey::DMFLAGS);
        m.dmflags2 = Json::objGetString(mp, ProfileKey::DMFLAGS2);
        m.savegame = Json::objGetString(mp, ProfileKey::SAVEGAME);
        m.listed = Json::objGetBool(mp, ProfileKey::LISTED, false);
    }

    if (yyjson_val *replay = Json::objGet(obj, ProfileKey::REPLAY)) {
        ReplaySettings &r = profile.replay;

        r.mode = Json::objGetInt(replay, ProfileKey::MODE);
        r.file = Json::objGetString(replay, ProfileKey::FILE);
        r.playback = Json::objGetInt(replay, ProfileKey::PLAYBACK);
        r.compatibility = Json::objGetInt(replay, ProfileKey::COMPATIBILITY, -1);
        r.longtics = Json::objGetBool(replay, ProfileKey::LONGTICS);
        r.soloNet = Json::objGetBool(replay, ProfileKey::SOLO_NET);
    }

    if (yyjson_val *save = Json::objGet(obj, ProfileKey::SAVE)) {
        profile.save.enabled = Json::objGetBool(save, ProfileKey::ENABLED);
        profile.save.file = Json::objGetString(save, ProfileKey::FILE);
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
    builder.addBool(obj, ProfileKey::CAPTURE_OUTPUT, captureOutput);
    builder.addBool(obj, ProfileKey::LEVELSTAT, levelstat);

    yyjson_mut_val *mp = builder.newObject();

    builder.addInt(mp, ProfileKey::GAME_TYPE, multiplayer.gameType);
    builder.addInt(mp, ProfileKey::PLAYERS, multiplayer.players);
    builder.addInt(mp, ProfileKey::EXTRATIC, multiplayer.extratic);
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

    builder.addInt(demo, ProfileKey::MODE, replay.mode);
    builder.addString(demo, ProfileKey::FILE, replay.file);
    builder.addInt(demo, ProfileKey::PLAYBACK, replay.playback);
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
