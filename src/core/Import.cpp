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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <map>

#include "core/Import.h"

#include <ranges>

#include "core/Text.h"

namespace {

const char *GENERAL = "zdl.general";
const char *SAVE = "zdl.save";

// What [zdl.save] has no room for because no other Doom tool has a use for it.
// Anything reading a .zdl looks for [zdl.save] and passes over the rest.
const char *PROFILE = "zdl.profile";

bool legacyBool(const Ini::Section *general, const char *key, const bool def) {
    return general != nullptr && general->has(key) ? general->get(key) == "1" : def;
}

std::string legacyString(const Ini::Section *general, const char *key) {
    return general != nullptr ? general->get(key) : std::string();
}

bool parsePair(const std::string &value, int *first, int *second) {
    const size_t comma = value.find(',');

    if (comma == std::string::npos) {
        return false;
    }

    const std::string left = value.substr(0, comma);
    const std::string right = value.substr(comma + 1);

    if (!Text::isInt(left) || !Text::isInt(right)) {
        return false;
    }

    *first = Text::toInt(left);
    *second = Text::toInt(right);

    return true;
}

void readNumberedEntries(const Ini::Section *section, const char prefix, std::vector<NameEntry> &out) {
    out.clear();

    if (section == nullptr) {
        return;
    }

    std::map<int, NameEntry> byIndex;

    for (const auto *entry : section->startingWith(std::string_view(&prefix, 1))) {
        const std::string &key = entry->first;
        const std::string digits = key.substr(1, key.size() - 2);

        if (!Text::isInt(digits)) {
            continue;
        }

        // The last letter says which half of the pair this is: n for the name
        // shown in the list, f for the file it stands for.
        const char kind = static_cast<char>(std::tolower(static_cast<unsigned char>(key.back())));

        if (kind == 'n') {
            byIndex[Text::toInt(digits)].name = entry->second;
        } else if (kind == 'f') {
            byIndex[Text::toInt(digits)].file = entry->second;
        }
    }

    for (const auto &entry: byIndex | std::views::values) {
        if (!entry.file.empty()) {
            out.push_back(entry);
        }
    }
}

std::vector<FileEntry> readNumberedFiles(const Ini::Section &section) {
    std::map<int, FileEntry> byIndex;

    for (const auto *entry : section.startingWith("file")) {
        const std::string &key = entry->first;
        const bool disabled = key.back() == 'd' || key.back() == 'D';
        const std::string digits = key.substr(4, key.size() - 4 - (disabled ? 1 : 0));

        if (!Text::isInt(digits)) {
            continue;
        }

        byIndex[Text::toInt(digits)] = FileEntry{.file = entry->second, .enabled = !disabled};
    }

    std::vector<FileEntry> files;

    for (const auto &entry: byIndex | std::views::values) {
        files.push_back(entry);
    }

    return files;
}

int sectionInt(const Ini::Section &section, const char *key, const int def) {
    return section.has(key) ? Text::toInt(section.get(key), def) : def;
}

void setIfSet(Ini::Section &section, const char *key, const std::string &value) {
    if (!value.empty()) {
        section.set(key, value);
    }
}

}

Profile Import::profileFromSection(const Ini::Section &section) {
    Profile profile;

    profile.iwad = section.get("iwad");
    profile.port = section.get("port");
    profile.skill = sectionInt(section, "skill", 0);
    profile.monsters = sectionInt(section, "monsters", 0);
    profile.warp = section.get("warp");
    profile.extra = section.get("extra");
    profile.dialogOpen = Text::iequals(section.get("dlgmode"), "open");
    profile.files = readNumberedFiles(section);

    MultiplayerSettings &mp = profile.multiplayer;

    mp.gameType = sectionInt(section, "gametype", 0);
    mp.players = sectionInt(section, "players", 0);
    mp.extratic = sectionInt(section, "extratic", 0);
    mp.netmode = sectionInt(section, "netmode", -1);
    mp.dup = sectionInt(section, "dup", 0);
    mp.host = section.get("host");
    mp.port = section.get("mp_port");
    mp.fragLimit = section.get("fraglimit");
    mp.timeLimit = section.get("timelimit");
    mp.dmflags = section.get("dmflags");
    mp.dmflags2 = section.get("dmflags2");
    mp.savegame = section.get("savegame");

    return profile;
}

