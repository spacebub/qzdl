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

// Every string the config files are spelled with. Nothing else spells one.

namespace ConfigFile {
inline constexpr const char *JSON = "zdl.json";
inline constexpr const char *INI = "zdl.ini";

inline constexpr const char *JSON_EXT = ".json";
inline constexpr const char *INI_EXT = ".ini";
inline constexpr const char *ZDL_EXT = ".zdl";
inline constexpr const char *CFG_EXT = ".cfg";

// A DOS port is handed this one, since DOS cannot spell a profile's own name.
inline constexpr const char *DOS_CFG = "zdl.cfg";

// Beneath the profile's own folder, so DOSBox reaches it through the drive it lands on.
inline constexpr const char *DOS_FILES_DIR = "dosfiles";

inline constexpr const char *PROFILES_DIR = "profiles";
inline constexpr const char *SHORTCUTS_DIR = "shortcuts";
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

// What the on-disk value spells. Above core/config it is the StartView enum.
namespace StartViewText {
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
inline constexpr const char *MOVE_AT_NEXT_START = "moveAtNextStart";
inline constexpr const char *SHOW_HIDDEN = "showHidden";
inline constexpr const char *PROFILE_CONFIGS = "profileConfigs";
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
inline constexpr const char *MAXIMIZED = "maximized";

// general.lastDirs. GENERAL doubles as its first key.
inline constexpr const char *LAST_DIRS = "lastDirs";
inline constexpr const char *WAD = "wad";
inline constexpr const char *SRC = "src";
inline constexpr const char *SAVE = "save";
inline constexpr const char *ZDL = "zdl";
inline constexpr const char *CONFIG = "config";
inline constexpr const char *REPLAY = "replay";

// iwads[] and ports[]. DOSBOX doubles as the port flag.
inline constexpr const char *NAME = "name";
inline constexpr const char *FILE = "file";
inline constexpr const char *PORT_ID = "portId";
inline constexpr const char *DIALECT = "dialect";
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
inline constexpr const char *GENERAL_OPEN = "generalOpen";
inline constexpr const char *REPLAY_OPEN = "replayOpen";
inline constexpr const char *SAVE_OPEN = "saveOpen";
inline constexpr const char *CONFIG = "config";
inline constexpr const char *SHARED_CONFIG = "sharedConfig";
inline constexpr const char *CUSTOM_COMMAND = "customCommand";
inline constexpr const char *COMMAND = "command";
inline constexpr const char *PREFIX = "prefix";

// environment. NAME doubles as the variable name.
inline constexpr const char *ENVIRONMENT = "environment";
inline constexpr const char *VALUE = "value";

inline constexpr const char *DOS_FULLSCREEN = "dosFullscreen";
inline constexpr const char *DOS_EXIT = "dosExit";
inline constexpr const char *CAPTURE_OUTPUT = "captureOutput";
inline constexpr const char *LEVELSTAT = "levelstat";
inline constexpr const char *PISTOL_START = "pistolStart";
inline constexpr const char *CHEATS = "cheats";
inline constexpr const char *SKIP_AUTOLOAD = "skipAutoload";
inline constexpr const char *MUSIC = "music";
inline constexpr const char *SOUND_EFFECTS = "soundEffects";
inline constexpr const char *DISPLAY = "display";
inline constexpr const char *SCREENSHOTS = "screenshots";
inline constexpr const char *SCREENSHOT_FOLDER = "screenshotFolder";
inline constexpr const char *ZDL_FILE = "zdlFile";

// The flag words by the cvar names the ports read, each kept as typed.
inline constexpr const char *FLAGS = "flags";
inline constexpr const char *RULES = "rules";
inline constexpr const char *COMPLEVEL = "complevel";
inline constexpr const char *GAMEPLAY_OPEN = "gameplayOpen";

// multiplayer. PORT doubles as its port.
inline constexpr const char *MULTIPLAYER = "multiplayer";
inline constexpr const char *GAME_TYPE = "gameType";
inline constexpr const char *PLAYERS = "players";
inline constexpr const char *EXTRATIC = "extratic";
inline constexpr const char *NETMODE = "netmode";
inline constexpr const char *DUP = "dup";
inline constexpr const char *HOST = "host";
inline constexpr const char *FRAG_LIMIT = "fragLimit";
inline constexpr const char *TIME_LIMIT = "timeLimit";
// Read only when a profile has no flags object.
inline constexpr const char *DMFLAGS = "dmflags";
inline constexpr const char *DMFLAGS2 = "dmflags2";
inline constexpr const char *SAVEGAME = "savegame";
inline constexpr const char *LISTED = "listed";
inline constexpr const char *PLAYER_NAME = "playerName";
inline constexpr const char *PLAYER_COLOR = "playerColor";
inline constexpr const char *TEAM = "team";

// replay. FILE doubles as its file.
inline constexpr const char *REPLAY = "replay";
inline constexpr const char *MODE = "mode";
inline constexpr const char *SOURCE = "source";
inline constexpr const char *PLAYBACK = "playback";

// Read only when a profile has no complevel key.
inline constexpr const char *COMPATIBILITY = "compatibility";
inline constexpr const char *LONGTICS = "longtics";
inline constexpr const char *SOLO_NET = "soloNet";

// save. ENABLED and FILE double as its keys.
inline constexpr const char *SAVE = "save";
}

// zdl.ini and .zdl files.
namespace IniSection {
inline constexpr const char *GENERAL = "zdl.general";
inline constexpr const char *IWADS = "zdl.iwads";
inline constexpr const char *PORTS = "zdl.ports";
inline constexpr const char *SAVE = "zdl.save";

// qZDL's own. Other tools only read SAVE.
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
inline constexpr const char *SAVEGAME = "savegame";
inline constexpr const char *LISTED = "listed";
inline constexpr const char *DEMO = "demo";
inline constexpr const char *DEMO_FILE = "demofile";
inline constexpr const char *DEMO_PLAY = "demoplay";
inline constexpr const char *COMPLEVEL = "complevel";
inline constexpr const char *LONGTICS = "longtics";
inline constexpr const char *SOLO_NET = "solonet";
inline constexpr const char *LEVELSTAT = "levelstat";
inline constexpr const char *PISTOL_START = "pistolstart";
inline constexpr const char *CHEATS = "cheats";
inline constexpr const char *NO_AUTOLOAD = "noautoload";
inline constexpr const char *MUSIC = "music";
inline constexpr const char *SOUND_EFFECTS = "sfx";
inline constexpr const char *DISPLAY = "display";
inline constexpr const char *SHOTDIR = "shotdir";
inline constexpr const char *SHOTDIR_PATH = "shotdirpath";
inline constexpr const char *GENERAL_MODE = "genmode";
inline constexpr const char *GAMEPLAY_MODE = "gameplaymode";
inline constexpr const char *PLAYER_NAME = "playername";
inline constexpr const char *PLAYER_COLOR = "playercolor";
inline constexpr const char *TEAM = "team";
inline constexpr const char *DEMO_FROM = "demofrom";
inline constexpr const char *LOAD_SAVE = "loadsave";
inline constexpr const char *SAVE_FILE = "savefile";

// file0, file1d: index after the prefix, the suffix marks a disabled one.
inline constexpr const char *FILE_PREFIX = "file";
inline constexpr char FILE_DISABLED = 'd';

// zdl.profile
inline constexpr const char *NAME = "name";
inline constexpr const char *CAPTURE_OUTPUT = "captureOutput";
inline constexpr const char *SHARED_CONFIG = "sharedConfig";
// Only in the .zdl of a desktop shortcut, which launches that profile and no other.
inline constexpr const char *ID = "id";
}

// Profile rules keys and zdl.save keys alike.
namespace RuleKey {
inline constexpr const char *COOP_SPAWNS = "coop_spawns";
inline constexpr const char *COOP = "coop";
inline constexpr const char *ALWAYS_APPLY_DMFLAGS = "always_apply_dmflags";
inline constexpr const char *CHAIN_EPISODES = "chain_episodes";
inline constexpr const char *DOUBLE_AMMO = "double_ammo";
inline constexpr const char *NO_CHEATS = "no_cheats";
inline constexpr const char *NO_OPTIONS = "no_options";
inline constexpr const char *LIMIT_REMOVING = "limit_removing";
inline constexpr const char *VANILLA = "vanilla";
inline constexpr const char *BETA = "beta";
inline constexpr const char *STROLLER = "stroller";
inline constexpr const char *FLIP_LEVELS = "flip_levels";
inline constexpr const char *FLIP_WEAPONS = "flip_weapons";
inline constexpr const char *TURBO = "turbo";
inline constexpr const char *SPEED = "speed";
inline constexpr const char *DOGS = "dogs";

inline constexpr std::array<const char *, 16> ALL = {
        COOP_SPAWNS,
        COOP,
        ALWAYS_APPLY_DMFLAGS,
        CHAIN_EPISODES,
        DOUBLE_AMMO,
        NO_CHEATS,
        NO_OPTIONS,
        LIMIT_REMOVING,
        VANILLA,
        BETA,
        STROLLER,
        FLIP_LEVELS,
        FLIP_WEAPONS,
        TURBO,
        SPEED,
        DOGS,
};
}

// Profile flags keys and zdl.save keys alike.
namespace FlagWord {
inline constexpr const char *DMFLAGS = "dmflags";
inline constexpr const char *DMFLAGS2 = "dmflags2";
inline constexpr const char *DMFLAGS3 = "dmflags3";
inline constexpr const char *ZADMFLAGS = "zadmflags";
inline constexpr const char *LMS_WEAPONS = "lmsallowedweapons";
inline constexpr const char *LMS_SPECTATORS = "lmsspectatorsettings";
inline constexpr const char *COMPATFLAGS = "compatflags";
inline constexpr const char *COMPATFLAGS2 = "compatflags2";
inline constexpr const char *ZACOMPATFLAGS = "zacompatflags";

inline constexpr std::array<const char *, 9> ALL = {
        DMFLAGS,
        DMFLAGS2,
        DMFLAGS3,
        ZADMFLAGS,
        LMS_WEAPONS,
        LMS_SPECTATORS,
        COMPATFLAGS,
        COMPATFLAGS2,
        ZACOMPATFLAGS,
};
}

namespace IniValue {
inline constexpr const char *OPEN = "open";
inline constexpr const char *CLOSED = "closed";
inline constexpr const char *ON = "1";
inline constexpr const char *OFF = "0";
}
