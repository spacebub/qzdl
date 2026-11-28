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

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ttk/system/Json.h"

#include "core/config/GameplayKeys.h"
#include "core/config/Groups.h"
#include "core/config/Options.h"
#include "core/util/Sparse.h"

struct FileEntry {
    std::string file;
    bool enabled{true};

    // An absolute path with nothing at it when Storage::markGoneAddons last looked. Never saved.
    bool gone{false};

    std::uint16_t group{0};
};

// The form in which two spellings of one file compare equal. Windows ignores case in file names.
[[nodiscard]] std::string fileKey(const std::string &path);

// The on-disk format stores these as integers. Nothing above core/config should.

// What the profile opens, if anything.
enum class GameType : std::uint8_t {
    None,
    Coop,
    Deathmatch,
    AltDeathmatch,
    TeamDeathmatch,

    // Weapons stay and items respawn. Hosted as plain deathmatch where a port has no such game.
    Deathmatch3,
};

// Which side of a netgame this profile is on. Worked out from the settings rather
// than stored.
enum class NetRole : std::uint8_t {
    Alone,
    Host,
    Join,
};

enum class ReplayMode : std::uint8_t {
    Off,
    Record,
    Play,

    // Plays a demo, then records on from where the player takes over.
    Continue,
};

// Where -shotdir sends screenshots. Port passes nothing.
enum class Screenshots : std::uint8_t {
    Port,
    Profile,
    Custom,
};

// Port passes nothing, and the port keeps whichever it last ran in.
enum class Display : std::uint8_t {
    Port,
    Window,
    Fullscreen,
};

enum class Playback : std::uint8_t {
    AsRecorded,
    Timed,
    Fast,
};

struct EnvVariable {
    std::string name;
    std::string value;

    friend bool operator==(const EnvVariable &, const EnvVariable &) = default;
};

// A value read off the disk, which may be anything at all.
constexpr GameType gameTypeOf(const int stored) {
    return stored >= 0 && std::cmp_less_equal(stored, std::to_underlying(GameType::Deathmatch3))
                   ? static_cast<GameType>(stored)
                   : GameType::None;
}

constexpr ReplayMode replayModeOf(const int stored) {
    return stored >= 0 && stored <= 3 ? static_cast<ReplayMode>(stored) : ReplayMode::Off;
}

constexpr Screenshots screenshotsOf(const int stored) {
    return stored >= 0 && stored <= 2 ? static_cast<Screenshots>(stored) : Screenshots::Port;
}

constexpr Display displayOf(const int stored) {
    return stored >= 0 && stored <= 2 ? static_cast<Display>(stored) : Display::Port;
}

constexpr Playback playbackOf(const int stored) {
    return stored >= 0 && stored <= 2 ? static_cast<Playback>(stored) : Playback::AsRecorded;
}

struct MultiplayerSettings {
    GameType gameType{GameType::None};

    // -extratic, which the port either gets or does not.
    bool extratic{false};

    // Advertise on the master server. Chocolate line only.
    bool listed{false};

    int players{0};
    int netmode{-1};
    int dup{0};

    // -1 leaves the team to the port.
    int team{-1};

    std::string host;
    std::string port;
    std::string fragLimit;
    std::string timeLimit;
    std::string savegame;

    // Empty leaves them to the port. A color is #rrggbb.
    std::string playerName{};
    std::string playerColor{};

    friend bool operator==(const MultiplayerSettings &, const MultiplayerSettings &) = default;
};

struct ReplaySettings {
    ReplayMode mode{ReplayMode::Off};
    Playback playback{Playback::AsRecorded};
    bool longtics{false};
    bool soloNet{false};

    // Name inside the profile's replays folder, or a full path.
    std::string file;

    // The demo Continue plays before recording into file. Named as file is.
    std::string source{};

    friend bool operator==(const ReplaySettings &, const ReplaySettings &) = default;
};

struct SaveSettings {
    bool enabled{false};

