/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2018-2019  Lcferrum
 * Copyright (C) 2023-2026  spacebub
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

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/system/Env.h"
#include "ttk/system/Paths.h"

#include "Launch.h"
#include "core/launch/Arguments.h"
#include "core/launch/KnownPorts.h"
#include "core/launch/PortSaves.h"
#include "core/launch/Storage.h"
#include "core/ports/Zip.h"

#ifdef __linux__
#include "support/FlatpakApp.h"
#endif

#ifndef _WIN32
namespace {

namespace fs = std::filesystem;

using launch::Strings;

fs::path home() {
    return ttk::Paths::home_directory();
}

fs::path dataHome() {
    return ttk::Env::get("XDG_DATA_HOME");
}

fs::path configHome() {
    return ttk::Env::get("XDG_CONFIG_HOME");
}

// A UZDoom or GZDoom save: a zip whose info.json names the game.
void zds(const fs::path &path, const std::string &game) {
    launch::write(path, ports::zip({{.name = "info.json", .bytes = R"({"Game WAD": ")" + game + R"("})"}}));
}

void hsg(const fs::path &path, const std::string &game) {
    launch::write(
            path,
            ports::zip({{.name = "save.json", .bytes = R"({"Files": {"IWad": {"FileName": ")" + game + R"("}}})"}}));
}

// A ZDoom 2.8 save: a PNG with the game in a text chunk. No reader checks the CRC.
void png(const fs::path &path, const std::string &game) {
    const auto chunk = [](const std::string &type, const std::string &data) {
        std::string out;

        for (int shift = 24; shift >= 0; shift -= 8) {
            out.push_back(static_cast<char>((data.size() >> static_cast<unsigned>(shift)) & 0xFFU));
        }

        return out + type + data + std::string(4, '\0');
    };

    launch::write(path,
                  std::string("\x89PNG\r\n\x1a\n", 8) + chunk("IHDR", std::string(13, '\0'))
                          + chunk("tEXt", std::string("Title") + '\0' + "x")
                          + chunk("tEXt", std::string("Game WAD") + '\0' + game) + chunk("IEND", ""));
}

// Each written a minute after the last, so the last is the newest.
void aged(const std::vector<fs::path> &files) {
    const auto start = fs::file_time_type::clock::now() - std::chrono::hours(1);
    int minute = 0;

    for (const fs::path &file : files) {
        fs::last_write_time(file, start + std::chrono::minutes(minute++));
    }
}

class PortSavesTest : public ::testing::Test {
protected:
    Config config;
    fs::path games;

    void SetUp() override {
        clearVariables();
        games = tests::Sandbox::scratch("port-saves");

        // The sandbox lasts the whole run, so what one case saved would show in the next.
        for (const fs::path &folder : {
                     home() / ".config",
                     home() / ".var",
                     home() / "Saves",
                     home() / ".prboom-plus",
                     home() / ".prboom",
                     home() / ".dsda-doom",
                     home() / ".nyan-doom",
                     home() / ".skulltag",
                     home() / ".zdoom",
                     home() / ".chocolate-doom",
                     home() / ".crispy-doom",
                     dataHome(),
                     configHome(),
                     launch::profiles(),
             }) {
            std::error_code code;

            fs::remove_all(folder, code);
        }

        config = launch::single("/games/gzdoom");
        config.iwads.front().file = launch::wad(games / "DOOM2.WAD", "IWAD", {"MAP01"}).string();
        config.general.profileConfigs = true;
        profile().config = "alpha.cfg";
        profile().set(Switch::sharedConfig, true);
    }

    void TearDown() override { clearVariables(); }

    // A value set on the host would move the folders the ports read.
    static void clearVariables() {
        for (const char *name : {"DOOMSAVEDIR", "DOOMDATADIR", "ETERNITYUSER", "ETERNITYBASE"}) {
            ttk::Env::set(name, "");
        }
    }

    Profile &profile() { return config.activeProfile(); }

    void on(const fs::path &file) { config.engines.front().file = file.string(); }

    void addOn(const fs::path &file, const bool enabled = true) {
        profile().files.push_back({.file = file.string(), .enabled = enabled});
    }
};

}

TEST_F(PortSavesTest, GzdoomListsTheSavesOfThisGameFromEveryGameFolder) {
    const fs::path root = home() / ".config" / "gzdoom" / "savegames";

    zds(root / "doom.id.doom2.commercial" / "save00.zds", "DOOM2.WAD");
    zds(root / "doom.id.doom2.commercial" / "auto00.zds", "doom2.wad");
    zds(root / "doom.id.doom1.ultimate" / "save00.zds", "DOOM.WAD");
    zds(root / "loose.zds", "DOOM2.WAD");
    aged({
            root / "loose.zds",
            root / "doom.id.doom2.commercial" / "save00.zds",
            root / "doom.id.doom2.commercial" / "auto00.zds",
            root / "doom.id.doom1.ultimate" / "save00.zds",
    });

    EXPECT_EQ(PortSaves::folder(config), root);
    EXPECT_EQ(PortSaves::saves(config),
              (Strings{"doom.id.doom2.commercial/auto00.zds", "doom.id.doom2.commercial/save00.zds", "loose.zds"}));
}

TEST_F(PortSavesTest, UzdoomLeavesItsNetgameFolderOut) {
    on("/games/uzdoom");

    const fs::path root = dataHome() / "games" / "uzdoom" / "savegames";

    zds(root / "doom.id.doom2.commercial" / "save00.zds", "DOOM2.WAD");
    zds(root / "netgame" / "doom.id.doom2.commercial" / "coop.zds", "DOOM2.WAD");
    zds(root / "netgame" / "coop.zds", "DOOM2.WAD");

    EXPECT_EQ(PortSaves::folder(config), root);
    EXPECT_EQ(PortSaves::saves(config), (Strings{"doom.id.doom2.commercial/save00.zds"}));
}

#ifdef __linux__
TEST_F(PortSavesTest, AUzdoomFlatpakKeepsItsSavesInItsSandbox) {
    on("/var/lib/flatpak/exports/bin/org.zdoom.UZDoom");

    EXPECT_EQ(PortSaves::folder(config),
              home() / ".var" / "app" / "org.zdoom.UZDoom" / "data" / "games" / "uzdoom" / "savegames");
}
#endif

TEST_F(PortSavesTest, ASaveDirInTheIniMovesTheZdoomSaves) {
    on("/games/uzdoom");
    ttk::Env::set("DOOMSAVEDIR", "elsewhere");

    launch::write(configHome() / "uzdoom" / "uzdoom.ini", "[GlobalSettings]\nsave_dir=~/Saves/$DOOMSAVEDIR\n");

    EXPECT_EQ(PortSaves::folder(config), home() / "Saves" / "elsewhere");
}

TEST_F(PortSavesTest, ASaveDirTakesTheProgramFolderForProgdir) {
    on(games / "gzdoom");
    launch::write(home() / ".config" / "gzdoom" / "gzdoom.ini", "[GlobalSettings]\nsave_dir=$PROGDIR/saves$\n");

    EXPECT_EQ(PortSaves::folder(config), games / "saves$");
}

TEST_F(PortSavesTest, ASaveInAnotherGameFolderIsReachedFromThePortsOwn) {
    const fs::path root = home() / ".config" / "gzdoom" / "savegames";

    zds(root / "doom.id.doom2.unity" / "save00.zds", "DOOM2.WAD");
    zds(root / "loose.zds", "DOOM2.WAD");

    EXPECT_EQ(PortSaves::loadName(config, root / "doom.id.doom2.unity" / "save00.zds"),
              "../doom.id.doom2.unity/save00.zds");
    EXPECT_EQ(PortSaves::loadName(config, root / "loose.zds"), "");
    EXPECT_EQ(PortSaves::loadName(config, "/elsewhere/save.zds"), "");
}

TEST_F(PortSavesTest, ALaunchOnThePortsSettingsLoadsWithoutASaveFolder) {
    const fs::path root = home() / ".config" / "gzdoom" / "savegames";

    zds(root / "doom.id.doom2.commercial" / "save02.zds", "DOOM2.WAD");
    profile().save = {.enabled = true, .file = "doom.id.doom2.commercial/save02.zds"};

    const Strings args = Arguments::of(config);
    const auto loads = std::ranges::find(args, "-loadgame");

    ASSERT_NE(loads, args.end());
    ASSERT_NE(loads + 1, args.end());
    EXPECT_EQ(*(loads + 1), "../doom.id.doom2.commercial/save02.zds");
    EXPECT_EQ(std::ranges::find(args, "-savedir"), args.end());
}

TEST_F(PortSavesTest, GzdoomSavesInSavegamesFrom4_11AndInItsConfigFolderBefore) {
    const fs::path own = home() / ".config" / "gzdoom";

    for (const auto &[version, root] : {
                 std::pair{"", own / "savegames"},
                 std::pair{"4.12.1", own / "savegames"},
                 std::pair{"4.11.0", own / "savegames"},
                 std::pair{"4.9.0", own},
                 std::pair{"4.8.0", own},
                 std::pair{"4.4.0", own},
                 std::pair{"4.3.3", own},
         }) {
        config.engines.front().version = version;

        EXPECT_EQ(PortSaves::folder(config), root) << version;
    }
}

TEST_F(PortSavesTest, Gzdoom4_9KeepsAFolderPerGameInItsConfigFolder) {
    config.engines.front().version = "4.9.0";

    const fs::path root = home() / ".config" / "gzdoom";

    zds(root / "doom.id.doom2.commercial" / "save00.zds", "DOOM2.WAD");
    zds(root / "doom.id.doom1.ultimate" / "save00.zds", "DOOM.WAD");
    zds(root / "loose.zds", "DOOM2.WAD");
    aged({root / "loose.zds", root / "doom.id.doom2.commercial" / "save00.zds"});

    EXPECT_EQ(PortSaves::saves(config), (Strings{"doom.id.doom2.commercial/save00.zds", "loose.zds"}));
    EXPECT_EQ(PortSaves::loadName(config, root / "doom.id.doom2.commercial" / "save00.zds"),
              "../doom.id.doom2.commercial/save00.zds");
}

TEST_F(PortSavesTest, GzdoomBefore4_9KeepsSavesFlatAndLoadsThemByPath) {
    const fs::path root = home() / ".config" / "gzdoom";

    zds(root / "doom.id.doom2.commercial" / "save00.zds", "DOOM2.WAD");
    zds(root / "save01.zds", "DOOM2.WAD");
    profile().save = {.enabled = true, .file = "save01.zds"};

    for (const char *version : {"4.8.0", "4.4.0", "4.3.3"}) {
        config.engines.front().version = version;

        EXPECT_EQ(PortSaves::saves(config), (Strings{"save01.zds"})) << version;
        EXPECT_EQ(PortSaves::loadName(config, root / "doom.id.doom2.commercial" / "save00.zds"), "") << version;

        const Strings args = Arguments::of(config);
        const auto loads = std::ranges::find(args, "-loadgame");

        ASSERT_NE(loads, args.end()) << version;
        ASSERT_NE(loads + 1, args.end()) << version;
        EXPECT_EQ(*(loads + 1), (root / "save01.zds").string()) << version;
    }
}

TEST_F(PortSavesTest, Uzdoom4_14_3KeepsTheSaveRootButHasNoNetgameFolder) {
    on("/games/uzdoom");

    const fs::path root = dataHome() / "games" / "uzdoom" / "savegames";

    EXPECT_EQ(Storage::netgameSaveFolder(config), root / "netgame");

    config.engines.front().version = "4.14.3";

    zds(root / "doom.id.doom2.commercial" / "save00.zds", "DOOM2.WAD");

    EXPECT_EQ(PortSaves::folder(config), root);
    EXPECT_EQ(PortSaves::saves(config), (Strings{"doom.id.doom2.commercial/save00.zds"}));
    EXPECT_EQ(Storage::netgameSaveFolder(config), fs::path());
}

TEST_F(PortSavesTest, VkdoomSortsItsSavesByGameUnderItsConfigFolder) {
    on("/games/vkdoom");

    const fs::path root = home() / ".config" / "vkdoom" / "savegames";

    zds(root / "doom.id.doom2.commercial" / "save00.zds", "DOOM2.WAD");
    zds(root / "doom.id.doom1.ultimate" / "save00.zds", "DOOM.WAD");

    EXPECT_EQ(PortSaves::folder(config), root);
    EXPECT_EQ(PortSaves::saves(config), (Strings{"doom.id.doom2.commercial/save00.zds"}));
    EXPECT_EQ(PortSaves::loadName(config, root / "doom.id.doom2.commercial" / "save00.zds"),
              "../doom.id.doom2.commercial/save00.zds");
}

TEST_F(PortSavesTest, ASaveDirInTheVkdoomIniMovesItsSaves) {
    on("/games/vkdoom");
    launch::write(home() / ".config" / "vkdoom" / "vkdoom.ini", "[GlobalSettings]\nsave_dir=~/Saves\n");

    EXPECT_EQ(PortSaves::folder(config), home() / "Saves");
}

TEST_F(PortSavesTest, LzdoomAndQzdoomHaveNoFolder) {
    on("/games/lzdoom");

    EXPECT_EQ(PortSaves::folder(config), fs::path());

    on("/games/qzdoom");

    EXPECT_EQ(PortSaves::folder(config), fs::path());
}

TEST_F(PortSavesTest, ZdoomReadsTheGameFromThePngText) {
    on("/games/zdoom");

    const fs::path root = home() / ".config" / "zdoom";

    png(root / "save0.zds", "DOOM2.WAD");
    png(root / "save1.zds", "TNT.WAD");
    launch::write(root / "broken.zds", "not a png");

    EXPECT_EQ(PortSaves::folder(config), root);
    EXPECT_EQ(PortSaves::saves(config), (Strings{"save0.zds"}));
}

TEST_F(PortSavesTest, ZdoomBefore2_7_0SavesInItsHomeDotFolder) {
    on("/games/zdoom");

    for (const char *version : {"2.6.1", "2.6.0", "2.5.0"}) {
        config.engines.front().version = version;

        EXPECT_EQ(PortSaves::folder(config), home() / ".zdoom") << version;
    }

    for (const char *version : {"2.7.0", "2.7.1", "2.8.0", ""}) {
        config.engines.front().version = version;

        EXPECT_EQ(PortSaves::folder(config), home() / ".config" / "zdoom") << version;
    }
}

TEST_F(PortSavesTest, ZdoomReadsTheSaveDirFromTheIniItsReleaseKeeps) {
    on("/games/zdoom");
    launch::write(home() / ".zdoom" / "zdoom.ini", "[GlobalSettings]\nsave_dir=~/Saves/old\n");
    launch::write(home() / ".config" / "zdoom" / "zdoom.ini", "[GlobalSettings]\nsave_dir=~/Saves/new\n");

    config.engines.front().version = "2.6.1";

    EXPECT_EQ(PortSaves::folder(config), home() / "Saves" / "old");

    config.engines.front().version = "2.7.0";

    EXPECT_EQ(PortSaves::folder(config), home() / "Saves" / "new");
}

