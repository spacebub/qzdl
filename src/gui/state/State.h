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
#include <functional>
#include <string>
#include <vector>

#include "core/config/Config.h"

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

// What a source port in the engine list is doing. Cached to disk as text by
// Releases, so gui/model/Engines maps it at that boundary.
enum class EngineState : std::uint8_t {
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

// Mirrors toolkit::Pill::Kind. The model layer cannot see the toolkit, so the
// pages map one onto the other.
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
    std::string port;
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

struct NameRow {
    int index = 0;
    std::string name;
    std::string file;
    std::string directory;
    std::string kind;
    bool missing = false;
    bool dosbox = false;

    // Fetched by ZDL, so it can be thrown away again.
    bool fetched = false;

    // Found on this machine.
    bool detected = false;

    bool operator==(const NameRow &) const = default;
};

struct EngineRow {
    int index = 0;
    std::string name;
    std::string blurb;
    std::string homepage;

    EngineState status = EngineState::Waiting;

    std::string version;
    std::string have;
    std::string sizeText;
    float progress = 0.0F;
    std::string file;
    std::string error;
    bool dos = false;

    // Its own field: an installed port stays "installed" while asked.
    bool asking = false;

    bool operator==(const EngineRow &) const = default;
};

struct LogRow {
    std::string line;
    bool own = false;

    bool operator==(const LogRow &) const = default;
};

// How loud a notice is, and how long it stays.
enum class Severity : std::uint8_t {
    Info,
    Success,
    Warning,
    Error,
};

struct Buzz {
    int id = 0;
    Severity severity = Severity::Info;
    std::string title;
    std::string body;
    int duration = 0;
};

struct DirEntry {
    std::string name;
    std::string path;
    bool directory = false;
    bool hidden = false;
    bool marked = false;

    bool operator==(const DirEntry &) const = default;
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

    // Bumped when the oldest lines were dropped, so a view that appends knows the
    // ones it holds are no longer the ones here.
    int lineGeneration = 0;

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

    int profileIndex = 0;
    std::string profileName;
    std::string profileKey;
    std::vector<ProfileCard> profileCards;

    // Active profile.
    std::string iwad;
    std::string port;
    int skill = 0;
    int monsters = 0;
    std::string warp;
    std::string extra;
    bool multiplayerOpen = false;
    bool sharedConfig = false;
    bool commandOverride = false;
    std::string command;
    std::string commandTrouble;
    bool dosFullscreen = false;
    bool dosExit = false;
    bool captureOutput = false;
    bool levelstat = false;
    bool hasLevelstat = false;
    std::string profileDirectory;

    NetRole netRole{};
    GameType gameType{};
    int players = 0;
    std::string host;
    std::string netPort;
    std::string fragLimit;
    std::string timeLimit;
    std::string dmflags;
    std::string dmflags2;
    bool extratic = false;
    int netmode = 0;
    int dup = 0;
    std::string savegame;
    bool listed = false;

    // What the port's netgame family supports.
    bool netHosts = false;
    bool netJoins = false;
    bool netPlayers = false;
    bool netListing = false;
    bool netFragLimit = false;
    bool netFlags = false;
    bool netSavegame = false;
    bool netExtratic = false;
    bool hasNetmode = false;
    bool netDup = false;
    bool multiplayerSet = false;

    // Replay panel.
    bool replayOpen = false;
    ReplayMode replayMode{};
    std::string replayFile;
    Playback replayPlayback{};
    bool replayLongtics = false;
    bool replaySoloNet = false;
    bool replaySet = false;

    std::vector<std::string> replayComplevels;
    std::vector<std::string> replayComplevelNumbers;
    int replayComplevel = 0;

    bool replayRecords = false;
    bool replayTimed = false;
    bool replayFast = false;
    bool replayHasComplevel = false;
    bool replayHasLongtics = false;
    bool replayHasSoloNet = false;

    // Newest first.
    std::vector<std::string> replayFiles;
    std::string replayFolder;

    // -1 when the demo is outside the folder.
    int replayIndex = -1;
    std::string replayPath;

    // A demo by the recording name is in the folder already.
    bool replayNameTaken = false;

    std::string replayTrouble;

    // Saves panel.
    bool saveOpen = false;
    bool saveEnabled = false;
    std::string saveFile;
    bool saveLoads = false;
    bool saveSlots = false;

    std::vector<std::string> saveFiles;
    std::vector<std::string> saveSlotLabels;
    std::string saveFolder;

    int saveIndex = -1;
    std::string savePath;

    std::string saveTrouble;

    // General settings.
    std::string gamePort;
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
    std::vector<NameRow> ports;
    std::vector<std::string> iwadNames;
    std::vector<std::string> portNames;

    // "DOS" or blank per port.
    std::vector<std::string> portBadges;

    std::vector<std::string> maps;

    std::string commandLine;

    // The -c commands a DOS launch spends of the eleven. Zero for any other port.
    int dosCommands = 0;

    // Read from disk, so filled on request.
    std::vector<ConfigDonor> configDonors;

    // The config file.
    std::string path;
    bool userConfig = false;
    bool ignoreUserConfig = false;
};

struct PortsState {
    std::vector<EngineRow> rows;
    bool checking = false;
    std::string directory;
    std::string downloads;
    std::string cachedText;
    bool cached = false;
    std::string trouble;
};

// Grouped by what it describes. There is one of these, so the padding costs nothing.
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
struct FilePickerState {
    bool open = false;
    std::string title;
    bool directories = false;

    // A folder can be taken as well as entered.
    bool folders = false;

    int markedFolders = 0;
    bool multiple = false;

    // A checkbox beside the pick button. Only the port picker uses it.
    std::string option;
    std::string optionHint;
    bool optionSet = false;

    std::string path;
    std::vector<std::string> parts;
    bool rooted = false;

    // The Windows drive list, above any root.
    bool drives = false;

    std::vector<DirEntry> entries;
    int marked = 0;

    bool hiddenShown = false;

    // Shown when the list is empty.
    std::string nothing;

    // Typing a path.
    bool editing = false;
    bool saving = false;

    // nameSeed is bumped each time a name is pushed in. The field is the user's
    // after that.
    std::string name;
    int nameStem = 0;
    int nameSeed = 0;

    // What the name would write, and whether that file exists.
    std::string target;
    bool replacing = false;
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
    PortsState ports;
    FilePickerState filePicker;
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
