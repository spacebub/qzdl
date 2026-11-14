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

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ttk/system/Json.h"

#include "core/config/GameplayKeys.h"

struct FileEntry {
    std::string file;
    bool enabled{true};
};

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
    int players{0};

    // -extratic, which the port either gets or does not.
    bool extratic{false};
    int netmode{-1};
    int dup{0};
    std::string host;
    std::string port;
    std::string fragLimit;
    std::string timeLimit;
    std::string savegame;

    // Advertise on the master server. Chocolate line only.
    bool listed{false};

    // Empty leaves them to the port. A color is #rrggbb.
    std::string playerName{};
    std::string playerColor{};

    // -1 leaves the team to the port.
    int team{-1};

    friend bool operator==(const MultiplayerSettings &, const MultiplayerSettings &) = default;
};

struct ReplaySettings {
    ReplayMode mode{ReplayMode::Off};

    // Name inside the profile's replays folder, or a full path.
    std::string file;

    // The demo Continue plays before recording into file. Named as file is.
    std::string source{};

    Playback playback{Playback::AsRecorded};

    bool longtics{false};
    bool soloNet{false};

    friend bool operator==(const ReplaySettings &, const ReplaySettings &) = default;
};

struct SaveSettings {
    bool enabled{false};

    // Relative to the folder the saves are listed from, or a full path.
    std::string file;

    friend bool operator==(const SaveSettings &, const SaveSettings &) = default;
};

struct Profile {
    std::string id;
    std::string name;
    std::string iwad;
    std::string port;
    std::vector<FileEntry> files;
    int skill{0};
    int monsters{0};
    std::string warp;
    std::string extra;
    bool dialogOpen{false};
    bool generalOpen{false};
    bool gameplayOpen{false};
    bool replayOpen{false};
    bool saveOpen{false};
    MultiplayerSettings multiplayer;
    ReplaySettings replay;
    SaveSettings save;

    std::string config;

    // Launch with the port's own config instead of the one above.
    bool sharedConfig{false};

    bool customCommand{false};

    // With the placeholders Command::tokens() lists.
    std::string command;

    // What a generated launch runs in front of the program, such as mangohud.
    std::string prefix;

    // Set in the environment of every launch, over what it already holds.
    std::vector<EnvVariable> environment;

    bool dosFullscreen{true};

    // Whether DOSBox is told to quit once the port has, or left at its prompt.
    bool dosExit{true};

    bool captureOutput{false};

    // -levelstat, on the ports that write one.
    bool levelstat{false};

    bool pistolStart{false};
    bool cheats{false};

    // -complevel. -1 leaves it to the port.
    int complevel{-1};

    // As typed. Empty leaves a word or a number to the port.
    std::array<std::string, Flags::WORDS> flags{};
    std::array<std::string, Rules::RULES> rules{};

    bool skipAutoload{false};
    bool music{true};
    bool soundEffects{true};
    Display display{Display::Port};
    Screenshots screenshots{Screenshots::Port};

    // Where Custom sends them.
    std::string screenshotFolder;

    // The .zdl this profile was read from, absolute and normalised. Empty for any other.
    std::string zdlFile;

    [[nodiscard]] const std::string &flag(const Flags::Word word) const { return flags[Flags::index(word)]; }
    [[nodiscard]] std::string &flag(const Flags::Word word) { return flags[Flags::index(word)]; }

    [[nodiscard]] const std::string &rule(const Rules::Rule which) const { return rules[Rules::index(which)]; }
    [[nodiscard]] std::string &rule(const Rules::Rule which) { return rules[Rules::index(which)]; }

    // A switch that turns on, which the profile keeps as IniValue::ON.
    [[nodiscard]] bool ruleOn(Rules::Rule which) const;
    void setRuleOn(Rules::Rule which, bool on);

    static std::string newId();

    static Profile fromJson(yyjson_val *obj);

    [[nodiscard]] yyjson_mut_val *toJson(const ttk::Json::Builder &builder) const;

    // Everything except id, name, config and zdlFile.
    void clearSettings();
};
