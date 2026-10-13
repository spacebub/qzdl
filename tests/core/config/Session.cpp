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
#include <regex>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/system/Paths.h"

#include "Same.h"
#include "core/config/ConfigPaths.h"
#include "core/config/Import.h"
#include "core/config/Session.h"
#include "support/Fixtures.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

using Strings = std::vector<std::string>;

// Session has no way to forget where it was opened from, so every case starts from
// a copy taken before any case ran.
const Session pristine = Session::get();

constexpr const char *LEGACY = "[zdl.general]\nalwaysadd=-legacy\nnouserconf=0\n"
                               "[zdl.iwads]\ni0n=Doom\ni0f=/g/doom.wad\n"
                               "[zdl.save]\niwad=Doom\nskill=4\nfile0=/m/a.wad\n";

fs::path userJson() {
    return ConfigPaths::get().configPath(ConfigPaths::USER);
}

fs::path userIni() {
    return ConfigPaths::get().legacyConfigPath(ConfigPaths::USER).front();
}

Config withNoUserConf(Config config) {
    config.general.noUserConf = true;

    return config;
}

std::vector<FileEntry> enabled(const Strings &paths) {
    std::vector<FileEntry> out;

    for (const std::string &path : paths) {
        out.push_back({.file = path, .enabled = true});
    }

    return out;
}

class SessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        Session::get() = pristine;

        _executable = ttk::Paths::executable();
        _beside = tests::Sandbox::scratch("session-exe");
        _dir = tests::Sandbox::scratch("session");

        ttk::Paths::set_executable(_beside / "qzdl");

        forgetUser();
    }

    void TearDown() override {
        Session::get() = pristine;
        ttk::Paths::set_executable(_executable);

        forgetUser();
    }

    [[nodiscard]] fs::path at(const char *name) const { return _dir / name; }

    [[nodiscard]] fs::path beside(const char *name) const { return _beside / name; }

    static Session &session() { return Session::get(); }

    static Config fromDisk(const fs::path &path) {
        Config config;

        EXPECT_TRUE(config.load(path)) << path;

        return config;
    }

    [[nodiscard]] fs::path zdl(const char *name, const Profile &profile) const {
        const fs::path path = at(name);

        EXPECT_TRUE(Import::saveZdlFile(path, profile));

        return path;
    }

private:
    static void forgetUser() {
        std::error_code code;

        fs::remove_all(userJson().parent_path(), code);
        fs::remove_all(userIni().parent_path(), code);
        fs::remove_all(Config::profileFolder("x").parent_path(), code);
    }

    fs::path _executable;
    fs::path _beside;
    fs::path _dir;
};

Profile zdlProfile(const std::string &name) {
    Profile profile;

    profile.name = name;
    profile.iwad = "Doom";
    profile.skill = 3;
    profile.files = {{.file = "/m/zdl.wad", .enabled = true}};

    return profile;
}

}

TEST_F(SessionTest, StartsUnopened) {
    EXPECT_EQ(session().source(), Session::Source::Unknown);
    EXPECT_TRUE(session().path().empty());
    EXPECT_FALSE(session().openedZdlFile());
}

TEST_F(SessionTest, WithNothingOnDiskItFallsBackToTheUserPath) {
    const Strings rest = session().start({});

    EXPECT_TRUE(rest.empty());
    EXPECT_EQ(session().path(), userJson());
    EXPECT_EQ(session().source(), Session::Source::Fallback);
    EXPECT_TRUE(session().config().profiles.empty());
}

TEST_F(SessionTest, StartingWritesNothingUntilSaved) {
    session().start({});

    EXPECT_FALSE(fs::exists(userJson()));

    ASSERT_TRUE(session().save());

    EXPECT_TRUE(fs::is_regular_file(userJson()));
}

TEST_F(SessionTest, AUserConfigIsOpened) {
    ASSERT_TRUE(tests::Fixtures::config(3, 2).save(userJson()));

    session().start({});

    EXPECT_EQ(session().source(), Session::Source::User);
    EXPECT_EQ(session().path(), userJson());
    same::config(session().config(), tests::Fixtures::config(3, 2));
}

