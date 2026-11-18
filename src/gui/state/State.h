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
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/config/Config.h"
#include "core/config/Options.h"
#include "core/launch/Arguments.h"
#include "core/launch/Dialect.h"
#include "core/launch/Feature.h"

// What the interface reads.
//
// Nothing here is bound to anything: a bridge writes a field and bumps a revision,
// and the page it belongs to reads the field back when it next syncs.
namespace State {

// Which page the window is showing.
enum class Page : std::uint8_t {
    Library,
    Profile,
    Engines,
    Settings,
};

// Which shelf the library is on.
enum class Shelf : std::uint8_t {
    Profiles,
    Games,
};

// Which list the engines page is on.
enum class EnginesTab : std::uint8_t {
    Installed,
    Browse,
};

// What a launched port is doing. None is a key with no run against it.
enum class RunState : std::uint8_t {
    None,
    Launching,
    Running,
    Stopping,
    Closed,
    Failed,
};

// What a port in the download list is doing. Cached to disk as text by
// Releases, so gui/services/Downloads maps it at that boundary.
enum class DownloadStatus : std::uint8_t {
    Waiting,
    Checking,
    Ready,
    Elsewhere,
    Unavailable,
    Fetching,
    Unpacking,
    Installed,
    Failed,
};

enum class BadgeKind : std::uint8_t {
    None, // drawn in the accent tone
    Muted,
    Success,
    Warning,
    Danger,
};

struct BadgeSpec {
    std::string text;
    BadgeKind kind = BadgeKind::None;
    bool dot = false;

    bool operator==(const BadgeSpec &) const = default;
};

struct ProfileCard {
    int index = 0;
    std::string id;
    std::string key;
    std::string name;
    std::string iwad;
    std::string artKey;
    std::string engine;
    bool dosPort = false;
    std::string warp;
    int files = 0;
    int loaded = 0;
    NetRole netRole{};
    bool ready = false;

    // What the badges are made of, so a change to them moves the shelf.
    bool netSupported = false;
    ReplayMode replayMode{};

    bool operator==(const ProfileCard &) const = default;
};

struct FileRow {
    int index = 0;
    std::string file;
    std::string name;
    std::string directory;
    bool loaded = false;
    bool missing = false;

    bool operator==(const FileRow &) const = default;
};

// One flag word field of the Gameplay panel.
struct FlagField {
    std::string text;

    // Not a whole number, so the launch leaves the word out.
    bool bad = false;
};

// One option of the profile page.
struct OptionField {
    std::string text;

    // What it does on this port.
    std::string_view hint;

    // The range a number has to be in.
    int least = 0;
    int most = 0;

    // As the toggle shows it.
    bool on = false;

    // Not a number in the range the port takes, so the launch leaves it out.
    bool bad = false;
};

struct NameRow {
    int index = 0;
    std::string name;
    std::string file;
    std::string directory;
    std::string kind;
    bool missing = false;
    bool dosbox = false;

    // ZDL4 downloaded it and keeps it: it can be thrown away again and is not edited by hand.
    bool managed = false;

    // Found on this machine.
    bool detected = false;

    // Runs through flatpak run.
    bool flatpak = false;

    // Acts as the latest release until a version is picked in the engine dialog.
    bool needsVersion = false;

    bool operator==(const NameRow &) const = default;
};

struct DownloadRow {
    int index = 0;
    std::string name;
    std::string blurb;
    std::string homepage;

    DownloadStatus status = DownloadStatus::Waiting;

    std::string version;
    std::string have;
    std::string sizeText;
    float progress = 0.0F;
    std::string file;
    std::string error;
    bool dos = false;

    // Its own field: an installed port stays "installed" while asked.
    bool asking = false;

    bool operator==(const DownloadRow &) const = default;
};

struct LogRow {
    std::string line;
    bool own = false;

    bool operator==(const LogRow &) const = default;
};

// A separator is a row with nothing else on it.
struct ConfigDonor {
    std::string id;
    std::string name;
    std::string file;

    // Launches on the port's own settings rather than this file.
    bool shared = false;
};

struct System {
    Page page = Page::Library;