TEST_F(PortSavesTest, ZandronumKeepsItsSavesBesideItsIni) {
    on("/usr/bin/zandronum");

    EXPECT_EQ(PortSaves::folder(config), home() / ".config" / "zandronum");
}

TEST_F(PortSavesTest, HelionReadsTheGameFromTheSaveJson) {
    on("/games/Helion-1.1.0.0-x86_64.AppImage");

    const fs::path root = configHome() / "Helion";

    hsg(root / "savegame0.hsg", "doom2.wad");
    hsg(root / "savegame1.hsg", "plutonia.wad");

    EXPECT_EQ(PortSaves::folder(config), root);
    EXPECT_EQ(PortSaves::saves(config), (Strings{"savegame0.hsg"}));
}

TEST_F(PortSavesTest, HelionPrefersALowercaseFolderThatIsThere) {
    on("/games/helion");
    fs::create_directories(configHome() / "helion");

    EXPECT_EQ(PortSaves::folder(config), configHome() / "helion");
}

TEST_F(PortSavesTest, APortableHelionKeepsEverythingBesideItself) {
    on(games / "helion");
    launch::write(games / "config.ini", "");

    EXPECT_EQ(PortSaves::folder(config), games);
}

TEST_F(PortSavesTest, PrboomListsOnlyTheSavesItCanLoadBySlot) {
    on("/games/prboom-plus");

    const fs::path root = dataHome() / "prboom-plus";

    launch::write(root / "prboom-plus-savegame3.dsg", "x");
    launch::write(root / "demosav1.dsg", "x");
    launch::write(root / "prboom-plus-savegame.dsg", "x");

    EXPECT_EQ(PortSaves::folder(config), root);
    EXPECT_EQ(PortSaves::saves(config), (Strings{"prboom-plus-savegame3.dsg"}));
}

TEST_F(PortSavesTest, PrboomTakesAnOldFolderInTheHomeFirst) {
    on("/games/prboom-plus");
    fs::create_directories(home() / ".prboom-plus");

    EXPECT_EQ(PortSaves::folder(config), home() / ".prboom-plus");

    fs::remove_all(home() / ".prboom-plus");
}

TEST_F(PortSavesTest, PrboomPlusBefore2_6_66AlwaysKeepsItsHomeDotFolder) {
    on("/games/prboom-plus");
    config.engines.front().version = "2.6.2";

    EXPECT_EQ(PortSaves::folder(config), home() / ".prboom-plus");

    config.engines.front().version = "2.6.66";

    EXPECT_EQ(PortSaves::folder(config), dataHome() / "prboom-plus");
}

TEST_F(PortSavesTest, TheOriginalPrboomListsItsSavesInItsHomeDotFolder) {
    for (const char *file : {"/games/prboom", "/games/glboom"}) {
        on(file);

        const fs::path root = home() / ".prboom";

        launch::write(root / "prbmsav3.dsg", "x");
        launch::write(root / "prboom-plus-savegame4.dsg", "x");
        launch::write(root / "prbmsav.dsg", "x");

        EXPECT_EQ(PortSaves::folder(config), root) << file;
        EXPECT_EQ(PortSaves::saves(config), (Strings{"prbmsav3.dsg"})) << file;
        EXPECT_EQ(PortSaves::slotLabel(config, Dialect::of(config), "prbmsav3.dsg"), "Slot 3") << file;
    }
}

TEST_F(PortSavesTest, PrboomFollowsDoomsavedir) {
    on("/games/prboom-plus");
    ttk::Env::set("DOOMSAVEDIR", (games / "saves").string().c_str());

    EXPECT_EQ(PortSaves::folder(config), games / "saves");
}

TEST_F(PortSavesTest, WoofFilesSavesUnderTheGame) {
    on("/games/woof");

    const fs::path folder = dataHome() / "woof" / "savegames" / "DOOM2.WAD";

    launch::write(folder / "woofsav15.dsg", "x");

    EXPECT_EQ(PortSaves::folder(config), folder);
    EXPECT_EQ(PortSaves::saves(config), (Strings{"woofsav15.dsg"}));
}

TEST_F(PortSavesTest, WoofListsItsAutosaveAmongTheSlotsNewestFirst) {
    on("/games/woof");

    const fs::path folder = dataHome() / "woof" / "savegames" / "DOOM2.WAD";

    launch::write(folder / "woofsav1.dsg", "x");
    launch::write(folder / "autosave.dsg", "x");
    launch::write(folder / "woofsav2.dsg", "x");
    aged({folder / "woofsav1.dsg", folder / "autosave.dsg", folder / "woofsav2.dsg"});

    EXPECT_EQ(PortSaves::saves(config), (Strings{"woofsav2.dsg", "autosave.dsg", "woofsav1.dsg"}));
}

TEST_F(PortSavesTest, NuggetListsItsAutosaveButNotThePeriodicOnes) {
    on("/games/nugget-doom");

    const fs::path folder = dataHome() / "nugget-doom" / "savegames" / "DOOM2.WAD";

    launch::write(folder / "nuggsav1.dsg", "x");
    launch::write(folder / "autosav1.dsg", "x");
    launch::write(folder / "autosave.dsg", "x");
    aged({folder / "nuggsav1.dsg", folder / "autosav1.dsg", folder / "autosave.dsg"});

    EXPECT_EQ(PortSaves::saves(config), (Strings{"autosave.dsg", "nuggsav1.dsg"}));
}

TEST_F(PortSavesTest, AWoofLaunchOnThePortsSettingsLoadsTheAutosaveAsSlot255) {
    on("/games/woof");

    launch::write(dataHome() / "woof" / "savegames" / "DOOM2.WAD" / "autosave.dsg", "x");
    profile().save = {.enabled = true, .file = "autosave.dsg"};

    const Strings args = Arguments::of(config);
    const auto loads = std::ranges::find(args, "-loadgame");

    ASSERT_NE(loads, args.end());
    ASSERT_NE(loads + 1, args.end());
    EXPECT_EQ(*(loads + 1), "255");
    EXPECT_EQ(Storage::saveTrouble(config), "");
}

TEST_F(PortSavesTest, ABoomPortLeavesAnAutosaveOut) {
    on("/games/prboom-plus");

    const fs::path folder = home() / ".prboom-plus";

    launch::write(folder / "prboom-plus-savegame0.dsg", "x");
    launch::write(folder / "autosave.dsg", "x");

    EXPECT_EQ(PortSaves::saves(config), (Strings{"prboom-plus-savegame0.dsg"}));
}

TEST_F(PortSavesTest, WoofFilesSavesUnderTheFirstAddOnWithMapsForTheGame) {
    on("/games/woof");
    addOn(launch::wad(games / "music.wad", "PWAD", {}));
    addOn(launch::wad(games / "Off.wad", "PWAD", {"MAP02"}), false);
    addOn(launch::wad(games / "Sigil.WAD", "PWAD", {"E5M1"}));
    addOn(launch::wad(games / "Later.wad", "PWAD", {"MAP03"}));

    EXPECT_EQ(PortSaves::folder(config), dataHome() / "woof" / "savegames" / "Later.wad");
}

TEST_F(PortSavesTest, WoofSkipsAnAddOnWhoseMapsALaterOneReplaces) {
    on("/games/woof");
    addOn(launch::wad(games / "first.wad", "PWAD", {"MAP01"}));
    addOn(launch::wad(games / "second.wad", "PWAD", {"MAP01"}));

    EXPECT_EQ(PortSaves::folder(config), dataHome() / "woof" / "savegames" / "second.wad");

    addOn(launch::wad(games / "third.wad", "PWAD", {"MAP02"}));
    profile().files.front() = {.file = launch::wad(games / "first.wad", "PWAD", {"MAP01", "MAP05"}).string()};

    EXPECT_EQ(PortSaves::folder(config), dataHome() / "woof" / "savegames" / "first.wad");
}

TEST_F(PortSavesTest, WoofFilesSavesUnderAWadInsideAZip) {
    on("/games/woof");

    const std::string inner = launch::slurp(launch::wad(games / "inner.wad", "PWAD", {"MAP01"}));

    launch::write(games / "pack.zip",
                  ports::zip({
                          {.name = "readme.txt", .bytes = "x"},
                          {.name = "sub/deep.wad", .bytes = inner},
                          {.name = "MYMAPS.WAD", .bytes = inner},
                  }));
    addOn(games / "pack.zip");

    EXPECT_EQ(PortSaves::folder(config), dataHome() / "woof" / "savegames" / "MYMAPS.WAD");
}

TEST_F(PortSavesTest, WoofKeepsSavesFlatWhenSomeAreAlreadyThere) {
    on("/games/woof");
    launch::write(dataHome() / "woof" / "WOOFSAV0.DSG", "x");

    EXPECT_EQ(PortSaves::folder(config), dataHome() / "woof");
    EXPECT_EQ(PortSaves::saves(config), (Strings{"WOOFSAV0.DSG"}));
}

TEST_F(PortSavesTest, WoofFollowsOrganizeSavefilesInItsCfg) {
    on("/games/woof");
    launch::write(dataHome() / "woof" / "woofsav0.dsg", "x");
    launch::write(dataHome() / "woof" / "woof.cfg", "screenblocks 10\norganize_savefiles   1\n");

    EXPECT_EQ(PortSaves::folder(config), dataHome() / "woof" / "savegames" / "DOOM2.WAD");

    launch::write(dataHome() / "woof" / "woof.cfg", "organize_savefiles 0\n");
    fs::remove(dataHome() / "woof" / "woofsav0.dsg");

    EXPECT_EQ(PortSaves::folder(config), dataHome() / "woof");
}

TEST_F(PortSavesTest, NuggetFollowsSavegameDirInItsCfg) {
    on("/games/nugget-doom");
    launch::write(dataHome() / "nugget-doom" / "nugget-doom.cfg",
                  "savegame_dir \"" + (games / "nugget").string() + "\"\n");
    launch::write(games / "nugget" / "DOOM2.WAD" / "nuggsav3.dsg", "x");

    EXPECT_EQ(PortSaves::folder(config), games / "nugget" / "DOOM2.WAD");
    EXPECT_EQ(PortSaves::saves(config), (Strings{"nuggsav3.dsg"}));
}

TEST_F(PortSavesTest, CherryListsItsSlotsAndAutosaveButNotThePeriodicOnes) {
    on("/games/cherry-doom");

    const fs::path folder = dataHome() / "cherry-doom" / "savegames" / "DOOM2.WAD";

    launch::write(folder / "chersav1.dsg", "x");
    launch::write(folder / "autosav1.dsg", "x");
    launch::write(folder / "autosave.dsg", "x");
    launch::write(folder / "woofsav2.dsg", "x");
    aged({folder / "chersav1.dsg", folder / "autosav1.dsg", folder / "autosave.dsg", folder / "woofsav2.dsg"});

    EXPECT_EQ(PortSaves::folder(config), folder);
    EXPECT_EQ(PortSaves::saves(config), (Strings{"autosave.dsg", "chersav1.dsg"}));
}

TEST_F(PortSavesTest, CherryFollowsSavegameDirInItsCfg) {
    on("/games/cherry-doom");
    launch::write(dataHome() / "cherry-doom" / "cherry-doom.cfg",
                  "savegame_dir \"" + (games / "cherry").string() + "\"\n");
    launch::write(games / "cherry" / "DOOM2.WAD" / "chersav3.dsg", "x");

    EXPECT_EQ(PortSaves::folder(config), games / "cherry" / "DOOM2.WAD");
    EXPECT_EQ(PortSaves::saves(config), (Strings{"chersav3.dsg"}));
}

TEST_F(PortSavesTest, WoofReadsNoSavegameDir) {
    on("/games/woof");
    launch::write(dataHome() / "woof" / "woof.cfg", "savegame_dir \"" + (games / "woof").string() + "\"\n");

    EXPECT_EQ(PortSaves::folder(config), dataHome() / "woof" / "savegames" / "DOOM2.WAD");
}

TEST_F(PortSavesTest, DsdaFilesSavesUnderTheGameAndListsOnlyItsSlots) {
    on("/games/dsda-doom");

    const fs::path folder = dataHome() / "dsda-doom" / "dsda_doom_data" / "doom2";

    launch::write(folder / "dsda-doom-savegame3.dsg", "x");
    launch::write(folder / "demosav1.dsg", "x");
    launch::write(folder / "dsda-doom-savegame.dsg", "x");
    launch::write(folder / "nyan-doom-savegame2.dsg", "x");

    EXPECT_EQ(PortSaves::folder(config), folder);
    EXPECT_EQ(PortSaves::saves(config), (Strings{"dsda-doom-savegame3.dsg"}));
}

TEST_F(PortSavesTest, DsdaTakesAnOldFolderInTheHomeFirst) {
    on("/games/dsda-doom");
    fs::create_directories(home() / ".dsda-doom");

    EXPECT_EQ(PortSaves::folder(config), home() / ".dsda-doom" / "dsda_doom_data" / "doom2");
}

TEST_F(PortSavesTest, DsdaFallsBackToLocalShareWithoutXdgDataHome) {
    on("/games/dsda-doom");

    const std::string kept = ttk::Env::get("XDG_DATA_HOME");

    ttk::Env::set("XDG_DATA_HOME", "");

    EXPECT_EQ(PortSaves::folder(config), home() / ".local" / "share" / "dsda-doom" / "dsda_doom_data" / "doom2");

    ttk::Env::set("XDG_DATA_HOME", kept.c_str());
}

TEST_F(PortSavesTest, DsdaNestsEachAddOnWadInLoadOrder) {
    on("/games/dsda-doom");
    addOn(launch::wad(games / "Alpha.WAD", "PWAD", {"MAP01"}));
    addOn(launch::wad(games / "off.wad", "PWAD", {"MAP02"}), false);
    addOn(launch::wad(games / "Beta.wad", "PWAD", {"MAP03"}));

    EXPECT_EQ(PortSaves::folder(config), dataHome() / "dsda-doom" / "dsda_doom_data" / "doom2" / "alpha" / "beta");
}

TEST_F(PortSavesTest, DsdaSkipsAnAddOnMarkedGone) {
    on("/games/dsda-doom");
    addOn(games / "gone.wad");
    addOn(launch::wad(games / "Beta.wad", "PWAD", {"MAP03"}));
    profile().files.front().gone = true;

    EXPECT_EQ(PortSaves::folder(config), dataHome() / "dsda-doom" / "dsda_doom_data" / "doom2" / "beta");
}

