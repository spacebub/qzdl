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
#include "core/config/Versions.h"
#include "support/Fixtures.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

using Strings = std::vector<std::string>;

// Session has no way to forget where it was opened from, so every case starts from
// a copy taken before any case ran.
// NOLINTNEXTLINE(bugprone-throwing-static-initialization,cert-err58-cpp): a failed copy may abort the run.
const Session pristine = Session::get();

constexpr const char *LEGACY = "[zdl.general]\nalwaysadd=-legacy\nnouserconf=0\n"
                               "[zdl.iwads]\ni0n=Doom\ni0f=/g/doom.wad\n"
                               "[zdl.save]\niwad=Doom\nskill=4\nfile0=/m/a.wad\n";

// Version 8, as 4.5.0 wrote it.
constexpr const char *FROM_4_5 = R"({"version": 1, "engine": "ZDL4", "appVersion": "4.5.0", "profiles": []})";

// A later release, with a key this one does not know.
std::string fromLater(const std::string &app) {
    return R"({"version": )" + std::to_string(Versions::CURRENT + 1) + R"(, "engine": "ZDL4", "appVersion": ")" + app
           + R"(", "future": {"kept": true}, "profiles": []})";
}

fs::path copyOf(const fs::path &config, const int version) {
    return fs::path(config) += ".v" + std::to_string(version);
}

// Empty for a folder that is not there.
Strings versionCopies(const fs::path &folder) {
    Strings out;
    std::error_code code;

    for (const fs::directory_entry &entry : fs::directory_iterator(folder, code)) {
        if (const std::string name = entry.path().filename().string(); name.find(".json.v") != std::string::npos) {
            out.push_back(name);
        }
    }

    return out;
}


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
        std::error_code code;

        fs::permissions(_beside, fs::perms::owner_all, fs::perm_options::replace, code);
        unlockUserFolder();

        Session::get() = pristine;
        ConfigPaths::setPortable(false);
        ttk::Paths::set_executable(_executable);

        forgetUser();
    }

    [[nodiscard]] fs::path at(const char *name) const { return _dir / name; }

    [[nodiscard]] fs::path beside(const char *name) const { return _beside / name; }

    static void restart(const Strings &arguments = {}) {
        Session::get() = pristine;
        session().start(arguments);
    }

    // False where a read-only folder still takes writes, as it does for root.
    [[nodiscard]] bool lockProgramFolder() const {
        fs::permissions(_beside, fs::perms::owner_read | fs::perms::owner_exec, fs::perm_options::replace);

        if (std::ofstream(_beside / "probe")) {
            unlockProgramFolder();
            fs::remove(_beside / "probe");

            return false;
        }

        return true;
    }

    void unlockProgramFolder() const { fs::permissions(_beside, fs::perms::owner_all, fs::perm_options::replace); }

    // False where a read-only folder still takes writes, as it does for root.
    [[nodiscard]] static bool lockUserFolder() {
        fs::permissions(
                userJson().parent_path(), fs::perms::owner_read | fs::perms::owner_exec, fs::perm_options::replace);

        if (std::ofstream(userJson().parent_path() / "probe")) {
            unlockUserFolder();
            fs::remove(userJson().parent_path() / "probe");

            return false;
        }

        return true;
    }

    static void unlockUserFolder() {
        std::error_code code;

        fs::permissions(userJson().parent_path(), fs::perms::owner_all, fs::perm_options::replace, code);
    }

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