TEST_F(SessionTest, AUserConfigTooSmallToHoldAnythingIsPassedOver) {
    same::spill(userJson(), "{}");

    session().start({});

    EXPECT_EQ(session().source(), Session::Source::Fallback);
    EXPECT_EQ(session().path(), userJson());
}

TEST_F(SessionTest, AUserConfigWinsOverAPortableOne) {
    ASSERT_TRUE(tests::Fixtures::config(1, 0).save(userJson()));
    ASSERT_TRUE(tests::Fixtures::config(4, 0).save(beside("zdl.json")));

    session().start({});

    EXPECT_EQ(session().source(), Session::Source::User);
    EXPECT_EQ(session().config().profiles.size(), 1U);
}

TEST_F(SessionTest, AUserConfigThatOptsOutGivesWayToAPortableOne) {
    ASSERT_TRUE(withNoUserConf(tests::Fixtures::config(1, 0)).save(userJson()));
    ASSERT_TRUE(tests::Fixtures::config(4, 0).save(beside("zdl.json")));

    session().start({});

    EXPECT_EQ(session().source(), Session::Source::Portable);
    EXPECT_EQ(session().path(), beside("zdl.json"));
    EXPECT_EQ(session().config().profiles.size(), 4U);
}

TEST_F(SessionTest, AUserConfigThatOptsOutIsStillTheFallback) {
    ASSERT_TRUE(withNoUserConf(tests::Fixtures::config(2, 0)).save(userJson()));

    session().start({});

    EXPECT_EQ(session().source(), Session::Source::Fallback);
    EXPECT_EQ(session().path(), userJson());
    EXPECT_EQ(session().config().profiles.size(), 2U);
    EXPECT_TRUE(session().config().general.noUserConf);
}

TEST_F(SessionTest, APortableConfigIsOpened) {
    ASSERT_TRUE(tests::Fixtures::config(2, 1).save(beside("zdl.json")));

    session().start({});

    EXPECT_EQ(session().source(), Session::Source::Portable);
    EXPECT_EQ(session().path(), beside("zdl.json"));
    EXPECT_EQ(session().config().profiles.size(), 2U);
}

TEST_F(SessionTest, APortableIniIsImportedAndMigratedBesideIt) {
    same::spill(beside("zdl.ini"), LEGACY);

    session().start({});

    EXPECT_EQ(session().source(), Session::Source::Portable);
    EXPECT_EQ(session().path(), beside("zdl.json"));
    EXPECT_EQ(session().config().general.alwaysAdd, "-legacy");
    ASSERT_TRUE(fs::is_regular_file(beside("zdl.json")));
    EXPECT_EQ(fromDisk(beside("zdl.json")).general.alwaysAdd, "-legacy");
}

TEST_F(SessionTest, ALegacyUserIniIsImportedAndMigrated) {
    same::spill(userIni(), LEGACY);

    session().start({});

    EXPECT_EQ(session().source(), Session::Source::User);
    EXPECT_EQ(session().path(), userJson());
    ASSERT_EQ(session().config().profiles.size(), 1U);
    EXPECT_EQ(session().config().profiles.front().skill, 4);
    ASSERT_TRUE(fs::is_regular_file(userJson()));
    EXPECT_EQ(fromDisk(userJson()).iwads.size(), 1U);
}

TEST_F(SessionTest, AUserJsonIsPreferredToTheLegacyIni) {
    same::spill(userIni(), LEGACY);
    ASSERT_TRUE(tests::Fixtures::config(3, 0).save(userJson()));

    session().start({});

    EXPECT_EQ(session().source(), Session::Source::User);
    EXPECT_EQ(session().config().profiles.size(), 3U);
}

TEST_F(SessionTest, ALegacyUserIniThatOptsOutIsTheFallback) {
    same::spill(userIni(), "[zdl.general]\nnouserconf=1\nalwaysadd=-x\n");

    session().start({});

    EXPECT_EQ(session().source(), Session::Source::Fallback);
    EXPECT_TRUE(session().config().general.noUserConf);
}