TEST_F(PortSavesTest, DsdaSkipsAddOnsThatAreNotWads) {
    on("/games/dsda-doom");
    addOn(games / "fixes.deh");
    addOn(games / "textures.pk3");
    addOn(games / "demo.lmp");
    addOn(launch::wad(games / "maps.wad", "PWAD", {"MAP01"}));
    addOn(games / "autoexec.cfg");

    EXPECT_EQ(PortSaves::folder(config), dataHome() / "dsda-doom" / "dsda_doom_data" / "doom2" / "maps");
}

TEST_F(PortSavesTest, DsdaNestsTheWadsOfAZipSortedWhereverTheyAreInIt) {
    on("/games/dsda-doom");
    launch::write(games / "pack.zip",
                  ports::zip({
                          {.name = "readme.txt", .bytes = "x"},
                          {.name = "sub/Zeta.wad", .bytes = "x"},
                          {.name = "beta.WAD", .bytes = "x"},
                          {.name = "skin.pk3", .bytes = "x"},
                  }));
    addOn(games / "pack.zip");
    addOn(launch::wad(games / "last.wad", "PWAD", {"MAP01"}));

    EXPECT_EQ(PortSaves::folder(config),
              dataHome() / "dsda-doom" / "dsda_doom_data" / "doom2" / "beta" / "zeta" / "last");
}

TEST_F(PortSavesTest, DsdaReadsAZipAgainOnlyOnceItChanges) {
    on("/games/dsda-doom");

    const auto pack = [](const std::string &name, const std::string &padding) {
        return ports::zip({{.name = name, .bytes = padding}});
    };
    const fs::path archive = games / "again.zip";
    const fs::path root = dataHome() / "dsda-doom" / "dsda_doom_data" / "doom2";

    launch::write(archive, pack("beta.wad", "x"));
    addOn(archive);

    EXPECT_EQ(PortSaves::folder(config), root / "beta");

    // Same size and time, so the stamp has not moved and the old listing stands.
    const auto stamp = fs::last_write_time(archive);

    launch::write(archive, pack("gamm.wad", "x"));
    fs::last_write_time(archive, stamp);

    EXPECT_EQ(PortSaves::folder(config), root / "beta");

    launch::write(archive, pack("gamma.wad", "x"));

    EXPECT_EQ(PortSaves::folder(config), root / "gamma");
}

TEST_F(PortSavesTest, DsdaNestsTheGameAndEightAddOnsAtMost) {
    on("/games/dsda-doom");

    fs::path expected = dataHome() / "dsda-doom" / "dsda_doom_data" / "doom2";

    for (int each = 1; each <= 10; each++) {
        const std::string name = "add" + std::to_string(each);

        addOn(launch::wad(games / (name + ".wad"), "PWAD", {"MAP01"}));

        if (each <= 8) {
            expected /= name;
        }
    }

    EXPECT_EQ(PortSaves::folder(config), expected);
}

TEST_F(PortSavesTest, DsdaBefore0_26_0NestsSevenAddOnsAtMost) {
    on("/games/dsda-doom");

    fs::path seven = home() / ".dsda-doom" / "dsda_doom_data" / "doom2";

    for (int each = 1; each <= 8; each++) {
        const std::string name = "add" + std::to_string(each);

        addOn(launch::wad(games / (name + ".wad"), "PWAD", {"MAP01"}));

        if (each <= 7) {
            seven /= name;
        }
    }

    config.engines.front().version = "0.26.0";

    EXPECT_EQ(PortSaves::folder(config), seven / "add8");

    config.engines.front().version = "0.25.6";

    EXPECT_EQ(PortSaves::folder(config), seven);
}

TEST_F(PortSavesTest, DsdaKeepsSavesFlatWhenTheCfgTurnsOrganizedSavesOff) {
    on("/games/dsda-doom");
    addOn(launch::wad(games / "maps.wad", "PWAD", {"MAP01"}));

    const fs::path base = dataHome() / "dsda-doom";

    launch::write(base / "dsda-doom.cfg", "screen_width            640\ndsda_organized_saves    0\n");
    launch::write(base / "dsda-doom-savegame2.dsg", "x");

    EXPECT_EQ(PortSaves::folder(config), base);
    EXPECT_EQ(PortSaves::saves(config), (Strings{"dsda-doom-savegame2.dsg"}));

    launch::write(base / "dsda-doom.cfg", "dsda_organized_saves 1\n");

    EXPECT_EQ(PortSaves::folder(config), base / "dsda_doom_data" / "doom2" / "maps");
}

TEST_F(PortSavesTest, DsdaFollowsDoomsavedirOnlyForFlatSavesAndDoomdatadirOnlyForOrganizedOnes) {
    on("/games/dsda-doom");
    ttk::Env::set("DOOMSAVEDIR", (games / "saves").string().c_str());
    ttk::Env::set("DOOMDATADIR", (games / "data").string().c_str());

    EXPECT_EQ(PortSaves::folder(config), games / "data" / "dsda_doom_data" / "doom2");

    launch::write(dataHome() / "dsda-doom" / "dsda-doom.cfg", "dsda_organized_saves  0\n");

    EXPECT_EQ(PortSaves::folder(config), games / "saves");
}

TEST_F(PortSavesTest, DsdaNeedsAGameToNameTheOrganizedFolder) {
    on("/games/dsda-doom");
    config.iwads.front().file.clear();

    EXPECT_EQ(PortSaves::folder(config), fs::path());
}

TEST_F(PortSavesTest, NyanDoomUsesItsOwnNames) {
    on("/games/nyan-doom");
    addOn(launch::wad(games / "maps.wad", "PWAD", {"MAP01"}));

    const fs::path root = dataHome() / "nyan-doom";
    const fs::path folder = root / "nyan_doom_data" / "doom2" / "maps";

    launch::write(folder / "nyan-doom-savegame4.dsg", "x");
    launch::write(folder / "dsda-doom-savegame1.dsg", "x");

    EXPECT_EQ(PortSaves::folder(config), folder);
    EXPECT_EQ(PortSaves::saves(config), (Strings{"nyan-doom-savegame4.dsg"}));

    launch::write(root / "nyan-doom.cfg", "dsda_organized_saves 0\n");

    EXPECT_EQ(PortSaves::folder(config), root);

    const std::string kept = ttk::Env::get("XDG_DATA_HOME");

    fs::create_directories(home() / ".nyan-doom");
    ttk::Env::set("XDG_DATA_HOME", "");

    EXPECT_EQ(PortSaves::folder(config), home() / ".nyan-doom" / "nyan_doom_data" / "doom2" / "maps");

    ttk::Env::set("XDG_DATA_HOME", kept.c_str());
}

TEST_F(PortSavesTest, ADsdaLaunchOnThePortsSettingsLoadsTheSaveBySlot) {
    on("/games/dsda-doom");

    const fs::path folder = dataHome() / "dsda-doom" / "dsda_doom_data" / "doom2";

    launch::write(folder / "dsda-doom-savegame5.dsg", "x");
    profile().save = {.enabled = true, .file = "dsda-doom-savegame5.dsg"};

    const Strings args = Arguments::of(config);
    const auto loads = std::ranges::find(args, "-loadgame");

    ASSERT_NE(loads, args.end());
    ASSERT_NE(loads + 1, args.end());
    EXPECT_EQ(*(loads + 1), "5");
    EXPECT_EQ(std::ranges::find(args, "-save"), args.end());
    EXPECT_EQ(Storage::saveFolder(config), folder);
    EXPECT_EQ(Storage::saves(config), (Strings{"dsda-doom-savegame5.dsg"}));
    EXPECT_EQ(Storage::saveTrouble(config), "");
}

TEST_F(PortSavesTest, ANyanLaunchOnThePortsSettingsLoadsTheSaveBySlot) {
    on("/games/nyan-doom");

    launch::write(dataHome() / "nyan-doom" / "nyan_doom_data" / "doom2" / "nyan-doom-savegame12.dsg", "x");
    profile().save = {.enabled = true, .file = "nyan-doom-savegame12.dsg"};

    const Strings args = Arguments::of(config);
    const auto loads = std::ranges::find(args, "-loadgame");

    ASSERT_NE(loads, args.end());
    ASSERT_NE(loads + 1, args.end());
    EXPECT_EQ(*(loads + 1), "12");
}

TEST_F(PortSavesTest, ChocolateNamesTheFolderAfterTheMission) {
    on("/usr/bin/chocolate-doom");

    EXPECT_EQ(PortSaves::folder(config), dataHome() / "chocolate-doom" / "savegames" / "doom2.wad");

    config.iwads.front().file = launch::wad(games / "MyGame.wad", "IWAD", {"E1M1"}).string();

    EXPECT_EQ(PortSaves::folder(config), dataHome() / "chocolate-doom" / "savegames" / "doom.wad");

    config.iwads.front().file = launch::wad(games / "doom2f.wad", "IWAD", {"MAP01"}).string();

    EXPECT_EQ(PortSaves::folder(config), dataHome() / "chocolate-doom" / "savegames" / "doom2.wad");
}

TEST_F(PortSavesTest, OnlyCrispyFilesRekkrUnderDoom) {
    config.iwads.front().file = launch::wad(games / "rekkrsa.wad", "IWAD", {"E1M1"}).string();
    on("/usr/bin/crispy-doom");

    EXPECT_EQ(PortSaves::folder(config), dataHome() / "crispy-doom" / "savegames" / "doom.wad");
}

#ifdef __linux__
TEST_F(PortSavesTest, TheCrispyFlatpakListsItsSavesWithNoPortPicked) {
    on("/var/lib/flatpak/exports/bin/io.github.fabiangreffrath.Doom");

    const fs::path folder = home() / ".var" / "app" / "io.github.fabiangreffrath.Doom" / "data" / "crispy-doom"
                            / "savegames" / "doom2.wad";

    launch::write(folder / "doomsav4.dsg", "x");

    EXPECT_EQ(PortSaves::folder(config), folder);
    EXPECT_EQ(PortSaves::saves(config), (Strings{"doomsav4.dsg"}));

    config.engines.front().port = Port::Port::crispy;

    EXPECT_EQ(PortSaves::folder(config), folder);
}

TEST_F(PortSavesTest, TheGzdoomAndZandronumFlatpaksKeepSavesInTheSandbox) {
    const fs::path apps = home() / ".var" / "app";

    on("/var/lib/flatpak/exports/bin/org.zdoom.GZDoom");

    EXPECT_EQ(PortSaves::folder(config), apps / "org.zdoom.GZDoom" / ".config" / "gzdoom" / "savegames");

    on("/var/lib/flatpak/exports/bin/com.zandronum.Zandronum");

    EXPECT_EQ(PortSaves::folder(config), apps / "com.zandronum.Zandronum" / ".config" / "zandronum");
}

TEST_F(PortSavesTest, AZdoomFlatpakReadsTheIniInTheSandbox) {
    on("/var/lib/flatpak/exports/bin/com.zandronum.Zandronum");
    launch::write(home() / ".config" / "zandronum" / "zandronum.ini", "[GlobalSettings]\nsave_dir=/host\n");
    launch::write(home() / ".var" / "app" / "com.zandronum.Zandronum" / ".config" / "zandronum" / "zandronum.ini",
                  "[GlobalSettings]\nsave_dir=/sandboxed\n");

    EXPECT_EQ(PortSaves::folder(config), fs::path("/sandboxed"));
}

TEST_F(PortSavesTest, AFlatpakLooksForTheDotFolderInTheSandbox) {
    const fs::path app = home() / ".var" / "app" / "io.github.kraflab.dsda-doom";

    on("/var/lib/flatpak/exports/bin/io.github.kraflab.dsda-doom");
    fs::create_directories(home() / ".dsda-doom");

    EXPECT_EQ(PortSaves::folder(config), app / "data" / "dsda-doom" / "dsda_doom_data" / "doom2");

    fs::create_directories(app / ".dsda-doom");

    EXPECT_EQ(PortSaves::folder(config), app / ".dsda-doom" / "dsda_doom_data" / "doom2");
}
#endif

TEST_F(PortSavesTest, SkulltagSavesInATildeFolderWhereItRuns) {
    on(games / "skulltag");

    // A profile on the settings of the port still runs in its own folder.
    ASSERT_FALSE(Storage::profileDirectory(profile()).empty());

    const fs::path root = Storage::profileDirectory(profile()) / "~.skulltag";

    png(root / "save0.zds", "DOOM2.WAD");
    png(root / "save1.zds", "TNT.WAD");
    launch::write(home() / ".config" / "zandronum" / "zandronum.ini", "[GlobalSettings]\nsave_dir=/elsewhere\n");

    EXPECT_EQ(PortSaves::folder(config), root);
    EXPECT_EQ(PortSaves::saves(config), (Strings{"save0.zds"}));
}

TEST_F(PortSavesTest, ListingSkulltagSavesCreatesNoFolder) {
    on(games / "skulltag");

    const fs::path own = Storage::profileDirectory(profile());

    ASSERT_FALSE(own.empty());
    fs::remove_all(own);

    EXPECT_EQ(PortSaves::folder(config), own / "~.skulltag");
    EXPECT_TRUE(PortSaves::saves(config).empty());
    EXPECT_TRUE(Storage::saves(config).empty());
    EXPECT_FALSE(fs::exists(own));
}

TEST_F(PortSavesTest, SkulltagWithoutAProfileFolderSavesBesideItself) {
    on(games / "skulltag");
    profile().config.clear();

    ASSERT_TRUE(Storage::profileDirectory(profile()).empty());
    EXPECT_EQ(PortSaves::folder(config), games / "~.skulltag");
}

TEST_F(PortSavesTest, SkulltagBefore0_98cSavesInItsHomeDotFolder) {
    on(games / "skulltag");
    config.engines.front().version = "0.98a";

    const fs::path root = home() / ".skulltag";

    png(root / "save0.zds", "DOOM2.WAD");

    EXPECT_EQ(PortSaves::folder(config), root);
    EXPECT_EQ(PortSaves::saves(config), (Strings{"save0.zds"}));
}

TEST_F(PortSavesTest, ASaveDirInTheSkulltagIniMovesItsSaves) {
    on(games / "skulltag");
    launch::write(home() / ".skulltag" / "skulltag.ini", "[GlobalSettings]\nsave_dir=~/Saves\n");

    EXPECT_EQ(PortSaves::folder(config), home() / "Saves");
}

TEST_F(PortSavesTest, SkulltagBefore0_98cReadsNoSaveDir) {
    on(games / "skulltag");
    launch::write(home() / ".skulltag" / "skulltag.ini", "[GlobalSettings]\nsave_dir=~/Saves\n");

    for (const char *version : {"0.98a", "0.97d"}) {
        config.engines.front().version = version;

        EXPECT_EQ(PortSaves::folder(config), home() / ".skulltag") << version;
    }

    config.engines.front().version = "0.98c";

    EXPECT_EQ(PortSaves::folder(config), home() / "Saves");
}