TEST_F(SessionTest, AJsonArgumentFromTheFirstReleasesIsOpened) {
    same::spill(at("first.json"), R"({"engine": "ZDL", "appVersion": "4.0.0", "profiles": []})");

    const Strings rest = session().start({at("first.json").string()});

    EXPECT_TRUE(rest.empty());
    EXPECT_EQ(session().source(), Session::Source::UserSpecified);
    EXPECT_EQ(session().path(), at("first.json"));
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

TEST_F(SessionTest, TheOldBackupStaysWhenTheConfigIsGoneAtTheFirstSave) {
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(at("mine.json")));
    ASSERT_TRUE(tests::Fixtures::config(5, 0).save(at("mine.json.bak")));

    const std::string old = same::slurp(at("mine.json.bak"));

    session().start({at("mine.json").string()});
    fs::remove(at("mine.json"));

    ASSERT_TRUE(session().save());

    EXPECT_EQ(same::slurp(at("mine.json.bak")), old);
    EXPECT_FALSE(fs::exists(at("mine.json.bak.new")));
    EXPECT_TRUE(fs::is_regular_file(at("mine.json")));
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

TEST_F(SessionTest, AnOlderConfigIsKeptBesideItUnderItsVersion) {
    same::spill(userJson(), FROM_4_5);

    session().start({});

    const Session::FormatNote &note = session().formatNote();

    EXPECT_EQ(same::slurp(copyOf(userJson(), 8)), FROM_4_5);
    EXPECT_EQ(same::slurp(userJson()), FROM_4_5);
    EXPECT_FALSE(note.newer);
    EXPECT_NE(note.text.find("ZDL4 4.5.0"), std::string::npos) << note.text;
    EXPECT_NE(note.text.find("kept as zdl.json.v8."), std::string::npos) << note.text;
    EXPECT_TRUE(session().warning().empty());
}

TEST_F(SessionTest, AnOlderConfigStillOnDiskSaysNothingAgain) {
    same::spill(userJson(), FROM_4_5);

    restart();
    restart();

    EXPECT_TRUE(session().formatNote().text.empty()) << session().formatNote().text;
    EXPECT_EQ(same::slurp(copyOf(userJson(), 8)), FROM_4_5);
}

TEST_F(SessionTest, AnOlderConfigOnceSavedSaysNothingAndTheCopyStays) {
    same::spill(userJson(), FROM_4_5);

    restart();
    ASSERT_TRUE(session().save());
    restart();

    EXPECT_EQ(fromDisk(userJson()).fileVersion, Versions::CURRENT);
    EXPECT_TRUE(session().formatNote().text.empty()) << session().formatNote().text;
    EXPECT_EQ(same::slurp(copyOf(userJson(), 8)), FROM_4_5);
}

TEST_F(SessionTest, ANewerConfigIsKeptWholeAndStillSaved) {
    const std::string later = fromLater("5.0.0");

    same::spill(userJson(), later);

    session().start({});

    const Session::FormatNote &note = session().formatNote();
    const fs::path copy = copyOf(userJson(), Versions::CURRENT + 1);

    EXPECT_EQ(same::slurp(copy), later);
    EXPECT_TRUE(note.newer);
    EXPECT_NE(note.text.find("ZDL4 5.0.0"), std::string::npos) << note.text;
    EXPECT_NE(note.text.find("kept as " + copy.filename().string() + "."), std::string::npos) << note.text;

    ASSERT_TRUE(session().save());

    EXPECT_EQ(fromDisk(userJson()).fileVersion, Versions::CURRENT);
    EXPECT_EQ(same::slurp(userJson()).find("future"), std::string::npos);
    EXPECT_EQ(same::slurp(copy), later);
}

TEST_F(SessionTest, ANewerConfigWrittenAgainReplacesTheCopyAndSaysSoAgain) {
    const std::string again = fromLater("5.0.1");

    same::spill(userJson(), fromLater("5.0.0"));
    restart();
    same::spill(userJson(), again);
    restart();

    EXPECT_EQ(same::slurp(copyOf(userJson(), Versions::CURRENT + 1)), again);
    EXPECT_NE(session().formatNote().text.find("ZDL4 5.0.1"), std::string::npos) << session().formatNote().text;
}

TEST_F(SessionTest, TheCopyKeepsItsBytesWhenTheConfigIsWrittenInPlace) {
    same::spill(userJson(), FROM_4_5);

    session().start({});
    same::spill(userJson(), R"({"version": 1, "engine": "ZDL4", "appVersion": "4.5.0", "edited": "in place"})");

    EXPECT_EQ(same::slurp(copyOf(userJson(), 8)), FROM_4_5);
}

TEST_F(SessionTest, TurningPortableModeOnKeepsAnOlderUserConfigAndSaysNothing) {
    same::spill(userJson(), FROM_4_5);
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(at("other.json")));
    ASSERT_TRUE(session().load(at("other.json")));

    ASSERT_TRUE(session().setPortableWanted(true));

    EXPECT_EQ(same::slurp(copyOf(userJson(), 8)), FROM_4_5);
    EXPECT_EQ(fromDisk(userJson()).fileVersion, Versions::CURRENT);
    EXPECT_TRUE(session().formatNote().text.empty()) << session().formatNote().text;
}

TEST_F(SessionTest, AnOlderUserConfigMovedBesideTheProgramIsKeptAndSaysSo) {
    const std::string older = R"({"version": 1, "engine": "ZDL4", "appVersion": "4.5.0",)"
                              R"( "general": {"noUserConf": true, "moveAtNextStart": true}, "profiles": []})";

    same::spill(userJson(), older);

    session().start({});

    EXPECT_EQ(session().path(), beside("zdl.json"));
    EXPECT_EQ(same::slurp(copyOf(userJson(), 8)), older);
    EXPECT_NE(session().formatNote().text.find("kept as zdl.json.v8."), std::string::npos)
            << session().formatNote().text;
}

TEST_F(SessionTest, AnOlderUserConfigReplacedByTheMoveBackIsKeptAndSaysSo) {
    const std::string older = R"({"version": 1, "engine": "ZDL4", "appVersion": "4.5.0",)"
                              R"( "general": {"moveAtNextStart": true}, "profiles": []})";

    same::spill(userJson(), older);
    ASSERT_TRUE(oneProfile("doom.cfg").save(beside("zdl.json")));

    session().start({});

    EXPECT_EQ(session().path(), userJson());
    EXPECT_EQ(session().config().profiles.size(), 1U);
    EXPECT_EQ(same::slurp(copyOf(userJson(), 8)), older);
    EXPECT_NE(session().formatNote().text.find("kept as zdl.json.v8."), std::string::npos)
            << session().formatNote().text;
}

TEST_F(SessionTest, AnOlderConfigBesideTheProgramReplacedByTheMoveIsKept) {
    Config user = withNoUserConf(oneProfile("doom.cfg"));

    user.general.moveAtNextStart = true;

    ASSERT_TRUE(user.save(userJson()));
    same::spill(beside("zdl.json"), FROM_4_5);

    session().start({});

    EXPECT_EQ(session().path(), beside("zdl.json"));
    EXPECT_EQ(session().config().profiles.size(), 1U);
    EXPECT_EQ(same::slurp(beside("zdl.json.v8")), FROM_4_5);
    EXPECT_TRUE(session().formatNote().text.empty()) << session().formatNote().text;
}

TEST_F(SessionTest, AnOlderUserConfigThatGivesWayToAPortableOneIsKeptSilently) {
    const std::string older = R"({"version": 1, "engine": "ZDL4", "appVersion": "4.5.0",)"
                              R"( "general": {"noUserConf": true}, "profiles": []})";

    same::spill(userJson(), older);
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(beside("zdl.json")));

    session().start({});

    EXPECT_EQ(session().source(), Session::Source::Portable);
    EXPECT_EQ(same::slurp(copyOf(userJson(), 8)), older);
    EXPECT_TRUE(session().formatNote().text.empty()) << session().formatNote().text;
}

TEST_F(SessionTest, AnOlderUserConfigThatOptsOutIsStillTheFallbackAndSaysSo) {
    const std::string older = R"({"version": 1, "engine": "ZDL4", "appVersion": "4.5.0",)"
                              R"( "general": {"noUserConf": true}, "profiles": []})";

    same::spill(userJson(), older);

    session().start({});

    EXPECT_EQ(session().source(), Session::Source::Fallback);
    EXPECT_EQ(same::slurp(copyOf(userJson(), 8)), older);
    EXPECT_NE(session().formatNote().text.find("kept as zdl.json.v8."), std::string::npos)
            << session().formatNote().text;
}

