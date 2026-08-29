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

#include "config/ZDLConfigModel.h"
#include "core/zdlcommon.h"

namespace {

const char *DEFAULT_PROFILE_NAME = "Default";

ZDLNameEntry entryFromJson(yyjson_val *obj) {
    ZDLNameEntry entry;
    entry.name = ZDLJson::objGetString(obj, "name");
    entry.file = ZDLJson::objGetString(obj, "file");
    return entry;
}

void readEntries(yyjson_val *root, const char *key, QVector<ZDLNameEntry> &out) {
    out.clear();
    yyjson_val *arr = ZDLJson::objGet(root, key);
    if ((arr == nullptr) || !yyjson_is_arr(arr)) {
        return;
    }
    size_t idx = 0;
    size_t max = 0;
    yyjson_val *item = nullptr;
    yyjson_arr_foreach(arr, idx, max, item) {
        ZDLNameEntry const entry = entryFromJson(item);
        // A nameless or fileless entry can't be selected or launched.
        if (!entry.file.isEmpty()) {
            out.append(entry);
        }
    }
}

void writeEntries(ZDLJson::Builder  const &builder, yyjson_mut_val *root, const char *key,
                  const QVector<ZDLNameEntry> &entries) {
    yyjson_mut_val *arr = builder.newArray();
    for (const ZDLNameEntry &entry: entries) {
        yyjson_mut_val *obj = builder.newObject();
        builder.addString(obj, "name", entry.name);
        builder.addString(obj, "file", entry.file);
        ZDLJson::Builder::appendValue(arr, obj);
    }
    builder.addValue(root, key, arr);
}

void readLastDirs(yyjson_val *general, ZDLLastDirs &dirs) {
    yyjson_val *obj = ZDLJson::objGet(general, "lastDirs");
    dirs.general = ZDLJson::objGetString(obj, "general");
    dirs.wad = ZDLJson::objGetString(obj, "wad");
    dirs.src = ZDLJson::objGetString(obj, "src");
    dirs.save = ZDLJson::objGetString(obj, "save");
    dirs.zdl = ZDLJson::objGetString(obj, "zdl");
    dirs.config = ZDLJson::objGetString(obj, "config");
}

}

ZDLConfigModel::ZDLConfigModel() {
    ensureProfile();
}

void ZDLConfigModel::clear() {
    general = ZDLGeneralSettings();
    iwads.clear();
    ports.clear();
    profiles.clear();
    activeProfileId.clear();
    ensureProfile();
}

void ZDLConfigModel::ensureProfile() {
    if (profiles.isEmpty()) {
        ZDLProfile profile;
        profile.id = ZDLProfile::newId();
        profile.name = DEFAULT_PROFILE_NAME;
        profiles.append(profile);
    }
    if (indexOfProfile(activeProfileId) < 0) {
        activeProfileId = profiles.first().id;
    }
}

int ZDLConfigModel::indexOfProfile(const QString &id) const {
    if (id.isEmpty()) {
        return -1;
    }
    for (int i = 0; i < profiles.size(); i++) {
        if (profiles[i].id == id) {
            return i;
        }
    }
    return -1;
}

int ZDLConfigModel::activeProfileIndex() const {
    int const index = indexOfProfile(activeProfileId);
    return index < 0 ? 0 : index;
}

ZDLProfile &ZDLConfigModel::activeProfile() {
    ensureProfile();
    return profiles[activeProfileIndex()];
}

bool ZDLConfigModel::setActiveProfile(const QString &id) {
    if (indexOfProfile(id) < 0) {
        return false;
    }
    activeProfileId = id;
    rememberProfileForIwad();
    return true;
}

QString ZDLConfigModel::uniqueProfileName(const QString &base) const {
    QString candidate = base.trimmed();
    if (candidate.isEmpty()) {
        candidate = DEFAULT_PROFILE_NAME;
    }

    bool taken = false;
    for (const ZDLProfile &profile: profiles) {
        if (profile.name.compare(candidate, Qt::CaseInsensitive) == 0) {
            taken = true;
            break;
        }
    }
    if (!taken) {
        return candidate;
    }

    for (int suffix = 2;; suffix++) {
        QString numbered = QString("%1 (%2)").arg(candidate).arg(suffix);
        taken = false;
        for (const ZDLProfile &profile: profiles) {
            if (profile.name.compare(numbered, Qt::CaseInsensitive) == 0) {
                taken = true;
                break;
            }
        }
        if (!taken) {
            return numbered;
        }
    }
}