void Import::profileToSection(const Profile &profile, Ini::Section &section) {
    setIfSet(section, "port", profile.port);
    setIfSet(section, "iwad", profile.iwad);

    if (profile.skill > 0) {
        section.set("skill", std::to_string(profile.skill));
    }

    if (profile.monsters > 0) {
        section.set("monsters", std::to_string(profile.monsters));
    }

    setIfSet(section, "warp", profile.warp);
    setIfSet(section, "extra", profile.extra);
    section.set("dlgmode", profile.dialogOpen ? "open" : "closed");

    for (size_t index = 0; index < profile.files.size(); index++) {
        const FileEntry &entry = profile.files[index];
        std::string key = "file" + std::to_string(index);

        if (!entry.enabled) {
            key.push_back('d');
        }

        section.set(key, entry.file);
    }

    const MultiplayerSettings &mp = profile.multiplayer;

    setIfSet(section, "host", mp.host);
    setIfSet(section, "mp_port", mp.port);
    setIfSet(section, "fraglimit", mp.fragLimit);
    setIfSet(section, "timelimit", mp.timeLimit);
    setIfSet(section, "dmflags", mp.dmflags);
    setIfSet(section, "dmflags2", mp.dmflags2);
    setIfSet(section, "savegame", mp.savegame);
    section.set("gametype", std::to_string(mp.gameType));
    section.set("players", std::to_string(mp.players));
    section.set("extratic", std::to_string(mp.extratic));
    section.set("netmode", std::to_string(mp.netmode));
    section.set("dup", std::to_string(mp.dup));
}

void Import::fromLegacy(const Ini &ini, Config &config) {
    config.clear();

    const Ini::Section *gen = ini.section(GENERAL);
    GeneralSettings &general = config.general;

    general.alwaysAdd = legacyString(gen, "alwaysadd");
    general.autoClose = legacyBool(gen, "autoclose", false);
    general.launchZdlImmediately = legacyBool(gen, "zdllaunch", false);
    general.showPaths = legacyBool(gen, "showpaths", true);
    general.noUserConf = legacyBool(gen, "nouserconf", false);
    general.isImported = legacyBool(gen, "isimported", false);
    general.importedFrom = legacyString(gen, "importedfrom");
    general.importDate = legacyString(gen, "importdate");

    general.lastDirs.general = legacyString(gen, "lastDir");
    general.lastDirs.wad = legacyString(gen, "wadLastDir");
    general.lastDirs.src = legacyString(gen, "srcLastDir");
    general.lastDirs.save = legacyString(gen, "saveLastDir");
    general.lastDirs.zdl = legacyString(gen, "zdlLastDir");
    general.lastDirs.config = legacyString(gen, "iniLastDir");

    int first = 0;
    int second = 0;

    if (parsePair(legacyString(gen, "windowsize"), &first, &second)) {
        general.window.hasSize = true;
        general.window.width = first;
        general.window.height = second;
    }

    if (parsePair(legacyString(gen, "windowpos"), &first, &second)) {
        general.window.hasPosition = true;
        general.window.x = first;
        general.window.y = second;
    }

    readNumberedEntries(ini.section("zdl.iwads"), 'i', config.iwads);
    readNumberedEntries(ini.section("zdl.ports"), 'p', config.ports);

    // The single [zdl.save] becomes the one and only profile.
    if (const Ini::Section *save = ini.section(SAVE)) {
        Profile profile = profileFromSection(*save);

        profile.id = config.profiles.front().id;
        profile.name = config.profiles.front().name;
        config.profiles[0] = std::move(profile);
        config.activeProfileId = config.profiles[0].id;
    }

    config.ensureProfile();
    config.ensureConfigFiles();
}

bool Import::loadLegacyFile(const std::filesystem::path &path, Config &config) {
    std::error_code code;

    if (!std::filesystem::exists(path, code)) {
        return false;
    }

    Ini ini;

    if (!ini.read(path)) {
        return false;
    }

    fromLegacy(ini, config);

    return true;
}

bool Import::loadZdlFile(const std::filesystem::path &path, Profile &profile) {
    Ini ini;

    if (!ini.read(path)) {
        return false;
    }

    const Ini::Section *section = ini.section(SAVE);

    if (section == nullptr) {
        return false;
    }

    profile = profileFromSection(*section);
    profile.id = Profile::newId();
    profile.name = path.stem().string();

    // A .zdl this wrote carries the rest of the profile; one from anywhere else
    // has only the launch, and the file name has to stand in for the name.
    if (const Ini::Section *own = ini.section(PROFILE)) {
        if (const std::string named = Text::trim(own->get("name")); !named.empty()) {
            profile.name = named;
        }

        profile.captureOutput = own->get("captureOutput") == "1";
        profile.sharedConfig = own->get("sharedConfig") == "1";
    }

    return true;
}

bool Import::saveZdlFile(const std::filesystem::path &path, const Profile &profile) {
    Ini ini;

    profileToSection(profile, ini.ensure(SAVE));

    Ini::Section &own = ini.ensure(PROFILE);

    own.set("name", profile.name);
    own.set("captureOutput", profile.captureOutput ? "1" : "0");
    own.set("sharedConfig", profile.sharedConfig ? "1" : "0");

    return ini.write(path);
}