TEST_F(PortSavesTest, SkulltagLoadsASaveByItsPath) {
    on(games / "skulltag");

    const fs::path root = Storage::profileDirectory(profile()) / "~.skulltag";

    png(root / "save3.zds", "DOOM2.WAD");
    profile().save = {.enabled = true, .file = "save3.zds"};

    const Strings args = Arguments::of(config);
    const auto loads = std::ranges::find(args, "-loadgame");

    ASSERT_NE(loads, args.end());
    ASSERT_NE(loads + 1, args.end());
    EXPECT_EQ(*(loads + 1), (root / "save3.zds").string());
}

TEST_F(PortSavesTest, VkdoomSavesInSavegamesFrom25_6_0AndInItsConfigFolderBefore) {
    on("/games/vkdoom");

    const fs::path own = home() / ".config" / "vkdoom";

    for (const auto &[version, root] : {
                 std::pair{"", own / "savegames"},
                 std::pair{"25.6.0", own / "savegames"},
                 std::pair{"0.9", own},
         }) {
        config.engines.front().version = version;

        EXPECT_EQ(PortSaves::folder(config), root) << version;
    }
}

#ifdef __linux__
TEST_F(PortSavesTest, AFlatpakExportListsTheSavesOfItsPort) {
    on("/var/lib/flatpak/exports/bin/io.github.fabiangreffrath.woof");

    EXPECT_EQ(PortSaves::folder(config),
              home() / ".var" / "app" / "io.github.fabiangreffrath.woof" / "data" / "woof" / "savegames" / "DOOM2.WAD");
}
#endif

TEST_F(PortSavesTest, AnUnknownPortOrOneUnderDosboxHasNoFolder) {
    on("/games/mystery-engine");

    EXPECT_EQ(PortSaves::folder(config), fs::path());

    on("/games/woof");
    config.engines.front().dosbox = true;

    EXPECT_EQ(PortSaves::folder(config), fs::path());

    config.engines.front().port = Port::Port::boom;

    EXPECT_EQ(PortSaves::folder(config), fs::path());
}

TEST_F(PortSavesTest, AGzdoomFileWithDsdaPickedListsTheSavesOfDsdaDoom) {
    on("/games/gzdoom");
    config.engines.front().port = Port::Port::dsda;

    const fs::path folder = dataHome() / "dsda-doom" / "dsda_doom_data" / "doom2";

    launch::write(folder / "dsda-doom-savegame3.dsg", "x");
    launch::write(folder / "doomsav2.dsg", "x");

    EXPECT_EQ(PortSaves::folder(config), folder);
    EXPECT_EQ(PortSaves::saves(config), (Strings{"dsda-doom-savegame3.dsg"}));
    EXPECT_EQ(PortSaves::loadSlot(config, Dialect::of(config), "dsda-doom-savegame3.dsg"), 3);
    EXPECT_EQ(PortSaves::loadSlot(config, Dialect::of(config), "doomsav2.dsg"), -1);
}

TEST_F(PortSavesTest, ASlotPortLabelsASaveByItsSlot) {
    on("/games/prboom-plus");

    EXPECT_EQ(PortSaves::slotLabel(config, Dialect::of(config), "prboom-plus-savegame3.dsg"), "Slot 3");
    EXPECT_EQ(PortSaves::slotLabel(config, Dialect::of(config), "quick.dsg"), "");
}

TEST_F(PortSavesTest, WoofLabelsItsAutosaveAndItsSlots) {
    on("/games/woof");

    EXPECT_EQ(PortSaves::slotLabel(config, Dialect::of(config), "autosave.dsg"), "Auto Save");
    EXPECT_EQ(PortSaves::slotLabel(config, Dialect::of(config), "woofsav3.dsg"), "Slot 3");
}

TEST_F(PortSavesTest, APortThatLoadsByNameLabelsNoSave) {
    on("/games/gzdoom");

    EXPECT_EQ(PortSaves::slotLabel(config, Dialect::of(config), "save7.zds"), "");
}

TEST_F(PortSavesTest, APickForAnUnknownNameListsTheSavesOfThePickedPort) {
    on("/games/mystery-engine");
    config.engines.front().port = Port::Port::prboomPlus;

    EXPECT_EQ(PortSaves::folder(config), dataHome() / "prboom-plus");

    config.engines.front().port = Port::Port::eternity;

    EXPECT_EQ(PortSaves::folder(config), configHome() / "eternity" / "user" / "doom2");

    config.engines.front().port = Port::Port::boom;

    EXPECT_EQ(PortSaves::folder(config), fs::path());
}

TEST_F(PortSavesTest, APickedPortListsItsOwnSaves) {
    on("/games/nugget-doom");
    config.engines.front().port = Port::Port::woof;

    EXPECT_EQ(PortSaves::folder(config), dataHome() / "woof" / "savegames" / "DOOM2.WAD");

    on("/games/woof");
    config.engines.front().port = Port::Port::cherry;

    EXPECT_EQ(PortSaves::folder(config), dataHome() / "cherry-doom" / "savegames" / "DOOM2.WAD");
}

TEST_F(PortSavesTest, NyanDoomPickedForAnUnknownNameListsNyanSaves) {
    on("/games/mystery-engine");
    config.engines.front().port = Port::Port::nyan;

    const fs::path folder = dataHome() / "nyan-doom" / "nyan_doom_data" / "doom2";

    launch::write(folder / "nyan-doom-savegame2.dsg", "x");
    launch::write(folder / "dsda-doom-savegame3.dsg", "x");

    EXPECT_EQ(PortSaves::folder(config), folder);
    EXPECT_EQ(PortSaves::saves(config), (Strings{"nyan-doom-savegame2.dsg"}));
}

TEST_F(PortSavesTest, AProfileOnThePortsSettingsListsThePortsSaves) {
    const fs::path root = home() / ".config" / "gzdoom" / "savegames";

    zds(root / "doom.id.doom2.commercial" / "save02.zds", "DOOM2.WAD");
    profile().save.file = "doom.id.doom2.commercial/save02.zds";

    EXPECT_EQ(Storage::saveFolder(config), root);
    EXPECT_EQ(Storage::saves(config), (Strings{"doom.id.doom2.commercial/save02.zds"}));
    EXPECT_EQ(Storage::saveFile(config), root / "doom.id.doom2.commercial" / "save02.zds");
    EXPECT_EQ(Storage::loadName(Storage::saveFile(config), Dialect::SaveNames::name), "save02.zds");
    EXPECT_EQ(Storage::saveTrouble(config), "");
}

TEST_F(PortSavesTest, ProfileConfigsOffListsThePortsSavesToo) {
    profile().set(Switch::sharedConfig, false);
    config.general.profileConfigs = false;

    EXPECT_EQ(Storage::saveFolder(config), home() / ".config" / "gzdoom" / "savegames");
}

TEST_F(PortSavesTest, AProfileWithItsOwnSettingsKeepsItsOwnFolder) {
    profile().set(Switch::sharedConfig, false);

    EXPECT_EQ(Storage::saveFolder(config), Storage::saveDirectory(config));
    EXPECT_NE(Storage::saveFolder(config), home() / ".config" / "gzdoom" / "savegames");
}

TEST_F(PortSavesTest, DsdaBefore0_28_3ListsOrganizedSavesUnlessTheHandedConfigTurnsThemOff) {
    on("/games/dsda-doom");
    config.engines.front().version = "0.21.2";
    profile().set(Switch::sharedConfig, false);
    profile().save = {.enabled = true, .file = "dsda-doom-savegame3.dsg"};

    const fs::path folder = home() / ".dsda-doom" / "dsda_doom_data" / "doom2";

    launch::write(folder / "dsda-doom-savegame3.dsg", "x");
    launch::write(Storage::saveDirectory(config) / "dsda-doom-savegame5.dsg", "x");

    const Strings args = Arguments::of(config);

    EXPECT_TRUE(Storage::savesFromPort(config));
    EXPECT_EQ(Storage::saveFolder(config), folder);
    EXPECT_EQ(Storage::saves(config), (Strings{"dsda-doom-savegame3.dsg"}));
    EXPECT_EQ(Storage::saveFile(config), folder / "dsda-doom-savegame3.dsg");
    EXPECT_FALSE(std::ranges::search(args, Strings{"-loadgame", "3"}).empty());
    EXPECT_FALSE(std::ranges::search(args, Strings{"-save", Storage::saveDirectory(config).string()}).empty());
    EXPECT_NE(std::ranges::find(args, "-config"), args.end());

    launch::write(Storage::configFile(config), "dsda_organized_saves 0\n");
    profile().save.file = "dsda-doom-savegame5.dsg";

    const Strings obeyed = Arguments::of(config);

    EXPECT_FALSE(Storage::savesFromPort(config));
    EXPECT_EQ(Storage::saveFolder(config), Storage::saveDirectory(config));
    EXPECT_EQ(Storage::saves(config), (Strings{"dsda-doom-savegame5.dsg"}));
    EXPECT_FALSE(std::ranges::search(obeyed, Strings{"-loadgame", "5"}).empty());

    launch::write(Storage::configFile(config), "dsda_organized_saves 1\n");
    config.engines.front().version = "0.30.0";

    EXPECT_FALSE(Storage::savesFromPort(config));
    EXPECT_EQ(Storage::saveFolder(config), Storage::saveDirectory(config));
}

TEST_F(PortSavesTest, AnOrganizedSaveIsKeptForACopy) {
    on("/games/dsda-doom");
    config.engines.front().version = "0.21.2";
    profile().set(Switch::sharedConfig, false);
    profile().save = {.enabled = true, .file = "dsda-doom-savegame3.dsg"};

    Storage::forgetOwnSave(config);

    EXPECT_EQ(profile().save, (SaveSettings{.enabled = true, .file = "dsda-doom-savegame3.dsg"}));

    config.engines.front().version = "0.30.0";
    Storage::forgetOwnSave(config);

    EXPECT_EQ(profile().save, SaveSettings{});
}

TEST_F(PortSavesTest, DsdaReadsOrganizedSavesFromTheConfigTheLaunchHandsIt) {
    on("/games/dsda-doom");
    config.engines.front().version = "0.28.0";
    profile().set(Switch::sharedConfig, false);

    const fs::path base = dataHome() / "dsda-doom";
    const fs::path organized = base / "dsda_doom_data" / "doom2";

    ASSERT_EQ(Storage::configFile(config).filename(), "dsda.cfg");

    launch::write(base / "dsda-doom.cfg", "dsda_organized_saves 0\n");

    EXPECT_EQ(PortSaves::folder(config), organized);

    launch::write(Storage::configFile(config), "dsda_organized_saves 0\n");

    EXPECT_EQ(PortSaves::folder(config), base);
    EXPECT_EQ(Storage::saveFolder(config), Storage::saveDirectory(config));

    profile().set(Switch::sharedConfig, true);
    launch::write(base / "dsda-doom.cfg", "dsda_organized_saves 1\n");

    EXPECT_EQ(PortSaves::folder(config), organized);
}

TEST_F(PortSavesTest, DsdaBefore0_28_0AlwaysSavesUnderItsHomeDotFolder) {
    on("/games/dsda-doom");

    for (const char *version : {"0.27.5", "0.21.1"}) {
        config.engines.front().version = version;

        EXPECT_EQ(PortSaves::folder(config), home() / ".dsda-doom" / "dsda_doom_data" / "doom2") << version;
    }

    config.engines.front().version = "0.28.0";

    EXPECT_EQ(PortSaves::folder(config), dataHome() / "dsda-doom" / "dsda_doom_data" / "doom2");
}

TEST_F(PortSavesTest, NyanDoomBefore1_1_8AlwaysSavesUnderItsHomeDotFolder) {
    on("/games/nyan-doom");
    config.engines.front().version = "1.1.7";

    EXPECT_EQ(PortSaves::folder(config), home() / ".nyan-doom" / "nyan_doom_data" / "doom2");

    config.engines.front().version = "1.1.8";

    EXPECT_EQ(PortSaves::folder(config), dataHome() / "nyan-doom" / "nyan_doom_data" / "doom2");
}

TEST_F(PortSavesTest, HelionBefore0_9_7_0SavesBesideItself) {
    on(games / "helion");

    for (const char *version : {"0.9.6.1", "0.9.4.0", "0.9.0.0"}) {
        config.engines.front().version = version;

        EXPECT_EQ(PortSaves::folder(config), games) << version;
    }

    config.engines.front().version = "";

    EXPECT_EQ(PortSaves::folder(config), configHome() / "Helion");

    // 0.9.7.0 and 0.9.8.0 make a lowercase folder on their first run.
    fs::create_directories(configHome() / "helion");

    for (const char *version : {"0.9.8.0", "0.9.7.0"}) {
        config.engines.front().version = version;

        EXPECT_EQ(PortSaves::folder(config), configHome() / "helion") << version;
    }
}

TEST_F(PortSavesTest, HelionWithoutASaveFolderLoadsASaveBesideItselfByName) {
    on(games / "helion");
    config.engines.front().version = "0.9.2.8";
    profile().set(Switch::sharedConfig, false);
    hsg(games / "savegame0.hsg", "doom2.wad");
    profile().save = {.enabled = true, .file = "savegame0.hsg"};

    const Strings args = Arguments::of(config);

    EXPECT_EQ(Storage::saveFolder(config), games);
    EXPECT_EQ(Storage::saves(config), (Strings{"savegame0.hsg"}));
    EXPECT_FALSE(std::ranges::search(args, Strings{"-loadgame", "savegame0.hsg"}).empty());
    EXPECT_EQ(std::ranges::find(args, "-savedir"), args.end());

    config.engines.front().version = "0.9.2.9";

    const Strings handed = Arguments::of(config);

    EXPECT_EQ(Storage::saveFolder(config), Storage::saveDirectory(config));
    EXPECT_NE(std::ranges::find(handed, "-savedir"), handed.end());
}

TEST_F(PortSavesTest, ChocolateBefore3_0_0SavesUnderItsHomeDotFolder) {
    on("/usr/bin/chocolate-doom");

    for (const char *version : {"2.3.0", "2.1.0", "2.0.0"}) {
        config.engines.front().version = version;

        EXPECT_EQ(PortSaves::folder(config), home() / ".chocolate-doom" / "savegames" / "doom2.wad") << version;
    }

    for (const char *version : {"3.0.0", ""}) {
        config.engines.front().version = version;

        EXPECT_EQ(PortSaves::folder(config), dataHome() / "chocolate-doom" / "savegames" / "doom2.wad") << version;
    }
}

