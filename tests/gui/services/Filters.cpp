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

namespace {

using Patterns = std::vector<std::string>;

bool offers(const Patterns &patterns, const std::string &pattern) {
    return std::ranges::find(patterns, pattern) != patterns.end();
}

TEST(Filters, TheWadPickerOffersEveryGameFileKind) {
    for (const char *pattern : {"*.wad", "*.pk3", "*.pk7", "*.ipk3", "*.zip", "*.7z", "*.deh", "*.bex"}) {
        EXPECT_TRUE(offers(Filters::wad(), pattern)) << pattern;
    }
}

TEST(Filters, EveryPatternIsAStarAndAnExtension) {
    for (const Patterns *list :
         {&Filters::wad(), &Filters::zdl(), &Filters::config(), &Filters::save(), &Filters::replay()}) {
        for (const std::string &pattern : *list) {
            EXPECT_TRUE(pattern.starts_with("*.")) << pattern;
            EXPECT_GT(pattern.size(), 2U) << pattern;
        }
    }
}

TEST(Filters, NoListRepeatsAPattern) {
    for (const Patterns *list : {&Filters::wad(),
                                 &Filters::port(),
                                 &Filters::zdl(),
                                 &Filters::config(),
                                 &Filters::save(),
                                 &Filters::replay()}) {
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
    EXPECT_EQ(&Filters::wad(), &Filters::wad());
    EXPECT_EQ(&Filters::save(), &Filters::save());
}

TEST(LastDir, EveryPickerRemembersADirectoryOfItsOwn) {
    const std::set<std::string> names = {
            LastDir::WAD, LastDir::SRC, LastDir::SAVE, LastDir::ZDL, LastDir::CONFIG, LastDir::REPLAY};

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

TEST(Picked, ACallbackThatTakesTheOptionGetsIt) {
    std::vector<std::pair<std::string, bool>> got;

    const ttk::FilePicker::Chosen chosen =
            Picked::first([&got](const std::string &path, const bool option) { got.emplace_back(path, option); });

    chosen({"/a.wad"}, true);
    chosen({"/b.wad"}, false);

    EXPECT_EQ(got, (std::vector<std::pair<std::string, bool>>{{"/a.wad", true}, {"/b.wad", false}}));
}

}
