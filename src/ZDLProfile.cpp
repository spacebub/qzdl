/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
 *
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QUuid>

#include "ZDLProfile.h"

QString ZDLProfile::newId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void ZDLProfile::clearSettings() {
    iwad.clear();
    port.clear();
    files.clear();
    skill = 0;
    monsters = 0;
    warp.clear();
    extra.clear();
    dialogOpen = false;
    multiplayer = ZDLMultiplayerSettings();
}

ZDLProfile ZDLProfile::fromJson(yyjson_val *obj) {
    ZDLProfile profile;
    if (!obj) {
        return profile;
    }

    profile.id = ZDLJson::objGetString(obj, "id");
    if (profile.id.isEmpty()) {
        profile.id = newId();
    }
    profile.name = ZDLJson::objGetString(obj, "name");
    profile.iwad = ZDLJson::objGetString(obj, "iwad");
    profile.port = ZDLJson::objGetString(obj, "port");
    yyjson_val *fileArr = ZDLJson::objGet(obj, "files");
    if (fileArr && yyjson_is_arr(fileArr)) {
        size_t idx, max;
        yyjson_val *item;
        yyjson_arr_foreach(fileArr, idx, max, item) {
            // Objects carry the enabled flag; a bare string is accepted so a
            // hand edited config can just list paths.
            if (yyjson_is_str(item)) {
                profile.files.append(ZDLFileEntry{QString::fromUtf8(yyjson_get_str(item)), true});
            } else if (yyjson_is_obj(item)) {
                QString file = ZDLJson::objGetString(item, "file");
                if (!file.isEmpty()) {
                    profile.files.append(ZDLFileEntry{file, ZDLJson::objGetBool(item, "enabled", true)});
                }
            }
        }
    }
    profile.skill = ZDLJson::objGetInt(obj, "skill");
    profile.monsters = ZDLJson::objGetInt(obj, "monsters");
    profile.warp = ZDLJson::objGetString(obj, "warp");
    profile.extra = ZDLJson::objGetString(obj, "extra");
    profile.dialogOpen = ZDLJson::objGetBool(obj, "dialogOpen");

    yyjson_val *mp = ZDLJson::objGet(obj, "multiplayer");
    if (mp) {
        ZDLMultiplayerSettings &m = profile.multiplayer;
        m.gameType = ZDLJson::objGetInt(mp, "gameType");
        m.players = ZDLJson::objGetInt(mp, "players");
        m.extratic = ZDLJson::objGetInt(mp, "extratic");
        m.netmode = ZDLJson::objGetInt(mp, "netmode", -1);
        m.dup = ZDLJson::objGetInt(mp, "dup");
        m.host = ZDLJson::objGetString(mp, "host");
        m.port = ZDLJson::objGetString(mp, "port");
        m.fragLimit = ZDLJson::objGetString(mp, "fragLimit");
        m.timeLimit = ZDLJson::objGetString(mp, "timeLimit");
        m.dmflags = ZDLJson::objGetString(mp, "dmflags");
        m.dmflags2 = ZDLJson::objGetString(mp, "dmflags2");
        m.savegame = ZDLJson::objGetString(mp, "savegame");
    }

    return profile;
}

yyjson_mut_val *ZDLProfile::toJson(ZDLJson::Builder &builder) const {
    yyjson_mut_val *obj = builder.newObject();
    builder.addString(obj, "id", id);
    builder.addString(obj, "name", name);
    builder.addString(obj, "iwad", iwad);
    builder.addString(obj, "port", port);

    yyjson_mut_val *fileArr = builder.newArray();
    for (const ZDLFileEntry &entry: files) {
        yyjson_mut_val *fileObj = builder.newObject();
        builder.addString(fileObj, "file", entry.file);
        builder.addBool(fileObj, "enabled", entry.enabled);
        builder.appendValue(fileArr, fileObj);
    }
    builder.addValue(obj, "files", fileArr);

    builder.addInt(obj, "skill", skill);
    builder.addInt(obj, "monsters", monsters);
    builder.addString(obj, "warp", warp);
    builder.addString(obj, "extra", extra);
    builder.addBool(obj, "dialogOpen", dialogOpen);

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
    builder.addValue(obj, "multiplayer", mp);

    return obj;
}