TEST_F(PortSavesTest, ChocolateBefore3_0_0ListsAndLoadsTheSavesThePortKeeps) {
    on("/usr/bin/chocolate-doom");
    config.engines.front().version = "2.3.0";
    profile().set(Switch::sharedConfig, false);
    profile().save = {.enabled = true, .file = "doomsav3.dsg"};

    const fs::path folder = home() / ".chocolate-doom" / "savegames" / "doom2.wad";

    launch::write(folder / "doomsav3.dsg", "x");

    const Strings args = Arguments::of(config);

    EXPECT_TRUE(Storage::savesFromPort(config));
    EXPECT_EQ(Storage::saveFolder(config), folder);
    EXPECT_EQ(Storage::saves(config), (Strings{"doomsav3.dsg"}));
    EXPECT_FALSE(std::ranges::search(args, Strings{"-loadgame", "3"}).empty());
    EXPECT_EQ(std::ranges::find(args, "-savedir"), args.end());
    EXPECT_NE(std::ranges::find(args, "-config"), args.end());

    config.engines.front().version = "3.0.0";

    const Strings handed = Arguments::of(config);

    EXPECT_FALSE(Storage::savesFromPort(config));
    EXPECT_EQ(Storage::saveFolder(config), Storage::saveDirectory(config));
    EXPECT_NE(std::ranges::find(handed, "-savedir"), handed.end());
}

TEST_F(PortSavesTest, FreedoomSavesInTheMissionFolderUntilTheReleaseSeparatesThem) {
    using Case = std::tuple<const char *, const char *, const char *, const char *>;

    for (const auto &[port, version, iwad, folder] : {
                 Case{"chocolate-doom", "", "freedoom1.wad", "freedoom1.wad"},
                 Case{"chocolate-doom", "3.1.0", "freedoom1.wad", "freedoom1.wad"},
                 Case{"chocolate-doom", "3.1.0", "freedoom2.wad", "freedoom2.wad"},
                 Case{"chocolate-doom", "3.1.0", "freedm.wad", "freedm.wad"},
                 Case{"chocolate-doom", "3.0.1", "freedoom1.wad", "doom.wad"},
                 Case{"chocolate-doom", "3.0.1", "freedoom2.wad", "doom2.wad"},
                 Case{"chocolate-doom", "3.0.1", "freedm.wad", "doom2.wad"},
                 Case{"crispy-doom", "", "freedm.wad", "freedm.wad"},
                 Case{"crispy-doom", "5.9.0", "freedoom1.wad", "freedoom1.wad"},
                 Case{"crispy-doom", "5.9.0", "freedoom2.wad", "freedoom2.wad"},
                 Case{"crispy-doom", "5.8.0", "freedoom1.wad", "doom.wad"},
                 Case{"crispy-doom", "5.8.0", "freedoom2.wad", "doom2.wad"},
                 Case{"crispy-doom", "5.8.0", "freedm.wad", "doom2.wad"},
         }) {
        const Strings marks =
                std::string_view(iwad) == "freedm.wad" ? Strings{"FREEDOOM", "FREEDM"} : Strings{"FREEDOOM"};

        on(fs::path("/usr/bin") / port);
        config.engines.front().version = version;
        config.iwads.front().file = launch::wad(games / iwad, "IWAD", {"MAP01"}, marks).string();

        EXPECT_EQ(PortSaves::folder(config), dataHome() / port / "savegames" / folder)
                << port << " " << version << " " << iwad;
    }
}

TEST_F(PortSavesTest, FreedoomOnAnOlderChocolateOrCrispySavesWithTheMissionUnderTheHomeDotFolder) {
    using Case = std::tuple<const char *, const char *, const char *, const char *>;

    for (const auto &[port, version, iwad, folder] : {
                 Case{"chocolate-doom", "2.3.0", "freedoom2.wad", "doom2.wad"},
                 Case{"chocolate-doom", "2.0.0", "freedoom1.wad", "doom.wad"},
                 Case{"crispy-doom", "4.3", "freedoom2.wad", "doom2.wad"},
                 Case{"crispy-doom", "1.0", "freedm.wad", "doom2.wad"},
         }) {
        on(fs::path("/usr/bin") / port);
        config.engines.front().version = version;
        config.iwads.front().file = launch::wad(games / iwad, "IWAD", {"MAP01"}).string();

        EXPECT_EQ(PortSaves::folder(config), home() / (std::string(".") + port) / "savegames" / folder)
                << port << " " << version << " " << iwad;
    }
}

TEST_F(PortSavesTest, CrispyBefore5_0SavesUnderItsHomeDotFolder) {
    on("/usr/bin/crispy-doom");

    for (const char *version : {"4.3", "4.0", "3.5", "1.0"}) {
        config.engines.front().version = version;

        EXPECT_EQ(PortSaves::folder(config), home() / ".crispy-doom" / "savegames" / "doom2.wad") << version;
    }

    for (const char *version : {"5.0", ""}) {
        config.engines.front().version = version;

        EXPECT_EQ(PortSaves::folder(config), dataHome() / "crispy-doom" / "savegames" / "doom2.wad") << version;
    }
}

TEST_F(PortSavesTest, CrispyBefore4_0ListsAndLoadsTheSavesThePortKeeps) {
    on("/usr/bin/crispy-doom");
    config.engines.front().version = "3.5";
    profile().set(Switch::sharedConfig, false);
    profile().save = {.enabled = true, .file = "doomsav3.dsg"};

    const fs::path folder = home() / ".crispy-doom" / "savegames" / "doom2.wad";

    launch::write(folder / "doomsav3.dsg", "x");

    const Strings args = Arguments::of(config);

    EXPECT_TRUE(Storage::savesFromPort(config));
    EXPECT_EQ(Storage::saveFolder(config), folder);
    EXPECT_EQ(Storage::saves(config), (Strings{"doomsav3.dsg"}));
    EXPECT_FALSE(std::ranges::search(args, Strings{"-loadgame", "3"}).empty());
    EXPECT_EQ(std::ranges::find(args, "-savedir"), args.end());

    config.engines.front().version = "4.0";

    const Strings handed = Arguments::of(config);

    EXPECT_FALSE(Storage::savesFromPort(config));
    EXPECT_EQ(Storage::saveFolder(config), Storage::saveDirectory(config));
    EXPECT_NE(std::ranges::find(handed, "-savedir"), handed.end());
}

namespace {

void makeEternityUser(const fs::path &folder) {
    fs::create_directories(folder / "doom");
    fs::create_directories(folder / "shots");
}

void makeEternityBase(const fs::path &folder) {
    fs::create_directories(folder / "doom");
    launch::write(folder / "startup.wad", "x");
    launch::write(folder / "root.edf", "x");
}

// Two changes to a folder can share one tick of the clock, and the folder listing is kept until the folder changes.
void touched(const fs::path &folder) {
    fs::last_write_time(folder, fs::last_write_time(folder) + std::chrono::seconds(1));
}

}

TEST_F(PortSavesTest, EternitySavesInTheGameFolderOfItsUserFolder) {
    on(games / "eternity");

    EXPECT_EQ(PortSaves::folder(config), configHome() / "eternity" / "user" / "doom2");

    config.iwads.front().file.clear();

    EXPECT_EQ(PortSaves::folder(config), fs::path());
}

TEST_F(PortSavesTest, EternityNamesTheGameFolderFromTheLumpsOfTheIwad) {
    on(games / "eternity");

    const fs::path user = configHome() / "eternity" / "user";
    const auto gameOf = [this](const char *file, const Strings &lumps) {
        config.iwads.front().file = launch::wad(games / file, "IWAD", lumps).string();

        return PortSaves::folder(config).filename().string();
    };

    EXPECT_EQ(gameOf("doom.wad", {"E1M1"}), "doom");
    EXPECT_EQ(gameOf("tnt.wad", {"MAP01"}), "doom2");
    EXPECT_EQ(gameOf("evilution.wad", {"MAP01", "CAVERN1", "CAVERN2", "CAVERN3", "CAVERN4"}), "tnt");
    EXPECT_EQ(gameOf("experiment.wad", {"MAP01", "MC1", "MC2", "MC3", "MC4", "MC5", "MC6", "MC7", "MC8"}), "plutonia");
    EXPECT_EQ(gameOf("twitch.wad", {"MAP01", "HACX-R"}), "hacx");
    EXPECT_EQ(gameOf("free.wad", {"MAP01", "FREEDOOM", "CAVERN1", "CAVERN2", "CAVERN3", "CAVERN4"}), "doom2");
    EXPECT_EQ(gameOf("raven.wad", {"E1M1", "ADVISOR", "TINTTAB", "SNDCURVE"}), "heretic");
    EXPECT_EQ(gameOf("mixed.wad", {"E2M1", "MAP01"}), "doom");
    EXPECT_EQ(PortSaves::folder(config).parent_path(), user);
}

TEST_F(PortSavesTest, EternityNeedsTheWholeCountOfEachKindOfLump) {
    on(games / "eternity");

    const auto gameOf = [this](const char *file, const Strings &lumps) {
        config.iwads.front().file = launch::wad(games / file, "IWAD", lumps).string();

        return PortSaves::folder(config).filename().string();
    };
    Strings caverns = {"MAP01", "CAVERN1", "CAVERN2", "CAVERN3"};
    Strings mcs = {"MAP01", "MC1", "MC2", "MC3", "MC4", "MC5", "MC6", "MC7"};
    Strings bfg = {"MAP01", "CAVERN1", "CAVERN2", "CAVERN3", "CAVERN4", "M_ACPT", "M_CAN", "M_EXITO", "M_CHG"};

    EXPECT_EQ(gameOf("caverns3.wad", caverns), "doom2");
    EXPECT_EQ(gameOf("mc7.wad", mcs), "doom2");
    EXPECT_EQ(gameOf("bfg4.wad", bfg), "tnt");

    caverns.emplace_back("CAV");
    mcs.emplace_back("MC");
    bfg.emplace_back("DMENUPIC");

    EXPECT_EQ(gameOf("caverns4.wad", caverns), "tnt");
    EXPECT_EQ(gameOf("mc8.wad", mcs), "plutonia");
    EXPECT_EQ(gameOf("bfg5.wad", bfg), "doom2");
    EXPECT_EQ(gameOf("raven4.wad", {"E1M1", "ADVISOR", "ADVISOR", "TINTTAB", "SNDCURVE"}), "doom");
}

TEST_F(PortSavesTest, EternityTakesThirtyMapsAsDoomTwoEvenBesideEpisodeMaps) {
    on(games / "eternity");

    Strings lumps = {"E2M1"};

    for (int map = 1; map < 30; ++map) {
        lumps.push_back(std::string(map < 10 ? "MAP0" : "MAP") + std::to_string(map));
    }

    config.iwads.front().file = launch::wad(games / "twentynine.wad", "IWAD", lumps).string();

    EXPECT_EQ(PortSaves::folder(config).filename(), "doom");

    lumps.emplace_back("MAP30");
    config.iwads.front().file = launch::wad(games / "thirty.wad", "IWAD", lumps).string();

    EXPECT_EQ(PortSaves::folder(config).filename(), "doom2");
}

TEST_F(PortSavesTest, EternityMatchesTheStartOfTheOtherLumpNamesAndTheirCase) {
    on(games / "eternity");

    const auto gameOf = [this](const char *file, const Strings &lumps) {
        config.iwads.front().file = launch::wad(games / file, "IWAD", lumps).string();

        return PortSaves::folder(config).filename().string();
    };

    EXPECT_EQ(gameOf("raven-longer.wad", {"E1M1", "ADVISOR1", "TINTTAB2", "SNDCURVE"}), "heretic");
    EXPECT_EQ(gameOf("hacx-longer.wad", {"MAP01", "HACX-R2"}), "hacx");
    EXPECT_EQ(gameOf("bfg-longer.wad",
                     {"MAP01",
                      "CAVERN1",
                      "CAVERN2",
                      "CAVERN3",
                      "CAVERN4",
                      "M_ACPTX",
                      "M_CANCEL",
                      "M_EXITO2",
                      "M_CHGX",
                      "DMENUPIC"}),
              "doom2");
    EXPECT_EQ(gameOf("lower.wad", {"map01"}), "doom");
    EXPECT_EQ(gameOf("caverns.wad", {"MAP01", "cavern1", "cavern2", "cavern3", "cavern4"}), "doom2");
}

TEST_F(PortSavesTest, EternityReadsTheGameOfAnArchiveFromItsGameVersion) {
    on(games / "eternity");

    const auto gameOf = [this](const char *file, const std::string &named) {
        launch::write(games / file, ports::zip({{.name = "gameversion.txt", .bytes = named}}));
        config.iwads.front().file = (games / file).string();

        return PortSaves::folder(config).filename().string();
    };

    EXPECT_EQ(gameOf("tnt.pke", "doom2 tnt"), "tnt");
    EXPECT_EQ(gameOf("plutonia.pke", "DOOM2 Plutonia"), "plutonia");
    EXPECT_EQ(gameOf("bfg.pke", "doom2 bfg"), "doom2");
    EXPECT_EQ(gameOf("heretic.pke", "heretic sosr"), "heretic");
    EXPECT_EQ(gameOf("other.pke", "doom3"), "doom");
}

TEST_F(PortSavesTest, EternityReadsTheLumpsTheHeaderCountsUntilTheFileEnds) {
    on(games / "eternity");

    const auto gameOf = [this](const char *file, const std::uint32_t count, const std::uint32_t table) {
        const fs::path path = launch::wad(games / file, "IWAD", {"MAP01", "CAVERN1", "CAVERN2", "CAVERN3", "CAVERN4"});
        std::fstream header(path, std::ios::binary | std::ios::in | std::ios::out);

        header.seekp(4);

        for (const std::uint32_t value : {count, table}) {
            for (unsigned shift = 0; shift < 32; shift += 8) {
                header.put(static_cast<char>((value >> shift) & 0xFFU));
            }
        }

        header.close();
        config.iwads.front().file = path.string();

        return PortSaves::folder(config).filename().string();
    };

    EXPECT_EQ(gameOf("all.wad", 10, 12), "tnt");
    EXPECT_EQ(gameOf("fewer.wad", 6, 12), "doom2");
    EXPECT_EQ(gameOf("past.wad", 0xFFFFFFFFU, 12), "tnt");
    EXPECT_EQ(gameOf("beyond.wad", 10, 4096), "doom");
}

TEST_F(PortSavesTest, EternityListsNoSavesForAnIwadThatIsNoWad) {
    on(games / "eternity");
    launch::write(games / "notes.wad", "these are notes");
    config.iwads.front().file = (games / "notes.wad").string();

    EXPECT_EQ(PortSaves::folder(config), fs::path());
}

TEST_F(PortSavesTest, EternityTakesTheFirstUserFolderItAccepts) {
    on(games / "eternity");

    const fs::path own = configHome() / "eternity" / "user";

    fs::create_directories(own / "doom");
    makeEternityUser(games / "user");

    EXPECT_EQ(PortSaves::folder(config), games / "user" / "doom2");

    fs::create_directories(own / "shots");
    touched(own);

    EXPECT_EQ(PortSaves::folder(config), own / "doom2");

    makeEternityUser(games / "elsewhere");
    ttk::Env::set("ETERNITYUSER", (games / "elsewhere").c_str());

    EXPECT_EQ(PortSaves::folder(config), games / "elsewhere" / "doom2");
}

