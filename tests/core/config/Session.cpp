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

#include <chrono>
#include <filesystem>
#include <fstream>
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

fs::path userProfile(const char *stem) {
    return ttk::Paths::data_directory() / "profiles" / stem;
}

Config oneProfile(const char *config) {
    Config out;
    Profile profile;

    profile.id = "p";
    profile.name = "Doom";
    profile.config = config;
    out.profiles.push_back(profile);
    out.activeProfileId = profile.id;

    return out;
}

void age(const fs::path &file) {
    fs::last_write_time(file, fs::file_time_type::clock::now() - std::chrono::hours(1));
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
        ConfigPaths::setPortable(false);

        _executable = ttk::Paths::executable();
        _beside = tests::Sandbox::scratch("session-exe");
        _dir = tests::Sandbox::scratch("session");

        ttk::Paths::set_executable(_beside / "qzdl");

        forgetUser();
    }

    void TearDown() override {
        Session::get() = pristine;
        ConfigPaths::setPortable(false);
        ttk::Paths::set_executable(_executable);

        forgetUser();
    }

    [[nodiscard]] fs::path at(const char *name) const { return _dir / name; }

    [[nodiscard]] fs::path beside(const char *name) const { return _beside / name; }

    [[nodiscard]] fs::path besideProfile(const char *stem) const { return _beside / "profiles" / stem; }

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

TEST_F(SessionTest, AJsonThatDoesNotParseIsSetAsideAndStillTakenAsTheConfig) {
    same::spill(at("broken.json"), "{ not json");

    const Strings rest = session().start({at("broken.json").string()});

    EXPECT_TRUE(rest.empty());
    EXPECT_EQ(session().path(), at("broken.json"));
    EXPECT_TRUE(session().config().profiles.empty());
    EXPECT_EQ(same::slurp(at("broken.json.bad")), "{ not json");
    EXPECT_FALSE(session().warning().empty());

    ASSERT_TRUE(session().save());
    EXPECT_EQ(same::slurp(at("broken.json.bad")), "{ not json");
}

TEST_F(SessionTest, AUserConfigThatDoesNotParseIsNotSavedOver) {
    const std::string broken = R"({"engine": "ZDL", "profiles": [],})";

    same::spill(userJson(), broken);

    session().start({});

    EXPECT_EQ(session().path(), userJson());
    EXPECT_EQ(same::slurp(fs::path(userJson()) += ".bad"), broken);
    EXPECT_NE(session().warning().find(userJson().string()), std::string::npos);
}

TEST_F(SessionTest, AnEarlierSetAsideCopyIsKept) {
    same::spill(at("broken.json.bad"), "first");
    same::spill(at("broken.json"), "second");

    session().start({at("broken.json").string()});

    EXPECT_EQ(same::slurp(at("broken.json.bad")), "first");
    EXPECT_EQ(same::slurp(at("broken.json.bad-2")), "second");
}

TEST_F(SessionTest, ABlankConfigIsNoConfigYet) {
    same::spill(at("blank.json"), " \n");

    session().start({at("blank.json").string()});

    EXPECT_TRUE(session().warning().empty());
    EXPECT_FALSE(fs::exists(at("blank.json.bad")));
    EXPECT_TRUE(session().save());
}

TEST_F(SessionTest, AConfigThatCannotBeMovedAsideIsNotSavedOver) {
    same::spill(at("locked") / "broken.json", "{ not json");
    fs::permissions(at("locked"), fs::perms::owner_read | fs::perms::owner_exec, fs::perm_options::replace);

    if (std::ofstream(at("locked") / "probe")) {
        fs::permissions(at("locked"), fs::perms::owner_all, fs::perm_options::replace);

        GTEST_SKIP() << "the read-only folder still takes writes, as it does for root";
    }

    session().start({(at("locked") / "broken.json").string()});

    std::string error;

    EXPECT_FALSE(session().warning().empty());
    EXPECT_FALSE(session().save(&error));
    EXPECT_FALSE(error.empty());

    fs::permissions(at("locked"), fs::perms::owner_all, fs::perm_options::replace);

    EXPECT_EQ(same::slurp(at("locked") / "broken.json"), "{ not json");
    ASSERT_TRUE(session().saveAs(at("elsewhere.json")));
    EXPECT_TRUE(session().save());
}