TEST_F(SessionTest, AConfigThatNamesNoReleaseComesFromAnotherVersion) {
    same::spill(userJson(), R"({"version": 5, "engine": "ZDL4", "profiles": []})");

    session().start({});

    EXPECT_NE(session().formatNote().text.find("another version of ZDL4"), std::string::npos)
            << session().formatNote().text;
    EXPECT_TRUE(fs::is_regular_file(copyOf(userJson(), 5)));
}

TEST_F(SessionTest, ACopyThatCannotBeWrittenIsNotClaimed) {
    fs::create_directories(copyOf(userJson(), 8) / "taken");
    same::spill(userJson(), FROM_4_5);

    session().start({});

    const std::string &text = session().formatNote().text;

    EXPECT_NE(text.find("could not be written"), std::string::npos) << text;
    EXPECT_EQ(text.find("kept as"), std::string::npos) << text;
    EXPECT_EQ(same::slurp(userJson()), FROM_4_5);
}

TEST_F(SessionTest, LoadingAnOlderConfigKeepsItBesideItUnderItsVersion) {
    same::spill(at("other.json"), FROM_4_5);

    ASSERT_TRUE(session().load(at("other.json")));

    EXPECT_EQ(same::slurp(at("other.json.v8")), FROM_4_5);
    EXPECT_NE(session().formatNote().text.find("kept as other.json.v8."), std::string::npos)
            << session().formatNote().text;
}

TEST_F(SessionTest, LoadingAnIniForgetsWhatTheConfigBeforeSaid) {
    same::spill(at("other.json"), FROM_4_5);
    same::spill(at("old.ini"), LEGACY);

    ASSERT_TRUE(session().load(at("other.json")));
    ASSERT_TRUE(session().load(at("old.ini")));

    EXPECT_TRUE(session().formatNote().text.empty()) << session().formatNote().text;
}

TEST_F(SessionTest, ACurrentConfigSaysNothingAboutItsFormat) {
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(userJson()));

    session().start({});

    EXPECT_TRUE(session().formatNote().text.empty()) << session().formatNote().text;
    EXPECT_TRUE(versionCopies(userJson().parent_path()).empty());
}

TEST_F(SessionTest, AStartFromALegacyIniSaysNothingAboutTheFormat) {
    same::spill(userIni(), LEGACY);

    session().start({});

    EXPECT_TRUE(session().formatNote().text.empty()) << session().formatNote().text;
    EXPECT_TRUE(versionCopies(userJson().parent_path()).empty());
    EXPECT_TRUE(versionCopies(userIni().parent_path()).empty());
}

TEST_F(SessionTest, AnOlderBackupLoadedAfterABadReadSaysNothingAboutTheFormat) {
    same::spill(userJson(), "{ not json, not at all");
    same::spill(fs::path(userJson()) += ".bak", FROM_4_5);

    session().start({});

    EXPECT_FALSE(session().warning().empty());
    EXPECT_EQ(session().config().fileVersion, 8);
    EXPECT_TRUE(session().formatNote().text.empty()) << session().formatNote().text;
    EXPECT_TRUE(versionCopies(userJson().parent_path()).empty());
}

TEST_F(SessionTest, AStartWithNoConfigSaysNothingAboutTheFormat) {
    session().start({});

    EXPECT_TRUE(session().formatNote().text.empty()) << session().formatNote().text;
    EXPECT_TRUE(versionCopies(userJson().parent_path()).empty());
}

TEST_F(SessionTest, ThePortableWishIsNotWrittenOverAUserConfigThatDoesNotParse) {
    same::spill(userJson(), "{ not json, not at all");
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(at("other.json")));
    ASSERT_TRUE(session().load(at("other.json")));

    std::string error;

    EXPECT_FALSE(session().setPortableWanted(true, &error));
    EXPECT_TRUE(error.starts_with("Could not write the user config: ")) << error;
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

TEST_F(SessionTest, TheLaunchSwitchIsNoFile) {
    const fs::path file = zdl("sunlust.zdl", zdlProfile("Sunlust"));
    const Strings rest = session().start({Session::LAUNCH, file.string()});

    EXPECT_TRUE(rest.empty());
    EXPECT_TRUE(session().launchAsked());
    EXPECT_TRUE(session().openedZdlFile());
    EXPECT_EQ(session().config().activeProfile().name, "Sunlust");
}

TEST_F(SessionTest, TheLaunchSwitchIsHeardAfterTheZdl) {
    session().start({zdl("s.zdl", zdlProfile("S")).string(), Session::LAUNCH});

    EXPECT_TRUE(session().launchAsked());
    EXPECT_TRUE(session().openedZdlFile());
}

TEST_F(SessionTest, TheLaunchSwitchWithAMissingZdlOpensNothing) {
    session().start({Session::LAUNCH, at("gone.zdl").string()});

    EXPECT_TRUE(session().launchAsked());
    EXPECT_FALSE(session().openedZdlFile());
}

TEST_F(SessionTest, TheLaunchSwitchReadsAConfigAfterTheZdl) {
    Config made = oneProfile("doom.cfg");

    made.profiles.front().warp = "MAP07";

    ASSERT_TRUE(made.save(at("mine.json")));
    ASSERT_TRUE(Import::saveZdlFile(at("icon.zdl"), made.profiles.front(), true));

    session().start({Session::LAUNCH, at("icon.zdl").string(), at("mine.json").string()});

    EXPECT_EQ(session().path(), at("mine.json"));
    EXPECT_TRUE(session().openedZdlFile());
    EXPECT_EQ(session().config().activeProfile().warp, "MAP07");
    EXPECT_EQ(session().config().profiles.size(), 1U);
}

