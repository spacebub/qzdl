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

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "gui/services/Filters.h"
#include "support/Sandbox.h"

namespace {

using Patterns = std::vector<std::string>;

bool offers(const Patterns &patterns, const std::string &pattern) {
    return std::ranges::find(patterns, pattern) != patterns.end();
}

TEST(Filters, TheAddonPickerOffersEveryGameFileKindAndPatches) {
    for (const char *pattern :
         {"*.wad", "*.pk3", "*.pk7", "*.ipk3", "*.zip", "*.7z", "*.deh", "*.bex", "*.hhe", "*.seh"}) {
        EXPECT_TRUE(offers(Filters::addon(), pattern)) << pattern;
    }
}

TEST(Filters, EveryGameKindIsAnAddonKindToo) {
    for (const std::string &pattern : Filters::game()) {
        EXPECT_TRUE(offers(Filters::addon(), pattern)) << pattern;
    }
}

TEST(Filters, EveryPatternIsAStarAndAnExtension) {
    for (const Patterns *list : {
                 &Filters::game(),
                 &Filters::addon(),
                 &Filters::zdl(),
                 &Filters::config(),
                 &Filters::save(),
                 &Filters::replay(),
         }) {
        for (const std::string &pattern : *list) {
            EXPECT_TRUE(pattern.starts_with("*.")) << pattern;
            EXPECT_GT(pattern.size(), 2U) << pattern;
        }
    }
}

TEST(Filters, NoListRepeatsAPattern) {
    for (const Patterns *list : {
                 &Filters::game(),
                 &Filters::addon(),
                 &Filters::port(),
                 &Filters::zdl(),
                 &Filters::config(),
                 &Filters::save(),
                 &Filters::replay(),
         }) {
        EXPECT_EQ(std::set<std::string>(list->begin(), list->end()).size(), list->size());
    }
}

TEST(Filters, APortIsAnyFileOffWindows) {
#ifdef _WIN32
    EXPECT_EQ(Filters::port(), (Patterns{"*.exe", "*.bat"}));
#else
    EXPECT_EQ(Filters::port(), (Patterns{"*"}));
#endif
}

TEST(Filters, TheOtherPickersAskForTheirOwnFiles) {
    EXPECT_EQ(Filters::zdl(), (Patterns{"*.zdl"}));
    EXPECT_EQ(Filters::config(), (Patterns{"*.json", "*.ini"}));
    EXPECT_EQ(Filters::replay(), (Patterns{"*.lmp"}));
    EXPECT_TRUE(offers(Filters::save(), "*.zds"));
    EXPECT_TRUE(offers(Filters::save(), "*.dsg"));
}

TEST(Filters, EachListIsBuiltOnce) {
    EXPECT_EQ(&Filters::addon(), &Filters::addon());
    EXPECT_EQ(&Filters::save(), &Filters::save());
}

TEST(LastDir, EveryPickerRemembersADirectoryOfItsOwn) {
    const std::set<std::string> names = {
            LastDir::WAD,
            LastDir::SRC,
            LastDir::SAVE,
            LastDir::ZDL,
            LastDir::CONFIG,
            LastDir::REPLAY,
    };

    EXPECT_EQ(names.size(), 6U);
}

TEST(Picked, NothingChosenCallsNothing) {
    int called = 0;

    const ttk::FilePicker::Chosen chosen = Picked::first([&called](const std::string &) { ++called; });

    chosen({}, false);

    EXPECT_EQ(called, 0);
}

TEST(Picked, OnlyTheFirstPathIsPassedOn) {
    std::vector<std::string> got;

    const ttk::FilePicker::Chosen chosen = Picked::first([&got](const std::string &path) { got.push_back(path); });

    chosen({"/a.wad", "/b.wad"}, false);

    EXPECT_EQ(got, (std::vector<std::string>{"/a.wad"}));
}

TEST(Filters, GamesAreTheWadKindsWithoutPatchesDemosOrConfigs) {
    for (const char *pattern : {"*.wad", "*.iwad", "*.pk3", "*.ipk3", "*.pke"}) {
        EXPECT_TRUE(offers(Filters::game(), pattern)) << pattern;
    }

    for (const char *pattern : {"*.deh", "*.bex", "*.lmp", "*.cfg"}) {
        EXPECT_FALSE(offers(Filters::game(), pattern)) << pattern;
    }
}

TEST(FiltersSplit, ADropKeepsTheFilesThatPassAndNamesTheRest) {
    const Filters::Dropped dropped =
            Filters::split({"/games/DOOM2.WAD", "/notes/readme.txt", "/mods/sigil.pk3"}, Filters::game(), false);

    EXPECT_EQ(dropped.taken, (std::vector<std::string>{"/games/DOOM2.WAD", "/mods/sigil.pk3"}));
    EXPECT_EQ(dropped.left, (std::vector<std::string>{"readme.txt"}));
}

TEST(FiltersSplit, AFolderIsTakenOnlyWhereFoldersAre) {
    const std::string folder = tests::Sandbox::scratch("filters").string();

    EXPECT_EQ(Filters::split({folder}, Filters::addon(), true).taken, (std::vector<std::string>{folder}));
    EXPECT_TRUE(Filters::split({folder}, Filters::port(), false).taken.empty());
}

TEST(FiltersListed, PastThreeNamesTheRestAreCounted) {
    EXPECT_EQ(Filters::listed({"a", "b", "c", "d"}), "a, b, c and d");
    EXPECT_EQ(Filters::listed({"a", "b", "c", "d", "e", "f"}), "a, b, c and 3 more");
}

TEST(FiltersSplit, TakenPathsComeAsThePickersGiveThemAndAFolderSlashStillNamesIt) {
    const Filters::Dropped dropped =
            Filters::split({"/games/./doom2.txt/", "/mods/x/../sigil.pk3"}, Filters::game(), false);

    EXPECT_EQ(dropped.taken, (std::vector<std::string>{"/mods/sigil.pk3"}));
    EXPECT_EQ(dropped.left, (std::vector<std::string>{"doom2.txt"}));
}

TEST(FiltersLeftOut, OneOrSeveralNamesReadAsASentence) {
    EXPECT_EQ(Filters::leftOut({"a.txt"}, "a game", "games"), "a.txt is not a game, so it was left out.");
    EXPECT_EQ(Filters::leftOut({"a.txt", "b.png"}, "a game", "games"),
              "a.txt and b.png are not games, so they were left out.");
    EXPECT_EQ(Filters::leftOut({"a", "b", "c"}, "a program", "programs"),
              "a, b and c are not programs, so they were left out.");
}

TEST(Picked, ACallbackThatTakesTheOptionGetsIt) {
    std::vector<std::pair<std::string, bool>> got;

    const ttk::FilePicker::Chosen chosen =
            Picked::first([&got](const std::string &path, const bool option) { got.emplace_back(path, option); });

    chosen({"/a.wad"}, true);
    chosen({"/b.wad"}, false);

    EXPECT_EQ(got, (std::vector<std::pair<std::string, bool>>{{"/a.wad", true}, {"/b.wad", false}}));
}

}
