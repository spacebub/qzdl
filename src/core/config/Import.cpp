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

#include <cctype>
#include <map>
#include <ranges>
#include <string_view>
#include <utility>

#include "core/config/Import.h"
#include "core/config/Schema.h"
#include "core/util/Text.h"

namespace {

bool legacyBool(const Ini::Section *general, const char *key, const bool def) {
    return general != nullptr && general->has(key) ? general->get(key) == IniValue::ON : def;
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

        // i0n / i0f: name and file of entry 0.
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
    constexpr size_t PREFIX = std::string_view(IniKey::FILE_PREFIX).size();
    std::map<int, FileEntry> byIndex;

    for (const auto *entry : section.startingWith(IniKey::FILE_PREFIX)) {
        const std::string &key = entry->first;
        const int last = std::tolower(static_cast<unsigned char>(key.back()));
        const bool disabled = last == IniKey::FILE_DISABLED;
        const std::string digits = key.substr(PREFIX, key.size() - PREFIX - (disabled ? 1 : 0));

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

    profile.iwad = section.get(IniKey::IWAD);
    profile.port = section.get(IniKey::PORT);
    profile.skill = sectionInt(section, IniKey::SKILL, 0);
    profile.monsters = sectionInt(section, IniKey::MONSTERS, 0);
    profile.warp = section.get(IniKey::WARP);
    profile.extra = section.get(IniKey::EXTRA);
    profile.dialogOpen = Text::iequals(section.get(IniKey::DLG_MODE), IniValue::OPEN);
    profile.replayOpen = Text::iequals(section.get(IniKey::DEMO_MODE), IniValue::OPEN);
    profile.saveOpen = Text::iequals(section.get(IniKey::SAVE_MODE), IniValue::OPEN);
    profile.files = readNumberedFiles(section);

    MultiplayerSettings &mp = profile.multiplayer;

    mp.gameType = sectionInt(section, IniKey::GAME_TYPE, 0);
    mp.players = sectionInt(section, IniKey::PLAYERS, 0);
    mp.extratic = sectionInt(section, IniKey::EXTRATIC, 0);
    mp.netmode = sectionInt(section, IniKey::NETMODE, -1);
    mp.dup = sectionInt(section, IniKey::DUP, 0);
    mp.host = section.get(IniKey::HOST);
    mp.port = section.get(IniKey::MP_PORT);
    mp.fragLimit = section.get(IniKey::FRAG_LIMIT);
    mp.timeLimit = section.get(IniKey::TIME_LIMIT);
    mp.dmflags = section.get(IniKey::DMFLAGS);
    mp.dmflags2 = section.get(IniKey::DMFLAGS2);
    mp.savegame = section.get(IniKey::SAVEGAME);
    mp.listed = sectionInt(section, IniKey::LISTED, 0) != 0;

    ReplaySettings &replay = profile.replay;

    replay.mode = sectionInt(section, IniKey::DEMO, 0);
    replay.file = section.get(IniKey::DEMO_FILE);
    replay.playback = sectionInt(section, IniKey::DEMO_PLAY, 0);
    replay.compatibility = sectionInt(section, IniKey::COMPLEVEL, -1);
    replay.longtics = sectionInt(section, IniKey::LONGTICS, 0) != 0;
    replay.soloNet = sectionInt(section, IniKey::SOLO_NET, 0) != 0;

    profile.levelstat = sectionInt(section, IniKey::LEVELSTAT, 0) != 0;

    profile.save.enabled = sectionInt(section, IniKey::LOAD_SAVE, 0) != 0;
    profile.save.file = section.get(IniKey::SAVE_FILE);

    return profile;
}

void Import::profileToSection(const Profile &profile, Ini::Section &section) {
    setIfSet(section, IniKey::PORT, profile.port);
    setIfSet(section, IniKey::IWAD, profile.iwad);

    if (profile.skill > 0) {
        section.set(IniKey::SKILL, std::to_string(profile.skill));
    }

    if (profile.monsters > 0) {
        section.set(IniKey::MONSTERS, std::to_string(profile.monsters));
    }

    setIfSet(section, IniKey::WARP, profile.warp);
    setIfSet(section, IniKey::EXTRA, profile.extra);
    section.set(IniKey::DLG_MODE, profile.dialogOpen ? IniValue::OPEN : IniValue::CLOSED);
    section.set(IniKey::DEMO_MODE, profile.replayOpen ? IniValue::OPEN : IniValue::CLOSED);
    section.set(IniKey::SAVE_MODE, profile.saveOpen ? IniValue::OPEN : IniValue::CLOSED);

    for (size_t index = 0; index < profile.files.size(); index++) {
        const FileEntry &entry = profile.files[index];
        std::string key = IniKey::FILE_PREFIX + std::to_string(index);

        if (!entry.enabled) {
            key.push_back(IniKey::FILE_DISABLED);
        }

        section.set(key, entry.file);
    }

    const MultiplayerSettings &mp = profile.multiplayer;

    setIfSet(section, IniKey::HOST, mp.host);
    setIfSet(section, IniKey::MP_PORT, mp.port);
    setIfSet(section, IniKey::FRAG_LIMIT, mp.fragLimit);
    setIfSet(section, IniKey::TIME_LIMIT, mp.timeLimit);
    setIfSet(section, IniKey::DMFLAGS, mp.dmflags);
    setIfSet(section, IniKey::DMFLAGS2, mp.dmflags2);
    setIfSet(section, IniKey::SAVEGAME, mp.savegame);
    section.set(IniKey::GAME_TYPE, std::to_string(mp.gameType));
    section.set(IniKey::PLAYERS, std::to_string(mp.players));
    section.set(IniKey::EXTRATIC, std::to_string(mp.extratic));
    section.set(IniKey::NETMODE, std::to_string(mp.netmode));
    section.set(IniKey::DUP, std::to_string(mp.dup));
    section.set(IniKey::LISTED, mp.listed ? IniValue::ON : IniValue::OFF);

    const ReplaySettings &replay = profile.replay;

    setIfSet(section, IniKey::DEMO_FILE, replay.file);
    section.set(IniKey::DEMO, std::to_string(replay.mode));
    section.set(IniKey::DEMO_PLAY, std::to_string(replay.playback));
    section.set(IniKey::COMPLEVEL, std::to_string(replay.compatibility));
    section.set(IniKey::LONGTICS, replay.longtics ? IniValue::ON : IniValue::OFF);
    section.set(IniKey::SOLO_NET, replay.soloNet ? IniValue::ON : IniValue::OFF);

    section.set(IniKey::LEVELSTAT, profile.levelstat ? IniValue::ON : IniValue::OFF);

    setIfSet(section, IniKey::SAVE_FILE, profile.save.file);
    section.set(IniKey::LOAD_SAVE, profile.save.enabled ? IniValue::ON : IniValue::OFF);
}

void Import::fromLegacy(const Ini &ini, Config &config) {
    config.reset();

    const Ini::Section *gen = ini.section(IniSection::GENERAL);
    GeneralSettings &general = config.general;

    general.alwaysAdd = legacyString(gen, IniKey::ALWAYS_ADD);
    general.autoClose = legacyBool(gen, IniKey::AUTO_CLOSE, false);
    general.launchZdlImmediately = legacyBool(gen, IniKey::ZDL_LAUNCH, false);
    general.showPaths = legacyBool(gen, IniKey::SHOW_PATHS, true);
    general.noUserConf = legacyBool(gen, IniKey::NO_USER_CONF, false);
    general.isImported = legacyBool(gen, IniKey::IS_IMPORTED, false);
    general.importedFrom = legacyString(gen, IniKey::IMPORTED_FROM);
    general.importDate = legacyString(gen, IniKey::IMPORT_DATE);

    general.lastDirs.general = legacyString(gen, IniKey::LAST_DIR);
    general.lastDirs.wad = legacyString(gen, IniKey::WAD_LAST_DIR);
    general.lastDirs.src = legacyString(gen, IniKey::SRC_LAST_DIR);
    general.lastDirs.save = legacyString(gen, IniKey::SAVE_LAST_DIR);
    general.lastDirs.zdl = legacyString(gen, IniKey::ZDL_LAST_DIR);
    general.lastDirs.config = legacyString(gen, IniKey::INI_LAST_DIR);

    int first = 0;
    int second = 0;

    if (parsePair(legacyString(gen, IniKey::WINDOW_SIZE), &first, &second)) {
        general.window.hasSize = true;
        general.window.width = first;
        general.window.height = second;
    }

    if (parsePair(legacyString(gen, IniKey::WINDOW_POS), &first, &second)) {
        general.window.hasPosition = true;
        general.window.x = first;
        general.window.y = second;
    }

    readNumberedEntries(ini.section(IniSection::IWADS), 'i', config.iwads);
    readNumberedEntries(ini.section(IniSection::PORTS), 'p', config.ports);

    if (const Ini::Section *save = ini.section(IniSection::SAVE)) {
        Profile profile = profileFromSection(*save);

        profile.id = Profile::newId();
        profile.name = ProfileName::IMPORTED;
        config.activeProfileId = profile.id;
        config.profiles.push_back(std::move(profile));
    }

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

    const Ini::Section *section = ini.section(IniSection::SAVE);

    if (section == nullptr) {
        return false;
    }

    profile = profileFromSection(*section);
    profile.id = Profile::newId();
    profile.name = path.stem().string();

    if (const Ini::Section *own = ini.section(IniSection::PROFILE)) {
        if (const std::string named = Text::trim(own->get(IniKey::NAME)); !named.empty()) {
            profile.name = named;
        }

        profile.captureOutput = own->get(IniKey::CAPTURE_OUTPUT) == IniValue::ON;
        profile.sharedConfig = own->get(IniKey::SHARED_CONFIG) == IniValue::ON;
    }

    return true;
}

bool Import::saveZdlFile(const std::filesystem::path &path, const Profile &profile) {
    Ini ini;

    profileToSection(profile, ini.ensure(IniSection::SAVE));

    Ini::Section &own = ini.ensure(IniSection::PROFILE);

    own.set(IniKey::NAME, profile.name);
    own.set(IniKey::CAPTURE_OUTPUT, profile.captureOutput ? IniValue::ON : IniValue::OFF);
    own.set(IniKey::SHARED_CONFIG, profile.sharedConfig ? IniValue::ON : IniValue::OFF);

    return ini.write(path);
}