TEST_F(PortSavesTest, EternityPassesOverAUserFolderItRefuses) {
    on(games / "eternity");

    const fs::path own = configHome() / "eternity" / "user";

    fs::create_directories(games / "refused" / "doom");
    ttk::Env::set("ETERNITYUSER", (games / "refused").c_str());

    EXPECT_EQ(PortSaves::folder(config), own / "doom2");
}

TEST_F(PortSavesTest, EternityTakesTheNamesInTheUserAndBaseFoldersInAnyCase) {
    on(games / "eternity");
    config.engines.front().version = "3.40.20";

    const fs::path base = games / "base";

    fs::create_directories(base / "DOOM");
    launch::write(base / "STARTUP.WAD", "x");
    launch::write(base / "Root.Edf", "x");

    EXPECT_EQ(PortSaves::folder(config), base / "doom2");

    config.engines.front().version = "3.40.25";
    fs::create_directories(games / "user" / "Doom");
    fs::create_directories(games / "user" / "SHOTS");

    EXPECT_EQ(PortSaves::folder(config), games / "user" / "doom2");
}

TEST_F(PortSavesTest, EternityFindsItsBaseFolderInTheEnvironmentThenBesideItThenWhereItRuns) {
    on(games / "eternity");
    config.engines.front().version = "3.40.20";

    const fs::path run = Storage::profileDirectory(profile());

    ASSERT_FALSE(run.empty());
    EXPECT_EQ(PortSaves::folder(config), fs::path());

    makeEternityBase(run / "base");

    EXPECT_EQ(PortSaves::folder(config), run / "base" / "doom2");

    makeEternityBase(games / "base");

    EXPECT_EQ(PortSaves::folder(config), games / "base" / "doom2");

    makeEternityBase(games / "set");
    ttk::Env::set("ETERNITYBASE", (games / "set").c_str());

    EXPECT_EQ(PortSaves::folder(config), games / "set" / "doom2");

    fs::create_directories(games / "refused" / "doom");
    ttk::Env::set("ETERNITYBASE", (games / "refused").c_str());

    EXPECT_EQ(PortSaves::folder(config), games / "base" / "doom2");
}

TEST_F(PortSavesTest, EternityTakesTheUserFolderBesideItsBaseFolderAndElseTheBaseFolder) {
    on(games / "eternity");
    config.engines.front().version = "3.40.37";

    const fs::path data = games / "data";

    makeEternityBase(data / "base");
    ttk::Env::set("ETERNITYBASE", (data / "base").c_str());

    EXPECT_EQ(PortSaves::folder(config), data / "base" / "doom2");

    makeEternityUser(data / "user");

    EXPECT_EQ(PortSaves::folder(config), data / "base" / ".." / "user" / "doom2");

    makeEternityUser(Storage::profileDirectory(profile()) / "user");

    EXPECT_EQ(PortSaves::folder(config), Storage::profileDirectory(profile()) / "user" / "doom2");
}

TEST_F(PortSavesTest, EternityReadsARelativeIwadBesideThePort) {
    on(games / "eternity");
    launch::wad(games / "evilution.wad", "IWAD", {"MAP01", "CAVERN1", "CAVERN2", "CAVERN3", "CAVERN4"});
    config.iwads.front().file = "evilution.wad";

    EXPECT_EQ(PortSaves::folder(config), configHome() / "eternity" / "user" / "tnt");
}

TEST_F(PortSavesTest, EternityTakesTheUserFolderBesideTheTargetOfALinkedBaseFolder) {
    on(games / "eternity");
    config.engines.front().version = "3.40.37";

    const fs::path real = games / "real";
    const fs::path linked = games / "linked" / "base";

    makeEternityBase(real / "base");
    makeEternityUser(real / "user");
    fs::create_directories(linked.parent_path());
    fs::create_directory_symlink(real / "base", linked);
    ttk::Env::set("ETERNITYBASE", linked.c_str());

    const fs::path folder = PortSaves::folder(config);

    EXPECT_EQ(folder, linked / ".." / "user" / "doom2");
    EXPECT_TRUE(fs::equivalent(folder.parent_path(), real / "user"));
}

TEST_F(PortSavesTest, EternityBefore3_40_46KeepsNoUserFolderInTheHome) {
    on(games / "eternity");
    makeEternityBase(games / "base");

    config.engines.front().version = "3.40.37";

    EXPECT_EQ(PortSaves::folder(config), games / "base" / "doom2");

    makeEternityUser(games / "user");

    for (const char *version : {"3.40.37", "3.40.25"}) {
        config.engines.front().version = version;

        EXPECT_EQ(PortSaves::folder(config), games / "user" / "doom2") << version;
    }

    config.engines.front().version = "3.40.46";

    EXPECT_EQ(PortSaves::folder(config), configHome() / "eternity" / "user" / "doom2");
}

TEST_F(PortSavesTest, EternityBefore3_40_25SavesInItsBaseFolder) {
    on(games / "eternity");
    makeEternityBase(games / "base");
    makeEternityUser(games / "user");

    for (const char *version : {"3.40.20", "3.40.00"}) {
        config.engines.front().version = version;

        EXPECT_EQ(PortSaves::folder(config), games / "base" / "doom2") << version;
    }
}

#ifdef __linux__
TEST_F(PortSavesTest, AnEternityFlatpakReadsTheBaseFolderItShips) {
    const tests::FlatpakApp app =
            tests::deployFlatpak(tests::Sandbox::scratch("eternity-flatpak") / "flatpak", "org.example.Eternity");
    const fs::path base = app.commit / "files" / "share" / "eternity" / "base";
    const fs::path own = home() / ".var" / "app" / "org.example.Eternity" / "config" / "eternity" / "user";

    on(app.exported);
    config.engines.front().version = "4.06.00";

    EXPECT_EQ(PortSaves::folder(config), own / "doom2");

    fs::create_directories(own / "doom");
    makeEternityBase(base);

    EXPECT_EQ(PortSaves::folder(config), base / "doom2");

    config.engines.front().version = "3.40.37";

    EXPECT_EQ(PortSaves::folder(config), fs::path());
}
#endif

TEST_F(PortSavesTest, EternityListsAndLoadsItsSlots) {
    on(games / "eternity");

    const fs::path user = configHome() / "eternity" / "user";
    const fs::path folder = user / "doom2";

    makeEternityUser(user);
    launch::write(folder / "etersav0.dsg", "x");
    launch::write(folder / "etersav12.dsg", "x");
    launch::write(folder / "doomsav1.dsg", "x");
    launch::write(folder / "eternity.cfg", "x");
    aged({folder / "etersav0.dsg", folder / "etersav12.dsg"});
    profile().save = {.enabled = true, .file = "etersav12.dsg"};

    const Strings args = Arguments::of(config);

    EXPECT_EQ(Storage::saveFolder(config), folder);
    EXPECT_EQ(Storage::saves(config), (Strings{"etersav12.dsg", "etersav0.dsg"}));
    EXPECT_EQ(PortSaves::slotLabel(config, Dialect::of(config), "etersav12.dsg"), "Slot 12");
    EXPECT_EQ(PortSaves::loadSlot(config, Dialect::of(config), "doomsav1.dsg"), -1);
    EXPECT_FALSE(std::ranges::search(args, Strings{"-loadgame", "12"}).empty());
    EXPECT_EQ(std::ranges::find(args, "-save"), args.end());

    profile().set(Switch::sharedConfig, false);

    const Strings handed = Arguments::of(config);

    EXPECT_EQ(Storage::saveFolder(config), Storage::saveDirectory(config));
    EXPECT_FALSE(std::ranges::search(handed, Strings{"-save", Storage::saveDirectory(config).string()}).empty());
}

namespace {

struct OtherGameFolder {
    const char *file;
    const char *package;
    const char *folder;
};

constexpr std::array OTHER_GAME_FOLDERS = std::to_array<OtherGameFolder>({
        {.file = "chocolate-heretic", .package = "chocolate-doom", .folder = "heretic.wad"},
        {.file = "chocolate-hexen", .package = "chocolate-doom", .folder = "hexen.wad"},
        {.file = "chocolate-strife", .package = "chocolate-doom", .folder = "strife1.wad"},
        {.file = "crispy-heretic", .package = "crispy-doom", .folder = "heretic.wad"},
        {.file = "crispy-hexen", .package = "crispy-doom", .folder = "hexen.wad"},
        {.file = "crispy-strife", .package = "crispy-doom", .folder = "strife1.wad"},
});

bool sends(const Config &config, const Strings &run) {
    const Strings args = Arguments::of(config);

    return !std::ranges::search(args, run).empty();
}

// The level the port lists under this -gameversion word.
int gameVersion(const std::string_view word) {
    for (const int level : Dialect::complevels(Dialect::Complevels::gameVersion)) {
        if (Dialect::complevelWord(Dialect::Complevels::gameVersion, level) == word) {
            return level;
        }
    }

    return -1;
}

}

TEST_F(PortSavesTest, HereticHexenAndStrifeSaveInAFolderNamedForTheGameWhateverTheIwad) {
    for (const OtherGameFolder &game : OTHER_GAME_FOLDERS) {
        on(fs::path("/usr/bin") / game.file);

        EXPECT_EQ(PortSaves::folder(config), dataHome() / game.package / "savegames" / game.folder) << game.file;
    }

    config.iwads.clear();
    on("/usr/bin/crispy-hexen");

    EXPECT_EQ(PortSaves::folder(config), dataHome() / "crispy-doom" / "savegames" / "hexen.wad") << "no IWAD";
}

TEST_F(PortSavesTest, OlderHereticHexenAndStrifeReleasesSaveUnderTheHomeDotFolder) {
    using Case = std::tuple<const char *, const char *, const char *>;

    for (const auto &[file, version, folder] : {
                 Case{"chocolate-heretic", "2.3.0", ".chocolate-doom/savegames/heretic.wad"},
                 Case{"chocolate-strife", "2.0.0", ".chocolate-doom/savegames/strife1.wad"},
                 Case{"crispy-hexen", "4.3", ".crispy-doom/savegames/hexen.wad"},
                 Case{"crispy-strife", "1.0", ".crispy-doom/savegames/strife1.wad"},
         }) {
        on(fs::path("/usr/bin") / file);
        config.engines.front().version = version;

        EXPECT_EQ(PortSaves::folder(config), home() / folder) << file << " " << version;
    }
}

TEST_F(PortSavesTest, HereticListsOnlyTheSlotsItsOneDigitLoadgameReaches) {
    on("/usr/bin/crispy-heretic");

    const fs::path folder = dataHome() / "crispy-doom" / "savegames" / "heretic.wad";
    const std::vector<fs::path> files = {
            folder / "hticsav0.hsg",
            folder / "hticsav10.hsg",
            folder / "hticsav47.hsg",
            folder / "hticsav9.hsg",
            folder / "doomsav1.dsg",
    };

    for (const fs::path &file : files) {
        launch::write(file, "x");
    }

    aged(files);
    profile().save = {.enabled = true, .file = "hticsav9.hsg"};

    const Dialect::Traits traits = Dialect::of(config);

    EXPECT_EQ(traits.lastSlot, 9);
    EXPECT_EQ(Storage::saves(config), (Strings{"hticsav9.hsg", "hticsav0.hsg"}));
    EXPECT_EQ(PortSaves::slotLabel(config, traits, "hticsav9.hsg"), "Slot 9");
    EXPECT_EQ(PortSaves::loadSlot(config, traits, "doomsav1.dsg"), -1);
    EXPECT_TRUE(sends(config, Strings{"-loadgame", "9"}));

    profile().save.file = "hticsav10.hsg";

    const Strings past = Arguments::of(config);

    EXPECT_EQ(Storage::saveTrouble(config), "This port loads slots up to 9, so it cannot load hticsav10.hsg.");
    EXPECT_EQ(std::ranges::find(past, "-loadgame"), past.end());
}

TEST_F(PortSavesTest, HexenListsTheMainFileOfASlotAndNeitherMapFilesNorTheSlotsInPlay) {
    on("/usr/bin/crispy-hexen");

    const fs::path folder = dataHome() / "crispy-doom" / "savegames" / "hexen.wad";
    const std::vector<fs::path> files = {
            folder / "hex0.hxs",
            folder / "hex001.hxs",
            folder / "hex5.hxs",
            folder / "hex6.hxs",
            folder / "hex601.hxs",
            folder / "hex7.hxs",
            folder / "hex12.hxs",
            folder / "hex1203.hxs",
            folder / "hex75.hxs",
    };

    for (const fs::path &file : files) {
        launch::write(file, "x");
    }

    aged(files);
    profile().save = {.enabled = true, .file = "hex75.hxs"};

    const Dialect::Traits traits = Dialect::of(config);

    EXPECT_EQ(Storage::saves(config), (Strings{"hex75.hxs", "hex12.hxs", "hex5.hxs", "hex0.hxs"}));

    for (const char *name : {"hex6.hxs", "hex7.hxs", "hex601.hxs", "hex1203.hxs"}) {
        EXPECT_EQ(PortSaves::loadSlot(config, traits, name), -1) << name;
    }

    EXPECT_TRUE(sends(config, Strings{"-loadgame", "75"}));
}

TEST_F(PortSavesTest, StrifeListsTheSlotFoldersThatHoldAName) {
    on("/usr/bin/chocolate-strife");

    const fs::path folder = dataHome() / "chocolate-doom" / "savegames" / "strife1.wad";

    launch::write(folder / "strfsav0.ssg" / "name", std::string(32, 'a'));
    launch::write(folder / "strfsav3.ssg" / "name", std::string(32, 'b'));
    launch::write(folder / "strfsav6.ssg" / "name", std::string(32, 'c'));
    launch::write(folder / "strfsav5.ssg" / "name", std::string(32, 'd'));
    launch::write(folder / "strfsav7.ssg" / "name", std::string(32, 'e'));
    launch::write(folder / "strfsav2.ssg" / "name", std::string(31, 'f'));
    launch::write(folder / "strfsav1.ssg" / "here", "x");
    launch::write(folder / "strfsav4.ssg", "a file, not a slot");
    aged({
            folder / "strfsav0.ssg",
            folder / "strfsav3.ssg",
            folder / "strfsav6.ssg",
            folder / "strfsav5.ssg",
            folder / "strfsav7.ssg",
            folder / "strfsav2.ssg",
            folder / "strfsav1.ssg",
    });
    profile().save = {.enabled = true, .file = "strfsav3.ssg"};

    EXPECT_EQ(Dialect::of(config).lastSlot, 5);
    EXPECT_EQ(Storage::saves(config), (Strings{"strfsav5.ssg", "strfsav3.ssg", "strfsav0.ssg"}));
    EXPECT_EQ(PortSaves::loadSlot(config, Dialect::of(config), "strfsav6.ssg"), -1);
    EXPECT_EQ(Storage::saveTrouble(config), "");
    EXPECT_TRUE(sends(config, Strings{"-loadgame", "3"}));

    for (const char *nameless : {"strfsav1.ssg", "strfsav2.ssg"}) {
        profile().save.file = nameless;

        const Strings args = Arguments::of(config);

        EXPECT_EQ(Storage::saveTrouble(config), "There is no save at " + (folder / nameless).string() + " any more.");
        EXPECT_EQ(std::ranges::find(args, "-loadgame"), args.end()) << nameless;
    }

    profile().save.file = "strfsav7.ssg";

    EXPECT_EQ(Storage::saveTrouble(config), "This port loads slots up to 5, so it cannot load strfsav7.ssg.");
}