TEST_F(SessionTest, ATiedZdlLaunchesItsProfileOnceTheFileIsGone) {
    const fs::path file = zdl("sunlust.zdl", zdlProfile("Sunlust"));

    session().start({file.string()});
    session().config().activeProfile().warp = "MAP07";

    ASSERT_TRUE(session().save());
    ASSERT_TRUE(fs::remove(file));

    Session::get() = pristine;

    session().start({Session::LAUNCH, file.string()});

    EXPECT_TRUE(session().openedZdlFile());
    EXPECT_EQ(session().config().activeProfile().warp, "MAP07");
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

TEST_F(SessionTest, WithNothingOnDiskPortableModeIsNotWanted) {
    EXPECT_FALSE(session().portableWanted());
}

TEST_F(SessionTest, AUserConfigAloneRunsAndStaysOutOfPortableMode) {
    ASSERT_TRUE(tests::Fixtures::config(1, 0).save(userJson()));

    session().start({});

    EXPECT_EQ(session().source(), Session::Source::User);
    EXPECT_FALSE(session().portableWanted());
    EXPECT_FALSE(session().openedByHand());
}

TEST_F(SessionTest, APortableConfigAloneRunsAndStaysInPortableMode) {
    ASSERT_TRUE(tests::Fixtures::config(1, 0).save(beside("zdl.json")));

    session().start({});

    EXPECT_EQ(session().source(), Session::Source::Portable);
    EXPECT_TRUE(session().portableWanted());
    EXPECT_FALSE(session().openedByHand());
}

TEST_F(SessionTest, WithBothTheUserConfigRunsAndPortableModeIsNotWanted) {
    ASSERT_TRUE(tests::Fixtures::config(1, 0).save(userJson()));
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(beside("zdl.json")));

    session().start({});

    EXPECT_EQ(session().source(), Session::Source::User);
    EXPECT_FALSE(session().portableWanted());
}

TEST_F(SessionTest, WithBothAndAUserConfigThatOptsOutPortableModeRunsAndIsWanted) {
    ASSERT_TRUE(withNoUserConf(tests::Fixtures::config(1, 0)).save(userJson()));
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(beside("zdl.json")));

    session().start({});

    EXPECT_EQ(session().source(), Session::Source::Portable);
    EXPECT_TRUE(session().portableWanted());
}

TEST_F(SessionTest, AUserConfigThatOptsOutWithNothingBesideTheProgramIsNotPortable) {
    ASSERT_TRUE(withNoUserConf(tests::Fixtures::config(1, 0)).save(userJson()));

    session().start({});

    EXPECT_EQ(session().source(), Session::Source::Fallback);
    EXPECT_FALSE(session().portableWanted());
}

TEST_F(SessionTest, AConfigByHandWithAUserConfigDoesNotWantPortableMode) {
    ASSERT_TRUE(tests::Fixtures::config(1, 0).save(userJson()));
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(beside("zdl.json")));
    ASSERT_TRUE(tests::Fixtures::config(3, 0).save(at("mine.json")));

    session().start({at("mine.json").string()});

    EXPECT_TRUE(session().openedByHand());
    EXPECT_FALSE(session().portableWanted());
}

TEST_F(SessionTest, AConfigByHandWithOnlyAPortableConfigWantsPortableMode) {
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(beside("zdl.json")));
    ASSERT_TRUE(tests::Fixtures::config(3, 0).save(at("mine.json")));

    session().start({at("mine.json").string()});

    EXPECT_EQ(session().source(), Session::Source::UserSpecified);
    EXPECT_TRUE(session().portableWanted());
}

TEST_F(SessionTest, PortableModeIsReadFromTheUserConfigWhileAnotherIsOpen) {
    ASSERT_TRUE(withNoUserConf(tests::Fixtures::config(1, 0)).save(userJson()));
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(beside("zdl.json")));
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(at("other.json")));
    ASSERT_TRUE(session().load(at("other.json")));

    EXPECT_TRUE(session().portableWanted());
    EXPECT_FALSE(session().config().general.noUserConf);
}

TEST_F(SessionTest, PortableModeIsReadFromALegacyUserIni) {
    same::spill(userIni(), "[zdl.general]\nnouserconf=1\nalwaysadd=-x\n");
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(beside("zdl.json")));

    EXPECT_TRUE(session().portableWanted());
    EXPECT_FALSE(fs::exists(userJson()));
}

TEST_F(SessionTest, AConfigOpenedFromBesideTheProgramIsNotByHand) {
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(beside("zdl.json")));
    ASSERT_TRUE(session().load(beside("zdl.json")));

    EXPECT_FALSE(session().openedByHand());
}

TEST_F(SessionTest, TurningPortableModeOnSavesTheWishAndTheMoveAndChangesNothingElse) {
    ASSERT_TRUE(oneProfile("doom.cfg").save(userJson()));

    session().start({});

    ASSERT_TRUE(session().setPortableWanted(true));

    const Config user = fromDisk(userJson());

    EXPECT_TRUE(user.general.noUserConf);
    EXPECT_TRUE(user.general.moveAtNextStart);
    EXPECT_TRUE(session().portableWanted());
    EXPECT_EQ(session().source(), Session::Source::User);
    EXPECT_EQ(session().path(), userJson());
    EXPECT_EQ(ConfigPaths::dataDirectory(), ttk::Paths::data_directory());
    EXPECT_FALSE(fs::exists(beside("zdl.json")));
}

TEST_F(SessionTest, FlippingOnAndOffBeforeARestartLeavesNoMove) {
    ASSERT_TRUE(oneProfile("doom.cfg").save(userJson()));

    session().start({});

    ASSERT_TRUE(session().setPortableWanted(true));
    ASSERT_TRUE(session().setPortableWanted(false));

    const Config user = fromDisk(userJson());

    EXPECT_FALSE(user.general.noUserConf);
    EXPECT_FALSE(user.general.moveAtNextStart);
    EXPECT_FALSE(session().portableWanted());

    restart();

    EXPECT_EQ(session().source(), Session::Source::User);
    EXPECT_FALSE(fs::exists(beside("zdl.json")));
}

