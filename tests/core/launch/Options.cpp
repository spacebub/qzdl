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
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "core/config/Options.h"
#include "core/launch/Dialect.h"
#include "core/launch/dialects/Dialects.h"

namespace {

using Dialect::Feature;

Dialect::Port named(const char *program) {
    return Dialect::of(std::filesystem::path("/games") / program);
}

constexpr std::array PROGRAMS = {
        "gzdoom",
        "uzdoom",
        "zdoom",
        "zandronum",
        "dsda-doom",
        "nyan-doom",
        "prboom-plus",
        "woof",
        "nugget-doom",
        "cherry-doom",
        "chocolate-doom",
        "crispy-doom",
        "eternity",
        "helion",
        "crispy-heretic",
        "boom",
        "chocolate-strife",
        "mystery-engine",
};

// DSDA-Doom and Nyan Doom stop on any switch outside their table.
constexpr std::array DSDA_KNOWS = std::to_array<std::string_view>({
        "-levelstat",
        "-noautoload",
        "-nomusic",
        "-nosfx",
        "-pistolstart",
        "-coop_spawns",
        "-chain_episodes",
        "-stroller",
        "-game_speed",
        "-turbo",
        "-nocheats",
});

constexpr Dialect::Features GZDOOM_OPTIONS =
        Feature::pistolStart | Feature::cheats | Feature::alwaysApplyDmflags | Feature::turbo;
constexpr Dialect::Features DSDA_OPTIONS = Feature::pistolStart | Feature::coopSpawns | Feature::chainEpisodes
                                           | Feature::noCheats | Feature::stroller | Feature::turbo
                                           | Feature::gameSpeed;
constexpr Dialect::Features WOOF_OPTIONS = Feature::pistolStart | Feature::coopSpawns | Feature::noCheats
                                           | Feature::noOptions | Feature::mbfBeta | Feature::turbo | Feature::gameSpeed
                                           | Feature::dogs;

constexpr std::array EXPECTED_OPTIONS = std::to_array<std::pair<const char *, Dialect::Features>>({
        {"gzdoom", GZDOOM_OPTIONS},
        {"uzdoom", GZDOOM_OPTIONS | Feature::coopPreset},
        {"zdoom", Feature::cheats | Feature::alwaysApplyDmflags | Feature::turbo},
        {"zandronum", Feature::cheats | Feature::alwaysApplyDmflags | Feature::turbo},
        {"mystery-engine", GZDOOM_OPTIONS},
        {"dsda-doom", DSDA_OPTIONS},
        {"nyan-doom", DSDA_OPTIONS | Feature::limitRemoving},
        {"prboom-plus",
         Feature::pistolStart | Feature::coopSpawns | Feature::noCheats | Feature::stroller | Feature::turbo
                 | Feature::dogs},
        {"woof", WOOF_OPTIONS},
        {"nugget-doom", WOOF_OPTIONS},
        {"cherry-doom", WOOF_OPTIONS},
        {"chocolate-doom", Feature::noCheats | Feature::turbo},
        {"crispy-doom",
         Feature::pistolStart | Feature::noCheats | Feature::turbo | Feature::coopSpawns | Feature::doubleAmmo
                 | Feature::flipLevels | Feature::flipWeapons},
        {"crispy-heretic", Feature::pistolStart},
        {"chocolate-strife", {}},
        {"eternity", Feature::vanillaMode | Feature::turbo | Feature::gameSpeed | Feature::dogs},
        {"helion", Feature::pistolStart},
        {"boom", {}},
        {"mbf", {}},
        {"legacy", {}},
        {"doom2", {}},
});

}

TEST(Options, PistolAndCheatsLeadTheRulesAndKeepKeysOfTheirOwn) {
    std::vector<Feature> rules;

    for (const Options::Option &option : Options::ALL) {
        const bool own = option.feature == Feature::pistolStart || option.feature == Feature::cheats;
        const bool rule = option.group == Options::Group::rules;

        EXPECT_NE(option.key, nullptr) << option.label;
        EXPECT_NE(option.iniKey, nullptr) << option.label;
        EXPECT_EQ(option.place == Options::Place::profile, own || !rule) << option.label;

        if (rule) {
            rules.push_back(option.feature);
        }
    }

    ASSERT_GE(rules.size(), 2U);
    EXPECT_EQ(rules[0], Feature::pistolStart);
    EXPECT_EQ(rules[1], Feature::cheats);
}