TEST_F(PortSavesTest, HexenAndStrifeHandedASaveFolderSaveStraightIntoIt) {
    profile().set(Switch::sharedConfig, false);
    on("/usr/bin/chocolate-hexen");

    const fs::path saves = Storage::saveDirectory(config);
    launch::write(saves / "hex2.hxs", "x");
    launch::write(saves / "hex201.hxs", "x");
    launch::write(saves / "hex6.hxs", "x");
    profile().save = {.enabled = true, .file = "hex2.hxs"};

    const Strings hexen = Arguments::of(config);

    EXPECT_FALSE(Storage::savesFromPort(config));
    EXPECT_EQ(Storage::saves(config), (Strings{"hex2.hxs"}));
    EXPECT_FALSE(std::ranges::search(hexen, Strings{"-savedir", saves.string()}).empty());
    EXPECT_FALSE(std::ranges::search(hexen, Strings{"-loadgame", "2"}).empty());

    on("/usr/bin/crispy-strife");
    launch::write(saves / "strfsav2.ssg" / "name", std::string(32, 'a'));
    launch::write(saves / "strfsav5.ssg" / "keys", "x");
    profile().save = {.enabled = true, .file = "strfsav2.ssg"};

    EXPECT_EQ(Storage::saves(config), (Strings{"strfsav2.ssg"}));
    EXPECT_TRUE(sends(config, Strings{"-loadgame", "2"}));
}

TEST_F(PortSavesTest, HereticHandedASaveFolderListsOnlyTheSlotsItsLoadgameReaches) {
    profile().set(Switch::sharedConfig, false);
    on("/usr/bin/chocolate-heretic");

    const fs::path saves = Storage::saveDirectory(config);
    const std::vector<fs::path> files = {
            saves / "hticsav3.hsg",
            saves / "hticsav12.hsg",
            saves / "hticsav9.hsg",
            saves / "doomsav1.dsg",
            saves / "hticsav5.dsg",
    };

    for (const fs::path &file : files) {
        launch::write(file, "x");
    }

    aged(files);
    profile().save = {.enabled = true, .file = "hticsav3.hsg"};

    const Strings args = Arguments::of(config);

    EXPECT_FALSE(Storage::savesFromPort(config));
    EXPECT_EQ(Storage::saves(config), (Strings{"hticsav9.hsg", "hticsav3.hsg"}));
    EXPECT_FALSE(std::ranges::search(args, Strings{"-savedir", saves.string()}).empty());
    EXPECT_FALSE(std::ranges::search(args, Strings{"-loadgame", "3"}).empty());
}

TEST_F(PortSavesTest, WoofLeavesOutASavePastItsLastSlotAndKeepsTheAutosave) {
    on("/games/woof");

    const Dialect::Traits traits = Dialect::of(config);
    const std::string past = "woofsav" + std::to_string(traits.lastSlot + 1) + ".dsg";
    const std::string last = "woofsav" + std::to_string(traits.lastSlot) + ".dsg";
    const auto put = [&past, &last](const fs::path &folder) {
        const std::vector<fs::path> files = {folder / last, folder / past, folder / "autosave.dsg"};

        for (const fs::path &file : files) {
            launch::write(file, "x");
        }

        aged(files);
    };

    ASSERT_LT(traits.lastSlot, Dialect::Traits{}.lastSlot);
    put(dataHome() / "woof" / "savegames" / "DOOM2.WAD");

    EXPECT_TRUE(Storage::savesFromPort(config));
    EXPECT_EQ(Storage::saves(config), (Strings{"autosave.dsg", last}));

    profile().set(Switch::sharedConfig, false);
    put(Storage::saveDirectory(config));

    EXPECT_FALSE(Storage::savesFromPort(config));
    EXPECT_EQ(Storage::saves(config), (Strings{"autosave.dsg", last}));
}

TEST_F(PortSavesTest, FreedoomIsToldByTheLumpsOfTheIwadAndTheMission) {
    using Case = std::tuple<const char *, Strings, Strings, const char *>;

    on("/usr/bin/chocolate-doom");

    for (const auto &[iwad, maps, marks, folder] : {
                 Case{"freedoom1.wad", {"E1M1"}, {"FREEDOOM"}, "freedoom1.wad"},
                 Case{"freedoom2.wad", {"MAP01"}, {"FREEDOOM"}, "freedoom2.wad"},
                 Case{"freedm.wad", {"MAP01"}, {"FREEDOOM", "FREEDM"}, "freedm.wad"},
                 Case{"phase1.wad", {"E1M1"}, {"FREEDOOM"}, "freedoom1.wad"},
                 Case{"phase2.wad", {"MAP01"}, {"FREEDOOM"}, "freedoom2.wad"},
                 Case{"deathmatch.wad", {"MAP01"}, {"FREEDOOM", "FREEDM"}, "freedm.wad"},
                 Case{"doom2.wad", {"MAP01"}, {"FREEDOOM"}, "freedoom2.wad"},
                 Case{"doom2.wad", {"MAP01"}, {"FREEDM"}, "doom2.wad"},
                 Case{"freedm.wad", {"MAP01"}, {"FREEDOOM"}, "freedoom2.wad"},
                 Case{"episodes.wad", {"E1M1"}, {"FREEDOOM", "FREEDM"}, "doom.wad"},
                 Case{"plutonia.wad", {"MAP01"}, {"FREEDOOM"}, "plutonia.wad"},
                 Case{"bare2.wad", {"MAP01"}, {}, "doom2.wad"},
                 Case{"freedoom2.wad", {"MAP01"}, {}, "doom2.wad"},
                 Case{"mapsfirst.wad", {"MAP01", "E1M1"}, {}, "doom2.wad"},
                 Case{"episodefirst.wad", {"E1M1", "MAP01"}, {}, "doom.wad"},
                 Case{"nomaps.wad", {}, {"FREEDOOM"}, "unknown.wad"},
         }) {
        const fs::path named = games / (std::string(folder) + "-" + iwad) / iwad;

        config.iwads.front().file = launch::wad(named, "IWAD", maps, marks).string();

        EXPECT_EQ(PortSaves::folder(config), dataHome() / "chocolate-doom" / "savegames" / folder) << iwad;
    }
}

TEST_F(PortSavesTest, AnIwadRewrittenWithOtherLumpsMovesTheFolder) {
    on("/usr/bin/chocolate-doom");

    const fs::path iwad = launch::wad(games / "rewritten" / "phase.wad", "IWAD", {"MAP01"}, {"FREEDOOM"});
    const fs::file_time_type written = fs::last_write_time(iwad);
    const auto size = fs::file_size(iwad);

    config.iwads.front().file = iwad.string();

    EXPECT_EQ(PortSaves::folder(config), dataHome() / "chocolate-doom" / "savegames" / "freedoom2.wad");

    // Same size, so only the time tells the change.
    launch::wad(iwad, "IWAD", {"E1M1"}, {"FREEDOOM"});
    fs::last_write_time(iwad, written + std::chrono::seconds(2));

    ASSERT_EQ(fs::file_size(iwad), size);
    EXPECT_EQ(PortSaves::folder(config), dataHome() / "chocolate-doom" / "savegames" / "freedoom1.wad");

    launch::wad(iwad, "IWAD", {"E1M1"}, {"NOTFREE"});
    fs::last_write_time(iwad, written + std::chrono::seconds(4));

    EXPECT_EQ(PortSaves::folder(config), dataHome() / "chocolate-doom" / "savegames" / "doom.wad");
}

TEST_F(PortSavesTest, OnlyTheIwadMakesFreedoom) {
    on("/usr/bin/crispy-doom");
    addOn(launch::wad(games / "pwad" / "marked.wad", "PWAD", {"MAP02"}, {"FREEDOOM", "FREEDM"}));

    EXPECT_EQ(PortSaves::folder(config), dataHome() / "crispy-doom" / "savegames" / "doom2.wad");
}

TEST_F(PortSavesTest, ARenamedFreedoomSavesWithItsMissionBeforeTheReleaseSeparatesThem) {
    using Case = std::tuple<const char *, const char *, const char *, const char *>;

    for (const auto &[port, version, iwad, folder] : {
                 Case{"chocolate-doom", "3.0.1", "phase2.wad", "doom2.wad"},
                 Case{"chocolate-doom", "3.1.0", "phase2.wad", "freedoom2.wad"},
                 Case{"crispy-doom", "5.8.0", "phase1.wad", "doom.wad"},
                 Case{"crispy-doom", "5.9.0", "phase1.wad", "freedoom1.wad"},
         }) {
        on(fs::path("/usr/bin") / port);
        config.engines.front().version = version;
        config.iwads.front().file = launch::wad(games / (std::string(port) + version) / iwad,
                                                "IWAD",
                                                {std::string_view(iwad) == "phase1.wad" ? "E1M1" : "MAP01"},
                                                {"FREEDOOM"})
                                            .string();

        EXPECT_EQ(PortSaves::folder(config), dataHome() / port / "savegames" / folder) << port << " " << version;
    }
}

TEST_F(PortSavesTest, FinalDoomPlayedAsAnOlderGameVersionSavesWithDoomTwo) {
    using Case = std::tuple<const char *, const char *, const char *>;

    for (const auto &[iwad, version, folder] : {
                 Case{"plutonia.wad", "1.9", "doom2.wad"},
                 Case{"tnt.wad", "1.2", "doom2.wad"},
                 Case{"tnt.wad", "ultimate", "doom2.wad"},
                 Case{"plutonia.wad", "hacx", "doom2.wad"},
                 Case{"plutonia.wad", "final", "plutonia.wad"},
                 Case{"tnt.wad", "final2", "tnt.wad"},
                 Case{"tnt.wad", "chex", "tnt.wad"},
                 Case{"tnt.wad", "", "tnt.wad"},
         }) {
        on("/usr/bin/chocolate-doom");
        config.iwads.front().file = launch::wad(games / "final" / iwad, "IWAD", {"MAP01"}).string();
        profile().complevel = gameVersion(version);

        EXPECT_EQ(PortSaves::folder(config), dataHome() / "chocolate-doom" / "savegames" / folder)
                << iwad << " " << version;

        if (profile().complevel >= 0) {
            EXPECT_TRUE(sends(config, Strings{"-gameversion", version})) << version;
        }
    }

    // 3.1.0 takes no -gameversion 1.5, so none is sent and the port plays Final Doom.
    config.engines.front().version = "3.1.0";
    config.iwads.front().file = launch::wad(games / "final" / "tnt.wad", "IWAD", {"MAP01"}).string();
    profile().complevel = gameVersion("1.5");

    EXPECT_EQ(PortSaves::folder(config), dataHome() / "chocolate-doom" / "savegames" / "tnt.wad");
}

TEST_F(PortSavesTest, CrispyPlaysNerveAndTheMasterLevelsAmongThePwadsAsMissionsOfTheirOwn) {
    const fs::path unknown = dataHome() / "crispy-doom" / "savegames" / "unknown.wad";
    const fs::path doom2 = dataHome() / "crispy-doom" / "savegames" / "doom2.wad";
    const Strings nerveMaps = {"MAP01", "MAP02", "MAP03", "MAP04", "MAP05", "MAP06", "MAP07", "MAP08", "MAP09"};
    const fs::path nerve = launch::wad(games / "expansions" / "NERVE.WAD", "PWAD", nerveMaps);
    const fs::path master = launch::wad(games / "expansions" / "masterlevels.wad", "PWAD", {"MAP01", "MAP20", "MAP21"});
    const fs::path later = launch::wad(games / "expansions" / "later.wad", "PWAD", {"MAP01"});
    const fs::path other = launch::wad(games / "expansions" / "other.wad", "PWAD", {"MAP05"});
    const fs::path partial = launch::wad(games / "partial" / "nerve.wad", "PWAD", {"MAP01"});
    const fs::path shortMaster = launch::wad(games / "partial" / "MASTERLEVELS.WAD", "PWAD", {"MAP01", "MAP20"});
    const fs::path longer = launch::wad(games / "expansions" / "xnerve.wad", "PWAD", nerveMaps);

    on("/usr/bin/crispy-doom");

    const auto with = [this](const std::vector<fs::path> &files) {
        profile().files.clear();

        for (const fs::path &file : files) {
            addOn(file);
        }

        return PortSaves::folder(config);
    };

    EXPECT_EQ(with({nerve}), unknown);
    EXPECT_EQ(with({nerve, other}), unknown) << "a later PWAD without MAP01 or MAP09";
    EXPECT_EQ(with({master}), unknown);
    EXPECT_EQ(with({nerve, later}), doom2) << "a later PWAD gives MAP01";
    EXPECT_EQ(with({partial}), doom2) << "no MAP09";
    EXPECT_EQ(with({later}), doom2);
    EXPECT_EQ(with({nerve, master}), unknown) << "the Master Levels give the last MAP01 and MAP21";
    EXPECT_EQ(with({shortMaster}), doom2) << "no MAP21";
    EXPECT_EQ(with({longer}), doom2) << "only the whole file name counts";

    with({nerve});
    profile().files.front().enabled = false;

    EXPECT_EQ(PortSaves::folder(config), doom2) << "turned off";

    with({nerve});
    profile().extra = "-nosideload";

    EXPECT_EQ(PortSaves::folder(config), doom2);

    profile().extra.clear();
    config.general.alwaysAdd = "-NOSIDELOAD";

    EXPECT_EQ(PortSaves::folder(config), doom2);

    config.general.alwaysAdd.clear();

    for (const ReplayMode mode : {ReplayMode::Play, ReplayMode::Record}) {
        profile().replay = {.mode = mode, .file = "run.lmp"};

        EXPECT_EQ(PortSaves::folder(config), doom2) << "a demo on the line";
    }

    profile().replay = {};
    addOn(games / "expansions" / "demo.lmp");

    EXPECT_EQ(PortSaves::folder(config), doom2) << "a demo among the add-ons";
}