TEST_F(SessionTest, FlippingOnAndOffByHandLeavesNoMove) {
    ASSERT_TRUE(tests::Fixtures::config(3, 0).save(userJson()));
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(at("other.json")));
    ASSERT_TRUE(session().load(at("other.json")));

    ASSERT_TRUE(session().setPortableWanted(true));

    EXPECT_TRUE(fromDisk(userJson()).general.moveAtNextStart);

    ASSERT_TRUE(session().setPortableWanted(false));

    EXPECT_FALSE(fromDisk(userJson()).general.moveAtNextStart);
    EXPECT_FALSE(fromDisk(userJson()).general.noUserConf);
}

TEST_F(SessionTest, TurningPortableModeOnWhileAnotherIsOpenWritesOnlyTheUserConfig) {
    ASSERT_TRUE(tests::Fixtures::config(3, 0).save(userJson()));
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(at("other.json")));
    ASSERT_TRUE(session().load(at("other.json")));

    ASSERT_TRUE(session().setPortableWanted(true));

    const Config user = fromDisk(userJson());

    EXPECT_TRUE(user.general.noUserConf);
    EXPECT_TRUE(user.general.moveAtNextStart);
    EXPECT_EQ(user.profiles.size(), 3U);
    EXPECT_FALSE(session().config().general.noUserConf);
    EXPECT_FALSE(fromDisk(at("other.json")).general.moveAtNextStart);
    EXPECT_TRUE(session().portableWanted());
}

TEST_F(SessionTest, TurningPortableModeOnWithNoUserConfigCreatesOne) {
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(at("other.json")));
    ASSERT_TRUE(session().load(at("other.json")));

    ASSERT_TRUE(session().setPortableWanted(true));

    ASSERT_TRUE(fs::is_regular_file(userJson()));
    EXPECT_TRUE(fromDisk(userJson()).general.noUserConf);
    EXPECT_TRUE(fromDisk(userJson()).general.moveAtNextStart);
    EXPECT_TRUE(fromDisk(userJson()).profiles.empty());
}

TEST_F(SessionTest, TurningPortableModeOffWhenItIsOffWritesNothing) {
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(at("other.json")));
    ASSERT_TRUE(session().load(at("other.json")));

    EXPECT_TRUE(session().setPortableWanted(false));
    EXPECT_FALSE(fs::exists(userJson()));
}

TEST_F(SessionTest, TurningPortableModeOffMigratesALegacyUserIni) {
    same::spill(userIni(), "[zdl.general]\nnouserconf=1\nalwaysadd=-x\n");
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(beside("zdl.json")));
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(at("other.json")));
    ASSERT_TRUE(session().load(at("other.json")));

    ASSERT_TRUE(session().setPortableWanted(false));

    const Config user = fromDisk(userJson());

    EXPECT_FALSE(user.general.noUserConf);
    EXPECT_TRUE(user.general.moveAtNextStart);
    EXPECT_EQ(user.general.alwaysAdd, "-x");
    EXPECT_FALSE(session().portableWanted());
}

TEST_F(SessionTest, AProgramFolderThatCannotBeWrittenRefusesPortableMode) {
    ASSERT_TRUE(oneProfile("doom.cfg").save(userJson()));

    session().start({});

    if (!lockProgramFolder()) {
        GTEST_SKIP() << "the read-only folder still takes writes, as it does for root";
    }

    std::string error;

    EXPECT_FALSE(session().setPortableWanted(true, &error));

    unlockProgramFolder();

    EXPECT_TRUE(error.starts_with("ZDL4 cannot write to ")) << error;
    EXPECT_FALSE(session().portableWanted());
    EXPECT_FALSE(session().config().general.noUserConf);
    EXPECT_FALSE(fromDisk(userJson()).general.moveAtNextStart);
}

TEST_F(SessionTest, TurningPortableModeOnMovesTheSetupBesideTheProgramAtTheNextStart) {
    Config made = oneProfile("doom.cfg");

    made.general.alwaysAdd = "-mine";

    ASSERT_TRUE(made.save(userJson()));
    same::spill(userProfile("doom") / "doom.cfg", "user");
    same::spill(userProfile("doom") / "saves" / "a.zds", "save");

    session().start({});
    ASSERT_TRUE(session().setPortableWanted(true));

    restart();

    EXPECT_EQ(session().source(), Session::Source::Portable);
    EXPECT_EQ(session().path(), beside("zdl.json"));
    EXPECT_EQ(session().config().general.alwaysAdd, "-mine");
    EXPECT_EQ(ConfigPaths::dataDirectory(), ttk::Paths::executable_directory());
    EXPECT_TRUE(session().portableWanted());
    EXPECT_TRUE(session().warning().empty()) << session().warning();

    const Config portable = fromDisk(beside("zdl.json"));
    const Config user = fromDisk(userJson());

    EXPECT_EQ(portable.general.alwaysAdd, "-mine");
    EXPECT_FALSE(portable.general.moveAtNextStart);
    EXPECT_TRUE(user.general.noUserConf);
    EXPECT_FALSE(user.general.moveAtNextStart);
    EXPECT_FALSE(fs::exists(beside("zdl.json.carry")));

    EXPECT_EQ(same::slurp(besideProfile("doom") / "doom.cfg"), "user");
    EXPECT_EQ(same::slurp(besideProfile("doom") / "saves" / "a.zds"), "save");
    EXPECT_EQ(same::slurp(userProfile("doom") / "doom.cfg"), "user");

    restart();

    EXPECT_EQ(session().source(), Session::Source::Portable);
    EXPECT_EQ(session().config().general.alwaysAdd, "-mine");
}