TEST(Options, OnlyMusicAndSoundEffectsReadOnWhileTheirSwitchIsOff) {
    for (const Options::Option &option : Options::ALL) {
        const bool sound = option.feature == Feature::nomusic || option.feature == Feature::nosfx;

        EXPECT_EQ(option.inverted, sound) << option.label;
        EXPECT_EQ(option.group == Options::Group::sound, sound) << option.label;
    }
}

TEST(Options, EachRowIsFoundByItsFeature) {
    Dialect::Features seen;
    Dialect::Features rules;

    for (std::size_t at = 0; at < Options::ALL.size(); ++at) {
        const Feature feature = Options::ALL[at].feature;

        EXPECT_FALSE(seen.has(feature)) << Options::ALL[at].label;
        EXPECT_EQ(Options::indexOf(feature), at);

        seen |= feature;

        if (Options::ALL[at].group == Options::Group::rules) {
            rules |= feature;
        }
    }

    EXPECT_EQ(rules, Options::RULES);
    EXPECT_EQ(Options::indexOf(Feature::skill), Options::ALL.size());
}

TEST(Options, EveryOptionHasABaselineSpelling) {
    for (const Options::Option &option : Options::ALL) {
        EXPECT_FALSE(Dialect::arg(Dialect::Port{}, option.feature).name.empty()) << option.label;
    }
}

TEST(Options, EachPortOffersItsOwnOptions) {
    for (const auto &[program, options] : EXPECTED_OPTIONS) {
        EXPECT_EQ(named(program).features & Options::RULES, options) << program;
    }
}

TEST(Options, ANumberHasARangeAndAToggleNone) {
    for (const char *program : PROGRAMS) {
        const Dialect::Port port = named(program);

        for (const Options::Option &option : Options::ALL) {
            if (!port.has(option.feature)) {
                continue;
            }

            const Dialect::Arg &arg = Dialect::arg(port, option.feature);
            const bool number = option.kind == Options::Kind::number;

            EXPECT_EQ(number, arg.range.least < arg.range.most) << program << ' ' << arg.name;
            EXPECT_TRUE(number || (arg.range.least == 0 && arg.range.most == 0)) << program << ' ' << arg.name;
            EXPECT_TRUE(!number || arg.word.empty()) << program << ' ' << arg.name;
            EXPECT_TRUE(arg.name.starts_with('-') || arg.name.starts_with('+')) << program << ' ' << arg.name;
        }
    }
}

TEST(Options, TheDescriptionsArePlainSentences) {
    for (const char *program : PROGRAMS) {
        const Dialect::Port port = named(program);

        for (const Options::Option &option : Options::ALL) {
            if (!port.has(option.feature)) {
                continue;
            }

            const std::string said(Dialect::describe(port, option.feature));

            EXPECT_TRUE(said.ends_with('.')) << program << ' ' << option.label;
            EXPECT_EQ(said.find(';'), std::string::npos) << program << ' ' << option.label;
            EXPECT_EQ(said.find(" - "), std::string::npos) << program << ' ' << option.label;
        }
    }
}

TEST(Options, EveryPortIsSound) {
    for (const char *program : PROGRAMS) {
        EXPECT_TRUE(Dialect::sound(named(program))) << program;
    }
}

TEST(Options, AChangeTwiceOrToTheBaselineIsNotSound) {
    const std::array twice = std::to_array<Dialect::Spelling>({
            {Feature::turbo, {"-turbo", "One.", {}, Dialect::Range{.least = 10, .most = 400}}},
            {Feature::turbo, {"-turbo", "Two.", {}, Dialect::Range{.least = 10, .most = 300}}},
    });
    const std::array same = std::to_array<Dialect::Spelling>({
            {Feature::turbo, Dialect::arg(Dialect::Port{}, Feature::turbo)},
    });

    EXPECT_FALSE(Dialect::sound(Dialect::Port{.features = {}, .changes = twice}));
    EXPECT_FALSE(Dialect::sound(Dialect::Port{.features = {}, .changes = same}));
}

