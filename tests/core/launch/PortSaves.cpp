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
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/system/Env.h"
#include "ttk/system/Paths.h"

#include "Launch.h"
#include "core/launch/Arguments.h"
#include "core/launch/PortSaves.h"
#include "core/launch/Storage.h"
#include "core/ports/Zip.h"

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
        games = tests::Sandbox::scratch("port-saves");

        // The sandbox lasts the whole run, so what one case saved would show in the next.
        for (const fs::path &folder : {
                     home() / ".config",
                     home() / ".var",
                     home() / "Saves",
                     home() / ".prboom-plus",
                     home() / ".dsda-doom",
                     home() / ".nyan-doom",
                     dataHome(),
                     configHome(),
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

    void TearDown() override {
        ttk::Env::set("DOOMSAVEDIR", "");
        ttk::Env::set("DOOMDATADIR", "");
    }

    Profile &profile() { return config.activeProfile(); }

    void on(const fs::path &program) { config.ports.front().file = program.string(); }

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

TEST_F(PortSavesTest, AUzdoomFlatpakKeepsItsSavesInItsSandbox) {
    on("/var/lib/flatpak/exports/bin/org.zdoom.UZDoom");

    EXPECT_EQ(PortSaves::folder(config),
              home() / ".var" / "app" / "org.zdoom.UZDoom" / "data" / "games" / "uzdoom" / "savegames");
}

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
        config.ports.front().version = version;

        EXPECT_EQ(PortSaves::folder(config), root) << version;
    }
}

TEST_F(PortSavesTest, Gzdoom4_9KeepsAFolderPerGameInItsConfigFolder) {
    config.ports.front().version = "4.9.0";

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
        config.ports.front().version = version;

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

    config.ports.front().version = "4.14.3";

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

TEST_F(PortSavesTest, TheCrispyFlatpakListsItsSavesWithNoProgramPicked) {
    on("/var/lib/flatpak/exports/bin/io.github.fabiangreffrath.Doom");

    const fs::path folder = home() / ".var" / "app" / "io.github.fabiangreffrath.Doom" / "data" / "crispy-doom"
                            / "savegames" / "doom2.wad";

    launch::write(folder / "doomsav4.dsg", "x");

    EXPECT_EQ(PortSaves::folder(config), folder);
    EXPECT_EQ(PortSaves::saves(config), (Strings{"doomsav4.dsg"}));

    config.ports.front().program = "crispy-doom";

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

TEST_F(PortSavesTest, SkulltagListsNoSaves) {
    on("/games/skulltag");
    launch::write(home() / ".config" / "zandronum" / "zandronum.ini", "[GlobalSettings]\nsave_dir=/elsewhere\n");

    EXPECT_EQ(PortSaves::folder(config), fs::path());
    EXPECT_TRUE(PortSaves::saves(config).empty());
}

TEST_F(PortSavesTest, AFlatpakExportListsTheSavesOfItsPort) {
    on("/var/lib/flatpak/exports/bin/io.github.fabiangreffrath.woof");

    EXPECT_EQ(PortSaves::folder(config),
              home() / ".var" / "app" / "io.github.fabiangreffrath.woof" / "data" / "woof" / "savegames" / "DOOM2.WAD");
}

TEST_F(PortSavesTest, AnUnknownPortOrOneUnderDosboxHasNoFolder) {
    on("/games/mystery-engine");

    EXPECT_EQ(PortSaves::folder(config), fs::path());

    on("/games/woof");
    config.ports.front().dosbox = true;

    EXPECT_EQ(PortSaves::folder(config), fs::path());

    config.ports.front().program = "boom";

    EXPECT_EQ(PortSaves::folder(config), fs::path());
}

TEST_F(PortSavesTest, AGzdoomFileWithDsdaPickedListsTheSavesOfDsdaDoom) {
    on("/games/gzdoom");
    config.ports.front().program = "dsda-doom";

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

TEST_F(PortSavesTest, APickForAnUnknownNameListsTheSavesOfThePickedProgram) {
    on("/games/mystery-engine");
    config.ports.front().program = "prboom-plus";

    EXPECT_EQ(PortSaves::folder(config), dataHome() / "prboom-plus");

    config.ports.front().program = "eternity";

    EXPECT_EQ(PortSaves::folder(config), fs::path());
}

TEST_F(PortSavesTest, APickedProgramListsItsOwnSaves) {
    on("/games/nugget-doom");
    config.ports.front().program = "woof";

    EXPECT_EQ(PortSaves::folder(config), dataHome() / "woof" / "savegames" / "DOOM2.WAD");

    on("/games/woof");
    config.ports.front().program = "cherry-doom";

    EXPECT_EQ(PortSaves::folder(config), dataHome() / "cherry-doom" / "savegames" / "DOOM2.WAD");
}

TEST_F(PortSavesTest, NyanDoomPickedForAnUnknownNameListsNyanSaves) {
    on("/games/mystery-engine");
    config.ports.front().program = "nyan-doom";

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
#endif

namespace {

using PortSaves::WindowsIni;
using PortSaves::WindowsRoot;

// Each answer in the order: writable without a portable ini, writable with one, under Program Files
// without one, under Program Files with one.
struct WindowsCase {
    const char *program;
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

constexpr std::array<WindowsIni, 4> INI_BY_INI = {
        WindowsIni::myGames, WindowsIni::portable, WindowsIni::myGames, WindowsIni::myGames};
constexpr std::array<WindowsIni, 4> PORTABLE_FIRST = {
        WindowsIni::besideForUser, WindowsIni::portable, WindowsIni::appData, WindowsIni::portable};
constexpr std::array<WindowsIni, 4> NO_PORTABLE = {
        WindowsIni::besideForUser, WindowsIni::besideForUser, WindowsIni::appData, WindowsIni::appData};

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
        {"zandronum", "", BESIDE, NO_PORTABLE},
});

}

TEST(PortSavesWindows, EachZdoomPortAndReleaseFindsItsSavesAndIniWhereItLooks) {
    for (const WindowsCase &each : WINDOWS_CASES) {
        const PortEntry entry{.name = "Port", .file = std::string("/games/") + each.program, .version = each.version};
        const Dialect::Features features = Dialect::of(entry).features;

        for (size_t at = 0; at < 4; ++at) {
            const bool underProgramFiles = at >= 2;
            const bool portableIni = at % 2 == 1;

            EXPECT_EQ(PortSaves::windowsRoot(features, underProgramFiles, portableIni), each.roots[at])
                    << each.program << " " << each.version << " case " << at;
            EXPECT_EQ(PortSaves::windowsIni(features, underProgramFiles, portableIni), each.inis[at])
                    << each.program << " " << each.version << " case " << at;
        }
    }
}