TEST_F(SessionTest, AJsonArgumentIsOpened) {
    ASSERT_TRUE(tests::Fixtures::config(5, 0).save(at("mine.json")));

    const Strings rest = session().start({at("mine.json").string()});

    EXPECT_TRUE(rest.empty());
    EXPECT_EQ(session().source(), Session::Source::UserSpecified);
    EXPECT_EQ(session().path(), at("mine.json"));
    EXPECT_EQ(session().config().profiles.size(), 5U);
}

TEST_F(SessionTest, AJsonArgumentWinsOverTheUserConfig) {
    ASSERT_TRUE(tests::Fixtures::config(1, 0).save(userJson()));
    ASSERT_TRUE(tests::Fixtures::config(5, 0).save(at("mine.json")));

    session().start({at("mine.json").string()});

    EXPECT_EQ(session().path(), at("mine.json"));
    EXPECT_EQ(session().config().profiles.size(), 5U);
}

TEST_F(SessionTest, AJsonArgumentNotYetOnDiskBecomesTheConfigPath) {
    session().start({at("new.json").string()});

    EXPECT_EQ(session().source(), Session::Source::UserSpecified);
    EXPECT_EQ(session().path(), at("new.json"));
    EXPECT_TRUE(session().config().profiles.empty());
    EXPECT_FALSE(fs::exists(at("new.json")));
}

TEST_F(SessionTest, ConfigExtensionsMatchWithoutCase) {
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(at("MINE.JSON")));

    session().start({at("MINE.JSON").string()});

    EXPECT_EQ(session().source(), Session::Source::UserSpecified);
    EXPECT_EQ(session().config().profiles.size(), 2U);
}

TEST_F(SessionTest, AnotherProgramsJsonIsAFileToAdd) {
    same::spill(at("mod.json"), R"({"name":"some mod data","engine":"other"})");

    const Strings rest = session().start({at("mod.json").string()});

    EXPECT_EQ(rest, (Strings{at("mod.json").string()}));
    EXPECT_NE(session().source(), Session::Source::UserSpecified);
    same::files(session().config().activeProfile().files, enabled({at("mod.json").string()}));
}

TEST_F(SessionTest, AJsonThatDoesNotParseIsStillTakenAsTheConfig) {
    same::spill(at("broken.json"), "{ not json");

    const Strings rest = session().start({at("broken.json").string()});

    EXPECT_TRUE(rest.empty());
    EXPECT_EQ(session().path(), at("broken.json"));
    EXPECT_TRUE(session().config().profiles.empty());
}

TEST_F(SessionTest, AnIniArgumentIsImportedAndMigratedToItsJsonSibling) {
    same::spill(at("old.ini"), LEGACY);

    session().start({at("old.ini").string()});

    EXPECT_EQ(session().source(), Session::Source::UserSpecified);
    EXPECT_EQ(session().path(), at("old.json"));
    EXPECT_EQ(session().config().general.alwaysAdd, "-legacy");
    ASSERT_TRUE(fs::is_regular_file(at("old.json")));
    EXPECT_EQ(fromDisk(at("old.json")).general.alwaysAdd, "-legacy");
}

TEST_F(SessionTest, AnIniArgumentWhoseJsonSiblingExistsOpensTheJson) {
    same::spill(at("old.ini"), LEGACY);
    ASSERT_TRUE(tests::Fixtures::config(6, 0).save(at("old.json")));

    session().start({at("old.ini").string()});

    EXPECT_EQ(session().path(), at("old.json"));
    EXPECT_EQ(session().config().profiles.size(), 6U);
    EXPECT_TRUE(session().config().general.alwaysAdd.starts_with("-nomusic"));
}

TEST_F(SessionTest, FlagsAreSkipped) {
    const Strings rest = session().start({"-fullscreen", "/m/a.wad", "--verbose"});

    EXPECT_EQ(rest, (Strings{"/m/a.wad"}));
}