QString ZDLConfigModel::addProfile(const QString &name) {
    ZDLProfile profile;
    profile.id = ZDLProfile::newId();
    profile.name = uniqueProfileName(name);
    profiles.append(profile);
    return profile.id;
}

QString ZDLConfigModel::duplicateActiveProfile(const QString &name) {
    ZDLProfile copy = activeProfile();
    copy.id = ZDLProfile::newId();
    copy.name = uniqueProfileName(name);
    profiles.append(copy);
    return copy.id;
}

void ZDLConfigModel::removeProfile(const QString &id) {
    int const index = indexOfProfile(id);
    if (index < 0) {
        return;
    }

    // Never leave the user with no profile at all; empty the last one instead.
    if (profiles.size() == 1) {
        profiles[0].clearSettings();
        profiles[0].name = DEFAULT_PROFILE_NAME;
        activeProfileId = profiles[0].id;
        return;
    }

    profiles.remove(index);

    for (auto it = general.lastProfileByIwad.begin(); it != general.lastProfileByIwad.end();) {
        if (it.value() == id) {
            it = general.lastProfileByIwad.erase(it);
        } else {
            ++it;
        }
    }

    if (activeProfileId == id) {
        activeProfileId = profiles[qMin(index, profiles.size() - 1)].id;
    }
}

QString ZDLConfigModel::profileForIwad(const QString &iwadName) const {
    if (iwadName.isEmpty()) {
        return {};
    }

    // The profile explicitly used with this game last time wins.
    QString remembered = general.lastProfileByIwad.value(iwadName);
    if (indexOfProfile(remembered) >= 0) {
        return remembered;
    }

    // Otherwise fall back on the first profile bound to it.
    for (const ZDLProfile &profile: profiles) {
        if (profile.iwad == iwadName) {
            return profile.id;
        }
    }
    return {};
}

void ZDLConfigModel::rememberProfileForIwad() {
    int const index = indexOfProfile(activeProfileId);
    if (index < 0) {
        return;
    }
    const ZDLProfile &profile = profiles[index];
    if (!profile.iwad.isEmpty()) {
        general.lastProfileByIwad[profile.iwad] = profile.id;
    }
}

const ZDLNameEntry *ZDLConfigModel::findIwad(const QString &name) const {
    for (const ZDLNameEntry &entry: iwads) {
        if (entry.name == name) {
            return &entry;
        }
    }
    return nullptr;
}

const ZDLNameEntry *ZDLConfigModel::findPort(const QString &name) const {
    for (const ZDLNameEntry &entry: ports) {
        if (entry.name == name) {
            return &entry;
        }
    }
    return nullptr;
}

bool ZDLConfigModel::load(const QString &path, QString *error) {
    ZDLJson::Doc const doc = ZDLJson::readFile(path, error);
    if (!doc.isValid()) {
        return false;
    }
    yyjson_val *root = doc.root();
    if ((root == nullptr) || !yyjson_is_obj(root)) {
        if (error != nullptr) {
            *error = "root value is not an object";
        }
        return false;
    }

    clear();
    profiles.clear();

    yyjson_val *gen = ZDLJson::objGet(root, "general");
    general.alwaysAdd = ZDLJson::objGetString(gen, "alwaysAdd");
    general.autoClose = ZDLJson::objGetBool(gen, "autoClose");
    general.launchZdlImmediately = ZDLJson::objGetBool(gen, "launchZdlImmediately");
    general.rememberFileList = ZDLJson::objGetBool(gen, "rememberFileList", true);
    general.showPaths = ZDLJson::objGetBool(gen, "showPaths", true);
    general.noUserConf = ZDLJson::objGetBool(gen, "noUserConf");
    general.isImported = ZDLJson::objGetBool(gen, "isImported");
    general.doNotImportThis = ZDLJson::objGetBool(gen, "doNotImportThis");
    general.importedFrom = ZDLJson::objGetString(gen, "importedFrom");
    general.importDate = ZDLJson::objGetString(gen, "importDate");
    readLastDirs(gen, general.lastDirs);

    yyjson_val *window = ZDLJson::objGet(gen, "window");
    int pair[2] = {0, 0};
    if (ZDLJson::objGetIntArray(window, "size", pair, 2)) {
        general.hasWindowSize = true;
        general.windowSize = QSize(pair[0], pair[1]);
    }
    if (ZDLJson::objGetIntArray(window, "pos", pair, 2)) {
        general.hasWindowPos = true;
        general.windowPos = QPoint(pair[0], pair[1]);
    }

    yyjson_val *lastByIwad = ZDLJson::objGet(gen, "lastProfileByIwad");
    if ((lastByIwad != nullptr) && yyjson_is_obj(lastByIwad)) {
        size_t idx = 0;
        size_t max = 0;
        yyjson_val *key = nullptr;
        yyjson_val *val = nullptr;
        yyjson_obj_foreach(lastByIwad, idx, max, key, val) {
            if (yyjson_is_str(key) && yyjson_is_str(val)) {
                general.lastProfileByIwad.insert(QString::fromUtf8(yyjson_get_str(key)),
                                                 QString::fromUtf8(yyjson_get_str(val)));
            }
        }
    }

    readEntries(root, "iwads", iwads);
    readEntries(root, "ports", ports);

    yyjson_val *profileArr = ZDLJson::objGet(root, "profiles");
    if ((profileArr != nullptr) && yyjson_is_arr(profileArr)) {
        size_t idx = 0;
        size_t max = 0;
        yyjson_val *item = nullptr;
        yyjson_arr_foreach(profileArr, idx, max, item) {
            profiles.append(ZDLProfile::fromJson(item));
        }
    }

    activeProfileId = ZDLJson::objGetString(root, "activeProfile");
    ensureProfile();

    LOGDATA() << "Loaded " << profiles.size() << " profile(s) from " << path << Qt::endl;
    return true;
}