TEST_F(SessionTest, TheMoveBesideTheProgramKeepsTheConfigItReplacesAsTheBackup) {
    ASSERT_TRUE(tests::Fixtures::config(4, 0).save(beside("zdl.json")));
    ASSERT_TRUE(oneProfile("doom.cfg").save(userJson()));

    const std::string replaced = same::slurp(beside("zdl.json"));

    session().start({});
    ASSERT_EQ(session().source(), Session::Source::User);
    ASSERT_TRUE(session().setPortableWanted(true));

    restart();

    EXPECT_EQ(session().config().profiles.size(), 1U);
    EXPECT_EQ(same::slurp(beside("zdl.json.bak")), replaced);

    session().config().addProfile("Added");
    ASSERT_TRUE(session().save());

    EXPECT_EQ(same::slurp(beside("zdl.json.bak")), replaced);
}

TEST_F(SessionTest, AGarbledConfigBesideTheProgramIsSetAsideNotTakenAsTheBackup) {
    same::spill(beside("zdl.json"), "{ not json");
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(beside("zdl.json.bak")));

    const std::string backup = same::slurp(beside("zdl.json.bak"));

    Config user = withNoUserConf(oneProfile("doom.cfg"));

    user.general.moveAtNextStart = true;
    ASSERT_TRUE(user.save(userJson()));

    session().start({});

    EXPECT_EQ(session().source(), Session::Source::Portable);
    EXPECT_EQ(same::slurp(beside("zdl.json.bad")), "{ not json");
    EXPECT_EQ(same::slurp(beside("zdl.json.bak")), backup);
}

TEST_F(SessionTest, TheMoveBesideTheProgramCopiesNewerFilesOverOlderOnes) {
    same::spill(besideProfile("doom") / "doom.cfg", "old");
    age(besideProfile("doom") / "doom.cfg");
    same::spill(userProfile("doom") / "doom.cfg", "new");
    same::spill(userProfile("doom") / "kept.cfg", "stale");
    age(userProfile("doom") / "kept.cfg");
    same::spill(besideProfile("doom") / "kept.cfg", "newer");
    ASSERT_TRUE(oneProfile("doom.cfg").save(userJson()));

    session().start({});
    ASSERT_TRUE(session().setPortableWanted(true));

    restart();

    EXPECT_EQ(same::slurp(besideProfile("doom") / "doom.cfg"), "new");
    EXPECT_EQ(same::slurp(besideProfile("doom") / "kept.cfg"), "newer");
}

TEST_F(SessionTest, AMoveCutShortAfterTheConfigWasWrittenFinishesOnTheNextStart) {
    ASSERT_TRUE(tests::Fixtures::config(4, 0).save(beside("zdl.json")));

    const std::string replaced = same::slurp(beside("zdl.json"));

    same::spill(userProfile("doom") / "doom.cfg", "user");

    Config user = withNoUserConf(oneProfile("doom.cfg"));

    user.general.moveAtNextStart = true;
    ASSERT_TRUE(user.save(userJson()));

    // What the move leaves when it stops after writing beside the program.
    same::spill(beside("zdl.json.bak"), replaced);

    Config written = fromDisk(userJson());

    written.general.moveAtNextStart = false;
    ASSERT_TRUE(written.save(beside("zdl.json")));

    session().start({});

    EXPECT_EQ(session().source(), Session::Source::Portable);
    EXPECT_EQ(session().config().profiles.size(), 1U);
    EXPECT_EQ(same::slurp(beside("zdl.json.bak")), replaced);
    EXPECT_EQ(same::slurp(besideProfile("doom") / "doom.cfg"), "user");
    EXPECT_FALSE(fromDisk(userJson()).general.moveAtNextStart);

    ASSERT_TRUE(session().save());
    EXPECT_EQ(same::slurp(beside("zdl.json.bak")), replaced);
}

TEST_F(SessionTest, AMoveToAProgramFolderThatCannotBeWrittenStaysOnTheUserConfig) {
    Config user = withNoUserConf(oneProfile("doom.cfg"));

    user.general.moveAtNextStart = true;
    ASSERT_TRUE(user.save(userJson()));

    if (!lockProgramFolder()) {
        GTEST_SKIP() << "the read-only folder still takes writes, as it does for root";
    }

    session().start({});

    unlockProgramFolder();

    EXPECT_EQ(session().source(), Session::Source::User);
    EXPECT_EQ(session().path(), userJson());
    EXPECT_EQ(session().config().profiles.size(), 1U);
    EXPECT_EQ(ConfigPaths::dataDirectory(), ttk::Paths::data_directory());
    EXPECT_NE(session().warning().find("portable mode is off"), std::string::npos) << session().warning();
    EXPECT_FALSE(session().portableWanted());
    EXPECT_FALSE(fs::exists(beside("zdl.json")));

    const Config written = fromDisk(userJson());

    EXPECT_FALSE(written.general.noUserConf);
    EXPECT_FALSE(written.general.moveAtNextStart);
    EXPECT_TRUE(fromDisk(fs::path(userJson()) += ".bak").general.moveAtNextStart);
}