TEST_F(SessionTest, LooseFilesGoToANewProfileWhenThereIsNone) {
    const Strings rest = session().start({"/m/a.wad", "/m/b.pk3"});

    EXPECT_EQ(rest, (Strings{"/m/a.wad", "/m/b.pk3"}));
    ASSERT_EQ(session().config().profiles.size(), 1U);
    EXPECT_EQ(session().config().activeProfile().name, "New profile");
    EXPECT_EQ(session().config().activeProfileId, session().config().profiles.front().id);
    same::files(session().config().activeProfile().files, enabled(rest));
}

TEST_F(SessionTest, LooseFilesReplaceTheActiveProfilesList) {
    Config made = tests::Fixtures::config(3, 4);

    made.activeProfileId = "profile-1";

    ASSERT_TRUE(made.save(userJson()));

    session().start({"/m/a.wad"});

    EXPECT_EQ(session().config().activeProfileId, "profile-1");
    same::files(session().config().activeProfile().files, enabled({"/m/a.wad"}));
    EXPECT_EQ(session().config().profiles[0].files.size(), 4U);
}

TEST_F(SessionTest, AZdlBecomesTheActiveProfile) {
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(userJson()));

    const fs::path file = zdl("sunlust.zdl", zdlProfile("Sunlust"));
    const Strings rest = session().start({file.string()});

    EXPECT_TRUE(rest.empty());
    EXPECT_TRUE(session().openedZdlFile());
    ASSERT_EQ(session().config().profiles.size(), 3U);

    const Profile &opened = session().config().activeProfile();

    EXPECT_EQ(opened.id, session().config().profiles.back().id);
    EXPECT_EQ(opened.name, "Sunlust");
    EXPECT_EQ(opened.skill, 3);
    EXPECT_EQ(opened.config, "sunlust.cfg");
    same::files(opened.files, enabled({"/m/zdl.wad"}));
}

TEST_F(SessionTest, AZdlTakesAFreeName) {
    Config made;

    made.addProfile("Sunlust");

    ASSERT_TRUE(made.save(userJson()));

    session().start({zdl("s.zdl", zdlProfile("Sunlust")).string()});

    EXPECT_EQ(session().config().activeProfile().name, "Sunlust (2)");
}

TEST_F(SessionTest, LooseFilesAfterAZdlAreAddedToIt) {
    const Strings rest = session().start({zdl("s.zdl", zdlProfile("S")).string(), "/m/extra.wad"});

    EXPECT_EQ(rest, (Strings{"/m/extra.wad"}));
    same::files(session().config().activeProfile().files, enabled({"/m/zdl.wad", "/m/extra.wad"}));
}

TEST_F(SessionTest, AZdlWithoutASaveSectionIsDropped) {
    same::spill(at("empty.zdl"), "[zdl.general]\nx=1\n");

    const Strings rest = session().start({at("empty.zdl").string()});

    EXPECT_TRUE(rest.empty());
    EXPECT_FALSE(session().openedZdlFile());
    EXPECT_TRUE(session().config().profiles.empty());
}

TEST_F(SessionTest, LoadingAJsonMakesItTheOpenConfig) {
    ASSERT_TRUE(tests::Fixtures::config(4, 0).save(at("other.json")));

    std::string error;

    ASSERT_TRUE(session().load(at("other.json"), &error)) << error;

    EXPECT_EQ(session().path(), at("other.json"));
    EXPECT_EQ(session().source(), Session::Source::UserSpecified);
    EXPECT_EQ(session().config().profiles.size(), 4U);
}

TEST_F(SessionTest, LoadingAnIniImportsItAndPointsAtItsJsonSibling) {
    same::spill(at("old.ini"), LEGACY);

    ASSERT_TRUE(session().load(at("old.ini")));

    EXPECT_EQ(session().path(), at("old.json"));
    EXPECT_EQ(session().config().general.alwaysAdd, "-legacy");
    EXPECT_FALSE(fs::exists(at("old.json")));
}

TEST_F(SessionTest, LoadingAMissingIniSaysSo) {
    std::string error;

    EXPECT_FALSE(session().load(at("absent.ini"), &error));
    EXPECT_EQ(error, "could not read " + at("absent.ini").string());
}

