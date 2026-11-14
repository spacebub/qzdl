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

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <set>
#include <string>

#include <gtest/gtest.h>

#include "core/config/Schema.h"
#include "core/launch/Dialect.h"
#include "core/launch/Flags.h"

namespace {

constexpr std::array SETS = {
        Flags::Set::uzdoom,
        Flags::Set::gzdoom,
        Flags::Set::zdoom,
        Flags::Set::zandronum,
        Flags::Set::eternity,
};

Flags::Set setOf(const char *program) {
    return Dialect::of(std::filesystem::path("/games") / program).flags;
}

}

TEST(Flags, EachPortNameSpeaksItsSet) {
    EXPECT_EQ(setOf("uzdoom"), Flags::Set::uzdoom);
    EXPECT_EQ(setOf("gzdoom"), Flags::Set::gzdoom);
    EXPECT_EQ(setOf("vkdoom"), Flags::Set::gzdoom);
    EXPECT_EQ(setOf("zdoom"), Flags::Set::zdoom);
    EXPECT_EQ(setOf("zandronum"), Flags::Set::zandronum);
    EXPECT_EQ(setOf("eternity"), Flags::Set::eternity);

    for (const char *program : {
                 "dsda-doom",
                 "prboom-plus",
                 "woof",
                 "nugget-doom",
                 "chocolate-doom",
                 "crispy-doom",
                 "helion",
                 "boom",
                 "mbf",
                 "doomlegacy",
                 "doom2",
         }) {
        EXPECT_EQ(setOf(program), Flags::Set::none) << program;
    }
}

TEST(Flags, ZandronumPickedByHandSpeaksItsOwnSet) {
    const Dialect::Port picked = Dialect::of(std::filesystem::path("/games/zan"), "zandronum");

    EXPECT_EQ(picked.flags, Flags::Set::zandronum);
    EXPECT_FALSE(picked.teamPick);
    EXPECT_EQ(Dialect::of(std::filesystem::path("/games/zan"), "zdoom").flags, Flags::Set::zdoom);
    EXPECT_NE(Dialect::choice("zandronum"), nullptr);
}

TEST(Flags, OnlyTheStrtolPortsSpellTheTopBitNegative) {
    EXPECT_EQ(Flags::spelled(Flags::Set::zdoom, 2147483647U), "2147483647");
    EXPECT_EQ(Flags::spelled(Flags::Set::zdoom, 2147483648U), "-2147483648");
    EXPECT_EQ(Flags::spelled(Flags::Set::zandronum, 4294967295U), "-1");
    EXPECT_EQ(Flags::spelled(Flags::Set::gzdoom, 4294967295U), "4294967295");
    EXPECT_EQ(Flags::spelled(Flags::Set::eternity, 32U), "32");
}

TEST(Flags, TheWordsAreTheOnesTheProfileKeeps) {
    for (size_t at = 0; at < Flags::WORDS; ++at) {
        EXPECT_EQ(Flags::info(static_cast<Flags::Word>(at)).name, FlagWord::ALL[at]) << at;
    }
}

TEST(Flags, AnOptionNamesOnlyPortsItsWordHas) {
    for (const Flags::Option &each : Flags::options()) {
        EXPECT_NE(each.sets, 0) << each.label;
        EXPECT_EQ(each.sets & ~Flags::info(each.word).sets, 0) << each.label;
    }
}

TEST(Flags, NoTwoOptionsShareABitOnOnePort) {
    for (const Flags::Set set : SETS) {
        for (size_t at = 0; at < Flags::WORDS; ++at) {
            const auto word = static_cast<Flags::Word>(at);
            std::uint32_t taken = 0;

            for (const Flags::Option &each : Flags::options()) {
                if (each.word != word || (each.sets & Flags::bit(set)) == 0) {
                    continue;
                }

                EXPECT_EQ(taken & each.mask, 0U) << each.label << " on set " << static_cast<int>(set);

                taken |= each.mask;
            }

            EXPECT_EQ(taken, Flags::known(set, word));
        }
    }
}

TEST(Flags, AChoiceStaysInsideItsBitsAndStartsAtNone) {
    for (const Flags::Option &each : Flags::options()) {
        if (each.choices.empty()) {
            EXPECT_EQ(each.mask & (each.mask - 1), 0U) << each.label << " is a check box on several bits";
            EXPECT_FALSE(each.description.empty());

            continue;
        }

        EXPECT_EQ(each.choices.front().value, 0U) << each.label;

        std::set<std::uint32_t> seen;

        for (const Flags::Choice &choice : each.choices) {
            EXPECT_EQ(choice.value & ~each.mask, 0U) << each.label << " " << choice.label;
            EXPECT_TRUE(seen.insert(choice.value).second) << each.label << " " << choice.label;
        }
    }
}

