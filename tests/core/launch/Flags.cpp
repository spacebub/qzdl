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
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "core/config/Config.h"
#include "core/config/Profile.h"
#include "core/config/Schema.h"
#include "core/launch/Dialect.h"
#include "core/launch/Flags.h"
#include "core/launch/KnownPorts.h"

namespace {

constexpr std::array LINES = {
        PortLine::uzdoom,
        PortLine::gzdoom,
        PortLine::zdoom28,
        PortLine::zdoom26,
        PortLine::zdoom25,
        PortLine::zandronum32,
        PortLine::zandronum31,
        PortLine::zandronum30,
        PortLine::skulltag,
        PortLine::eternity,
};

Dialect::Traits traitsOf(const char *file) {
    return Dialect::of(std::filesystem::path("/games") / file);
}

PortLine lineOf(const char *file) {
    return traitsOf(file).line;
}

// The bits of the word the newest release of the port is offered.
std::uint32_t knownOn(const char *file, const Flags::Word word) {
    const Dialect::Traits traits = traitsOf(file);

    return Flags::known(traits.line, traits.features, word);
}

bool signedWords(const char *file) {
    return traitsOf(file).has(Dialect::Feature::signedWords);
}

// Every mix of the features that gate an option, each one a release some port may run. Each reads every word.
std::vector<Dialect::Features> gateMixes() {
    std::vector<Dialect::Feature> gated;

    for (const Flags::Option &each : Flags::options()) {
        if (each.gate.feature != Dialect::Feature::count
            && std::ranges::find(gated, each.gate.feature) == gated.end()) {
            gated.push_back(each.gate.feature);
        }
    }

    EXPECT_LT(gated.size(), 8U);

    std::vector<Dialect::Features> releases;

    for (unsigned mix = 0; mix < (1U << gated.size()); ++mix) {
        Dialect::Features features;

        for (const Flags::Word word : Flags::ALL) {
            features |= Flags::feature(word);
        }

        for (size_t at = 0; at < gated.size(); ++at) {
            if (((mix >> at) & 1U) != 0) {
                features |= gated[at];
            }
        }

        releases.push_back(features);
    }

    return releases;
}

// The labels a release is offered on some bits of one word.
std::set<std::string_view> offeredAt(const Dialect::Traits &traits, const Flags::Word word, const std::uint32_t mask) {
    std::set<std::string_view> labels;

    for (const Flags::Option &each : Flags::options()) {
        if (each.word == word && (each.mask & mask) != 0 && Flags::offered(each, traits.line, traits.features)) {
            labels.insert(each.label);
        }
    }

    return labels;
}

}

TEST(Flags, EachPortNameSpeaksItsLine) {
    EXPECT_EQ(lineOf("uzdoom"), PortLine::uzdoom);
    EXPECT_EQ(lineOf("gzdoom"), PortLine::gzdoom);
    EXPECT_EQ(lineOf("vkdoom"), PortLine::gzdoom);
    EXPECT_EQ(lineOf("zdoom"), PortLine::zdoom28);
    EXPECT_EQ(lineOf("zandronum"), PortLine::zandronum32);
    EXPECT_EQ(lineOf("skulltag"), PortLine::skulltag);
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

    for (const char *file : {"boom", "mbf", "doomlegacy", "doom2"}) {
        EXPECT_EQ(lineOf(file), PortLine::none) << file;
    }
}

TEST(Flags, ZandronumPickedByHandSpeaksItsOwnLine) {
    const Dialect::Traits picked = Dialect::of(std::filesystem::path("/games/zan"), Port::Port::zandronum);

    EXPECT_EQ(picked.line, PortLine::zandronum32);
    EXPECT_FALSE(picked.has(Dialect::Feature::teamPick));
    EXPECT_EQ(Dialect::of(std::filesystem::path("/games/zan"), Port::Port::zdoom).line, PortLine::zdoom28);
    EXPECT_NE(Port::withId("zandronum"), nullptr);
}

TEST(Flags, OnlyTheStrtolPortsSpellTheTopBitNegative) {
    EXPECT_EQ(Flags::decimal(2147483647U, signedWords("zdoom")), "2147483647");
    EXPECT_EQ(Flags::decimal(2147483648U, signedWords("zdoom")), "-2147483648");
    EXPECT_EQ(Flags::decimal(4294967295U, signedWords("zandronum")), "-1");
    EXPECT_EQ(Flags::decimal(4294967295U, signedWords("gzdoom")), "4294967295");
    EXPECT_EQ(Flags::decimal(32U, signedWords("eternity")), "32");
}

