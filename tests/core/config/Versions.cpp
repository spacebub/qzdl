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
#include <initializer_list>
#include <limits>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/system/Json.h"

#include "Same.h"
#include "core/config/Config.h"
#include "core/config/GameplayKeys.h"
#include "core/config/Options.h"
#include "core/config/Schema.h"
#include "core/config/Versions.h"
#include "support/Fixtures.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;
namespace Json = ttk::Json;

constexpr std::array<std::string_view, 8> RELEASES{
        "4.0.0", "4.1.0", "4.2.0", "4.3.0", "4.4.0", "4.4.2", "4.4.7", "4.5.0"};

static_assert(RELEASES.size() + 1 == Versions::CURRENT, "the release of every sample before the current one");

[[nodiscard]] fs::path fixture(const std::string &name) {
    return fs::path(QZDL_CONFIG_FIXTURES) / name;
}

[[nodiscard]] std::string sample(const int version) {
    return "v" + std::to_string(version) + ".json";
}

[[nodiscard]] Config loadedFixture(const std::string &name, Versions::Stamp *stamp = nullptr) {
    Config config;
    std::string error;

    EXPECT_TRUE(config.load(fixture(name), &error, stamp)) << name << ": " << error;

    return config;
}

// Every key a namespace of Schema.h spells, read from the header so that a new key cannot be left out.
[[nodiscard]] std::vector<std::string> keysOf(const std::string_view space) {
    const std::string header = same::slurp(QZDL_SCHEMA_HEADER);
    const std::size_t from = header.find("namespace " + std::string(space) + " {");
    const std::size_t to = header.find("\n}", from);
    const std::string body = from == std::string::npos ? std::string() : header.substr(from, to - from);
    const std::regex constant(R"re(inline constexpr const char \*\w+ = "([^"]+)";)re");
    std::vector<std::string> out;

    for (auto at = std::sregex_iterator(body.begin(), body.end(), constant); at != std::sregex_iterator(); ++at) {
        out.push_back((*at)[1]);
    }

    return out;
}

[[nodiscard]] bool holds(const std::vector<std::string> &keys, const std::string &key) {
    return std::ranges::find(keys, key) != keys.end();
}

[[nodiscard]] std::string withoutApp(std::string text) {
    const std::string key = "\"appVersion\": \"";
    const std::size_t at = text.find(key);

    if (at != std::string::npos) {
        const std::size_t from = at + key.size();

        text.erase(from, text.find('"', from) - from);
    }

    return text;
}

[[nodiscard]] std::string lineAt(const std::string &text, const std::size_t at) {
    const std::size_t start = text.rfind('\n', at == 0 ? 0 : at - 1);
    const std::size_t from = start == std::string::npos ? 0 : start + 1;

    return text.substr(from, text.find('\n', at) - from);
}

[[nodiscard]] std::string firstDifference(const std::string &got, const std::string &want) {
    const auto [at, unused] = std::ranges::mismatch(got, want);
    const auto offset = static_cast<std::size_t>(at - got.begin());

    return "The writer wrote: " + lineAt(got, offset) + "\nThe sample has: " + lineAt(want, offset);
}

void byDefault(Profile &profile, const std::initializer_list<Switch> which) {
    const Profile fresh;

    for (const Switch each : which) {
        profile.set(each, fresh.on(each));
    }
}

// Fields an older format lacks read as their default.
void withoutWhatItLacked(Config &want, const int version) {
    if (version < 9) {
        for (PortEntry &port : want.ports) {
            port.version.clear();
        }

        for (Profile &profile : want.profiles) {
            const std::string dmflags = profile.flag(Flags::Word::dmflags);
            const std::string dmflags2 = profile.flag(Flags::Word::dmflags2);

            byDefault(profile, {Switch::gameplayOpen});
            profile.numbers.clear();
            profile.flags.clear();
            profile.setFlag(Flags::Word::dmflags, dmflags);
            profile.setFlag(Flags::Word::dmflags2, dmflags2);

            for (const Options::Option &option : Options::ALL) {
                if (option.key != nullptr) {
                    profile.set(option.feature, false);
                }
            }
        }
    }

    if (version < 8) {
        want.general.moveAtNextStart = false;
        want.general.window.maximized = false;

        for (PortEntry &port : want.ports) {
            port.dialect.clear();
        }

        for (Profile &profile : want.profiles) {
            byDefault(profile, {Switch::generalOpen, Switch::skipAutoload, Switch::music, Switch::soundEffects});
            profile.set(Dialect::Feature::pistolStart, false);
            profile.set(Dialect::Feature::cheats, false);
            profile.prefix.clear();
            profile.environment.clear();
            profile.display = Display::Port;
            profile.screenshots = Screenshots::Port;
            profile.screenshotFolder.clear();
            profile.zdlFile.clear();
            profile.replay.source.clear();
            profile.multiplayer.playerName.clear();
            profile.multiplayer.playerColor.clear();
            profile.multiplayer.team = -1;
        }
    }

    if (version < 7) {
        for (Profile &profile : want.profiles) {
            byDefault(profile, {Switch::dosExit});
        }
    }

    if (version < 6) {
        for (PortEntry &port : want.ports) {
            port.portId.clear();
        }
    }

    if (version < 5) {
        want.general.detected.clear();

        for (Profile &profile : want.profiles) {
            byDefault(profile, {Switch::levelstat});
            profile.multiplayer.listed = false;
        }
    }

    if (version < 4) {
        want.general.lastDirs.replay.clear();

        for (Profile &profile : want.profiles) {
            byDefault(profile, {Switch::replayOpen, Switch::saveOpen});
            profile.replay = ReplaySettings{};
            profile.save = SaveSettings{};
            profile.complevel = -1;
        }
    }

    if (version < 3) {
        want.general.showHidden = false;
    }

    if (version < 2) {
        for (Profile &profile : want.profiles) {
            byDefault(profile, {Switch::customCommand});
            profile.command.clear();

            // Not the default: the first release never started DOSBox fullscreen.
            profile.set(Switch::dosFullscreen, false);
        }
    }
}

int versionOf(const std::string &text) {
    const Json::Doc doc = Json::read_data(text);

    EXPECT_TRUE(doc.valid()) << text;

    return Versions::of(doc.root());
}

int stampedBy(const std::string &app) {
    return versionOf(R"({"version":1,"appVersion":")" + app + R"("})");
}

class VersionsUpgrade : public ::testing::Test {
protected:
    void SetUp() override { _dir = tests::Sandbox::scratch("versions"); }

    [[nodiscard]] fs::path at(const char *name) const { return _dir / name; }

    Config loaded(const std::string_view text, Versions::Stamp *stamp = nullptr) {
        const fs::path path = at("old.json");

        same::spill(path, text);

        Config config;
        std::string error;

        EXPECT_TRUE(config.load(path, &error, stamp)) << error;

        return config;
    }

    // The one profile of a file stamped by `app`.
    Profile profileBy(const std::string &app, const std::string &profile) {
        const Config config = loaded(R"({"version":1,"appVersion":")" + app + R"(","profiles":[)" + profile + "]}");

        EXPECT_EQ(config.profiles.size(), 1U);

        return config.profiles.empty() ? Profile() : config.profiles.front();
    }

private:
    fs::path _dir;
};

}

TEST(VersionsOf, AFileThatSaysMoreThanOneIsThatVersion) {
    EXPECT_EQ(versionOf(R"({"version":9})"), 9);
    EXPECT_EQ(versionOf(R"({"version":12,"appVersion":"4.0.0"})"), 12);
}

TEST(VersionsOf, AFileWithNeitherNumberNorReleaseIsOfTheCurrentFormat) {
    EXPECT_EQ(versionOf("{}"), Versions::CURRENT);
    EXPECT_EQ(versionOf(R"({"engine":"ZDL4","profiles":[]})"), Versions::CURRENT);
    EXPECT_EQ(versionOf(R"({"version":0})"), Versions::CURRENT);
    EXPECT_EQ(versionOf(R"({"version":"9","appVersion":450})"), Versions::CURRENT);
}

TEST(VersionsOf, AFileWithAReleaseAndNoNumberIsToldByTheRelease) {
    EXPECT_EQ(versionOf(R"({"appVersion":"4.4.7"})"), 7);
    EXPECT_EQ(versionOf(R"({"appVersion":"garbage"})"), 1);
}

TEST(VersionsOf, AVersionBelowOneIsNoNumber) {
    EXPECT_EQ(versionOf(R"({"version":0,"appVersion":"4.5.0"})"), 8);
    EXPECT_EQ(versionOf(R"({"version":-3,"appVersion":"4.0.0"})"), 1);
}

TEST(VersionsOf, OnlyAWholeNumberThatFitsAnIntIsANumber) {
    for (const char *odd : {"1e30", "-1e30", "99999999999999", "2147483648", "\"9\"", "8.9", "9.0", "true", "null"}) {
        SCOPED_TRACE(odd);

        EXPECT_EQ(versionOf(std::string(R"({"version":)") + odd + R"(,"appVersion":"4.4.7"})"), 7);
        EXPECT_EQ(versionOf(std::string(R"({"version":)") + odd + "}"), Versions::CURRENT);
    }

    EXPECT_EQ(versionOf(R"({"version":2147483647})"), std::numeric_limits<int>::max());
}

TEST(VersionsOf, AFileThatSaysOneIsTheFormatOfTheReleaseThatWroteIt) {
    EXPECT_EQ(stampedBy("4.0.0"), 1);
    EXPECT_EQ(stampedBy("4.1.0"), 2);
    EXPECT_EQ(stampedBy("4.2.0"), 3);
    EXPECT_EQ(stampedBy("4.3.2"), 4);
    EXPECT_EQ(stampedBy("4.4.1"), 5);
    EXPECT_EQ(stampedBy("4.4.2"), 6);
    EXPECT_EQ(stampedBy("4.4.6"), 6);
    EXPECT_EQ(stampedBy("4.4.7"), 7);
    EXPECT_EQ(stampedBy("4.4.10"), 7);
    EXPECT_EQ(stampedBy("4.4.17"), 7);
    EXPECT_EQ(stampedBy("4.5.0"), 8);
}

TEST(VersionsOf, AMissingPartOfTheReleaseCountsAsZero) {
    EXPECT_EQ(stampedBy("4.1"), 2);
    EXPECT_EQ(stampedBy("4.4"), 5);
}

TEST(VersionsOf, AFileThatSaysOneWithoutAReadableReleaseIsTheFirst) {
    EXPECT_EQ(versionOf(R"({"version":1})"), 1);
    EXPECT_EQ(versionOf(R"({"version":1,"appVersion":450})"), 1);
    EXPECT_EQ(stampedBy("garbage"), 1);
    EXPECT_EQ(stampedBy("4.5.0-rc1"), 1);
    EXPECT_EQ(stampedBy("4..0"), 1);
    EXPECT_EQ(stampedBy("4.5.0.1"), 1);
    EXPECT_EQ(stampedBy("4.-1.0"), 1);
    EXPECT_EQ(stampedBy(""), 1);
}

TEST_F(VersionsUpgrade, AProfileFromTheFirstReleaseStartsDosboxInAWindow) {
    EXPECT_FALSE(profileBy("4.0.0", R"({"id":"a"})").on(Switch::dosFullscreen));
    EXPECT_TRUE(profileBy("4.0.0", R"({"id":"a","dosFullscreen":true})").on(Switch::dosFullscreen));
}

TEST_F(VersionsUpgrade, AProfileFromALaterReleaseStartsDosboxFullscreenWithoutTheKey) {
    EXPECT_TRUE(profileBy("4.1.0", R"({"id":"a"})").on(Switch::dosFullscreen));
}

TEST_F(VersionsUpgrade, TheFlagsAndTheComplevelOf4_5_0MoveToTheirNewPlaces) {
    const Profile profile = profileBy(
            "4.5.0", R"({"id":"a","multiplayer":{"dmflags":"1024","dmflags2":"2048"},"replay":{"compatibility":9}})");

    EXPECT_EQ(profile.flag(Flags::Word::dmflags), "1024");
    EXPECT_EQ(profile.flag(Flags::Word::dmflags2), "2048");
    EXPECT_EQ(profile.complevel, 9);
}

TEST_F(VersionsUpgrade, TheNewPlacesWinOverTheOldOnesInAFileStamped4_5_0) {
    const Profile profile = profileBy("4.5.0",
                                      R"({"id":"a","flags":{"compatflags":"5"},"complevel":-1,)"
                                      R"("multiplayer":{"dmflags":"1024"},"replay":{"compatibility":9}})");

    EXPECT_EQ(profile.flag(Flags::Word::compatflags), "5");
    EXPECT_EQ(profile.flag(Flags::Word::dmflags), "");
    EXPECT_EQ(profile.complevel, -1);
}

TEST_F(VersionsUpgrade, EachNewPlaceIsCheckedByItself) {
    const Profile profile = profileBy("4.5.0",
                                      R"({"id":"a","flags":{},"multiplayer":{"dmflags":"1024"},)"
                                      R"("replay":{"compatibility":9}})");

    EXPECT_EQ(profile.flag(Flags::Word::dmflags), "");
    EXPECT_EQ(profile.complevel, 9);
}

TEST_F(VersionsUpgrade, AFileOfTheCurrentFormatIgnoresTheOldPlaces) {
    const Config config = loaded(R"({"version":9,"profiles":[{"id":"a","multiplayer":{"dmflags":"1024"},)"
                                 R"("replay":{"compatibility":9}}]})");

    ASSERT_EQ(config.profiles.size(), 1U);
    EXPECT_EQ(config.profiles.front().flag(Flags::Word::dmflags), "");
    EXPECT_EQ(config.profiles.front().complevel, -1);
}

TEST_F(VersionsUpgrade, EveryStepKeepsWhatTheCurrentFormatHolds) {
    const Config made = tests::Fixtures::shaped(4, 3, 2);

    ASSERT_TRUE(made.save(at("saved.json")));

    std::string text = same::slurp(at("saved.json"));
    const std::string stamp = "\"version\": " + std::to_string(Versions::CURRENT);
    const std::string app = std::string("\"appVersion\": \"") + QZDL_VERSION + '"';

    ASSERT_NE(text.find(stamp), std::string::npos);
    text.replace(text.find(stamp), stamp.size(), "\"version\": 1");
    ASSERT_NE(text.find(app), std::string::npos);
    text.replace(text.find(app), app.size(), "\"appVersion\": \"4.0.0\"");

    Versions::Stamp said;
    const Config back = loaded(text, &said);

    EXPECT_EQ(said.version, 1);
    same::config(back, made);
}

TEST_F(VersionsUpgrade, ALoadHandsBackWhatTheFileSaid) {
    Versions::Stamp stamp;

    loaded(R"({"version":1,"appVersion":"4.4.10"})", &stamp);

    EXPECT_EQ(stamp.version, 7);
    EXPECT_EQ(stamp.release, "4.4.10");
}

TEST_F(VersionsUpgrade, AFileNewerThanTheCurrentFormatIsHandedBackAsItSays) {
    Versions::Stamp stamp;

    loaded(R"({"version":12,"appVersion":"5.0.0"})", &stamp);

    EXPECT_EQ(stamp.version, 12);
    EXPECT_EQ(stamp.release, "5.0.0");
}

TEST_F(VersionsUpgrade, ADeeplyNestedOldFileLoads) {
    constexpr std::size_t DEPTH = 100000;
    const std::string text = R"({"version":1,"appVersion":"4.0.0","unknown":)" + std::string(DEPTH, '[')
                             + std::string(DEPTH, ']') + R"(,"profiles":[{"id":"a"}]})";
    Versions::Stamp stamp;
    const Config config = loaded(text, &stamp);

    EXPECT_EQ(stamp.version, 1);
    EXPECT_EQ(config.profiles.size(), 1U);
}

TEST_F(VersionsUpgrade, ASavedFileIsOfTheCurrentFormatAndLoadsWithoutAnUpgrade) {
    ASSERT_TRUE(tests::Fixtures::config(1, 1).save(at("saved.json")));

    const Json::Doc doc = Json::read_file(at("saved.json"));

    EXPECT_EQ(Json::obj_get_int(doc.root(), ConfigKey::VERSION), Versions::CURRENT);
    EXPECT_EQ(Versions::of(doc.root()), Versions::CURRENT);

    Config back;
    Versions::Stamp stamp;

    ASSERT_TRUE(back.load(at("saved.json"), nullptr, &stamp));
    EXPECT_EQ(stamp.version, Versions::CURRENT);
    EXPECT_EQ(stamp.release, QZDL_VERSION);
}

TEST_F(VersionsUpgrade, ThereIsASampleForEveryVersionAndItLoads) {
    for (int version = 1; version <= Versions::CURRENT; ++version) {
        const std::string name = sample(version);

        if (!fs::exists(fixture(name))) {
            ADD_FAILURE() << "tests/fixtures/config/" << name << " is missing";

            continue;
        }

        EXPECT_EQ(loadedFixture(name).profiles.size(), 3U);
    }
}

TEST_F(VersionsUpgrade, ASampleOfAnOlderReleaseLoadsAsThatReleaseMeantIt) {
    const Config current = loadedFixture(sample(Versions::CURRENT));

    for (int version = 1; version < Versions::CURRENT; ++version) {
        SCOPED_TRACE(sample(version));

        Config want = current;
        Versions::Stamp stamp;
        const Config got = loadedFixture(sample(version), &stamp);

        withoutWhatItLacked(want, version);
        same::config(got, want);
        EXPECT_EQ(stamp.version, version);
        EXPECT_EQ(stamp.release, RELEASES[static_cast<std::size_t>(version) - 1]);
    }
}

TEST_F(VersionsUpgrade, AFileOfTheCurrentFormatWithoutTheNumberLoadsAsTheCurrentSample) {
    Versions::Stamp stamp;
    const Config got = loadedFixture("unnumbered-v9.json", &stamp);

    same::config(got, loadedFixture(sample(Versions::CURRENT)));
    EXPECT_EQ(stamp.version, 8);
    EXPECT_EQ(stamp.release, "4.5.0");
}

TEST_F(VersionsUpgrade, TheSampleOfTheCurrentFormatHoldsEveryKey) {
    const std::string text = same::slurp(fixture(sample(Versions::CURRENT)));
    const std::vector<std::string> config = keysOf("ConfigKey");
    const std::vector<std::string> profile = keysOf("ProfileKey");
    std::vector<std::string> keys = config;

    ASSERT_TRUE(holds(config, ConfigKey::VERSION) && holds(config, ConfigKey::PORT_VERSION));
    ASSERT_TRUE(holds(profile, ProfileKey::ID) && holds(profile, ProfileKey::SAVE));

    keys.insert(keys.end(), profile.begin(), profile.end());

    for (const Flags::Word word : Flags::ALL) {
        keys.emplace_back(Flags::name(word));
    }

    for (const Options::Option &option : Options::ALL) {
        if (option.key != nullptr) {
            keys.emplace_back(option.key);
        }
    }

    for (const std::string &key : keys) {
        EXPECT_NE(text.find('"' + key + "\":"), std::string::npos)
                << "tests/fixtures/config/" << sample(Versions::CURRENT) << " has no \"" << key
                << "\". The sample of the current version must hold every key, with a value that is not the default.";
    }
}

TEST_F(VersionsUpgrade, TheCurrentSampleLoadsTheValuesItSpells) {
    const Config got = loadedFixture(sample(Versions::CURRENT));

    ASSERT_EQ(got.profiles.size(), 3U);

    EXPECT_TRUE(got.general.autoClose);
    EXPECT_TRUE(got.general.launchZdlImmediately);
    EXPECT_FALSE(got.general.showPaths);
    EXPECT_TRUE(got.general.noUserConf);
    EXPECT_TRUE(got.general.moveAtNextStart);
    EXPECT_TRUE(got.general.showHidden);
    EXPECT_TRUE(got.general.profileConfigs);
    EXPECT_TRUE(got.general.isImported);
    EXPECT_TRUE(got.general.window.maximized);
    EXPECT_EQ(got.general.startView, StartView::Games);

    const Profile &first = got.profiles[0];
    const Profile &second = got.profiles[1];
    const Profile &third = got.profiles[2];

    EXPECT_EQ(first.display, Display::Fullscreen);
    EXPECT_EQ(second.display, Display::Window);
    EXPECT_EQ(third.display, Display::Port);

    EXPECT_EQ(first.screenshots, Screenshots::Custom);
    EXPECT_EQ(second.screenshots, Screenshots::Profile);
    EXPECT_EQ(third.screenshots, Screenshots::Port);

    EXPECT_EQ(first.multiplayer.gameType, GameType::AltDeathmatch);
    EXPECT_EQ(second.multiplayer.gameType, GameType::Deathmatch);
    EXPECT_EQ(third.multiplayer.gameType, GameType::None);

    EXPECT_EQ(first.replay.mode, ReplayMode::Play);
    EXPECT_EQ(second.replay.mode, ReplayMode::Record);
    EXPECT_EQ(third.replay.mode, ReplayMode::Off);

    EXPECT_EQ(first.replay.playback, Playback::Fast);
    EXPECT_EQ(second.replay.playback, Playback::Timed);
    EXPECT_EQ(third.replay.playback, Playback::AsRecorded);

    for (const Switch each : {Switch::dosExit, Switch::dosFullscreen, Switch::music, Switch::soundEffects}) {
        SCOPED_TRACE(static_cast<int>(each));

        EXPECT_FALSE(first.on(each));
        EXPECT_FALSE(second.on(each));
        EXPECT_TRUE(third.on(each));
    }

    EXPECT_TRUE(first.on(Switch::skipAutoload));
    EXPECT_FALSE(third.on(Switch::skipAutoload));
    EXPECT_TRUE(first.replay.longtics);
    EXPECT_FALSE(third.replay.longtics);
    EXPECT_TRUE(first.multiplayer.extratic);
    EXPECT_FALSE(third.multiplayer.extratic);
}

TEST_F(VersionsUpgrade, TheWriterWritesTheSampleOfTheCurrentFormat) {
    const std::string name = sample(Versions::CURRENT);

    ASSERT_TRUE(loadedFixture(name).save(at("saved.json")));

    const std::string saved = withoutApp(same::slurp(at("saved.json")));
    const std::string want = withoutApp(same::slurp(fixture(name)));

    EXPECT_TRUE(saved == want)
            << "The config file format changed, so it needs a new version. Raise Versions::CURRENT, add the row for it "
               "to STEPS in Versions.cpp, add its sample to tests/fixtures/config/, add its rung to "
               "withoutWhatItLacked and its release to RELEASES. The old samples stay as they are.\n"
            << firstDifference(saved, want);
}

TEST_F(VersionsUpgrade, AFileOfAVersionNotKnownYetKeepsWhatTheCurrentFormatHolds) {
    std::string text = same::slurp(fixture(sample(Versions::CURRENT)));
    const std::string stamp = "\"version\": " + std::to_string(Versions::CURRENT) + ",";
    const int next = Versions::CURRENT + 1;

    ASSERT_NE(text.find(stamp), std::string::npos);
    text.replace(text.find(stamp), stamp.size(), "\"version\": " + std::to_string(next) + ",\"unknown\":{\"x\":[1]},");

    Versions::Stamp said;
    const Config got = loaded(text, &said);

    same::config(got, loadedFixture(sample(Versions::CURRENT)));
    EXPECT_EQ(said.version, next);
}