    // Relative to the folder the saves are listed from, or a full path.
    std::string file;

    friend bool operator==(const SaveSettings &, const SaveSettings &) = default;
};

// The settings of a profile that are on or off, one bit each.
enum class Switch : std::uint8_t {
    dialogOpen,
    generalOpen,
    gameplayOpen,
    replayOpen,
    saveOpen,

    // Launch with the port's own config instead of the profile config.
    sharedConfig,

    customCommand,
    dosFullscreen,

    // Whether DOSBox is told to quit once the port has, or left at its prompt.
    dosExit,

    captureOutput,
};

[[nodiscard]] constexpr std::uint32_t switchBit(const Switch which) {
    return 1U << static_cast<unsigned>(which);
}

struct Profile {
    std::string id;
    std::string name;
    std::string iwad;
    std::string engine;
    std::vector<FileEntry> files;
    std::vector<Group> fileGroups;
    std::string warp;
    std::string extra;
    MultiplayerSettings multiplayer;
    ReplaySettings replay;
    SaveSettings save;

    std::string config;

    // With the placeholders Command::tokens() lists.
    std::string command;

    // What a generated launch runs in front of the program, such as mangohud.
    std::string prefix;

    // Set in the environment of every launch, over what it already holds.
    std::vector<EnvVariable> environment;

    // Where Custom sends them.
    std::string screenshotFolder;

    // The .zdl this profile was read from, absolute and normalised. Empty for any other.
    std::string zdlFile;

    // The desktop icon or menu entry made for it. Empty when there is none.
    std::string shortcut;

    // Listed by the file browser in place of the one in Settings. Empty uses that one.
    std::string fileBrowserFolder;

    // As typed. A word left out is left to the port.
    Sparse<Flags::Word> flags;

    // The options that are on, and the text typed for the ones that take a number.
    Dialect::Features options;
    Sparse<Dialect::Feature> numbers;

    int skill{0};
    int monsters{0};

    // -complevel. -1 leaves it to the port.
    int complevel{-1};

    Slot slot{};

    std::uint32_t switches{DEFAULT_SWITCHES};

    Display display{Display::Port};
    Screenshots screenshots{Screenshots::Port};

    [[nodiscard]] bool on(const Switch which) const { return (switches & switchBit(which)) != 0; }

    void set(const Switch which, const bool value) {
        switches = value ? switches | switchBit(which) : switches & ~switchBit(which);
    }

    [[nodiscard]] const std::string &flag(const Flags::Word word) const { return flags.get(word); }
    void setFlag(const Flags::Word word, std::string value) { flags.set(word, std::move(value)); }

    [[nodiscard]] bool optionOn(const Dialect::Feature option) const { return options.has(option); }

    void setOption(const Dialect::Feature option, const bool value) {
        options = value ? options | option : options & ~Dialect::Features(option);
    }

    // A toggle as the user and the files see it.
    [[nodiscard]] bool checked(const Options::Option &option) const {
        return optionOn(option.feature) != option.inverted;
    }

    void setChecked(const Options::Option &option, const bool value) {
        setOption(option.feature, value != option.inverted);
    }

    [[nodiscard]] const std::string &number(const Dialect::Feature option) const { return numbers.get(option); }
    void setNumber(const Dialect::Feature option, std::string value) { numbers.set(option, std::move(value)); }

    // An option as the files keep it: IniValue::ON for a toggle that is on, the text of a number, empty otherwise.
    [[nodiscard]] const std::string &stored(const Options::Option &option) const;
    void store(const Options::Option &option, std::string text);

    static std::string newId();

    static Profile fromJson(yyjson_val *obj);

    [[nodiscard]] yyjson_mut_val *toJson(const ttk::Json::Builder &builder) const;

    // Everything except id, name, group, config, zdlFile and shortcut.
    void clearSettings();

    static constexpr std::uint32_t DEFAULT_SWITCHES = switchBit(Switch::dosFullscreen) | switchBit(Switch::dosExit);
};