TEST(Flags, AnOptionNamesOnlyPortsItsWordHas) {
    for (const Flags::Option &each : Flags::options()) {
        EXPECT_NE(each.lines, 0) << each.label;

        for (const char *file : {"uzdoom", "gzdoom", "zdoom", "zandronum", "skulltag", "eternity"}) {
            const Dialect::Traits traits = traitsOf(file);

            EXPECT_TRUE(!has(each.lines, traits.line) || traits.has(Flags::feature(each.word)))
                    << each.label << " on " << file;
        }
    }
}

TEST(Flags, AnOlderReleaseIsOfferedNoOptionOfAWordItDoesNotRead) {
    for (const Port::Known &port : Port::known()) {
        for (const Dialect::VersionRange &range : port.releases) {
            const Dialect::Traits traits = Dialect::of(Engine{
                    .name = "Port", .file = "/games/port", .port = port.port, .version = std::string(range.first)});

            for (const Flags::Option &each : Flags::options()) {
                EXPECT_TRUE(!Flags::offered(each, traits.line, traits.features)
                            || traits.has(Flags::feature(each.word)))
                        << each.label << " on " << port.id << " " << range.first;
            }
        }
    }

    const auto compatflags2 = [](const char *version) {
        const Dialect::Traits traits = Dialect::of(Engine{.name = "Port", .file = "/games/zdoom", .version = version});

        return Flags::known(traits.line, traits.features, Flags::Word::compatflags2);
    };

    EXPECT_EQ(compatflags2("2.5.0"), 0U);
    EXPECT_NE(compatflags2("2.6.0"), 0U);
}

TEST(Flags, NoTwoOptionsShareABitOnOnePort) {
    const std::vector<Dialect::Features> releases = gateMixes();

    for (const PortLine line : LINES) {
        for (size_t at = 0; at < Flags::WORDS; ++at) {
            const auto word = static_cast<Flags::Word>(at);

            for (const Dialect::Features &features : releases) {
                std::uint32_t taken = 0;

                for (const Flags::Option &each : Flags::options()) {
                    if (each.word != word || !Flags::offered(each, line, features)) {
                        continue;
                    }

                    EXPECT_EQ(taken & each.mask, 0U) << each.label << " on line " << static_cast<int>(line);

                    taken |= each.mask;
                }

                EXPECT_EQ(taken, Flags::known(line, features, word)) << static_cast<int>(line);
            }
        }
    }
}

TEST(Flags, CompatflagsBit25IsOfferedAsThePortReadsIt) {
    const auto offered = [](const Dialect::Traits &traits) {
        std::set<std::string_view> labels;

        for (const Flags::Option &each : Flags::options()) {
            if (each.word == Flags::Word::compatflags && each.mask == (1U << 25U)
                && Flags::offered(each, traits.line, traits.features)) {
                labels.insert(each.label);
            }
        }

        return labels;
    };
    const std::set<std::string_view> ghosts = {"Arch-vile ghosts"};
    const std::set<std::string_view> gibs = {"Crushed monsters turn to gibs"};

    EXPECT_EQ(offered(traitsOf("gzdoom")), ghosts);
    EXPECT_EQ(offered(traitsOf("uzdoom")), ghosts);
    EXPECT_EQ(offered(traitsOf("zdoom")), gibs);
    EXPECT_EQ(offered(traitsOf("zandronum")), gibs);
    EXPECT_EQ(offered(traitsOf("skulltag")), std::set<std::string_view>{"No crosshairs"});
    EXPECT_TRUE(offered(traitsOf("eternity")).empty());

    Dialect::Traits older = traitsOf("gzdoom");

    older.features |= Dialect::Feature::corpseGibs;

    EXPECT_EQ(offered(older), gibs);
}