TEST_F(PortSavesTest, PwadExpansionsTakeTheMissionAfterTheGameVersionHasItsSay) {
    on("/usr/bin/crispy-doom");
    config.iwads.front().file = launch::wad(games / "final" / "tnt.wad", "IWAD", {"MAP01"}).string();
    addOn(launch::wad(games / "expansions" / "NERVE.WAD", "PWAD", {"MAP01", "MAP09"}));

    EXPECT_EQ(PortSaves::folder(config), dataHome() / "crispy-doom" / "savegames" / "tnt.wad");

    profile().complevel = gameVersion("1.9");

    EXPECT_EQ(PortSaves::folder(config), dataHome() / "crispy-doom" / "savegames" / "unknown.wad");
}

TEST_F(PortSavesTest, CrispyStopsPwadExpansionsForARecordingOnlyFrom7_1) {
    addOn(launch::wad(games / "expansions" / "NERVE.WAD", "PWAD", {"MAP01", "MAP09"}));
    on("/usr/bin/crispy-doom");

    using Case = std::tuple<const char *, ReplayMode, const char *>;

    for (const auto &[version, mode, folder] : {
                 Case{"7.0", ReplayMode::Record, "unknown.wad"},
                 Case{"7.1", ReplayMode::Record, "doom2.wad"},
                 Case{"5.11", ReplayMode::Record, "unknown.wad"},
                 Case{"5.11", ReplayMode::Play, "doom2.wad"},
                 Case{"7.0", ReplayMode::Play, "doom2.wad"},
         }) {
        config.engines.front().version = version;
        profile().replay = {.mode = mode, .file = "run.lmp"};

        EXPECT_EQ(PortSaves::folder(config), dataHome() / "crispy-doom" / "savegames" / folder)
                << version << " " << (mode == ReplayMode::Record ? "record" : "play");
    }

    // The recording takes the place of a demo among the add-ons, so that demo stops nothing.
    addOn(games / "expansions" / "demo.lmp");
    config.engines.front().version = "7.0";
    profile().replay = {.mode = ReplayMode::Record, .file = "run.lmp"};

    EXPECT_EQ(PortSaves::folder(config), dataHome() / "crispy-doom" / "savegames" / "unknown.wad");
}

TEST_F(PortSavesTest, OnlyCrispyFrom5_11PlaysPwadExpansionsApart) {
    addOn(launch::wad(games / "expansions" / "NERVE.WAD", "PWAD", {"MAP01", "MAP09"}));

    using Case = std::tuple<const char *, const char *, const char *>;

    for (const auto &[port, version, folder] : {
                 Case{"crispy-doom", "5.11", "unknown.wad"},
                 Case{"crispy-doom", "5.10.3", "doom2.wad"},
                 Case{"chocolate-doom", "", "doom2.wad"},
         }) {
        on(fs::path("/usr/bin") / port);
        config.engines.front().version = version;

        EXPECT_EQ(PortSaves::folder(config), dataHome() / port / "savegames" / folder) << port << " " << version;
    }
}
#endif

namespace {

using PortSaves::WindowsIni;
using PortSaves::WindowsRoot;

// Each answer in the order: writable without a portable ini, writable with one, under Program Files
// without one, under Program Files with one.
struct WindowsCase {
    const char *file;
    const char *version;
    std::array<WindowsRoot, 4> roots;
    std::array<WindowsIni, 4> inis;
};

constexpr std::array<WindowsRoot, 4> ROOT_BY_INI = {
        WindowsRoot::savedGames, WindowsRoot::saveBeside, WindowsRoot::savedGames, WindowsRoot::savedGames};
constexpr std::array<WindowsRoot, 4> SAVE_BESIDE = {
        WindowsRoot::saveBeside, WindowsRoot::saveBeside, WindowsRoot::savedGames, WindowsRoot::savedGames};
constexpr std::array<WindowsRoot, 4> BESIDE = {
        WindowsRoot::beside, WindowsRoot::beside, WindowsRoot::savedGames, WindowsRoot::savedGames};

constexpr std::array<WindowsRoot, 4> ALWAYS_BESIDE = {
        WindowsRoot::beside, WindowsRoot::beside, WindowsRoot::beside, WindowsRoot::beside};

constexpr std::array<WindowsIni, 4> INI_BY_INI = {
        WindowsIni::myGames, WindowsIni::portable, WindowsIni::myGames, WindowsIni::myGames};
constexpr std::array<WindowsIni, 4> PORTABLE_FIRST = {
        WindowsIni::besideForUser, WindowsIni::portable, WindowsIni::appData, WindowsIni::portable};
constexpr std::array<WindowsIni, 4> NO_PORTABLE = {
        WindowsIni::besideForUser, WindowsIni::besideForUser, WindowsIni::appData, WindowsIni::appData};

constexpr std::array<WindowsIni, 4> ALWAYS_FOR_USER = {
        WindowsIni::besideForUser, WindowsIni::besideForUser, WindowsIni::besideForUser, WindowsIni::besideForUser};

constexpr std::array WINDOWS_CASES = std::to_array<WindowsCase>({
        {"gzdoom", "", ROOT_BY_INI, INI_BY_INI},
        {"gzdoom", "4.12.1", ROOT_BY_INI, INI_BY_INI},
        {"gzdoom", "4.11.0", ROOT_BY_INI, INI_BY_INI},
        {"gzdoom", "4.9.0", ROOT_BY_INI, INI_BY_INI},
        {"gzdoom", "4.8.0", SAVE_BESIDE, PORTABLE_FIRST},
        {"gzdoom", "4.4.0", SAVE_BESIDE, PORTABLE_FIRST},
        {"gzdoom", "4.3.3", BESIDE, PORTABLE_FIRST},
        {"vkdoom", "", ROOT_BY_INI, INI_BY_INI},
        {"uzdoom", "", ROOT_BY_INI, INI_BY_INI},
        {"uzdoom", "5.0.0", ROOT_BY_INI, INI_BY_INI},
        {"uzdoom", "4.14.3", ROOT_BY_INI, INI_BY_INI},
        {"zdoom", "", BESIDE, NO_PORTABLE},
        {"zdoom", "2.8.1", BESIDE, NO_PORTABLE},
        {"zdoom", "2.8.0", BESIDE, NO_PORTABLE},
        {"zdoom", "2.7.1", ALWAYS_BESIDE, ALWAYS_FOR_USER},
        {"zdoom", "2.7.0", ALWAYS_BESIDE, ALWAYS_FOR_USER},
        {"zdoom", "2.6.1", ALWAYS_BESIDE, ALWAYS_FOR_USER},
        {"zdoom", "2.5.0", ALWAYS_BESIDE, ALWAYS_FOR_USER},
        {"zandronum", "", BESIDE, NO_PORTABLE},
        {"vkdoom", "25.6.0", ROOT_BY_INI, INI_BY_INI},
        {"vkdoom", "0.9", ROOT_BY_INI, INI_BY_INI},
        {"skulltag", "", ALWAYS_BESIDE, ALWAYS_FOR_USER},
        {"skulltag", "0.98a", ALWAYS_BESIDE, ALWAYS_FOR_USER},
        {"skulltag", "0.97d", ALWAYS_BESIDE, ALWAYS_FOR_USER},
});

// Answers the ini rule as if only these inis were on disk.
auto onDisk(const std::vector<WindowsIni> &there) {
    return [there](const WindowsIni ini) { return std::ranges::find(there, ini) != there.end(); };
}

}

TEST(PortSavesWindows, EachZdoomPortAndReleaseFindsItsSavesAndIniWhereItLooks) {
    for (const WindowsCase &each : WINDOWS_CASES) {
        const Engine engine{.name = "Port", .file = std::string("/games/") + each.file, .version = each.version};
        const Dialect::Features features = Dialect::of(engine).features;

        for (size_t at = 0; at < 4; ++at) {
            const bool underProgramFiles = at >= 2;
            const bool portableIni = at % 2 == 1;

            EXPECT_EQ(PortSaves::windowsRoot(features, underProgramFiles, portableIni), each.roots[at])
                    << each.file << " " << each.version << " case " << at;

            const auto found = [portableIni](const WindowsIni ini) {
                return ini != WindowsIni::portable || portableIni;
            };

            EXPECT_EQ(PortSaves::windowsIni(features, underProgramFiles, found), each.inis[at])
                    << each.file << " " << each.version << " case " << at;
        }
    }
}

TEST(PortSavesWindows, SkulltagAndOldZdoomFallBackToTheSharedIniWithoutOneForTheUser) {
    for (const auto &[file, version] : std::to_array<std::pair<const char *, const char *>>({
                 {"skulltag", ""},
                 {"skulltag", "0.98a"},
                 {"skulltag", "0.97d"},
                 {"zdoom", "2.7.1"},
                 {"zdoom", "2.5.0"},
         })) {
        const Engine engine{.name = "Port", .file = std::string("/games/") + file, .version = version};
        const Dialect::Features features = Dialect::of(engine).features;

        for (const bool underProgramFiles : {false, true}) {
            EXPECT_EQ(PortSaves::windowsIni(features, underProgramFiles, onDisk({})), WindowsIni::besideShared)
                    << file << " " << version;
            EXPECT_EQ(PortSaves::windowsIni(features, underProgramFiles, onDisk({WindowsIni::besideForUser})),
                      WindowsIni::besideForUser)
                    << file << " " << version;
        }
    }
}

TEST(PortSavesWindows, ZdoomZandronumAndGzdoomBefore4_9_0ReadTheSharedIniWhileTheOneTheyWriteIsMissing) {
    for (const auto &[file, version] : std::to_array<std::pair<const char *, const char *>>({
                 {"zdoom", ""},
                 {"zdoom", "2.8.1"},
                 {"zdoom", "2.8.0"},
                 {"zandronum", ""},
                 {"gzdoom", "4.8.2"},
                 {"gzdoom", "4.8.0"},
                 {"gzdoom", "4.4.0"},
                 {"gzdoom", "4.3.3"},
         })) {
        const Engine engine{.name = "Port", .file = std::string("/games/") + file, .version = version};
        const Dialect::Features features = Dialect::of(engine).features;

        EXPECT_EQ(PortSaves::windowsIni(features, false, onDisk({WindowsIni::besideForUser})),
                  WindowsIni::besideForUser)
                << file << " " << version;
        EXPECT_EQ(PortSaves::windowsIni(features, false, onDisk({WindowsIni::appData})), WindowsIni::besideShared)
                << file << " " << version;
        EXPECT_EQ(PortSaves::windowsIni(features, true, onDisk({WindowsIni::appData})), WindowsIni::appData)
                << file << " " << version;
        EXPECT_EQ(PortSaves::windowsIni(features, true, onDisk({WindowsIni::besideForUser})), WindowsIni::besideShared)
                << file << " " << version;
    }
}

TEST(PortSavesWindows, GzdoomFrom4_9_0ReadsMyGamesThenTheUserIniThenAppDataThenTheSharedIni) {
    for (const auto &[file, version] : std::to_array<std::pair<const char *, const char *>>({
                 {"gzdoom", ""},
                 {"gzdoom", "4.12.1"},
                 {"gzdoom", "4.9.0"},
                 {"uzdoom", ""},
                 {"vkdoom", ""},
         })) {
        const Engine engine{.name = "Port", .file = std::string("/games/") + file, .version = version};
        const Dialect::Features features = Dialect::of(engine).features;
        const std::vector<WindowsIni> every = {
                WindowsIni::portable, WindowsIni::myGames, WindowsIni::besideForUser, WindowsIni::appData};

        EXPECT_EQ(PortSaves::windowsIni(features, false, onDisk(every)), WindowsIni::portable)
                << file << " " << version;
        EXPECT_EQ(PortSaves::windowsIni(features, true, onDisk(every)), WindowsIni::myGames) << file << " " << version;

        for (const bool underProgramFiles : {false, true}) {
            const auto read = [&features, underProgramFiles](const std::vector<WindowsIni> &there) {
                return PortSaves::windowsIni(features, underProgramFiles, onDisk(there));
            };

            EXPECT_EQ(read({WindowsIni::myGames, WindowsIni::besideForUser, WindowsIni::appData}), WindowsIni::myGames)
                    << file << " " << version;
            EXPECT_EQ(read({WindowsIni::besideForUser, WindowsIni::appData}), WindowsIni::besideForUser)
                    << file << " " << version;
            EXPECT_EQ(read({WindowsIni::appData}), WindowsIni::appData) << file << " " << version;
            EXPECT_EQ(read({}), WindowsIni::besideShared) << file << " " << version;
        }
    }
}

// GZDoom compares its folder, with the slash it ends in, to the start of the root as text in any case.
TEST(PortSavesWindows, AFolderIsUnderProgramFilesWhenItsPathStartsWithTheRoot) {
    const auto under = [](const char *folder, const char *root) { return PortSaves::underRoot(folder, root); };

    EXPECT_TRUE(under("C:/Program Files/GZDoom", "C:/Program Files"));
    EXPECT_TRUE(under("c:/program files/gzdoom", "C:/Program Files"));
    EXPECT_TRUE(under("C:/Program Files", "C:/Program Files"));
    EXPECT_TRUE(under("C:/Program Files", "C:/Program Files/"));
    EXPECT_TRUE(under("C:/Program Files (x86)/GZDoom", "C:/Program Files"));
    EXPECT_TRUE(under("C:/Program FilesX/GZDoom", "C:/Program Files"));
    EXPECT_FALSE(under("C:/Games/GZDoom", "C:/Program Files"));
    EXPECT_FALSE(under("D:/Program Files/GZDoom", "C:/Program Files"));
    EXPECT_FALSE(under("C:/Program Files/GZDoom", ""));
}

TEST(PortSavesWindows, TheIniRuleAsksOnlyAboutTheInisThePortReads) {
    const auto asked = [](const char *file, const char *version, const bool underProgramFiles) {
        const Engine engine{.name = "Port", .file = std::string("/games/") + file, .version = version};
        std::vector<WindowsIni> out;

        (void)PortSaves::windowsIni(Dialect::of(engine).features, underProgramFiles, [&out](const WindowsIni ini) {
            out.push_back(ini);

            return false;
        });

        return out;
    };
    using Inis = std::vector<WindowsIni>;

    EXPECT_EQ(asked("zdoom", "", false), Inis{WindowsIni::besideForUser});
    EXPECT_EQ(asked("zdoom", "", true), Inis{WindowsIni::appData});
    EXPECT_EQ(asked("zdoom", "2.7.1", true), Inis{WindowsIni::besideForUser});
    EXPECT_EQ(asked("skulltag", "", true), Inis{WindowsIni::besideForUser});
    EXPECT_EQ(asked("gzdoom", "4.8.0", true), (Inis{WindowsIni::portable, WindowsIni::appData}));
    EXPECT_EQ(asked("gzdoom", "", false),
              (Inis{WindowsIni::portable, WindowsIni::myGames, WindowsIni::besideForUser, WindowsIni::appData}));
    EXPECT_EQ(asked("gzdoom", "", true), (Inis{WindowsIni::myGames, WindowsIni::besideForUser, WindowsIni::appData}));
}