TEST(Flags, TheDescriptionsArePlainSentences) {
    for (const Flags::Option &each : Flags::options()) {
        const std::string said(each.description);

        EXPECT_FALSE(each.label.empty());
        EXPECT_TRUE(said.ends_with('.')) << each.label;
        EXPECT_EQ(said.find(';'), std::string::npos) << each.label;
        EXPECT_EQ(said.find(" - "), std::string::npos) << each.label;
        EXPECT_EQ(said.find("--"), std::string::npos) << each.label;
    }
}

TEST(Flags, OneLabelPerPortAndWordFamily) {
    for (const Flags::Set set : SETS) {
        std::set<std::string> seen;

        for (const Flags::Option &each : Flags::options()) {
            if ((each.sets & Flags::bit(set)) == 0) {
                continue;
            }

            const std::string key = std::to_string(static_cast<int>(Flags::info(each.word).family)) + " "
                                    + std::to_string(static_cast<int>(each.group)) + " " + std::string(each.label);

            EXPECT_TRUE(seen.insert(key).second) << each.label << " twice in one group";
        }
    }
}

TEST(Flags, OptionsComeInGroupOrder) {
    const auto all = Flags::options();

    for (size_t at = 1; at < all.size(); ++at) {
        EXPECT_LE(all[at - 1].group, all[at].group) << all[at].label;
    }
}

TEST(Flags, TheModesStartAtDefault) {
    EXPECT_EQ(Flags::presets(Flags::Set::uzdoom).size(), 10U);
    EXPECT_EQ(Flags::presets(Flags::Set::gzdoom).size(), 10U);
    EXPECT_EQ(Flags::presets(Flags::Set::zdoom).size(), 7U);
    EXPECT_EQ(Flags::presets(Flags::Set::zandronum).size(), 7U);
    EXPECT_TRUE(Flags::presets(Flags::Set::eternity).empty());
    EXPECT_TRUE(Flags::presets(Flags::Set::none).empty());

    for (const Flags::Set set : {Flags::Set::uzdoom, Flags::Set::gzdoom, Flags::Set::zdoom, Flags::Set::zandronum}) {
        const Flags::Preset &first = Flags::presets(set).front();

        EXPECT_EQ(first.label, "Default");
        EXPECT_EQ(first.compatflags, 0U);
        EXPECT_EQ(first.compatflags2, 0U);
    }
}

TEST(Flags, TheModesDifferInTheSecondWordOnly) {
    const auto uzdoom = Flags::presets(Flags::Set::uzdoom);
    const auto gzdoom = Flags::presets(Flags::Set::gzdoom);

    for (size_t at = 0; at < uzdoom.size(); ++at) {
        EXPECT_EQ(uzdoom[at].label, gzdoom[at].label);
        EXPECT_EQ(uzdoom[at].compatflags, gzdoom[at].compatflags);
    }

    EXPECT_EQ(uzdoom[1].compatflags2, 1048834U);
    EXPECT_EQ(gzdoom[1].compatflags2, 16642U);
}

TEST(Flags, OnlyWholeDecimalNumbersParse) {
    EXPECT_EQ(Flags::parse("0"), 0U);
    EXPECT_EQ(Flags::parse("1024"), 1024U);
    EXPECT_EQ(Flags::parse("010"), 10U);
    EXPECT_EQ(Flags::parse("4294967295"), 4294967295U);

    for (const char *bad : {"", " 1", "1 ", "-1", "+1", "0x10", "1e3", "4294967296", "99999999999999999999"}) {
        EXPECT_FALSE(Flags::parse(bad).has_value()) << bad;
    }
}

TEST(Flags, TheSwitchIsTheCvarOrEternitysOwn) {
    EXPECT_EQ(Flags::switchFor(Flags::Set::gzdoom, Flags::Word::dmflags2), "+dmflags2");
    EXPECT_EQ(Flags::switchFor(Flags::Set::zandronum, Flags::Word::zacompatflags), "+zacompatflags");
    EXPECT_EQ(Flags::switchFor(Flags::Set::eternity, Flags::Word::dmflags), "-dmflags");
}

TEST(Flags, OnlyTheZdoomLineKeepsItsCompatibilityWords) {
    for (const Flags::Word word : {Flags::Word::compatflags, Flags::Word::compatflags2}) {
        const Flags::Sets kept = Flags::info(word).kept;

        EXPECT_NE(kept & Flags::bit(Flags::Set::uzdoom), 0);
        EXPECT_NE(kept & Flags::bit(Flags::Set::gzdoom), 0);
        EXPECT_NE(kept & Flags::bit(Flags::Set::zdoom), 0);
        EXPECT_EQ(kept & Flags::bit(Flags::Set::zandronum), 0);
    }

    EXPECT_EQ(Flags::info(Flags::Word::dmflags).kept, 0);
}

TEST(Flags, TheLastManStandingWeaponsStartAllowed) {
    EXPECT_EQ(Flags::info(Flags::Word::lmsWeapons).start, 1023U);
    EXPECT_EQ(Flags::known(Flags::Set::zandronum, Flags::Word::lmsWeapons), 1023U);
}
