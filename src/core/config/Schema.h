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

// Every string the config files are spelled with. Nothing else spells one.

namespace ConfigFile {
inline constexpr const char *JSON = "zdl.json";
inline constexpr const char *INI = "zdl.ini";

inline constexpr const char *JSON_EXT = ".json";
inline constexpr const char *INI_EXT = ".ini";
inline constexpr const char *ZDL_EXT = ".zdl";
inline constexpr const char *CFG_EXT = ".cfg";

inline constexpr const char *PROFILES_DIR = "profiles";
inline constexpr const char *PROFILE_STEM = "profile";

inline constexpr const char *ENGINE = "ZDL4";
}

namespace ProfileName {
inline constexpr const char *UNNAMED = "New profile";
inline constexpr const char *IMPORTED = "Imported";
}

namespace ThemeMode {
inline constexpr const char *SYSTEM = "system";
inline constexpr const char *LIGHT = "light";
inline constexpr const char *DARK = "dark";
}

namespace StartView {
inline constexpr const char *PROFILES = "profiles";
inline constexpr const char *GAMES = "games";
}

// zdl.json outside the profile objects.
namespace ConfigKey {
inline constexpr const char *VERSION = "version";
inline constexpr const char *ENGINE = "engine";
inline constexpr const char *APP_VERSION = "appVersion";
inline constexpr const char *GENERAL = "general";
inline constexpr const char *IWADS = "iwads";
inline constexpr const char *PORTS = "ports";
inline constexpr const char *PROFILES = "profiles";
inline constexpr const char *ACTIVE_PROFILE = "activeProfile";

// general
inline constexpr const char *ALWAYS_ADD = "alwaysAdd";
inline constexpr const char *DOSBOX = "dosbox";
inline constexpr const char *DETECTED = "detected";
inline constexpr const char *AUTO_CLOSE = "autoClose";
inline constexpr const char *LAUNCH_ZDL_IMMEDIATELY = "launchZdlImmediately";
inline constexpr const char *SHOW_PATHS = "showPaths";
inline constexpr const char *NO_USER_CONF = "noUserConf";
inline constexpr const char *SHOW_HIDDEN = "showHidden";
inline constexpr const char *PROFILE_CONFIGS = "profileConfigs";
inline constexpr const char *HARDWARE_RENDERING = "hardwareRendering";
inline constexpr const char *START_VIEW = "startView";
inline constexpr const char *GAME_PORT = "gamePort";
inline constexpr const char *THEME = "theme";
inline constexpr const char *IS_IMPORTED = "isImported";
inline constexpr const char *IMPORTED_FROM = "importedFrom";
inline constexpr const char *IMPORT_DATE = "importDate";

// general.window
inline constexpr const char *WINDOW = "window";
inline constexpr const char *SIZE = "size";
inline constexpr const char *POS = "pos";

// general.lastDirs; GENERAL doubles as its first key.
inline constexpr const char *LAST_DIRS = "lastDirs";
inline constexpr const char *WAD = "wad";
inline constexpr const char *SRC = "src";
inline constexpr const char *SAVE = "save";
inline constexpr const char *ZDL = "zdl";
inline constexpr const char *CONFIG = "config";
inline constexpr const char *REPLAY = "replay";

// iwads[] and ports[]; DOSBOX doubles as the port flag.
inline constexpr const char *NAME = "name";
inline constexpr const char *FILE = "file";
}

// One profile object in zdl.json.
namespace ProfileKey {
inline constexpr const char *ID = "id";
inline constexpr const char *NAME = "name";
inline constexpr const char *IWAD = "iwad";
inline constexpr const char *PORT = "port";
inline constexpr const char *FILES = "files";
inline constexpr const char *FILE = "file";
inline constexpr const char *ENABLED = "enabled";
inline constexpr const char *SKILL = "skill";
inline constexpr const char *MONSTERS = "monsters";
inline constexpr const char *WARP = "warp";
inline constexpr const char *EXTRA = "extra";
inline constexpr const char *DIALOG_OPEN = "dialogOpen";
inline constexpr const char *REPLAY_OPEN = "replayOpen";
inline constexpr const char *SAVE_OPEN = "saveOpen";
inline constexpr const char *CONFIG = "config";
inline constexpr const char *SHARED_CONFIG = "sharedConfig";
inline constexpr const char *CUSTOM_COMMAND = "customCommand";
inline constexpr const char *COMMAND = "command";
inline constexpr const char *DOS_FULLSCREEN = "dosFullscreen";
inline constexpr const char *CAPTURE_OUTPUT = "captureOutput";
inline constexpr const char *LEVELSTAT = "levelstat";

// multiplayer; PORT doubles as its port.
inline constexpr const char *MULTIPLAYER = "multiplayer";
inline constexpr const char *GAME_TYPE = "gameType";
inline constexpr const char *PLAYERS = "players";
inline constexpr const char *EXTRATIC = "extratic";
inline constexpr const char *NETMODE = "netmode";
inline constexpr const char *DUP = "dup";
inline constexpr const char *HOST = "host";
inline constexpr const char *FRAG_LIMIT = "fragLimit";
inline constexpr const char *TIME_LIMIT = "timeLimit";
inline constexpr const char *DMFLAGS = "dmflags";
inline constexpr const char *DMFLAGS2 = "dmflags2";
inline constexpr const char *SAVEGAME = "savegame";
inline constexpr const char *LISTED = "listed";

// replay; FILE doubles as its file.
inline constexpr const char *REPLAY = "replay";
inline constexpr const char *MODE = "mode";
inline constexpr const char *PLAYBACK = "playback";
inline constexpr const char *COMPATIBILITY = "compatibility";
inline constexpr const char *LONGTICS = "longtics";
inline constexpr const char *SOLO_NET = "soloNet";

// save; ENABLED and FILE double as its keys.
inline constexpr const char *SAVE = "save";
}