TEST_F(SessionTest, ABlankConfigWithABackupLoadsTheBackup) {
    same::spill(at("mine.json"), "");
    ASSERT_TRUE(tests::Fixtures::config(3, 1).save(at("mine.json.bak")));

    session().start({at("mine.json").string()});

    same::config(session().config(), tests::Fixtures::config(3, 1));
    EXPECT_NE(session().warning().find("was empty"), std::string::npos) << session().warning();
    EXPECT_NE(session().warning().find("mine.json.bak"), std::string::npos) << session().warning();
    EXPECT_FALSE(fs::exists(at("mine.json.bad")));
}

TEST_F(SessionTest, AGarbledConfigWithABackupIsKeptAsideAndTheBackupLoaded) {
    same::spill(at("mine.json"), "{ not json");
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(at("mine.json.bak")));

    session().start({at("mine.json").string()});

    same::config(session().config(), tests::Fixtures::config(2, 0));
    EXPECT_EQ(same::slurp(at("mine.json.bad")), "{ not json");
    EXPECT_NE(session().warning().find("mine.json.bad"), std::string::npos) << session().warning();
    EXPECT_NE(session().warning().find("mine.json.bak"), std::string::npos) << session().warning();
}

TEST_F(SessionTest, AZeroFilledUserConfigIsKeptAsideAndTheBackupLoaded) {
    const std::string zeros(4096, '\0');

    same::spill(userJson(), zeros);
    ASSERT_TRUE(tests::Fixtures::config(4, 0).save(fs::path(userJson()) += ".bak"));

    session().start({});

    EXPECT_EQ(session().path(), userJson());
    EXPECT_EQ(session().config().profiles.size(), 4U);
    EXPECT_EQ(same::slurp(fs::path(userJson()) += ".bad"), zeros);
    EXPECT_FALSE(session().warning().empty());
}

TEST_F(SessionTest, ABlankConfigWithNoBackupStartsEmptyAndMakesNone) {
    same::spill(at("mine.json"), "\n");

    session().start({at("mine.json").string()});

    EXPECT_TRUE(session().config().profiles.empty());
    EXPECT_TRUE(session().warning().empty());

    ASSERT_TRUE(session().save());
    EXPECT_FALSE(fs::exists(at("mine.json.bak")));
}

TEST_F(SessionTest, AGarbledConfigIsNeverTakenAsTheBackup) {
    same::spill(at("mine.json"), "{ not json");

    session().start({at("mine.json").string()});

    ASSERT_TRUE(session().save());
    EXPECT_FALSE(fs::exists(at("mine.json.bak")));
}

TEST_F(SessionTest, AMissingUserConfigWithABackupIsAFreshStart) {
    const fs::path backup = fs::path(userJson()) += ".bak";

    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(backup));

    const std::string kept = same::slurp(backup);

    session().start({});

    EXPECT_EQ(session().path(), userJson());
    EXPECT_TRUE(session().config().profiles.empty());
    EXPECT_TRUE(session().warning().empty());

    ASSERT_TRUE(session().save());
    EXPECT_EQ(same::slurp(backup), kept);
}

TEST_F(SessionTest, TheFirstSaveBacksUpTheLoadedFile) {
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(at("mine.json")));

    const std::string loaded = same::slurp(at("mine.json"));

    session().start({at("mine.json").string()});
    session().config().addProfile("Added");

    EXPECT_FALSE(fs::exists(at("mine.json.bak")));
    ASSERT_TRUE(session().save());

    EXPECT_EQ(same::slurp(at("mine.json.bak")), loaded);
    EXPECT_EQ(fromDisk(at("mine.json")).profiles.size(), 3U);
}

TEST_F(SessionTest, ALaterSaveLeavesTheBackupAlone) {
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(at("mine.json")));

    const std::string loaded = same::slurp(at("mine.json"));

    session().start({at("mine.json").string()});
    session().config().addProfile("First");
    ASSERT_TRUE(session().save());
    session().config().addProfile("Second");
    ASSERT_TRUE(session().save());

    EXPECT_EQ(same::slurp(at("mine.json.bak")), loaded);
    EXPECT_EQ(fromDisk(at("mine.json")).profiles.size(), 4U);
}