TEST(Options, AnOptionSpelledAgainstItsKindIsNotSound) {
    const std::array noRange = std::to_array<Dialect::Spelling>({
            {Feature::turbo, {"-turbo", "No range."}},
    });
    const std::array ranged = std::to_array<Dialect::Spelling>({
            {Feature::cheats, {"+sv_cheats", "A range.", "1", Dialect::Range{.least = 0, .most = 1}}},
    });
    const std::array unnamed = std::to_array<Dialect::Spelling>({
            {Feature::cheats, {"", "No name.", "1"}},
    });

    EXPECT_FALSE(Dialect::sound(Dialect::Port{.features = Feature::turbo, .changes = noRange}));
    EXPECT_FALSE(Dialect::sound(Dialect::Port{.features = Feature::cheats, .changes = ranged}));
    EXPECT_FALSE(Dialect::sound(Dialect::Port{.features = Feature::cheats, .changes = unnamed}));
    EXPECT_TRUE(Dialect::sound(Dialect::Port{.features = {}, .changes = noRange})) << "an unused change does nothing";
}

TEST(Options, DsdaIsHandedOnlySwitchesItKnows) {
    for (const char *program : {"dsda-doom", "nyan-doom"}) {
        const Dialect::Port port = named(program);

        for (const Options::Option &option : Options::ALL) {
            if (!port.has(option.feature) || option.feature == Feature::limitRemoving) {
                continue;
            }

            const std::string_view name = Dialect::arg(port, option.feature).name;

            EXPECT_NE(std::ranges::find(DSDA_KNOWS, name), DSDA_KNOWS.end()) << program << ' ' << name;
        }
    }

    EXPECT_TRUE(named("nyan-doom").has(Feature::limitRemoving));
    EXPECT_FALSE(named("dsda-doom").has(Feature::limitRemoving));
}

TEST(Options, HelionTakesOnlyThePistolOptionAndTheDosPortsNone) {
    EXPECT_EQ(named("helion").features & Options::RULES, Dialect::Features(Feature::pistolStart));

    const Dialect::Port dos = Dialect::of(PortEntry{.name = "DOS", .file = "/games/doom2.exe", .dosbox = true});

    EXPECT_EQ(dos.features & Options::RULES, Dialect::Features());
}

TEST(Options, TheTurboRangeFollowsThePort) {
    EXPECT_EQ(Dialect::arg(named("gzdoom"), Feature::turbo).range.most, 255);
    EXPECT_EQ(Dialect::arg(named("dsda-doom"), Feature::turbo).range.most, 255);
    EXPECT_EQ(Dialect::arg(named("woof"), Feature::turbo).range.most, 400);
    EXPECT_EQ(Dialect::arg(named("chocolate-doom"), Feature::turbo).range.most, 400);
}

TEST(Options, ANumberHasToBeWholeAndInRange) {
    const Dialect::Arg &turbo = Dialect::arg(named("dsda-doom"), Feature::turbo);

    EXPECT_EQ(Dialect::parse(turbo, "10"), 10);
    EXPECT_EQ(Dialect::parse(turbo, "255"), 255);

    for (const char *bad : {"", "9", "256", "1e2", "-50", " 50", "x"}) {
        EXPECT_FALSE(Dialect::parse(turbo, bad).has_value()) << bad;
    }

    EXPECT_EQ(Dialect::parse(Dialect::arg(named("woof"), Feature::dogs), "0"), 0);
}

TEST(Options, ASpellingCarriesItsOwnWord) {
    const Dialect::Arg &always = Dialect::arg(named("zandronum"), Feature::alwaysApplyDmflags);

    EXPECT_EQ(always.name, "+alwaysapplydmflags");
    EXPECT_EQ(always.word, "1");
}

TEST(Options, GzdoomStartsWithAPistolByACvarThatNothingRefuses) {
    const Dialect::Arg &cvar = Dialect::arg(named("gzdoom"), Feature::pistolStart);
    const Dialect::Arg &flag = Dialect::arg(named("woof"), Feature::pistolStart);

    EXPECT_EQ(cvar.name, "+sv_pistolstart");
    EXPECT_EQ(cvar.word, "1");
    EXPECT_EQ(cvar.refused, 0U);
    EXPECT_EQ(flag.name, "-pistolstart");
    EXPECT_EQ(flag.refused, Dialect::RECORDING | Dialect::PLAYBACK | Dialect::NETGAME);
}