    std::string version;
    std::string runtime;
    bool windows = false;


    // Bumped when a title screen arrives.
    int artRev = 0;
};

struct RunsState {
    int rev = 0;
    bool busy = false;

    // Runs with a tab, oldest first.
    std::vector<std::string> docked;

    // Empty is all folded away.
    std::string showing;

    // Names with a log, tab or no tab.
    std::vector<std::string> logged;

    std::vector<LogRow> lines;

    // Bumped when the lines were replaced, so a view that appends knows the ones
    // it holds are no longer the ones here.
    int lineGeneration = 0;

    // How many have gone off the front since then, so a view that appends drops
    // the same.
    size_t linesDropped = 0;

    bool live = false;
};

// Grouped by what it describes. There is one of these, so the padding costs nothing.
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
struct Cfg {
    // Bumped so a view knows to read itself back.
    int rev = 0;

    // Bumped only by what a library launch depends on.
    int gameRev = 0;

    // Bumped only when what the shelf is built from has moved.
    int shelfRev = 0;

    // Bumped only when a row of the active profile's files changed.
    int filesRev = 0;

    int profileIndex = 0;
    std::string profileName;
    std::string profileKey;
    std::vector<ProfileCard> profileCards;

    // Active profile.
    std::string iwad;
    std::string engine;

    // What the port of the active profile can do.
    Dialect::Features features;

    int skill = 0;

    // The values the Monsters list offers, the one that plays, and its hint.
    std::span<const int> monsterChoices;
    int monstersPlayed = 0;
    std::string monstersHint;

    std::string warp;

    // The port starts on a picked map.
    bool mapTaken = false;

    std::string extra;
    bool multiplayerOpen = false;
    bool sharedConfig = false;
    bool commandOverride = false;
    std::string command;
    std::string commandTrouble;
    std::string prefix;
    std::vector<EnvVariable> environment;
    bool dosFullscreen = false;
    bool dosExit = false;
    bool captureOutput = false;
    std::string profileDirectory;

    Display display{};

    // Gameplay panel.
    bool gameplayOpen = false;

    Dialect::Compatibility compatibility = Dialect::Compatibility::none;
    std::vector<std::string> compatibilityNames;
    std::vector<std::string> compatibilityBadges;
    std::string_view compatibilityHint;

    // -1 when the words match no mode.
    int compatibilityIndex = 0;

    // The profile records, and the port stops without a complevel or at the one picked.
    bool complevelNeeded = false;

    // The profile records, the port stops without a skill, and none is picked.
    bool skillNeeded = false;

    // The picked complevel as the Replay panel names it. Empty for the port's own.
    std::string complevelName;

    std::array<FlagField, Flags::WORDS> flagFields{};
    std::array<OptionField, Options::ALL.size()> optionFields{};

    // General panel.
    bool generalOpen = false;
    Screenshots screenshots{};

    // Where -shotdir points, shown as it would be passed. Empty for Port or a Custom with no folder.
    std::string screenshotPath;

    // As typed, for the Custom field.
    std::string screenshotFolder;

    NetRole netRole{};

    // The port does not take the side the profile is on.
    bool netUnsupported = false;

    GameType gameType{};

    // The game types a host can pick, and what the port plays for the picked one.
    std::vector<GameType> gameTypes;
    GameType gameTypePlayed{};

    int players = 0;
    std::string host;
    std::string netPort;
    std::string fragLimit;
    std::string timeLimit;
    bool extratic = false;
    int netmode = 0;
    int dup = 0;
    std::string savegame;
    bool listed = false;
    std::string playerName;
    std::string playerColor;
    int team = -1;

    bool netSaveByName = false;
    std::string netSaveExt;

    // Where the port reads netgame saves from. Empty when it reads them from the save folder.
    std::string netSaveFolder;

    // The features of the port without what the netgame leaves out.
    Dialect::Features netFeatures;

    // The launch hosts a server, which loads no save and plays no game itself.
    bool serverHosted = false;

    // The player counts a host is opened for.
    Dialect::Range playerRange;