TEST_F(SessionTest, TheFirstSaveReplacesABackupFromAnEarlierSession) {
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(at("mine.json")));
    ASSERT_TRUE(tests::Fixtures::config(5, 0).save(at("mine.json.bak")));

    const std::string loaded = same::slurp(at("mine.json"));

    session().start({at("mine.json").string()});
    ASSERT_TRUE(session().save());

    EXPECT_EQ(same::slurp(at("mine.json.bak")), loaded);
}

TEST_F(SessionTest, ARestoredConfigSurvivesASecondCrash) {
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(at("mine.json")));

    const std::string good = same::slurp(at("mine.json"));

    session().start({at("mine.json").string()});
    ASSERT_TRUE(session().save());
    same::spill(at("mine.json"), "");

    Session::get() = pristine;
    session().start({at("mine.json").string()});

    ASSERT_EQ(session().config().profiles.size(), 2U);
    session().config().addProfile("After the restore");
    ASSERT_TRUE(session().save());

    EXPECT_EQ(same::slurp(at("mine.json.bak")), good);

    same::spill(at("mine.json"), "");

    Session::get() = pristine;
    session().start({at("mine.json").string()});

    EXPECT_EQ(session().config().profiles.size(), 2U);
    EXPECT_FALSE(session().warning().empty());
    EXPECT_EQ(same::slurp(at("mine.json.bak")), good);
}

TEST_F(SessionTest, AConfigThatCannotBeMovedAsideIsHeldEvenWithABackup) {
    same::spill(at("locked") / "broken.json", "{ not json");
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(at("locked") / "broken.json.bak"));
    fs::permissions(at("locked"), fs::perms::owner_read | fs::perms::owner_exec, fs::perm_options::replace);

    if (std::ofstream(at("locked") / "probe")) {
        fs::permissions(at("locked"), fs::perms::owner_all, fs::perm_options::replace);

        GTEST_SKIP() << "the read-only folder still takes writes, as it does for root";
    }

    session().start({(at("locked") / "broken.json").string()});

    const bool saved = session().save();

    fs::permissions(at("locked"), fs::perms::owner_all, fs::perm_options::replace);

    EXPECT_FALSE(saved);
    EXPECT_TRUE(session().config().profiles.empty());
    EXPECT_NE(session().warning().find("nothing will be saved"), std::string::npos) << session().warning();
    EXPECT_EQ(same::slurp(at("locked") / "broken.json"), "{ not json");
}

TEST_F(SessionTest, AMigratedIniIsNotTakenAsTheBackup) {
    same::spill(at("old.ini"), LEGACY);

    session().start({at("old.ini").string()});

    ASSERT_TRUE(session().save());
    EXPECT_FALSE(fs::exists(at("old.json.bak")));
}

TEST_F(SessionTest, LoadingAJsonBacksItUpOnTheFirstSave) {
    ASSERT_TRUE(tests::Fixtures::config(3, 0).save(at("other.json")));

    const std::string loaded = same::slurp(at("other.json"));

    ASSERT_TRUE(session().load(at("other.json")));
    session().config().addProfile("Added");
    ASSERT_TRUE(session().save());

    EXPECT_EQ(same::slurp(at("other.json.bak")), loaded);
}

TEST_F(SessionTest, SavingAsAnotherPathMakesNoBackup) {
    ASSERT_TRUE(tests::Fixtures::config(3, 0).save(at("mine.json")));

    session().start({at("mine.json").string()});

    ASSERT_TRUE(session().saveAs(at("moved.json")));
    ASSERT_TRUE(session().save());

    EXPECT_FALSE(fs::exists(at("mine.json.bak")));
    EXPECT_FALSE(fs::exists(at("moved.json.bak")));
}

TEST_F(SessionTest, AReadableConfigLeavesNoWarning) {
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(userJson()));

    session().start({});

    EXPECT_TRUE(session().warning().empty());
    EXPECT_FALSE(fs::exists(fs::path(userJson()) += ".bad"));
}