TEST_F(SessionTest, AFailedLoadKeepsTheOpenConfig) {
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(at("good.json")));
    ASSERT_TRUE(session().load(at("good.json")));

    same::spill(at("bad.json"), "[");

    std::string error;

    EXPECT_FALSE(session().load(at("bad.json"), &error));
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(session().path(), at("good.json"));
    EXPECT_EQ(session().config().profiles.size(), 2U);
}

TEST_F(SessionTest, SaveWritesTheOpenConfigToItsPath) {
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(at("mine.json")));
    ASSERT_TRUE(session().load(at("mine.json")));

    session().config().addProfile("Added");

    ASSERT_TRUE(session().save());

    EXPECT_EQ(fromDisk(at("mine.json")).profiles.size(), 3U);
}

TEST_F(SessionTest, SaveAsMovesTheConfigToTheNewPath) {
    session().start({});
    session().config().addProfile("Moved");

    ASSERT_TRUE(session().saveAs(at("moved.json")));

    EXPECT_EQ(session().path(), at("moved.json"));
    EXPECT_EQ(session().source(), Session::Source::UserSpecified);
    EXPECT_EQ(fromDisk(at("moved.json")).profiles.size(), 1U);
    EXPECT_FALSE(fs::exists(userJson()));
}

TEST_F(SessionTest, AFailedSaveAsKeepsThePath) {
    session().start({});
    same::spill(at("blocker"), "x");

    std::string error;

    EXPECT_FALSE(session().saveAs(at("blocker") / "zdl.json", &error));
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(session().path(), userJson());
}

TEST_F(SessionTest, AdoptingCopiesTheConfigToTheUserPathAndStampsIt) {
    ASSERT_TRUE(tests::Fixtures::config(3, 0).save(at("portable.json")));
    ASSERT_TRUE(session().load(at("portable.json")));

    std::string error;

    ASSERT_TRUE(session().adoptAsUserConfig(&error)) << error;

    EXPECT_EQ(session().path(), userJson());
    EXPECT_EQ(session().source(), Session::Source::User);

    const Config written = fromDisk(userJson());

    EXPECT_EQ(written.profiles.size(), 3U);
    EXPECT_TRUE(written.general.isImported);
    EXPECT_EQ(written.general.importedFrom, at("portable.json").string());
    EXPECT_TRUE(std::regex_match(written.general.importDate, std::regex(R"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z)")))
            << written.general.importDate;
}

TEST_F(SessionTest, AdoptingTheUserConfigItselfStampsNothing) {
    session().start({});

    ASSERT_TRUE(session().adoptAsUserConfig());

    EXPECT_FALSE(session().config().general.isImported);
    EXPECT_TRUE(session().config().general.importedFrom.empty());
    EXPECT_TRUE(fs::is_regular_file(userJson()));
}

TEST_F(SessionTest, AFailedAdoptionKeepsThePathAndSaysSo) {
    ASSERT_TRUE(tests::Fixtures::config(1, 0).save(at("portable.json")));
    ASSERT_TRUE(session().load(at("portable.json")));

    same::spill(userJson().parent_path(), "blocks the directory");

    std::string error;

    EXPECT_FALSE(session().adoptAsUserConfig(&error));
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(session().path(), at("portable.json"));
    EXPECT_EQ(session().source(), Session::Source::UserSpecified);
}

TEST_F(SessionTest, AFailedAdoptionLeavesTheOpenConfigUnstamped) {
    ASSERT_TRUE(tests::Fixtures::config(1, 0).save(at("portable.json")));
    ASSERT_TRUE(session().load(at("portable.json")));

    same::spill(userJson().parent_path(), "blocks the directory");

    EXPECT_FALSE(session().adoptAsUserConfig());
    EXPECT_FALSE(session().config().general.isImported);
    EXPECT_TRUE(session().config().general.importedFrom.empty());
    EXPECT_TRUE(session().config().general.importDate.empty());
}

