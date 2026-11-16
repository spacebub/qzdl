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

#include <filesystem>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "ttk/system/Json.h"

#include "Same.h"
#include "core/config/Config.h"
#include "core/config/Schema.h"
#include "core/config/Versions.h"
#include "support/Fixtures.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;
namespace Json = ttk::Json;

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