TEST_F(SessionTest, TheIgnoreFlagIsNotWrittenOverAUserConfigThatDoesNotParse) {
    same::spill(userJson(), "{ not json, not at all");
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(at("other.json")));
    ASSERT_TRUE(session().load(at("other.json")));

    std::string error;

    EXPECT_FALSE(session().setUserConfigIgnored(true, &error));
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(same::slurp(userJson()), "{ not json, not at all");
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

TEST_F(SessionTest, AZdlOpenedAgainActivatesTheProfileItCameInAs) {
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(userJson()));

    const std::string file = zdl("sunlust.zdl", zdlProfile("Sunlust")).string();

    session().start({file});

    const std::string first = session().config().activeProfileId;

    session().config().activeProfile().warp = "MAP07";
    session().config().setActiveProfile("profile-0");

    ASSERT_TRUE(session().save());

    Session::get() = pristine;

    EXPECT_TRUE(session().start({file}).empty());
    EXPECT_TRUE(session().openedZdlFile());
    EXPECT_EQ(session().config().profiles.size(), 3U);
    EXPECT_EQ(session().config().activeProfileId, first);
    EXPECT_EQ(session().config().activeProfile().warp, "MAP07");
    EXPECT_EQ(session().config().profiles.back().config, "sunlust.cfg");
}

TEST_F(SessionTest, AZdlReachedThroughAnotherSpellingIsKnown) {
    const fs::path file = zdl("sunlust.zdl", zdlProfile("Sunlust"));

    session().start({file.string()});

    ASSERT_TRUE(session().save());

    Session::get() = pristine;

    session().start({(file.parent_path() / "." / "sunlust.zdl").string()});

    EXPECT_EQ(session().config().profiles.size(), 1U);
}

TEST_F(SessionTest, AnotherZdlWithTheSameNameIsANewProfile) {
    session().start({zdl("a.zdl", zdlProfile("Sunlust")).string()});

    ASSERT_TRUE(session().save());

    Session::get() = pristine;

    session().start({zdl("b.zdl", zdlProfile("Sunlust")).string()});

    ASSERT_EQ(session().config().profiles.size(), 2U);
    EXPECT_EQ(session().config().activeProfile().name, "Sunlust (2)");
}