TEST_F(SessionTest, AFinalWriteThatFailsStillMovesTheSetupAndAUnchangedSessionFinishesOnTheNextStart) {
    ASSERT_TRUE(tests::Fixtures::config(4, 0).save(beside("zdl.json")));

    const std::string replaced = same::slurp(beside("zdl.json"));
    Config user = withNoUserConf(oneProfile("doom.cfg"));

    user.general.moveAtNextStart = true;
    ASSERT_TRUE(user.save(userJson()));

    if (!lockUserFolder()) {
        GTEST_SKIP() << "the read-only folder still takes writes, as it does for root";
    }

    session().start({});

    EXPECT_EQ(session().source(), Session::Source::Portable);
    EXPECT_EQ(session().path(), beside("zdl.json"));
    EXPECT_EQ(session().config().profiles.size(), 1U);
    EXPECT_NE(session().warning().find("Could not update the user config"), std::string::npos) << session().warning();
    EXPECT_TRUE(session().portableWanted());
    EXPECT_TRUE(fromDisk(userJson()).general.moveAtNextStart);

    unlockUserFolder();
    restart();

    EXPECT_EQ(session().source(), Session::Source::Portable);
    EXPECT_EQ(session().config().profiles.size(), 1U);
    EXPECT_TRUE(session().warning().empty()) << session().warning();
    EXPECT_FALSE(fromDisk(userJson()).general.moveAtNextStart);
    EXPECT_TRUE(fromDisk(userJson()).general.noUserConf);
    EXPECT_FALSE(fs::exists(beside("zdl.json.placed")));
    EXPECT_EQ(same::slurp(beside("zdl.json.bak")), replaced);
}

TEST_F(SessionTest, AFinalWriteThatFailsDoesNotCostTheChangesOfThatSession) {
    ASSERT_TRUE(tests::Fixtures::config(4, 0).save(beside("zdl.json")));

    Config user = withNoUserConf(oneProfile("doom.cfg"));

    user.general.moveAtNextStart = true;
    ASSERT_TRUE(user.save(userJson()));

    if (!lockUserFolder()) {
        GTEST_SKIP() << "the read-only folder still takes writes, as it does for root";
    }

    session().start({});
    session().config().addProfile("Added");
    ASSERT_TRUE(session().save());

    EXPECT_TRUE(fs::exists(beside("zdl.json.placed")));

    restart();

    EXPECT_EQ(session().source(), Session::Source::Portable);
    EXPECT_EQ(session().config().profiles.size(), 2U);
    EXPECT_NE(session().warning().find("Could not update the user config"), std::string::npos) << session().warning();
    EXPECT_TRUE(session().portableWanted());
    EXPECT_TRUE(fromDisk(userJson()).general.moveAtNextStart);
    EXPECT_TRUE(fs::exists(beside("zdl.json.placed")));

    unlockUserFolder();
    restart();

    EXPECT_EQ(session().source(), Session::Source::Portable);
    EXPECT_EQ(session().config().profiles.size(), 2U);
    EXPECT_TRUE(session().warning().empty()) << session().warning();
    EXPECT_FALSE(fromDisk(userJson()).general.moveAtNextStart);
    EXPECT_FALSE(fs::exists(beside("zdl.json.placed")));
    EXPECT_EQ(fromDisk(beside("zdl.json")).profiles.size(), 2U);
}

TEST_F(SessionTest, AFinalWriteThatFailsWithNothingReplacedAlsoKeepsTheLaterChanges) {
    Config user = withNoUserConf(oneProfile("doom.cfg"));

    user.general.moveAtNextStart = true;
    ASSERT_TRUE(user.save(userJson()));

    if (!lockUserFolder()) {
        GTEST_SKIP() << "the read-only folder still takes writes, as it does for root";
    }

    session().start({});
    session().config().general.alwaysAdd = "-later";
    ASSERT_TRUE(session().save());

    unlockUserFolder();
    restart();

    EXPECT_EQ(session().source(), Session::Source::Portable);
    EXPECT_EQ(session().config().general.alwaysAdd, "-later");
    EXPECT_FALSE(fromDisk(userJson()).general.moveAtNextStart);
    EXPECT_FALSE(fs::exists(beside("zdl.json.placed")));
}

TEST_F(SessionTest, AMoveCutShortBeforeTheConfigWasWrittenStartsOverOnTheNextStart) {
    ASSERT_TRUE(tests::Fixtures::config(4, 0).save(beside("zdl.json")));

    Config user = withNoUserConf(oneProfile("doom.cfg"));

    user.general.moveAtNextStart = true;
    ASSERT_TRUE(user.save(userJson()));
    same::spill(beside("zdl.json.carry"), "half");

    session().start({});

    EXPECT_EQ(session().source(), Session::Source::Portable);
    EXPECT_EQ(session().config().profiles.size(), 1U);
    EXPECT_FALSE(fs::exists(beside("zdl.json.carry")));
    EXPECT_FALSE(fromDisk(userJson()).general.moveAtNextStart);
}

TEST_F(SessionTest, ARestoredBackupThatCarriedTheMoveDoesNotMoveAtTheNextStart) {
    Config user = withNoUserConf(oneProfile("doom.cfg"));

    user.general.moveAtNextStart = true;
    same::spill(userJson(), "{ not json");
    ASSERT_TRUE(user.save(fs::path(userJson()) += ".bak"));

    const std::string kept = same::slurp(fs::path(userJson()) += ".bak");

    session().start({});

    EXPECT_EQ(session().config().profiles.size(), 1U);
    EXPECT_FALSE(session().config().general.moveAtNextStart);
    EXPECT_EQ(same::slurp(fs::path(userJson()) += ".bak"), kept);

    ASSERT_TRUE(session().save());
    EXPECT_FALSE(fromDisk(userJson()).general.moveAtNextStart);

    restart();

    EXPECT_EQ(session().config().profiles.size(), 1U);
    EXPECT_FALSE(fs::exists(beside("zdl.json")));
}

