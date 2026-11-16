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
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "ttk/system/Json.h"

#include "Same.h"
#include "core/config/Config.h"
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

[[nodiscard]] Config loadedFixture(const std::string &name) {
    Config config;
    std::string error;

    EXPECT_TRUE(config.load(fixture(name), &error)) << name << ": " << error;

    return config;
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

// What a shape could not hold reads as the default.
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

    Config loaded(const std::string_view text) {
        const fs::path path = at("old.json");

        same::spill(path, text);

        Config config;
        std::string error;

        EXPECT_TRUE(config.load(path, &error)) << error;

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

TEST(VersionsOf, AFileWithoutAVersionIsTheFirst) {
    EXPECT_EQ(versionOf("{}"), 1);
}

TEST(VersionsOf, AVersionBelowOneCountsAsOne) {
    EXPECT_EQ(versionOf(R"({"version":0,"appVersion":"4.5.0"})"), 8);
    EXPECT_EQ(versionOf(R"({"version":-3,"appVersion":"4.0.0"})"), 1);
}

TEST(VersionsOf, AFileThatSaysOneIsTheShapeOfTheReleaseThatWroteIt) {
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

TEST_F(VersionsUpgrade, AFileOfTheCurrentShapeIgnoresTheOldPlaces) {
    const Config config = loaded(R"({"version":9,"profiles":[{"id":"a","multiplayer":{"dmflags":"1024"},)"
                                 R"("replay":{"compatibility":9}}]})");

    ASSERT_EQ(config.profiles.size(), 1U);
    EXPECT_EQ(config.profiles.front().flag(Flags::Word::dmflags), "");
    EXPECT_EQ(config.profiles.front().complevel, -1);
}

TEST_F(VersionsUpgrade, EveryStepKeepsWhatTheCurrentShapeHolds) {
    const Config made = tests::Fixtures::shaped(4, 3, 2);

    ASSERT_TRUE(made.save(at("saved.json")));

    std::string text = same::slurp(at("saved.json"));
    const std::string stamp = "\"version\": " + std::to_string(Versions::CURRENT);
    const std::string app = std::string("\"appVersion\": \"") + QZDL_VERSION + '"';

    ASSERT_NE(text.find(stamp), std::string::npos);
    text.replace(text.find(stamp), stamp.size(), "\"version\": 1");
    ASSERT_NE(text.find(app), std::string::npos);
    text.replace(text.find(app), app.size(), "\"appVersion\": \"4.0.0\"");

    const Config back = loaded(text);

    EXPECT_EQ(back.fileVersion, 1);
    same::config(back, made);
}

TEST_F(VersionsUpgrade, ALoadRecordsWhatTheFileSaid) {
    const Config config = loaded(R"({"version":1,"appVersion":"4.4.10"})");

    EXPECT_EQ(config.fileVersion, 7);
    EXPECT_EQ(config.fileApp, "4.4.10");
}

TEST_F(VersionsUpgrade, AFileNewerThanTheCurrentShapeIsRecordedAsItSays) {
    const Config config = loaded(R"({"version":12,"appVersion":"5.0.0"})");

    EXPECT_EQ(config.fileVersion, 12);
    EXPECT_EQ(config.fileApp, "5.0.0");
}

TEST_F(VersionsUpgrade, AConfigMadeHereOrResetIsOfTheCurrentShape) {
    Config config = loaded(R"({"version":1,"appVersion":"4.0.0"})");

    config.reset();

    EXPECT_EQ(config.fileVersion, Versions::CURRENT);
    EXPECT_TRUE(config.fileApp.empty());
    EXPECT_EQ(Config{}.fileVersion, Versions::CURRENT);
    EXPECT_TRUE(Config{}.fileApp.empty());
}

TEST_F(VersionsUpgrade, ASavedFileIsOfTheCurrentShapeAndLoadsWithoutAnUpgrade) {
    ASSERT_TRUE(tests::Fixtures::config(1, 1).save(at("saved.json")));

    const Json::Doc doc = Json::read_file(at("saved.json"));

    EXPECT_EQ(Json::obj_get_int(doc.root(), ConfigKey::VERSION), Versions::CURRENT);
    EXPECT_EQ(Versions::of(doc.root()), Versions::CURRENT);

    Config back;

    ASSERT_TRUE(back.load(at("saved.json")));
    EXPECT_EQ(back.fileVersion, Versions::CURRENT);
    EXPECT_EQ(back.fileApp, QZDL_VERSION);
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
        const Config got = loadedFixture(sample(version));

        withoutWhatItLacked(want, version);
        same::config(got, want);
        EXPECT_EQ(got.fileVersion, version);
        EXPECT_EQ(got.fileApp, RELEASES[static_cast<std::size_t>(version) - 1]);
    }
}

TEST_F(VersionsUpgrade, AFileOfTheCurrentShapeWithoutTheNumberLoadsAsTheCurrentSample) {
    const Config got = loadedFixture("unnumbered-v9.json");

    same::config(got, loadedFixture(sample(Versions::CURRENT)));
    EXPECT_EQ(got.fileVersion, 8);
    EXPECT_EQ(got.fileApp, "4.5.0");
}

TEST_F(VersionsUpgrade, TheWriterWritesTheSampleOfTheCurrentShape) {
    const std::string name = sample(Versions::CURRENT);

    ASSERT_TRUE(loadedFixture(name).save(at("saved.json")));

    const std::string saved = withoutApp(same::slurp(at("saved.json")));
    const std::string want = withoutApp(same::slurp(fixture(name)));

    EXPECT_TRUE(saved == want)
            << "The config file format changed. Versions.cpp needs a new row for the next version, "
               "and tests/fixtures/config/ needs a sample for it. The old samples stay as they are.\n"
            << firstDifference(saved, want);
}

TEST_F(VersionsUpgrade, AFileOfAVersionNotKnownYetKeepsWhatTheCurrentShapeHolds) {
    std::string text = same::slurp(fixture(sample(Versions::CURRENT)));
    const std::string stamp = "\"version\": " + std::to_string(Versions::CURRENT) + ",";
    const int next = Versions::CURRENT + 1;

    ASSERT_NE(text.find(stamp), std::string::npos);
    text.replace(text.find(stamp), stamp.size(), "\"version\": " + std::to_string(next) + ",\"unknown\":{\"x\":[1]},");

    const Config got = loaded(text);

    same::config(got, loadedFixture(sample(Versions::CURRENT)));
    EXPECT_EQ(got.fileVersion, next);
}
