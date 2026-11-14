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

constexpr std::array LINES = {
        PortLine::uzdoom,
        PortLine::gzdoom,
        PortLine::zdoom,
        PortLine::zandronum,
        PortLine::eternity,
};

PortLine lineOf(const char *program) {
    return Dialect::of(std::filesystem::path("/games") / program).line;
}

}

TEST(Flags, EachPortNameSpeaksItsLine) {
    EXPECT_EQ(lineOf("uzdoom"), PortLine::uzdoom);
    EXPECT_EQ(lineOf("gzdoom"), PortLine::gzdoom);
    EXPECT_EQ(lineOf("vkdoom"), PortLine::gzdoom);
    EXPECT_EQ(lineOf("zdoom"), PortLine::zdoom);
    EXPECT_EQ(lineOf("zandronum"), PortLine::zandronum);
    EXPECT_EQ(lineOf("eternity"), PortLine::eternity);
    EXPECT_EQ(lineOf("dsda-doom"), PortLine::dsda);
    EXPECT_EQ(lineOf("nyan-doom"), PortLine::nyan);
    EXPECT_EQ(lineOf("prboom-plus"), PortLine::prboom);
    EXPECT_EQ(lineOf("glboom-plus"), PortLine::prboom);
    EXPECT_EQ(lineOf("woof"), PortLine::woof);
    EXPECT_EQ(lineOf("cherry-doom"), PortLine::woof);
    EXPECT_EQ(lineOf("nugget-doom"), PortLine::woof);
    EXPECT_EQ(lineOf("chocolate-doom"), PortLine::chocolate);
    EXPECT_EQ(lineOf("crispy-doom"), PortLine::crispy);
    EXPECT_EQ(lineOf("helion"), PortLine::helion);

    for (const char *program : {"boom", "mbf", "doomlegacy", "doom2"}) {
        EXPECT_EQ(lineOf(program), PortLine::none) << program;
    }
}

TEST(Flags, ZandronumPickedByHandSpeaksItsOwnLine) {
    const Dialect::Port picked = Dialect::of(std::filesystem::path("/games/zan"), "zandronum");

    EXPECT_EQ(picked.line, PortLine::zandronum);
    EXPECT_FALSE(picked.teamPick);
    EXPECT_EQ(Dialect::of(std::filesystem::path("/games/zan"), "zdoom").line, PortLine::zdoom);
    EXPECT_NE(Dialect::choice("zandronum"), nullptr);
}

TEST(Flags, OnlyTheStrtolPortsSpellTheTopBitNegative) {
    EXPECT_EQ(Flags::decimal(PortLine::zdoom, 2147483647U), "2147483647");
    EXPECT_EQ(Flags::decimal(PortLine::zdoom, 2147483648U), "-2147483648");
    EXPECT_EQ(Flags::decimal(PortLine::zandronum, 4294967295U), "-1");
    EXPECT_EQ(Flags::decimal(PortLine::gzdoom, 4294967295U), "4294967295");
    EXPECT_EQ(Flags::decimal(PortLine::eternity, 32U), "32");
}

TEST(Flags, AnOptionNamesOnlyPortsItsWordHas) {
    for (const Flags::Option &each : Flags::options()) {
        EXPECT_NE(each.lines, 0) << each.label;
        EXPECT_EQ(each.lines & ~Flags::info(each.word).lines, 0) << each.label;
    }
}

TEST(Flags, NoTwoOptionsShareABitOnOnePort) {
    for (const PortLine line : LINES) {
        for (size_t at = 0; at < Flags::WORDS; ++at) {
            const auto word = static_cast<Flags::Word>(at);
            std::uint32_t taken = 0;

            for (const Flags::Option &each : Flags::options()) {
                if (each.word != word || !has(each.lines, line)) {
                    continue;
                }

                EXPECT_EQ(taken & each.mask, 0U) << each.label << " on line " << static_cast<int>(line);

                taken |= each.mask;
            }

            EXPECT_EQ(taken, Flags::known(line, word));
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
    for (const PortLine line : LINES) {
        std::set<std::string> seen;

        for (const Flags::Option &each : Flags::options()) {
            if (!has(each.lines, line)) {
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
    EXPECT_EQ(Flags::modes(PortLine::uzdoom).size(), 10U);
    EXPECT_EQ(Flags::modes(PortLine::gzdoom).size(), 10U);
    EXPECT_EQ(Flags::modes(PortLine::zdoom).size(), 7U);
    EXPECT_EQ(Flags::modes(PortLine::zandronum).size(), 7U);
    EXPECT_TRUE(Flags::modes(PortLine::eternity).empty());
    EXPECT_TRUE(Flags::modes(PortLine::none).empty());

    for (const PortLine line : {PortLine::uzdoom, PortLine::gzdoom, PortLine::zdoom, PortLine::zandronum}) {
        const Flags::Mode &first = Flags::modes(line).front();

        EXPECT_EQ(first.label, "Default");
        EXPECT_EQ(first.compatflags, 0U);
        EXPECT_EQ(first.compatflags2, 0U);
    }
}

TEST(Flags, TheModesDifferInTheSecondWordOnly) {
    const auto uzdoom = Flags::modes(PortLine::uzdoom);
    const auto gzdoom = Flags::modes(PortLine::gzdoom);

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
    EXPECT_EQ(Flags::switchFor(PortLine::gzdoom, Flags::Word::dmflags2), "+dmflags2");
    EXPECT_EQ(Flags::switchFor(PortLine::zandronum, Flags::Word::zacompatflags), "+zacompatflags");
    EXPECT_EQ(Flags::switchFor(PortLine::eternity, Flags::Word::dmflags), "-dmflags");
}

TEST(Flags, OnlyTheZdoomLineKeepsItsCompatibilityWords) {
    for (const Flags::Word word : {Flags::Word::compatflags, Flags::Word::compatflags2}) {
        const PortLines kept = Flags::info(word).kept;

        EXPECT_NE(kept & bitOf(PortLine::uzdoom), 0);
        EXPECT_NE(kept & bitOf(PortLine::gzdoom), 0);
        EXPECT_NE(kept & bitOf(PortLine::zdoom), 0);
        EXPECT_EQ(kept & bitOf(PortLine::zandronum), 0);
    }

    EXPECT_EQ(Flags::info(Flags::Word::dmflags).kept, 0);
}

TEST(Flags, TheLastManStandingWeaponsStartAllowed) {
    EXPECT_EQ(Flags::info(Flags::Word::lmsWeapons).start, 1023U);
    EXPECT_EQ(Flags::known(PortLine::zandronum, Flags::Word::lmsWeapons), 1023U);
}