TEST_F(SessionTest, TurningPortableModeOffMovesTheSetupBackAtTheNextStart) {
    ASSERT_TRUE(withNoUserConf(tests::Fixtures::config(2, 0)).save(userJson()));

    Config made = oneProfile("doom.cfg");

    made.general.alwaysAdd = "-portable";

    ASSERT_TRUE(made.save(beside("zdl.json")));
    same::spill(besideProfile("doom") / "doom.cfg", "live");
    same::spill(besideProfile("doom") / "saves" / "a.zds", "save");

    session().start({});
    ASSERT_EQ(session().source(), Session::Source::Portable);
    ASSERT_TRUE(session().setPortableWanted(false));

    EXPECT_EQ(session().source(), Session::Source::Portable);
    EXPECT_EQ(ConfigPaths::dataDirectory(), ttk::Paths::executable_directory());
    EXPECT_FALSE(session().portableWanted());
    EXPECT_TRUE(fromDisk(userJson()).general.moveAtNextStart);

    restart();

    EXPECT_EQ(session().source(), Session::Source::User);
    EXPECT_EQ(session().path(), userJson());
    EXPECT_EQ(session().config().general.alwaysAdd, "-portable");
    EXPECT_EQ(ConfigPaths::dataDirectory(), ttk::Paths::data_directory());
    EXPECT_FALSE(session().portableWanted());

    const Config user = fromDisk(userJson());

    EXPECT_EQ(user.general.alwaysAdd, "-portable");
    EXPECT_FALSE(user.general.noUserConf);
    EXPECT_FALSE(user.general.moveAtNextStart);
    EXPECT_EQ(same::slurp(userProfile("doom") / "doom.cfg"), "live");
    EXPECT_EQ(same::slurp(userProfile("doom") / "saves" / "a.zds"), "save");
    EXPECT_TRUE(fs::is_regular_file(beside("zdl.json")));
    EXPECT_TRUE(fs::is_regular_file(besideProfile("doom") / "doom.cfg"));

    const fs::path backup = fs::path(userJson()) += ".bak";
    const std::string replaced = same::slurp(backup);

    EXPECT_EQ(fromDisk(backup).profiles.size(), 2U);

    ASSERT_TRUE(session().save());
    EXPECT_EQ(same::slurp(backup), replaced);
}

TEST_F(SessionTest, TheMoveBackCopiesNewerFilesOverOlderOnes) {
    ASSERT_TRUE(withNoUserConf(tests::Fixtures::config(1, 0)).save(userJson()));
    ASSERT_TRUE(oneProfile("doom.cfg").save(beside("zdl.json")));
    same::spill(userProfile("doom") / "doom.cfg", "stale");
    age(userProfile("doom") / "doom.cfg");
    same::spill(besideProfile("doom") / "doom.cfg", "live");
    same::spill(besideProfile("doom") / "kept.cfg", "old");
    age(besideProfile("doom") / "kept.cfg");
    same::spill(userProfile("doom") / "kept.cfg", "newer");

    session().start({});
    ASSERT_TRUE(session().setPortableWanted(false));

    restart();

    EXPECT_EQ(same::slurp(userProfile("doom") / "doom.cfg"), "live");
    EXPECT_EQ(same::slurp(userProfile("doom") / "kept.cfg"), "newer");
}

TEST_F(SessionTest, APortableSetupWithNoUserConfigTurnedOffWritesOne) {
    Config made = oneProfile("doom.cfg");

    made.general.alwaysAdd = "-portable";

    ASSERT_TRUE(made.save(beside("zdl.json")));

    session().start({});
    ASSERT_TRUE(session().setPortableWanted(false));

    const Config copy = fromDisk(userJson());

    EXPECT_EQ(copy.general.alwaysAdd, "-portable");
    EXPECT_FALSE(copy.general.noUserConf);
    EXPECT_TRUE(copy.general.moveAtNextStart);

    session().config().general.alwaysAdd = "-later";
    ASSERT_TRUE(session().save());

    restart();

    EXPECT_EQ(session().source(), Session::Source::User);
    EXPECT_EQ(session().config().general.alwaysAdd, "-later");
    EXPECT_EQ(fromDisk(fs::path(userJson()) += ".bak").general.alwaysAdd, "-portable");
}

TEST_F(SessionTest, FlippingBackOnBeforeARestartKeepsThePortableSetup) {
    ASSERT_TRUE(withNoUserConf(tests::Fixtures::config(1, 0)).save(userJson()));
    ASSERT_TRUE(tests::Fixtures::config(3, 0).save(beside("zdl.json")));

    session().start({});
    ASSERT_TRUE(session().setPortableWanted(false));
    ASSERT_TRUE(session().setPortableWanted(true));

    EXPECT_FALSE(fromDisk(userJson()).general.moveAtNextStart);
    EXPECT_TRUE(fromDisk(userJson()).general.noUserConf);

    restart();

    EXPECT_EQ(session().source(), Session::Source::Portable);
    EXPECT_EQ(session().config().profiles.size(), 3U);
    EXPECT_EQ(fromDisk(userJson()).profiles.size(), 1U);
}

TEST_F(SessionTest, AMoveBackWithNothingBesideTheProgramOpensTheUserConfig) {
    Config user = tests::Fixtures::config(2, 0);

    user.general.moveAtNextStart = true;
    ASSERT_TRUE(user.save(userJson()));

    session().start({});

    EXPECT_EQ(session().source(), Session::Source::User);
    EXPECT_EQ(session().config().profiles.size(), 2U);
    EXPECT_FALSE(session().config().general.moveAtNextStart);
    EXPECT_TRUE(session().warning().empty());
}

TEST_F(SessionTest, AdoptingClearsThePortableWish) {
    ASSERT_TRUE(withNoUserConf(tests::Fixtures::config(1, 0)).save(userJson()));
    ASSERT_TRUE(tests::Fixtures::config(2, 0).save(beside("zdl.json")));
    ASSERT_TRUE(withNoUserConf(tests::Fixtures::config(3, 0)).save(at("mine.json")));
    ASSERT_TRUE(session().load(at("mine.json")));
    ASSERT_TRUE(session().portableWanted());

    ASSERT_TRUE(session().adoptAsUserConfig());

    EXPECT_FALSE(fromDisk(userJson()).general.noUserConf);
    EXPECT_FALSE(session().portableWanted());

    restart();

    EXPECT_EQ(session().source(), Session::Source::User);
    EXPECT_EQ(session().config().profiles.size(), 3U);
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