TEST_F(SessionTest, AZdlOfADeletedProfileImportsAgain) {
    const std::string file = zdl("sunlust.zdl", zdlProfile("Sunlust")).string();

    session().start({file});
    session().config().removeProfile(session().config().activeProfileId);

    ASSERT_TRUE(session().save());

    Session::get() = pristine;

    session().start({file});

    EXPECT_EQ(session().config().profiles.size(), 1U);
    EXPECT_EQ(session().config().activeProfile().name, "Sunlust");
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

TEST_F(SessionTest, APortableStartKeepsDataBesideTheExecutable) {
    ASSERT_TRUE(oneProfile("doom.cfg").save(beside("zdl.json")));

    session().start({});

    EXPECT_EQ(ConfigPaths::dataDirectory(), ttk::Paths::executable_directory());
    EXPECT_EQ(Config::profileFolder("doom"), besideProfile("doom"));
}

TEST_F(SessionTest, AUserStartKeepsDataInTheUserFolder) {
    ASSERT_TRUE(oneProfile("doom.cfg").save(userJson()));
    ASSERT_TRUE(oneProfile("doom.cfg").save(beside("zdl.json")));

    session().start({});

    EXPECT_EQ(session().source(), Session::Source::User);
    EXPECT_EQ(ConfigPaths::dataDirectory(), ttk::Paths::data_directory());
    EXPECT_EQ(Config::profileFolder("doom"), userProfile("doom"));
}

TEST_F(SessionTest, APortableStartNamesNewConfigsAgainstFoldersBesideIt) {
    fs::create_directories(besideProfile("doom"));
    ASSERT_TRUE(oneProfile("").save(beside("zdl.json")));

    session().start({});

    ASSERT_EQ(session().config().profiles.size(), 1U);
    EXPECT_EQ(session().config().profiles.front().config, "doom-2.cfg");
}

TEST_F(SessionTest, APortableStartCopiesItsProfileFolderFromTheUserFolder) {
    same::spill(userProfile("doom") / "doom.cfg", "user");
    same::spill(userProfile("doom") / "saves" / "a.zds", "save");
    ASSERT_TRUE(oneProfile("doom.cfg").save(beside("zdl.json")));

    session().start({});

    EXPECT_EQ(same::slurp(besideProfile("doom") / "doom.cfg"), "user");
    EXPECT_EQ(same::slurp(besideProfile("doom") / "saves" / "a.zds"), "save");
    EXPECT_EQ(same::slurp(userProfile("doom") / "doom.cfg"), "user");
    EXPECT_TRUE(fs::is_regular_file(userProfile("doom") / "saves" / "a.zds"));
}

TEST_F(SessionTest, AProfileFolderIsCopiedOnlyOnce) {
    same::spill(userProfile("doom") / "doom.cfg", "user");
    ASSERT_TRUE(oneProfile("doom.cfg").save(beside("zdl.json")));

    session().start({});

    same::spill(userProfile("doom") / "doom.cfg", "changed");
    same::spill(userProfile("doom") / "later.txt", "later");

    Session::get() = pristine;
    session().start({});

    EXPECT_EQ(same::slurp(besideProfile("doom") / "doom.cfg"), "user");
    EXPECT_FALSE(fs::exists(besideProfile("doom") / "later.txt"));
}

TEST_F(SessionTest, AProfileFolderBesideTheExecutableIsNotOverwritten) {
    same::spill(besideProfile("doom") / "doom.cfg", "mine");
    same::spill(userProfile("doom") / "doom.cfg", "theirs");
    same::spill(userProfile("doom") / "extra.txt", "extra");
    ASSERT_TRUE(oneProfile("doom.cfg").save(beside("zdl.json")));

    session().start({});

    EXPECT_EQ(same::slurp(besideProfile("doom") / "doom.cfg"), "mine");
    EXPECT_FALSE(fs::exists(besideProfile("doom") / "extra.txt"));
}

TEST_F(SessionTest, OnlyTheFoldersOfPortableProfilesAreCopied) {
    same::spill(userProfile("doom") / "doom.cfg", "user");
    same::spill(userProfile("other") / "other.cfg", "other");
    ASSERT_TRUE(oneProfile("doom.cfg").save(beside("zdl.json")));

    session().start({});

    EXPECT_TRUE(fs::is_directory(besideProfile("doom")));
    EXPECT_FALSE(fs::exists(besideProfile("other")));
}

TEST_F(SessionTest, NoProfileFolderIsCopiedForAConfigOnTheCommandLine) {
    same::spill(userProfile("doom") / "doom.cfg", "user");
    ASSERT_TRUE(oneProfile("doom.cfg").save(at("mine.json")));

    session().start({at("mine.json").string()});

    EXPECT_EQ(ConfigPaths::dataDirectory(), ttk::Paths::data_directory());
    EXPECT_FALSE(fs::exists(beside("profiles")));
}

TEST_F(SessionTest, LoadingAnotherConfigLeavesPortableMode) {
    ASSERT_TRUE(oneProfile("doom.cfg").save(beside("zdl.json")));
    ASSERT_TRUE(oneProfile("doom.cfg").save(at("other.json")));

    session().start({});
    ASSERT_TRUE(session().load(at("other.json")));

    EXPECT_EQ(ConfigPaths::dataDirectory(), ttk::Paths::data_directory());
}

TEST_F(SessionTest, SavingAsLeavesPortableMode) {
    ASSERT_TRUE(oneProfile("doom.cfg").save(beside("zdl.json")));

    session().start({});
    ASSERT_TRUE(session().saveAs(at("copy.json")));

    EXPECT_EQ(ConfigPaths::dataDirectory(), ttk::Paths::data_directory());
}

TEST_F(SessionTest, AdoptingAsTheUserConfigLeavesPortableMode) {
    ASSERT_TRUE(oneProfile("doom.cfg").save(beside("zdl.json")));

    session().start({});
    ASSERT_TRUE(session().adoptAsUserConfig());

    EXPECT_EQ(ConfigPaths::dataDirectory(), ttk::Paths::data_directory());
}

TEST_F(SessionTest, AdoptingFromAPortableSessionCopiesProfileFoldersToTheUserFolder) {
    same::spill(besideProfile("doom") / "doom.cfg", "live");
    same::spill(besideProfile("doom") / "saves" / "a.zds", "save");
    ASSERT_TRUE(oneProfile("doom.cfg").save(beside("zdl.json")));

    session().start({});
    ASSERT_TRUE(session().adoptAsUserConfig());

    EXPECT_EQ(same::slurp(userProfile("doom") / "doom.cfg"), "live");
    EXPECT_EQ(same::slurp(userProfile("doom") / "saves" / "a.zds"), "save");
    EXPECT_EQ(same::slurp(besideProfile("doom") / "doom.cfg"), "live");
}

TEST_F(SessionTest, AdoptingReplacesAnOlderUserFileWithANewerPortableOne) {
    same::spill(userProfile("doom") / "doom.cfg", "stale");
    age(userProfile("doom") / "doom.cfg");
    same::spill(besideProfile("doom") / "doom.cfg", "live");
    ASSERT_TRUE(oneProfile("doom.cfg").save(beside("zdl.json")));

    session().start({});
    ASSERT_TRUE(session().adoptAsUserConfig());

    EXPECT_EQ(same::slurp(userProfile("doom") / "doom.cfg"), "live");
}

TEST_F(SessionTest, AdoptingKeepsANewerUserFileOverAnOlderPortableOne) {
    same::spill(besideProfile("doom") / "doom.cfg", "old");
    age(besideProfile("doom") / "doom.cfg");
    same::spill(userProfile("doom") / "doom.cfg", "newer");
    same::spill(userProfile("doom") / "user.txt", "user");
    ASSERT_TRUE(oneProfile("doom.cfg").save(beside("zdl.json")));

    session().start({});
    ASSERT_TRUE(session().adoptAsUserConfig());

    EXPECT_EQ(same::slurp(userProfile("doom") / "doom.cfg"), "newer");
    EXPECT_EQ(same::slurp(userProfile("doom") / "user.txt"), "user");
    EXPECT_EQ(same::slurp(besideProfile("doom") / "doom.cfg"), "old");
}

TEST_F(SessionTest, SavingAsFromAPortableSessionCopiesOnlyNewerFiles) {
    same::spill(userProfile("doom") / "stale.cfg", "stale");
    age(userProfile("doom") / "stale.cfg");
    same::spill(besideProfile("doom") / "stale.cfg", "live");
    same::spill(besideProfile("doom") / "kept.cfg", "old");
    age(besideProfile("doom") / "kept.cfg");
    same::spill(userProfile("doom") / "kept.cfg", "newer");
    same::spill(besideProfile("doom") / "saves" / "a.zds", "save");
    ASSERT_TRUE(oneProfile("doom.cfg").save(beside("zdl.json")));

    session().start({});
    ASSERT_TRUE(session().saveAs(at("copy.json")));

    EXPECT_EQ(same::slurp(userProfile("doom") / "stale.cfg"), "live");
    EXPECT_EQ(same::slurp(userProfile("doom") / "kept.cfg"), "newer");
    EXPECT_EQ(same::slurp(userProfile("doom") / "saves" / "a.zds"), "save");
}

TEST_F(SessionTest, LoadingFromAPortableSessionCopiesNothing) {
    same::spill(besideProfile("doom") / "doom.cfg", "live");
    ASSERT_TRUE(oneProfile("doom.cfg").save(beside("zdl.json")));
    ASSERT_TRUE(oneProfile("doom.cfg").save(at("other.json")));

    session().start({});
    ASSERT_TRUE(session().load(at("other.json")));

    EXPECT_FALSE(fs::exists(userProfile("doom")));
}

TEST_F(SessionTest, SavingAsOutsidePortableModeCopiesNothing) {
    same::spill(besideProfile("doom") / "doom.cfg", "live");
    ASSERT_TRUE(oneProfile("doom.cfg").save(userJson()));

    session().start({});
    ASSERT_EQ(session().source(), Session::Source::User);
    ASSERT_TRUE(session().saveAs(at("copy.json")));

    EXPECT_FALSE(fs::exists(userProfile("doom")));
}