// zdl.ini and .zdl files.
namespace IniSection {
inline constexpr const char *GENERAL = "zdl.general";
inline constexpr const char *IWADS = "zdl.iwads";
inline constexpr const char *PORTS = "zdl.ports";
inline constexpr const char *SAVE = "zdl.save";

// qZDL's own; other tools only read SAVE.
inline constexpr const char *PROFILE = "zdl.profile";
}

namespace IniKey {
// zdl.general
inline constexpr const char *ALWAYS_ADD = "alwaysadd";
inline constexpr const char *AUTO_CLOSE = "autoclose";
inline constexpr const char *ZDL_LAUNCH = "zdllaunch";
inline constexpr const char *SHOW_PATHS = "showpaths";
inline constexpr const char *NO_USER_CONF = "nouserconf";
inline constexpr const char *IS_IMPORTED = "isimported";
inline constexpr const char *IMPORTED_FROM = "importedfrom";
inline constexpr const char *IMPORT_DATE = "importdate";
inline constexpr const char *LAST_DIR = "lastDir";
inline constexpr const char *WAD_LAST_DIR = "wadLastDir";
inline constexpr const char *SRC_LAST_DIR = "srcLastDir";
inline constexpr const char *SAVE_LAST_DIR = "saveLastDir";
inline constexpr const char *ZDL_LAST_DIR = "zdlLastDir";
inline constexpr const char *INI_LAST_DIR = "iniLastDir";
inline constexpr const char *WINDOW_SIZE = "windowsize";
inline constexpr const char *WINDOW_POS = "windowpos";

// zdl.save
inline constexpr const char *IWAD = "iwad";
inline constexpr const char *PORT = "port";
inline constexpr const char *SKILL = "skill";
inline constexpr const char *MONSTERS = "monsters";
inline constexpr const char *WARP = "warp";
inline constexpr const char *EXTRA = "extra";
inline constexpr const char *DLG_MODE = "dlgmode";
inline constexpr const char *DEMO_MODE = "demomode";
inline constexpr const char *SAVE_MODE = "savemode";
inline constexpr const char *GAME_TYPE = "gametype";
inline constexpr const char *PLAYERS = "players";
inline constexpr const char *EXTRATIC = "extratic";
inline constexpr const char *NETMODE = "netmode";
inline constexpr const char *DUP = "dup";
inline constexpr const char *HOST = "host";
inline constexpr const char *MP_PORT = "mp_port";
inline constexpr const char *FRAG_LIMIT = "fraglimit";
inline constexpr const char *TIME_LIMIT = "timelimit";
inline constexpr const char *DMFLAGS = "dmflags";
inline constexpr const char *DMFLAGS2 = "dmflags2";
inline constexpr const char *SAVEGAME = "savegame";
inline constexpr const char *LISTED = "listed";
inline constexpr const char *DEMO = "demo";
inline constexpr const char *DEMO_FILE = "demofile";
inline constexpr const char *DEMO_PLAY = "demoplay";
inline constexpr const char *COMPLEVEL = "complevel";
inline constexpr const char *LONGTICS = "longtics";
inline constexpr const char *SOLO_NET = "solonet";
inline constexpr const char *LEVELSTAT = "levelstat";
inline constexpr const char *LOAD_SAVE = "loadsave";
inline constexpr const char *SAVE_FILE = "savefile";

// file0, file1d: index after the prefix, the suffix marks a disabled one.
inline constexpr const char *FILE_PREFIX = "file";
inline constexpr char FILE_DISABLED = 'd';

// zdl.profile
inline constexpr const char *NAME = "name";
inline constexpr const char *CAPTURE_OUTPUT = "captureOutput";
inline constexpr const char *SHARED_CONFIG = "sharedConfig";
}

namespace IniValue {
inline constexpr const char *OPEN = "open";
inline constexpr const char *CLOSED = "closed";
inline constexpr const char *ON = "1";
inline constexpr const char *OFF = "0";
}
