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
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/config/Profile.h"
#include "core/config/Schema.h"
#include "core/config/Versions.h"

// Which shelf the library opens on.
enum class StartView : std::uint8_t {
    Profiles,
    Games,
};

namespace ConfigDefaults {
inline constexpr bool SHOW_PATHS = true;
inline constexpr StartView START_VIEW = StartView::Profiles;
inline constexpr const char *THEME = ThemeMode::SYSTEM;
}

struct GameEntry {
    std::string name;
    std::string file;
};

struct PortEntry {
    std::string name;
    std::string file;
    bool dosbox{false};

    // The catalog port ZDL4 unpacked into this row, when it is one.
    std::string portId{};

    // The Dialect id picked by hand. Empty leaves it to the file name.
    std::string dialect{};

    // A range of Dialect::versions, by its first field. Empty is the latest release.
    std::string version{};
};

struct LastDirs {
    std::string general;
    std::string wad;
    std::string src;
    std::string save;
    std::string zdl;
    std::string config;
    std::string replay;
};

struct WindowGeometry {
    int width{0};
    int height{0};
    int x{0};
    int y{0};
    bool hasSize{false};
    bool hasPosition{false};
    bool maximized{false};
};

struct GeneralSettings {
    std::string alwaysAdd;

    std::string dosbox;

    // Port ids detection has already offered, so removed ones are not re-added.
    std::vector<std::string> detected;

    bool autoClose{false};
    bool launchZdlImmediately{false};
    bool showPaths{ConfigDefaults::SHOW_PATHS};
    bool noUserConf{false};

    // Read from the user config alone. The next start moves the setup to the place noUserConf asks for.
    bool moveAtNextStart{false};

    bool showHidden{false};

    bool profileConfigs{false};

    StartView startView{ConfigDefaults::START_VIEW};

    // Port for games launched from the library. Empty uses the open profile's.
    std::string gamePort;

    std::string theme{ConfigDefaults::THEME};

    bool isImported{false};
    std::string importedFrom;
    std::string importDate;

    WindowGeometry window;
    LastDirs lastDirs;
};

class Config {
public:
    bool load(const std::filesystem::path &path, std::string *error = nullptr);

    bool save(const std::filesystem::path &path, std::string *error = nullptr) const;

    void reset();

    GeneralSettings general;
    std::vector<GameEntry> iwads;
    std::vector<PortEntry> ports;
    std::vector<Profile> profiles;
    std::string activeProfileId;

    // What the file it was read from said: its shape and the release that wrote it. CURRENT and empty for a config
    // made here.
    int fileVersion{Versions::CURRENT};
    std::string fileApp;

    Profile &activeProfile();

    [[nodiscard]] const Profile &activeProfile() const;

    [[nodiscard]] int activeProfileIndex() const;

    [[nodiscard]] int indexOfProfile(const std::string &id) const;

    bool setActiveProfile(const std::string &id);

    std::string addProfile(const std::string &name);

    std::string duplicateActiveProfile(const std::string &name);

    void removeProfile(const std::string &id);

    void ensureActiveProfile();

    [[nodiscard]] std::string uniqueProfileName(const std::string &base) const;

    // Free of the other profiles and of anything on disk. The excepted profile's own
    // file counts as free.
    [[nodiscard]] std::string uniqueConfigFile(const std::string &name, const std::string &except = {}) const;

    // Where a profile's own config and saves live. Empty with no data directory.
    [[nodiscard]] static std::filesystem::path profileFolder(const std::string &stem);

    void ensureConfigFiles();

    // The form a .zdl path is stored and compared in: absolute and normalised.
    [[nodiscard]] static std::string zdlKey(const std::filesystem::path &path);

    // The profile read from this .zdl, or an empty id.
    [[nodiscard]] std::string profileOfZdl(const std::filesystem::path &path) const;

    // Makes the profile the only one that came from this .zdl.
    void claimZdl(const std::string &id, const std::filesystem::path &path);

    [[nodiscard]] const GameEntry *findIwad(const std::string &name) const;

    // Empty when the active profile has no game.
    [[nodiscard]] std::string activeIwadFile() const;

    [[nodiscard]] const PortEntry *findPort(const std::string &name) const;

private:
    // Returned by activeProfile() when there are no profiles. Never saved.
    mutable Profile _none;
};