TEST_F(SessionTest, WithNoUserConfigNothingIsIgnored) {
    EXPECT_FALSE(session().userConfigIgnored());
}

TEST_F(SessionTest, TheIgnoreFlagIsReadFromTheUserConfigWhileAnotherIsOpen) {
    ASSERT_TRUE(withNoUserConf(tests::Fixtures::config(1, 0)).save(userJson()));
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(at("other.json")));
    ASSERT_TRUE(session().load(at("other.json")));

    EXPECT_TRUE(session().userConfigIgnored());
    EXPECT_FALSE(session().config().general.noUserConf);
}

TEST_F(SessionTest, TheIgnoreFlagIsReadFromALegacyUserIni) {
    same::spill(userIni(), "[zdl.general]\nnouserconf=1\nalwaysadd=-x\n");

    EXPECT_TRUE(session().userConfigIgnored());
    EXPECT_FALSE(fs::exists(userJson()));
}

TEST_F(SessionTest, WhileTheUserConfigIsOpenTheIgnoreFlagIsItsOwn) {
    session().start({});

    ASSERT_TRUE(session().setUserConfigIgnored(true));

    EXPECT_TRUE(session().config().general.noUserConf);
    EXPECT_TRUE(session().userConfigIgnored());
    EXPECT_FALSE(fs::exists(userJson()));
}

TEST_F(SessionTest, SettingTheIgnoreFlagWhileAnotherIsOpenWritesTheUserConfig) {
    ASSERT_TRUE(tests::Fixtures::config(3, 0).save(userJson()));
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(at("other.json")));
    ASSERT_TRUE(session().load(at("other.json")));

    ASSERT_TRUE(session().setUserConfigIgnored(true));

    const Config user = fromDisk(userJson());

    EXPECT_TRUE(user.general.noUserConf);
    EXPECT_EQ(user.profiles.size(), 3U);
    EXPECT_FALSE(session().config().general.noUserConf);
    EXPECT_TRUE(session().userConfigIgnored());
}

TEST_F(SessionTest, SettingTheIgnoreFlagWithNoUserConfigCreatesOne) {
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(at("other.json")));
    ASSERT_TRUE(session().load(at("other.json")));

    ASSERT_TRUE(session().setUserConfigIgnored(true));

    ASSERT_TRUE(fs::is_regular_file(userJson()));
    EXPECT_TRUE(fromDisk(userJson()).general.noUserConf);
    EXPECT_TRUE(fromDisk(userJson()).profiles.empty());
}

TEST_F(SessionTest, ClearingTheIgnoreFlagWithNoUserConfigWritesNothing) {
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(at("other.json")));
    ASSERT_TRUE(session().load(at("other.json")));

    EXPECT_TRUE(session().setUserConfigIgnored(false));
    EXPECT_FALSE(fs::exists(userJson()));
}

TEST_F(SessionTest, ClearingTheIgnoreFlagOfALegacyUserIniMigratesIt) {
    same::spill(userIni(), "[zdl.general]\nnouserconf=1\nalwaysadd=-x\n");
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(at("other.json")));
    ASSERT_TRUE(session().load(at("other.json")));

    ASSERT_TRUE(session().setUserConfigIgnored(false));

    const Config user = fromDisk(userJson());

    EXPECT_FALSE(user.general.noUserConf);
    EXPECT_EQ(user.general.alwaysAdd, "-x");
    EXPECT_FALSE(session().userConfigIgnored());
}

TEST_F(SessionTest, AnIgnoredUserConfigIsPassedOverOnTheNextStart) {
    ASSERT_TRUE(tests::Fixtures::config(3, 0).save(userJson()));
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(beside("zdl.json")));
    ASSERT_TRUE(tests::Fixtures::config(1, 0).save(at("other.json")));
    ASSERT_TRUE(session().load(at("other.json")));
    ASSERT_TRUE(session().setUserConfigIgnored(true));

    Session::get() = pristine;
    session().start({});

    EXPECT_EQ(session().source(), Session::Source::Portable);
    EXPECT_EQ(session().config().profiles.size(), 2U);
}