    bool multiplayerSet = false;

    // Replay panel.
    bool replayOpen = false;
    ReplayMode replayMode{};
    std::string replayFile;
    Playback replayPlayback{};
    bool replayLongtics = false;
    bool replaySoloNet = false;
    bool replaySet = false;

    bool replayHasComplevel = false;

    // Newest first.
    std::vector<std::string> replayFiles;
    std::string replayFolder;

    // -1 when the demo is outside the folder.
    int replayIndex = -1;
    std::string replayPath;

    // A demo by the recording name is in the folder already, and what the port does with it.
    bool replayNameTaken = false;
    Arguments::TakenDemo replayTaken{};

    // What goes on the end of a recording name without one.
    std::string_view replayExtension{".lmp"};

    // Play resolves to a client demo, which plays only as recorded.
    bool replayPlaysClientDemo = false;

    // The name of a client demo the port plays in place of the one picked or recorded. Empty without one.
    std::string replayClientDemoInstead;

    // The demo a Continue plays first. -1 when it is outside the folder.
    std::string replaySource;
    int replaySourceIndex = -1;
    std::string replaySourcePath;

    std::string replayTrouble;
    Arguments::ReplayLeftOut replayLeftOut{};

    // Saves panel.
    bool saveOpen = false;
    bool saveEnabled = false;
    std::string saveFile;
    bool saveLoads = false;
    bool saveSlots = false;

    std::vector<std::string> saveFiles;
    std::vector<std::string> saveSlotLabels;
    std::string saveFolder;

    // The port saves where it would on its own, so the saves are the ones it keeps.
    bool savesFromPort = false;

    int saveIndex = -1;
    std::string savePath;

    std::string saveTrouble;

    // General settings.
    std::string gameEngine;
    std::string alwaysAdd;
    std::string dosbox;
    bool dosPort = false;
    std::string systemDosbox;
    bool autoClose = false;
    bool launchZdlImmediately = false;
    bool showPaths = false;
    StartView startView{};
    bool profileConfigs = false;

    // Filtered in C++, so the shelf can lay out by counting.
    std::string filter;
    std::vector<ProfileCard> shelfProfiles;
    std::vector<NameRow> shelfGames;

    // Lists.
    std::vector<FileRow> files;
    int enabledCount = 0;
    std::vector<NameRow> iwads;
    std::vector<NameRow> engines;
    std::vector<std::string> iwadNames;
    std::vector<std::string> engineNames;

    // "DOS" or blank per engine.
    std::vector<std::string> engineBadges;

    std::vector<std::string> maps;

    std::string commandLine;

    // The -c commands a DOS launch spends of the eleven. Zero for any other port.
    int dosCommands = 0;

    // Read from disk, so filled on request.
    std::vector<ConfigDonor> configDonors;

    // The config file, and where the profile folders, ports and downloads are kept.
    std::string path;
    std::string dataDirectory;
    bool openedByHand = false;
    bool portableRunning = false;

    // What the next start runs in. The page says when it differs from the running mode.
    bool portableWanted = false;

#ifdef _WIN32
    // Read from the registry, where the installer may have written it too.
    bool zdlAssociated = false;
#endif
};

struct DownloadsState {
    std::vector<DownloadRow> rows;
    bool checking = false;

    // Detection is walking the disk for source ports.
    bool finding = false;

    std::string directory;
    std::string downloadDirectory;
    std::string cachedText;
    bool cached = false;
    std::string trouble;
};

// Which dialog is up, and what it was opened with.

// Where the interface is, which C++ owns because the history does.
struct NavState {
    Shelf shelf = Shelf::Profiles;

    EnginesTab engines = EnginesTab::Installed;

    bool tuning = false;
};

// All of it, and the one call that says something moved.
struct All {
    System sys;
    Cfg cfg;
    RunsState runs;
    DownloadsState downloads;
    NavState nav;

    // Set by the application: marks the interface for a sync at the next frame.
    std::function<void()> changed;

    void touch() const {
        if (changed) {
            changed();
        }
    }
};

All &get();

}