bool ZDLConfigModel::save(const QString &path, QString *error) const {
    const ZDLJson::Builder builder;
    yyjson_mut_val *root = builder.newObject();
    builder.setRoot(root);

    builder.addInt(root, "version", SCHEMA_VERSION);
    builder.addString(root, "engine", "ZDL");
    builder.addString(root, "appVersion", ZDL_PRIVATE_VERSION_STRING);

    yyjson_mut_val *gen = builder.newObject();
    builder.addString(gen, "alwaysAdd", general.alwaysAdd);
    builder.addBool(gen, "autoClose", general.autoClose);
    builder.addBool(gen, "launchZdlImmediately", general.launchZdlImmediately);
    builder.addBool(gen, "rememberFileList", general.rememberFileList);
    builder.addBool(gen, "showPaths", general.showPaths);
    builder.addBool(gen, "noUserConf", general.noUserConf);
    builder.addBool(gen, "isImported", general.isImported);
    builder.addBool(gen, "doNotImportThis", general.doNotImportThis);
    builder.addString(gen, "importedFrom", general.importedFrom);
    builder.addString(gen, "importDate", general.importDate);

    yyjson_mut_val *window = builder.newObject();
    if (general.hasWindowSize) {
        yyjson_mut_val *size = builder.newArray();
        builder.appendInt(size, general.windowSize.width());
        builder.appendInt(size, general.windowSize.height());
        builder.addValue(window, "size", size);
    }
    if (general.hasWindowPos) {
        yyjson_mut_val *pos = builder.newArray();
        builder.appendInt(pos, general.windowPos.x());
        builder.appendInt(pos, general.windowPos.y());
        builder.addValue(window, "pos", pos);
    }
    builder.addValue(gen, "window", window);

    yyjson_mut_val *dirs = builder.newObject();
    builder.addString(dirs, "general", general.lastDirs.general);
    builder.addString(dirs, "wad", general.lastDirs.wad);
    builder.addString(dirs, "src", general.lastDirs.src);
    builder.addString(dirs, "save", general.lastDirs.save);
    builder.addString(dirs, "zdl", general.lastDirs.zdl);
    builder.addString(dirs, "config", general.lastDirs.config);
    builder.addValue(gen, "lastDirs", dirs);

    yyjson_mut_val *lastByIwad = builder.newObject();
    for (auto it = general.lastProfileByIwad.constBegin(); it != general.lastProfileByIwad.constEnd(); ++it) {
        // Don't persist pointers to profiles that no longer exist.
        if (indexOfProfile(it.value()) >= 0) {
            builder.addString(lastByIwad, it.key().toUtf8().constData(), it.value());
        }
    }
    builder.addValue(gen, "lastProfileByIwad", lastByIwad);

    builder.addValue(root, "general", gen);

    writeEntries(builder, root, "iwads", iwads);
    writeEntries(builder, root, "ports", ports);

    builder.addString(root, "activeProfile", activeProfileId);

    yyjson_mut_val *profileArr = builder.newArray();
    for (const ZDLProfile &profile: profiles) {
        ZDLJson::Builder::appendValue(profileArr, profile.toJson(builder));
    }
    builder.addValue(root, "profiles", profileArr);

    LOGDATA() << "Writing config to " << path << Qt::endl;
    return builder.writeFile(path, error);
}