TEST(Flags, AnOptionWithoutAGateGoesByTheLineAndTheWord) {
    const Flags::Option plain{.word = Flags::Word::dmflags,
                              .mask = 1U,
                              .lines = Lines::GZDOOM,
                              .group = Flags::Group::monsters,
                              .label = "x",
                              .cvar = {},
                              .description = "x.",
                              .choices = {},
                              .gate = {}};

    EXPECT_TRUE(Flags::offered(plain, PortLine::gzdoom, Dialect::Feature::dmflags));
    EXPECT_TRUE(Flags::offered(plain, PortLine::gzdoom, Dialect::Feature::dmflags | Dialect::Feature::corpseGibs));
    EXPECT_FALSE(Flags::offered(plain, PortLine::zdoom28, Dialect::Feature::dmflags));
    EXPECT_FALSE(Flags::offered(plain, PortLine::gzdoom, {}));
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

TEST(Flags, OneLabelPerPortReleaseAndWordFamily) {
    for (const PortLine line : LINES) {
        for (const Dialect::Features &features : gateMixes()) {
            std::set<std::string> seen;

            for (const Flags::Option &each : Flags::options()) {
                if (!Flags::offered(each, line, features)) {
                    continue;
                }

                const std::string key = std::to_string(static_cast<int>(Flags::info(each.word).family)) + " "
                                        + std::to_string(static_cast<int>(each.group)) + " " + std::string(each.label);

                EXPECT_TRUE(seen.insert(key).second) << each.label << " twice in one group";
            }
        }
    }
}

TEST(Flags, ZdoomBefore2_8_0ForbidsMouseLookAndRespawnsBigPowerupsWithDmflagsBits) {
    const auto at = [](const char *version) {
        return Dialect::of(Engine{.name = "Port", .file = "/games/zdoom", .version = version});
    };
    const std::uint32_t look = (1U << 18U) | (1U << 19U);
    const std::uint32_t respawnSuper = 1U << 27U;

    for (const char *version : {"", "2.8.1", "2.8.0"}) {
        EXPECT_EQ(offeredAt(at(version), Flags::Word::dmflags, look), std::set<std::string_view>{"Mouse look"})
                << version;
        EXPECT_EQ(offeredAt(at(version), Flags::Word::dmflags2, respawnSuper),
                  std::set<std::string_view>{"Big powerups respawn"})
                << version;
    }

    for (const char *version : {"2.7.1", "2.7.0", "2.6.1", "2.5.0"}) {
        EXPECT_EQ(offeredAt(at(version), Flags::Word::dmflags, 1U << 18U), std::set<std::string_view>{"No mouse look"})
                << version;
        EXPECT_EQ(offeredAt(at(version), Flags::Word::dmflags, 1U << 19U),
                  std::set<std::string_view>{"Big powerups respawn"})
                << version;
        EXPECT_TRUE(offeredAt(at(version), Flags::Word::dmflags2, respawnSuper).empty()) << version;
    }

    for (const char *version : {"", "3.2", "3.1", "3.0"}) {
        const Dialect::Traits zandronum =
                Dialect::of(Engine{.name = "Port", .file = "/games/zandronum", .version = version});

        EXPECT_EQ(offeredAt(zandronum, Flags::Word::dmflags, look),
                  (std::set<std::string_view>{"No mouse look", "Big powerups respawn"}))
                << version;
    }

    EXPECT_EQ(offeredAt(traitsOf("gzdoom"), Flags::Word::dmflags, look), std::set<std::string_view>{"Mouse look"});
    EXPECT_EQ(offeredAt(traitsOf("uzdoom"), Flags::Word::dmflags, look), std::set<std::string_view>{"Mouse look"});
}

TEST(Flags, SkulltagOffersTheMouseLookPowerupAndFieldOfViewBitsAtTheirOwnPlaces) {
    for (const char *version : {"", "0.98a", "0.97d"}) {
        const Dialect::Traits skulltag =
                Dialect::of(Engine{.name = "Port", .file = "/games/skulltag", .version = version});

        EXPECT_EQ(offeredAt(skulltag, Flags::Word::dmflags, 1U << 17U), std::set<std::string_view>{"No mouse look"})
                << version;
        EXPECT_EQ(offeredAt(skulltag, Flags::Word::dmflags, 1U << 18U),
                  std::set<std::string_view>{"Big powerups respawn"})
                << version;
        EXPECT_EQ(offeredAt(skulltag, Flags::Word::dmflags, 1U << 19U),
                  std::set<std::string_view>{"Lock the field of view"})
                << version;
    }
}

TEST(Flags, EachReleaseKnowsExactlyTheBitsItIsOffered) {
    for (const Port::Known &port : Port::known()) {
        std::vector<std::string> versions = {""};

        for (const Dialect::VersionRange &each : port.releases) {
            versions.emplace_back(each.first);
        }

        for (const std::string &version : versions) {
            const Dialect::Traits traits =
                    Dialect::of(Engine{.name = "Port", .file = "/games/port", .port = port.port, .version = version});

            for (size_t at = 0; at < Flags::WORDS; ++at) {
                const auto word = static_cast<Flags::Word>(at);
                std::uint32_t offered = 0;

                for (const Flags::Option &each : Flags::options()) {
                    if (each.word == word && Flags::offered(each, traits.line, traits.features)) {
                        offered |= each.mask;
                    }
                }

                EXPECT_EQ(Flags::known(traits.line, traits.features, word), offered)
                        << port.id << " " << version << " word " << at;
            }
        }
    }

    const auto zdoom = [](const char *version) {
        const Dialect::Traits traits = Dialect::of(Engine{.name = "Port", .file = "/games/zdoom", .version = version});

        return Flags::known(traits.line, traits.features, Flags::Word::dmflags2) & (1U << 27U);
    };

    EXPECT_EQ(zdoom("2.7.1"), 0U);
    EXPECT_NE(zdoom("2.8.0"), 0U);
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
    EXPECT_EQ(Flags::modes(PortLine::zdoom28).size(), 7U);
    EXPECT_EQ(Flags::modes(PortLine::zandronum32).size(), 7U);
    EXPECT_EQ(Flags::modes(PortLine::skulltag).size(), 5U);
    EXPECT_TRUE(Flags::modes(PortLine::eternity).empty());
    EXPECT_TRUE(Flags::modes(PortLine::none).empty());

    for (const PortLine line : LINES) {
        if (line == PortLine::eternity) {
            continue;
        }

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
    EXPECT_EQ(Dialect::arg(traitsOf("gzdoom"), Flags::feature(Flags::Word::dmflags2)).name, "+dmflags2");
    EXPECT_EQ(Dialect::arg(traitsOf("zandronum"), Flags::feature(Flags::Word::zacompatflags)).name, "+zacompatflags");
    EXPECT_EQ(Dialect::arg(traitsOf("eternity"), Flags::feature(Flags::Word::dmflags)).name, "-dmflags");
}

TEST(Flags, EachPortReadsItsOwnWords) {
    EXPECT_TRUE(traitsOf("gzdoom").has(Flags::feature(Flags::Word::dmflags3)));
    EXPECT_FALSE(traitsOf("zdoom").has(Flags::feature(Flags::Word::dmflags3)));
    EXPECT_TRUE(traitsOf("zandronum").has(Flags::feature(Flags::Word::lmsSpectators)));
    EXPECT_FALSE(traitsOf("gzdoom").has(Flags::feature(Flags::Word::zadmflags)));
    EXPECT_TRUE(traitsOf("eternity").has(Flags::feature(Flags::Word::dmflags)));
    EXPECT_FALSE(traitsOf("eternity").has(Flags::feature(Flags::Word::dmflags2)));

    for (const char *file : {"dsda-doom", "prboom-plus", "woof", "chocolate-doom", "helion", "boom", "doom2"}) {
        for (const Flags::Word word : Flags::ALL) {
            EXPECT_FALSE(traitsOf(file).has(Flags::feature(word))) << file;
        }
    }
}

TEST(Flags, OnlyTheZdoomLineKeepsItsCompatibilityWords) {
    EXPECT_TRUE(traitsOf("uzdoom").has(Dialect::Feature::keepsCompatflags));
    EXPECT_TRUE(traitsOf("gzdoom").has(Dialect::Feature::keepsCompatflags));
    EXPECT_TRUE(traitsOf("zdoom").has(Dialect::Feature::keepsCompatflags));
    EXPECT_FALSE(traitsOf("zandronum").has(Dialect::Feature::keepsCompatflags));
    EXPECT_FALSE(traitsOf("skulltag").has(Dialect::Feature::keepsCompatflags));
    EXPECT_FALSE(traitsOf("eternity").has(Dialect::Feature::keepsCompatflags));
}

TEST(Flags, TheLastManStandingWeaponsStartAllowed) {
    EXPECT_EQ(Flags::info(Flags::Word::lmsWeapons).start, 1023U);
    EXPECT_EQ(knownOn("zandronum", Flags::Word::lmsWeapons), 1023U);
}

TEST(Flags, SkulltagLabelsItsOwnBitsAndKnowsEveryBitItReads) {
    const auto labels = [](const Flags::Word word, const std::uint32_t mask) {
        std::set<std::string_view> out;

        for (const Flags::Option &each : Flags::options()) {
            if (each.word == word && (each.mask & mask) != 0 && has(each.lines, PortLine::skulltag)) {
                out.insert(each.label);
            }
        }

        return out;
    };

    EXPECT_EQ(labels(Flags::Word::dmflags, 1U << 24U), std::set<std::string_view>{"Lose weapons"});
    EXPECT_EQ(labels(Flags::Word::dmflags, 1U << 29U), std::set<std::string_view>{"Jumping"});
    EXPECT_EQ(labels(Flags::Word::dmflags, 1U << 30U), std::set<std::string_view>{"Crouching"});
    EXPECT_EQ(labels(Flags::Word::dmflags2, 1U << 13U), std::set<std::string_view>{"Keep teams between maps"});
    EXPECT_EQ(labels(Flags::Word::dmflags3, 1U << 0U), std::set<std::string_view>{"No target names"});
    EXPECT_EQ(labels(Flags::Word::compatflags, 1U << 17U), std::set<std::string_view>{"Limit air movement"});
    EXPECT_EQ(labels(Flags::Word::compatflags2, 1U << 2U), std::set<std::string_view>{"No land command"});
    EXPECT_EQ(labels(Flags::Word::dmflags, 1U << 12U), std::set<std::string_view>{"No monsters"});

    EXPECT_EQ(knownOn("skulltag", Flags::Word::dmflags), 0x7FFFFFDFU);
    EXPECT_EQ(knownOn("skulltag", Flags::Word::dmflags2), 0x3FFFFFFEU);
    EXPECT_EQ(knownOn("skulltag", Flags::Word::dmflags3), 0xFU);
    EXPECT_EQ(knownOn("skulltag", Flags::Word::compatflags), 0x7FFFFFFFU);
    EXPECT_EQ(knownOn("skulltag", Flags::Word::compatflags2), 0x7U);
    EXPECT_EQ(knownOn("skulltag", Flags::Word::lmsWeapons), 1023U);
    EXPECT_EQ(knownOn("skulltag", Flags::Word::lmsSpectators), 3U);
    EXPECT_EQ(knownOn("skulltag", Flags::Word::zadmflags), 0U);
    EXPECT_EQ(knownOn("skulltag", Flags::Word::zacompatflags), 0U);
}

TEST(Flags, SkulltagModesSetCompatflagsAlone) {
    const auto modes = Flags::modes(PortLine::skulltag);

    ASSERT_EQ(modes.size(), 5U);
    EXPECT_EQ(modes[1].label, "Doom");
    EXPECT_EQ(modes[1].compatflags, 1610627843U);
    EXPECT_EQ(modes[2].compatflags, 1610693399U);
    EXPECT_EQ(modes[3].label, "Boom");
    EXPECT_EQ(modes[3].compatflags, 43008U);
    EXPECT_EQ(modes[4].label, "ZDoom 2.0.63");
    EXPECT_EQ(modes[4].compatflags, 2048U);

    for (const Flags::Mode &each : modes) {
        EXPECT_EQ(each.compatflags2, 0U) << each.label;
    }
}

TEST(Flags, SkulltagJumpingAndCrouchingAreForcedWithBitsOfTheirOwn) {
    const auto choices = [](const std::string_view label) {
        for (const Flags::Option &each : Flags::options()) {
            if (each.label == label && has(each.lines, PortLine::skulltag)) {
                return each;
            }
        }

        return Flags::Option{};
    };

    const Flags::Option jumping = choices("Jumping");
    const Flags::Option crouching = choices("Crouching");

    EXPECT_EQ(jumping.mask, (1U << 16U) | (1U << 29U));
    ASSERT_EQ(jumping.choices.size(), 3U);
    EXPECT_EQ(jumping.choices[1].label, "Off");
    EXPECT_EQ(jumping.choices[1].value, 1U << 16U);
    EXPECT_EQ(jumping.choices[2].label, "On");
    EXPECT_EQ(jumping.choices[2].value, 1U << 29U);

    EXPECT_EQ(crouching.mask, (1U << 21U) | (1U << 30U));
    ASSERT_EQ(crouching.choices.size(), 3U);
    EXPECT_EQ(crouching.choices[1].value, 1U << 21U);
    EXPECT_EQ(crouching.choices[2].value, 1U << 30U);
}

TEST(Flags, AMovedSkulltagBitSaysWhatTheRowItCopiesSays) {
    for (const Flags::Option &each : Flags::options()) {
        if (each.lines != Lines::SKULLTAG || each.label == "No respawning") {
            continue;
        }

        const bool written = std::ranges::any_of(Flags::options(), [&each](const Flags::Option &other) {
            return other.lines != Lines::SKULLTAG && other.label == each.label && other.description == each.description
                   && other.group == each.group;
        });

        EXPECT_TRUE(written) << each.label;
    }
}

TEST(Flags, ASkulltagModeFillsCompatflagsAndLeavesCompatflags2) {
    Profile profile;

    profile.setFlag(Flags::Word::compatflags2, "4");

    // Boom, after the port's own and Default.
    Flags::setMode(PortLine::skulltag, profile, 4);

    EXPECT_EQ(profile.flag(Flags::Word::compatflags), "43008");
    EXPECT_EQ(profile.flag(Flags::Word::compatflags2), "4");
    EXPECT_EQ(Flags::modeIndex(PortLine::skulltag, profile), 4);

    profile.setFlag(Flags::Word::compatflags2, "");

    EXPECT_EQ(Flags::modeIndex(PortLine::skulltag, profile), 4);

    profile.setFlag(Flags::Word::compatflags2, "7");

    EXPECT_EQ(Flags::modeIndex(PortLine::skulltag, profile), 4);

    Flags::setMode(PortLine::skulltag, profile, 0);

    EXPECT_EQ(profile.flag(Flags::Word::compatflags), "");
    EXPECT_EQ(profile.flag(Flags::Word::compatflags2), "7");
    EXPECT_EQ(Flags::modeIndex(PortLine::skulltag, profile), 0);
}

TEST(Flags, AZdoomModeFillsBothWords) {
    Profile profile;

    profile.setFlag(Flags::Word::compatflags2, "4");
    Flags::setMode(PortLine::zandronum32, profile, 4);

    EXPECT_EQ(profile.flag(Flags::Word::compatflags), "567296");
    EXPECT_EQ(profile.flag(Flags::Word::compatflags2), "0");
    EXPECT_EQ(Flags::modeIndex(PortLine::zandronum32, profile), 4);

    profile.setFlag(Flags::Word::compatflags2, "");

    EXPECT_EQ(Flags::modeIndex(PortLine::zandronum32, profile), -1);
}

TEST(Flags, AZdoomModeFillsOnlyTheWordsTheReleaseReads) {
    const auto at = [](const char *version) {
        return Dialect::of(Engine{.name = "Port", .file = "/games/zdoom", .version = version});
    };
    const Dialect::Traits before = at("2.5.0");
    const Dialect::Traits after = at("2.6.0");
    Profile profile;

    // Boom, typed into compatflags alone.
    profile.setFlag(Flags::Word::compatflags, "567296");

    EXPECT_EQ(Flags::modeIndex(before.line, profile), 4);
    EXPECT_EQ(Flags::modeIndex(after.line, profile), -1);

    profile.setFlag(Flags::Word::compatflags, "");
    profile.setFlag(Flags::Word::compatflags2, "4");
    Flags::setMode(before.line, profile, 4);

    EXPECT_EQ(profile.flag(Flags::Word::compatflags), "567296");
    EXPECT_EQ(profile.flag(Flags::Word::compatflags2), "4");

    Flags::setMode(after.line, profile, 4);

    EXPECT_EQ(profile.flag(Flags::Word::compatflags2), "0");
}

namespace {

Dialect::Traits releaseOf(const char *file, const char *version) {
    const Port::Port port = Dialect::of(std::filesystem::path("/games") / file).port;

    return Dialect::of(Engine{.name = "Port", .file = std::string("/games/") + file, .port = port, .version = version});
}

std::uint32_t bitsOf(const std::initializer_list<unsigned> bits) {
    std::uint32_t out = 0;

    for (const unsigned at : bits) {
        out |= 1U << at;
    }

    return out;
}

// The mode of the label on the release, as the list offers it.
const Flags::Mode &modeOf(const Dialect::Traits &traits, const std::string_view label) {
    const std::span<const Flags::Mode> modes = Flags::modes(traits.line);

    return *std::ranges::find(modes, label, &Flags::Mode::label);
}

}

// Doom (strict) and Boom (strict) set compatflags2 bit 3 from 2.8.0. ZDoom 2.5.0 has no compatflags2 and no bit 31.
TEST(Flags, EachZdoomReleaseFillsTheModesItSetsItself) {
    const Dialect::Traits newest = releaseOf("zdoom", "2.8.1");
    const Dialect::Traits middle = releaseOf("zdoom", "2.7.1");
    const Dialect::Traits sixth = releaseOf("zdoom", "2.6.0");
    const Dialect::Traits oldest = releaseOf("zdoom", "2.5.0");

    EXPECT_EQ(modeOf(newest, "Doom (strict)").compatflags, 3122215895U);
    EXPECT_EQ(modeOf(newest, "Doom (strict)").compatflags2, 11U);
    EXPECT_EQ(modeOf(newest, "Boom (strict)").compatflags2, 8U);

    for (const Dialect::Traits &traits : {middle, sixth}) {
        EXPECT_EQ(modeOf(traits, "Doom").compatflags, 672676611U);
        EXPECT_EQ(modeOf(traits, "Doom").compatflags2, 2U);
        EXPECT_EQ(modeOf(traits, "Doom (strict)").compatflags, 3122215895U);
        EXPECT_EQ(modeOf(traits, "Doom (strict)").compatflags2, 3U);
        EXPECT_EQ(modeOf(traits, "Boom").compatflags, 567296U);
        EXPECT_EQ(modeOf(traits, "Boom (strict)").compatflags, 302622928U);
        EXPECT_EQ(modeOf(traits, "Boom (strict)").compatflags2, 0U);
        EXPECT_EQ(modeOf(traits, "MBF").compatflags, 92841984U);
        EXPECT_EQ(modeOf(traits, "ZDoom 2.0.63").compatflags, 536872960U);
    }

    EXPECT_EQ(modeOf(oldest, "Doom").compatflags, 672676611U);
    EXPECT_EQ(modeOf(oldest, "Doom (strict)").compatflags, 974732247U);
    EXPECT_EQ(modeOf(oldest, "Boom (strict)").compatflags, 302622928U);
    EXPECT_EQ(Flags::modes(oldest.line).size(), Flags::modes(newest.line).size());

    Profile profile;

    // As the port fills them for Doom (strict).
    profile.setFlag(Flags::Word::compatflags, "3122215895");
    profile.setFlag(Flags::Word::compatflags2, "3");

    EXPECT_EQ(Flags::modeIndex(sixth.line, profile), 3);
    EXPECT_EQ(Flags::modeIndex(newest.line, profile), -1);

    profile.setFlag(Flags::Word::compatflags, "974732247");

    EXPECT_EQ(Flags::modeIndex(oldest.line, profile), 3);
}

// A mode never sets a bit the release lacks, and the dialog offers none.
TEST(Flags, EachZdoomReleaseIsOfferedTheCompatibilityBitsItReads) {
    const auto known = [](const char *version, const Flags::Word word) {
        const Dialect::Traits traits = releaseOf("zdoom", version);

        return Flags::known(traits.line, traits.features, word);
    };

    EXPECT_EQ(known("2.8.0", Flags::Word::compatflags2), bitsOf({0, 1, 2, 3}));
    EXPECT_EQ(known("2.7.1", Flags::Word::compatflags2), bitsOf({0, 1}));
    EXPECT_EQ(known("2.6.0", Flags::Word::compatflags2), bitsOf({0, 1}));
    EXPECT_EQ(known("2.5.0", Flags::Word::compatflags2), 0U);
    EXPECT_EQ(known("2.6.0", Flags::Word::compatflags), 0xFFFFFFFFU);
    EXPECT_EQ(known("2.5.0", Flags::Word::compatflags), 0x3FFFFFFFU);

    for (const char *version : {"2.8.0", "2.7.1", "2.6.0", "2.5.0"}) {
        const Dialect::Traits traits = releaseOf("zdoom", version);

        for (const Flags::Mode &mode : Flags::modes(traits.line)) {
            EXPECT_EQ(mode.compatflags & ~known(version, Flags::Word::compatflags), 0U) << version << ' ' << mode.label;

            if (traits.has(Dialect::Feature::compatflags2)) {
                EXPECT_EQ(mode.compatflags2 & ~known(version, Flags::Word::compatflags2), 0U)
                        << version << ' ' << mode.label;
            }
        }
    }
}

TEST(Flags, EachZandronumReleaseIsOfferedTheBitsItReads) {
    const auto known = [](const char *version, const Flags::Word word) {
        const Dialect::Traits traits = releaseOf("zandronum", version);

        return Flags::known(traits.line, traits.features, word);
    };
    const std::uint32_t compat30 =
            bitsOf({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}) | bitsOf({17, 18, 19, 20, 21, 22, 23, 24, 25, 26});

    EXPECT_EQ(known("3.2.1", Flags::Word::zadmflags), 0x1FFFFFFFU);
    EXPECT_EQ(known("3.2", Flags::Word::zadmflags), 0x1FFFFFFFU);
    EXPECT_EQ(known("3.1", Flags::Word::zadmflags), 0x7FFFFFU);
    EXPECT_EQ(known("3.0.1", Flags::Word::zadmflags), 0x3FFFFU);
    EXPECT_EQ(known("3.0", Flags::Word::zadmflags), 0x3FFFFU);

    EXPECT_EQ(known("3.2", Flags::Word::zacompatflags), compat30 | bitsOf({13, 14}));
    EXPECT_EQ(known("3.1", Flags::Word::zacompatflags), compat30 | bitsOf({13, 14}));
    EXPECT_EQ(known("3.0", Flags::Word::zacompatflags), compat30);
}

// 3.1 renamed the cvar of zadmflags bit 8 and kept the old name as another.
TEST(Flags, ZandronumBit8GoesByTheCvarOfTheRelease) {
    const auto cvar = [](const char *version) {
        const Dialect::Traits traits = releaseOf("zandronum", version);
        std::vector<std::string_view> names;

        for (const Flags::Option &each : Flags::options()) {
            if (each.word == Flags::Word::zadmflags && each.mask == (1U << 8U)
                && Flags::offered(each, traits.line, traits.features)) {
                names.push_back(each.cvar);
            }
        }

        return names;
    };

    EXPECT_EQ(cvar("3.2"), std::vector<std::string_view>{"sv_forcevideodefaults"});
    EXPECT_EQ(cvar("3.1"), std::vector<std::string_view>{"sv_forcevideodefaults"});
    EXPECT_EQ(cvar("3.0.1"), std::vector<std::string_view>{"sv_forcegldefaults"});
}

TEST(Flags, InstantRespawnSaysItDropsTheDeathDelay) {
    const auto found = std::ranges::find(Flags::options(), "compat_instantrespawn", &Flags::Option::cvar);

    ASSERT_NE(found, Flags::options().end());
    EXPECT_EQ(found->description, "Dead players can respawn at once, without the one second wait after death.");
}

TEST(Flags, ZandronumTakesSpectatorVoiceChatFrom3_2) {
    const auto known = [](const char *version) {
        const Dialect::Traits traits = releaseOf("zandronum", version);

        return Flags::known(traits.line, traits.features, Flags::Word::lmsSpectators);
    };

    EXPECT_EQ(known(""), bitsOf({0, 1, 2}));
    EXPECT_EQ(known("3.2.1"), bitsOf({0, 1, 2}));
    EXPECT_EQ(known("3.2"), bitsOf({0, 1, 2}));
    EXPECT_EQ(known("3.1"), bitsOf({0, 1}));
    EXPECT_EQ(known("3.0.1"), bitsOf({0, 1}));
    EXPECT_EQ(known("3.0"), bitsOf({0, 1}));
    EXPECT_EQ(knownOn("skulltag", Flags::Word::lmsSpectators), bitsOf({0, 1}));
}

// From doomdef.h of each release from 2.5.0. dmflags2 bit 26 comes in 2.6.0 and bit 27 in 2.8.0. Older releases run as
// 2.5.0.
TEST(Flags, EachZdoomReleaseIsOfferedTheGameplayBitsItReads) {
    const auto known = [](const char *version, const Flags::Word word) {
        const Dialect::Traits traits = releaseOf("zdoom", version);

        return Flags::known(traits.line, traits.features, word);
    };
    const std::uint32_t dmflags = 0x7FFFFFFFU & ~bitsOf({5});
    const std::uint32_t dmflags2 =
            bitsOf({1, 4, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18}) | bitsOf({19, 20, 21, 22, 23, 24, 25});

    for (const char *version : {"2.8.1", "2.8.0", "2.7.1", "2.6.0", "2.5.0"}) {
        EXPECT_EQ(known(version, Flags::Word::dmflags), dmflags) << version;
    }

    EXPECT_EQ(known("2.8.1", Flags::Word::dmflags2), dmflags2 | bitsOf({26, 27}));
    EXPECT_EQ(known("2.8.0", Flags::Word::dmflags2), dmflags2 | bitsOf({26, 27}));
    EXPECT_EQ(known("2.7.1", Flags::Word::dmflags2), dmflags2 | bitsOf({26}));
    EXPECT_EQ(known("2.6.0", Flags::Word::dmflags2), dmflags2 | bitsOf({26}));
    EXPECT_EQ(known("2.5.0", Flags::Word::dmflags2), dmflags2);
    EXPECT_EQ(known("2.4.1", Flags::Word::dmflags2), dmflags2);
}

// 2.5.0 reads dmflags bits 24 to 30 but has no cvar for any of them.
TEST(Flags, ZdoomNamesTheCoopLossCvarsFrom2_6_0) {
    const auto cvars = [](const char *version) {
        const Dialect::Traits traits = releaseOf("zdoom", version);
        std::vector<std::string_view> names;

        for (const Flags::Option &each : Flags::options()) {
            if (each.word == Flags::Word::dmflags && (each.mask & bitsOf({24, 25, 26, 27, 28, 29, 30})) != 0
                && Flags::offered(each, traits.line, traits.features)) {
                names.push_back(each.cvar);
            }
        }

        return names;
    };
    const std::vector<std::string_view> named = {"sv_cooploseinventory",
                                                 "sv_cooplosekeys",
                                                 "sv_cooploseweapons",
                                                 "sv_cooplosearmor",
                                                 "sv_cooplosepowerups",
                                                 "sv_cooploseammo",
                                                 "sv_coophalveammo"};

    EXPECT_EQ(cvars("2.8.1"), named);
    EXPECT_EQ(cvars("2.6.0"), named);
    EXPECT_EQ(cvars("2.5.0"), std::vector<std::string_view>(7));
    EXPECT_EQ(cvars("2.4.1"), std::vector<std::string_view>(7));
}
