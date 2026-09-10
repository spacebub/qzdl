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
    multiplayer = MultiplayerSettings();
    replay = ReplaySettings();
    save = SaveSettings();
}

Profile Profile::fromJson(yyjson_val *obj) {
    Profile profile = {};

    if (obj == nullptr) {
        return profile;
    }

    profile.id = Json::objGetString(obj, "id");

    if (profile.id.empty()) {
        profile.id = newId();
    }

    profile.name = Json::objGetString(obj, "name");
    profile.iwad = Json::objGetString(obj, "iwad");
    profile.port = Json::objGetString(obj, "port");

    yyjson_val *fileArr = Json::objGet(obj, "files");

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
                const std::string file = Json::objGetString(item, "file");

                if (!file.empty()) {
                    profile.files.push_back(FileEntry{
                        .file = file,
                        .enabled = Json::objGetBool(item, "enabled", true),
                    });
                }
            }
        }
    }

    profile.skill = Json::objGetInt(obj, "skill");
    profile.monsters = Json::objGetInt(obj, "monsters");
    profile.warp = Json::objGetString(obj, "warp");
    profile.extra = Json::objGetString(obj, "extra");
    profile.dialogOpen = Json::objGetBool(obj, "dialogOpen");
    profile.replayOpen = Json::objGetBool(obj, "replayOpen");
    profile.saveOpen = Json::objGetBool(obj, "saveOpen");
    profile.config = Json::objGetString(obj, "config");
    profile.sharedConfig = Json::objGetBool(obj, "sharedConfig");
    profile.customCommand = Json::objGetBool(obj, "customCommand");
    profile.command = Json::objGetString(obj, "command");
    profile.dosFullscreen = Json::objGetBool(obj, "dosFullscreen", true);
    profile.captureOutput = Json::objGetBool(obj, "captureOutput", false);

    if (yyjson_val *mp = Json::objGet(obj, "multiplayer")) {
        MultiplayerSettings &m = profile.multiplayer;

        m.gameType = Json::objGetInt(mp, "gameType");
        m.players = Json::objGetInt(mp, "players");
        m.extratic = Json::objGetInt(mp, "extratic");
        m.netmode = Json::objGetInt(mp, "netmode", -1);
        m.dup = Json::objGetInt(mp, "dup");
        m.host = Json::objGetString(mp, "host");
        m.port = Json::objGetString(mp, "port");
        m.fragLimit = Json::objGetString(mp, "fragLimit");
        m.timeLimit = Json::objGetString(mp, "timeLimit");
        m.dmflags = Json::objGetString(mp, "dmflags");
        m.dmflags2 = Json::objGetString(mp, "dmflags2");
        m.savegame = Json::objGetString(mp, "savegame");
        m.listed = Json::objGetBool(mp, "listed", false);
    }

    if (yyjson_val *replay = Json::objGet(obj, "replay")) {
        ReplaySettings &r = profile.replay;

        r.mode = Json::objGetInt(replay, "mode");
        r.file = Json::objGetString(replay, "file");
        r.playback = Json::objGetInt(replay, "playback");
        r.compatibility = Json::objGetInt(replay, "compatibility", -1);
        r.longtics = Json::objGetBool(replay, "longtics");
        r.soloNet = Json::objGetBool(replay, "soloNet");
    }

    if (yyjson_val *save = Json::objGet(obj, "save")) {
        profile.save.enabled = Json::objGetBool(save, "enabled");
        profile.save.file = Json::objGetString(save, "file");
    }

    return profile;
}

yyjson_mut_val *Profile::toJson(const Json::Builder &builder) const {
    yyjson_mut_val *obj = builder.newObject();

    builder.addString(obj, "id", id);
    builder.addString(obj, "name", name);
    builder.addString(obj, "iwad", iwad);
    builder.addString(obj, "port", port);

    yyjson_mut_val *fileArr = builder.newArray();

    for (const FileEntry &entry : files) {
        yyjson_mut_val *fileObj = builder.newObject();

        builder.addString(fileObj, "file", entry.file);
        builder.addBool(fileObj, "enabled", entry.enabled);
        Json::Builder::appendValue(fileArr, fileObj);
    }

    builder.addValue(obj, "files", fileArr);

    builder.addInt(obj, "skill", skill);
    builder.addInt(obj, "monsters", monsters);
    builder.addString(obj, "warp", warp);
    builder.addString(obj, "extra", extra);
    builder.addBool(obj, "dialogOpen", dialogOpen);
    builder.addBool(obj, "replayOpen", replayOpen);
    builder.addBool(obj, "saveOpen", saveOpen);
    builder.addString(obj, "config", config);
    builder.addBool(obj, "sharedConfig", sharedConfig);
    builder.addBool(obj, "customCommand", customCommand);
    builder.addString(obj, "command", command);
    builder.addBool(obj, "dosFullscreen", dosFullscreen);
    builder.addBool(obj, "captureOutput", captureOutput);

    yyjson_mut_val *mp = builder.newObject();

    builder.addInt(mp, "gameType", multiplayer.gameType);
    builder.addInt(mp, "players", multiplayer.players);
    builder.addInt(mp, "extratic", multiplayer.extratic);
    builder.addInt(mp, "netmode", multiplayer.netmode);
    builder.addInt(mp, "dup", multiplayer.dup);
    builder.addString(mp, "host", multiplayer.host);
    builder.addString(mp, "port", multiplayer.port);
    builder.addString(mp, "fragLimit", multiplayer.fragLimit);
    builder.addString(mp, "timeLimit", multiplayer.timeLimit);
    builder.addString(mp, "dmflags", multiplayer.dmflags);
    builder.addString(mp, "dmflags2", multiplayer.dmflags2);
    builder.addString(mp, "savegame", multiplayer.savegame);
    builder.addBool(mp, "listed", multiplayer.listed);
    builder.addValue(obj, "multiplayer", mp);

    yyjson_mut_val *demo = builder.newObject();

    builder.addInt(demo, "mode", replay.mode);
    builder.addString(demo, "file", replay.file);
    builder.addInt(demo, "playback", replay.playback);
    builder.addInt(demo, "compatibility", replay.compatibility);
    builder.addBool(demo, "longtics", replay.longtics);
    builder.addBool(demo, "soloNet", replay.soloNet);
    builder.addValue(obj, "replay", demo);

    yyjson_mut_val *saved = builder.newObject();

    builder.addBool(saved, "enabled", save.enabled);
    builder.addString(saved, "file", save.file);
    builder.addValue(obj, "save", saved);

    return obj;
}
