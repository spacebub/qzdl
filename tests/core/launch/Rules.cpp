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
#include <array>
#include <cstddef>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "core/config/Schema.h"
#include "core/launch/Rules.h"

namespace {

using Rules::Rule;

// DSDA-Doom and Nyan Doom stop on any switch outside their table.
constexpr std::array DSDA_KNOWS = std::to_array<std::string_view>({
        "-coop_spawns",
        "-chain_episodes",
        "-stroller",
        "-game_speed",
        "-turbo",
        "-nocheats",
});

}

TEST(Rules, EveryRuleHasASpelling) {
    for (size_t at = 0; at < Rules::RULES; ++at) {
        const auto rule = static_cast<Rule>(at);

        EXPECT_TRUE(std::ranges::any_of(Rules::spellings(), [rule](const Rules::Spelling &each) {
            return each.rule == rule;
        })) << Rules::info(rule).label;
    }
}

TEST(Rules, NoLineSpellsARuleTwice) {
    for (const Rules::Spelling &each : Rules::spellings()) {
        for (const Rules::Spelling &other : Rules::spellings()) {
            if (&each != &other && each.rule == other.rule) {
                EXPECT_EQ(each.lines & other.lines, 0U) << each.name;
            }
        }
    }
}

TEST(Rules, ANumberHasARangeAndAToggleNone) {
    for (const Rules::Spelling &each : Rules::spellings()) {
        const bool number = Rules::info(each.rule).kind == Rules::Kind::number;

        EXPECT_EQ(number, each.range.least < each.range.most) << each.name;
        EXPECT_TRUE(number || (each.range.least == 0 && each.range.most == 0)) << each.name;
        EXPECT_TRUE(!number || each.value.empty()) << each.name;
        EXPECT_TRUE(each.name.starts_with('-') || each.name.starts_with('+')) << each.name;
    }
}

TEST(Rules, TheDescriptionsArePlainSentences) {
    for (const Rules::Spelling &each : Rules::spellings()) {
        const std::string said(each.description);

        EXPECT_TRUE(said.ends_with('.')) << each.name;
        EXPECT_EQ(said.find(';'), std::string::npos) << each.name;
        EXPECT_EQ(said.find(" - "), std::string::npos) << each.name;
    }
}

TEST(Rules, DsdaIsHandedOnlySwitchesItKnows) {
    for (const PortLine line : {PortLine::dsda, PortLine::nyan}) {
        for (size_t at = 0; at < Rules::RULES; ++at) {
            const Rules::Spelling *spelled = Rules::spelling(line, static_cast<Rule>(at));

            if (spelled == nullptr || spelled->rule == Rule::limitRemoving) {
                continue;
            }

            EXPECT_NE(std::ranges::find(DSDA_KNOWS, spelled->name), DSDA_KNOWS.end()) << spelled->name;
        }
    }

    EXPECT_NE(Rules::spelling(PortLine::nyan, Rule::limitRemoving), nullptr);
    EXPECT_EQ(Rules::spelling(PortLine::dsda, Rule::limitRemoving), nullptr);
}

TEST(Rules, HelionAndTheDosPortsTakeNone) {
    for (const PortLine line : {PortLine::helion, PortLine::none}) {
        for (size_t at = 0; at < Rules::RULES; ++at) {
            EXPECT_EQ(Rules::spelling(line, static_cast<Rule>(at)), nullptr) << at;
        }
    }
}

TEST(Rules, TheTurboRangeFollowsThePort) {
    EXPECT_EQ(Rules::spelling(PortLine::gzdoom, Rule::turbo)->range.most, 255);
    EXPECT_EQ(Rules::spelling(PortLine::dsda, Rule::turbo)->range.most, 255);
    EXPECT_EQ(Rules::spelling(PortLine::woof, Rule::turbo)->range.most, 400);
    EXPECT_EQ(Rules::spelling(PortLine::chocolate, Rule::turbo)->range.most, 400);
}

TEST(Rules, ANumberHasToBeWholeAndInRange) {
    const Rules::Spelling &turbo = *Rules::spelling(PortLine::dsda, Rule::turbo);

    EXPECT_EQ(Rules::parse(turbo, "10"), 10);
    EXPECT_EQ(Rules::parse(turbo, "255"), 255);

    for (const char *bad : {"", "9", "256", "1e2", "-50", " 50", "x"}) {
        EXPECT_FALSE(Rules::parse(turbo, bad).has_value()) << bad;
    }

    EXPECT_EQ(Rules::parse(*Rules::spelling(PortLine::woof, Rule::dogs), "0"), 0);
}

TEST(Rules, ASpellingCarriesItsOwnValue) {
    const Rules::Spelling &always = *Rules::spelling(PortLine::zandronum, Rule::alwaysApplyDmflags);

    EXPECT_EQ(always.name, "+alwaysapplydmflags");
    EXPECT_EQ(always.value, "1");
}
