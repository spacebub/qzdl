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

#include <cstddef>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/system/Paths.h"
#include "ttk/system/Text.h"

#include "Same.h"
#include "core/config/Config.h"
#include "support/Corpus.h"
#include "support/Fixtures.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

constexpr int MANY = 200;

Config named(const std::vector<std::string> &names) {
    Config config;

    for (const std::string &name : names) {
        config.addProfile(name);
    }

    return config;
}

std::vector<std::string> idsOf(const Config &config) {
    std::vector<std::string> out;

    for (const Profile &profile : config.profiles) {
        out.push_back(profile.id);
    }

    return out;
}

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        _dir = tests::Sandbox::scratch("config");
        clearFolders();
    }

    void TearDown() override { clearFolders(); }

    [[nodiscard]] fs::path at(const char *name) const { return _dir / name; }

    // A profile's folder outlives the profile, so one left by another case would take a name.
    static void clearFolders() {
        std::error_code code;

        fs::remove_all(Config::profileFolder("x").parent_path(), code);
    }

    static void makeFolder(const std::string &stem) { fs::create_directories(Config::profileFolder(stem)); }

private:
    fs::path _dir;
};

}

TEST_F(ConfigTest, ADefaultConfigHasNoProfiles) {
    const Config config;

    EXPECT_TRUE(config.profiles.empty());
    EXPECT_EQ(config.activeProfileIndex(), -1);
    EXPECT_TRUE(config.activeProfile().id.empty());
    EXPECT_TRUE(config.activeIwadFile().empty());
}

TEST_F(ConfigTest, AddingAProfileNamesItAndGivesItAConfigFile) {
    Config config;

    const std::string id = config.addProfile("Doom");

    ASSERT_EQ(config.profiles.size(), 1U);
    EXPECT_EQ(config.profiles.front().id, id);
    EXPECT_EQ(id.size(), 36U);
    EXPECT_EQ(config.profiles.front().name, "Doom");
    EXPECT_EQ(config.profiles.front().config, "doom.cfg");
}

TEST_F(ConfigTest, AddingAProfileDoesNotMakeItActive) {
    Config config = named({"First"});

    const std::string second = config.addProfile("Second");

    EXPECT_NE(config.activeProfileId, second);
    EXPECT_EQ(config.activeProfile().name, "First");
}

TEST_F(ConfigTest, AProfileAddedWithNoNameIsUnnamed) {
    Config config;

    config.addProfile("   ");

    EXPECT_EQ(config.profiles.front().name, "New profile");
    EXPECT_EQ(config.profiles.front().config, "new-profile.cfg");
}

TEST_F(ConfigTest, ATakenNameIsNumbered) {
    const Config config = named({"Doom", "Doom", "Doom"});

    EXPECT_EQ(config.profiles[1].name, "Doom (2)");
    EXPECT_EQ(config.profiles[2].name, "Doom (3)");
    EXPECT_EQ(config.profiles[1].config, "doom-2.cfg");
    EXPECT_EQ(config.profiles[2].config, "doom-3.cfg");
}

TEST_F(ConfigTest, NamesCompareWithoutCase) {
    EXPECT_EQ(named({"Doom"}).uniqueProfileName("DOOM"), "DOOM (2)");
}

TEST_F(ConfigTest, AUniqueNameIsTrimmed) {
    EXPECT_EQ(Config{}.uniqueProfileName("  Doom \t"), "Doom");
}

TEST_F(ConfigTest, AFreeNameIsKept) {
    EXPECT_EQ(named({"Doom"}).uniqueProfileName("Heretic"), "Heretic");
}

TEST_F(ConfigTest, NumberingSkipsNumbersAlreadyTaken) {
    EXPECT_EQ(named({"Doom", "Doom (2)", "Doom (3)"}).uniqueProfileName("Doom"), "Doom (4)");
}

TEST_F(ConfigTest, ProfileIdsAndNamesStayUnique) {
    Config config;

    for (int at = 0; at < MANY; ++at) {
        config.addProfile(at % 2 == 0 ? "Same" : "same");
    }

    std::set<std::string> ids;
    std::set<std::string> names;
    std::set<std::string> files;

    for (const Profile &profile : config.profiles) {
        ids.insert(profile.id);
        names.insert(ttk::Text::lower(profile.name));
        files.insert(profile.config);
    }

    EXPECT_EQ(ids.size(), static_cast<size_t>(MANY));
    EXPECT_EQ(names.size(), static_cast<size_t>(MANY));
    EXPECT_EQ(files.size(), static_cast<size_t>(MANY));
}

TEST_F(ConfigTest, DuplicatingCopiesTheActiveProfileUnderANewIdNameAndFile) {
    Config config = tests::Fixtures::config(3, 4);

    config.setActiveProfile("profile-1");

    const Profile original = config.activeProfile();
    const std::string id = config.duplicateActiveProfile(original.name);

    ASSERT_EQ(config.profiles.size(), 4U);

    const Profile &copy = config.profiles.back();

    EXPECT_EQ(copy.id, id);
    EXPECT_NE(copy.id, original.id);
    EXPECT_EQ(copy.name, original.name + " (2)");
    EXPECT_NE(copy.config, original.config);

    Profile want = original;

    want.id = copy.id;
    want.name = copy.name;
    want.config = copy.config;

    same::profile(copy, want);
}

TEST_F(ConfigTest, DuplicatingLeavesTheOriginalActive) {
    Config config = tests::Fixtures::config(2, 1);

    config.duplicateActiveProfile("Copy");

    EXPECT_EQ(config.activeProfileId, "profile-0");
}

TEST_F(ConfigTest, DuplicatingWithNoProfilesAddsABlankOne) {
    Config config;

    config.duplicateActiveProfile("Copy");

    ASSERT_EQ(config.profiles.size(), 1U);
    EXPECT_EQ(config.profiles.front().name, "Copy");
    EXPECT_TRUE(config.profiles.front().files.empty());
}

TEST_F(ConfigTest, SettingAnUnknownProfileActiveIsRefused) {
    Config config = tests::Fixtures::config(2, 0);

    EXPECT_FALSE(config.setActiveProfile("nope"));
    EXPECT_FALSE(config.setActiveProfile(""));
    EXPECT_EQ(config.activeProfileId, "profile-0");
    EXPECT_TRUE(config.setActiveProfile("profile-1"));
    EXPECT_EQ(config.activeProfileIndex(), 1);
}

TEST_F(ConfigTest, ADanglingActiveIdReadsAsTheFirstProfile) {
    Config config = tests::Fixtures::config(3, 0);

    config.activeProfileId = "gone";

    EXPECT_EQ(config.activeProfileIndex(), 0);
    EXPECT_EQ(config.activeProfile().id, "profile-0");
}

TEST_F(ConfigTest, IndexOfAnEmptyOrUnknownIdIsMinusOne) {
    const Config config = tests::Fixtures::config(3, 0);

    EXPECT_EQ(config.indexOfProfile(""), -1);
    EXPECT_EQ(config.indexOfProfile("nope"), -1);
    EXPECT_EQ(config.indexOfProfile("profile-2"), 2);
}

TEST_F(ConfigTest, RemovingTheActiveProfileActivatesTheOneThatTookItsPlace) {
    Config config = tests::Fixtures::config(3, 0);

    config.setActiveProfile("profile-1");
    config.removeProfile("profile-1");

    EXPECT_EQ(idsOf(config), (std::vector<std::string>{"profile-0", "profile-2"}));
    EXPECT_EQ(config.activeProfileId, "profile-2");
}

TEST_F(ConfigTest, RemovingTheLastActiveProfileActivatesTheNewLast) {
    Config config = tests::Fixtures::config(3, 0);

    config.setActiveProfile("profile-2");
    config.removeProfile("profile-2");

    EXPECT_EQ(config.activeProfileId, "profile-1");
}

TEST_F(ConfigTest, RemovingTheOnlyProfileLeavesNoneActive) {
    Config config = tests::Fixtures::config(1, 0);

    config.removeProfile("profile-0");

    EXPECT_TRUE(config.profiles.empty());
    EXPECT_TRUE(config.activeProfileId.empty());
    EXPECT_EQ(config.activeProfileIndex(), -1);
}

TEST_F(ConfigTest, RemovingAnotherProfileKeepsTheActiveOne) {
    Config config = tests::Fixtures::config(3, 0);

    config.setActiveProfile("profile-2");
    config.removeProfile("profile-0");

    EXPECT_EQ(config.activeProfileId, "profile-2");
    EXPECT_EQ(config.activeProfileIndex(), 1);
}

TEST_F(ConfigTest, RemovingByTheProfilesOwnIdStillMovesTheActiveOne) {
    Config config = tests::Fixtures::config(3, 0);

    config.setActiveProfile("profile-1");
    config.removeProfile(config.profiles[1].id);

    EXPECT_EQ(idsOf(config), (std::vector<std::string>{"profile-0", "profile-2"}));
    EXPECT_EQ(config.activeProfileId, "profile-2");
}

TEST_F(ConfigTest, RemovingAnUnknownProfileChangesNothing) {
    Config config = tests::Fixtures::config(3, 0);

    config.removeProfile("nope");
    config.removeProfile("");

    EXPECT_EQ(config.profiles.size(), 3U);
    EXPECT_EQ(config.activeProfileId, "profile-0");
}

TEST_F(ConfigTest, TheActiveProfileStaysValidThroughAddsAndRemovals) {
    Config config;
    unsigned state = 7;

    for (int step = 0; step < MANY; ++step) {
        state = (state * 1103515245U) + 12345U;

        const unsigned roll = (state >> 16U) % 4U;

        if (roll == 0 || config.profiles.empty()) {
            config.setActiveProfile(config.addProfile("p"));
        } else if (roll == 1) {
            config.setActiveProfile(config.profiles[(state >> 8U) % config.profiles.size()].id);
        } else {
            const std::string id = config.profiles[(state >> 4U) % config.profiles.size()].id;

            config.removeProfile(id);
        }

        if (config.profiles.empty()) {
            ASSERT_TRUE(config.activeProfileId.empty()) << "step " << step;
        } else {
            ASSERT_GE(config.indexOfProfile(config.activeProfileId), 0) << "step " << step;
        }
    }
}

TEST_F(ConfigTest, EnsureActiveProfileRepairsADanglingId) {
    Config config = tests::Fixtures::config(2, 0);

    config.activeProfileId = "gone";
    config.ensureActiveProfile();

    EXPECT_EQ(config.activeProfileId, "profile-0");
}

TEST_F(ConfigTest, EnsureActiveProfileClearsTheIdWithNoProfiles) {
    Config config;

    config.activeProfileId = "gone";
    config.ensureActiveProfile();

    EXPECT_TRUE(config.activeProfileId.empty());
}

TEST_F(ConfigTest, FindIwadAndFindPortMatchTheExactName) {
    const Config config = tests::Fixtures::config(1, 0);

    ASSERT_NE(config.findIwad("Game 3"), nullptr);
    EXPECT_EQ(config.findIwad("Game 3")->file, "/games/game3.wad");
    EXPECT_EQ(config.findIwad("game 3"), nullptr);
    EXPECT_EQ(config.findIwad(""), nullptr);
    ASSERT_NE(config.findPort("Port 5"), nullptr);
    EXPECT_TRUE(config.findPort("Port 5")->dosbox);
    EXPECT_EQ(config.findPort("Port 99"), nullptr);
}

TEST_F(ConfigTest, TheActiveIwadFileIsTheActiveProfilesGame) {
    Config config = tests::Fixtures::config(3, 0);

    config.setActiveProfile("profile-2");

    EXPECT_EQ(config.activeIwadFile(), "/games/game2.wad");

    config.activeProfile().iwad = "Missing";

    EXPECT_TRUE(config.activeIwadFile().empty());
}

TEST_F(ConfigTest, AConfigFileNameIsLowercaseLettersDigitsAndSingleDashes) {
    EXPECT_EQ(Config{}.uniqueConfigFile("  Doom II: Hell on Earth!! "), "doom-ii-hell-on-earth.cfg");
    EXPECT_EQ(Config{}.uniqueConfigFile("--Plutonia__2--"), "plutonia-2.cfg");
}

TEST_F(ConfigTest, ANameWithNothingUsableIsCalledProfile) {
    EXPECT_EQ(Config{}.uniqueConfigFile(""), "profile.cfg");
    EXPECT_EQ(Config{}.uniqueConfigFile("!!! ???"), "profile.cfg");
    EXPECT_EQ(Config{}.uniqueConfigFile("\xE6\x97\xA5\xE6\x9C\xAC"), "profile.cfg");
}

TEST_F(ConfigTest, NonAsciiLettersBecomeSeparators) {
    EXPECT_EQ(Config{}.uniqueConfigFile("\xC3\x89"
                                        "dition sp\xC3\xA9"
                                        "ciale"),
              "dition-sp-ciale.cfg");
}

TEST_F(ConfigTest, AConfigFileNameIsCappedAt48Characters) {
    const std::string file = Config{}.uniqueConfigFile(std::string(100, 'a'));

    EXPECT_EQ(file, std::string(48, 'a') + ".cfg");
}

TEST_F(ConfigTest, ACappedNameDoesNotEndInADash) {
    const std::string file = Config{}.uniqueConfigFile(std::string(47, 'a') + " b");

    EXPECT_EQ(file, std::string(47, 'a') + ".cfg");
}

TEST_F(ConfigTest, AConfigFileTakenByAnotherProfileIsNumbered) {
    Config config = named({"Doom"});

    config.profiles.front().config = "DOOM.cfg";

    EXPECT_EQ(config.uniqueConfigFile("Doom"), "doom-2.cfg");
}

TEST_F(ConfigTest, AProfileKeepsItsOwnConfigFileWhenExcepted) {
    const Config config = named({"Doom"});

    EXPECT_EQ(config.uniqueConfigFile("Doom", config.profiles.front().id), "doom.cfg");
    EXPECT_EQ(config.uniqueConfigFile("Doom"), "doom-2.cfg");
}

TEST_F(ConfigTest, AFolderLeftOnDiskTakesItsName) {
    makeFolder("doom");
    makeFolder("doom-2");

    EXPECT_EQ(Config{}.uniqueConfigFile("Doom"), "doom-3.cfg");
}

TEST_F(ConfigTest, TheProfileFolderIsUnderTheDataDirectory) {
    EXPECT_EQ(Config::profileFolder("doom"), ttk::Paths::data_directory() / "profiles" / "doom");
    EXPECT_TRUE(Config::profileFolder("doom").string().starts_with(tests::Sandbox::root().string()));
}

TEST_F(ConfigTest, EnsureConfigFilesFillsOnlyTheEmptyOnes) {
    Config config;

    for (const char *id : {"a", "b", "c"}) {
        Profile profile;

        profile.id = id;
        profile.name = "Doom";
        config.profiles.push_back(profile);
    }

    config.profiles[2].config = "mine.cfg";

    config.ensureConfigFiles();

    EXPECT_EQ(config.profiles[0].config, "doom.cfg");
    EXPECT_EQ(config.profiles[1].config, "doom-2.cfg");
    EXPECT_EQ(config.profiles[2].config, "mine.cfg");
}

TEST_F(ConfigTest, ResetEmptiesEverything) {
    Config config = tests::Fixtures::shaped(4, 3, 2);

    config.reset();

    same::config(config, Config{});
}

TEST_F(ConfigTest, ASavedConfigLoadsBackTheSame) {
    const Config made = tests::Fixtures::shaped(20, 16, 12);

    ASSERT_TRUE(made.save(at("round.json")));

    Config back;

    ASSERT_TRUE(back.load(at("round.json")));

    same::config(back, made);
}

TEST_F(ConfigTest, ASmallConfigLoadsBackTheSame) {
    const Config made = tests::Fixtures::config(5, 4);

    ASSERT_TRUE(made.save(at("round.json")));

    Config back;

    ASSERT_TRUE(back.load(at("round.json")));

    same::config(back, made);
}

TEST_F(ConfigTest, ALargeConfigLoadsBackTheSame) {
    const Config made = tests::Fixtures::config(512, 16);

    ASSERT_TRUE(made.save(at("large.json")));

    Config back;

    ASSERT_TRUE(back.load(at("large.json")));

    same::config(back, made);
}

TEST_F(ConfigTest, SavingTwiceWritesTheSameBytes) {
    const Config made = tests::Fixtures::shaped(4, 4, 4);

    ASSERT_TRUE(made.save(at("one.json")));

    Config back;

    ASSERT_TRUE(back.load(at("one.json")));
    ASSERT_TRUE(back.save(at("two.json")));

    EXPECT_EQ(same::slurp(at("one.json")), same::slurp(at("two.json")));
}

TEST_F(ConfigTest, TheCorpusConfigLoads) {
    Config config;

    ASSERT_TRUE(config.load(tests::Corpus::json(8, 16)));

    EXPECT_EQ(config.profiles.size(), 8U);
    EXPECT_EQ(config.profiles.front().files.size(), 16U);
    EXPECT_EQ(config.activeProfileId, "profile-0");
}

TEST_F(ConfigTest, LoadingReplacesWhatWasThere) {
    Config config = tests::Fixtures::shaped(4, 4, 4);

    ASSERT_TRUE(Config{}.save(at("empty.json")));
    ASSERT_TRUE(config.load(at("empty.json")));

    same::config(config, Config{});
}

TEST_F(ConfigTest, LoadingRepairsADanglingActiveProfile) {
    Config made = tests::Fixtures::config(3, 0);

    made.activeProfileId = "gone";

    ASSERT_TRUE(made.save(at("dangling.json")));

    Config back;

    ASSERT_TRUE(back.load(at("dangling.json")));

    EXPECT_EQ(back.activeProfileId, "profile-0");
}

TEST_F(ConfigTest, LoadingGivesAProfileWithoutAConfigFileOne) {
    Config made = tests::Fixtures::config(2, 0);

    made.profiles[0].config.clear();
    made.profiles[0].name = "Ultimate Doom";

    ASSERT_TRUE(made.save(at("noconfig.json")));

    Config back;

    ASSERT_TRUE(back.load(at("noconfig.json")));

    EXPECT_EQ(back.profiles[0].config, "ultimate-doom.cfg");
    EXPECT_EQ(back.profiles[1].config, made.profiles[1].config);
}

TEST_F(ConfigTest, AMissingFileFailsToLoadAndSaysSo) {
    Config config;
    std::string error;

    EXPECT_FALSE(config.load(at("absent.json"), &error));
    EXPECT_EQ(error, "cannot open " + at("absent.json").string());
}

TEST_F(ConfigTest, ADirectoryFailsToLoadAndSaysSo) {
    Config config;
    std::string error;

    EXPECT_FALSE(config.load(at(""), &error));
    EXPECT_TRUE(error.starts_with("cannot read ")) << error;
}

TEST_F(ConfigTest, BrokenJsonFailsToLoadWithTheOffset) {
    same::spill(at("broken.json"), R"({"general": {"alwaysAdd": })");

    Config config;
    std::string error;

    EXPECT_FALSE(config.load(at("broken.json"), &error));
    EXPECT_NE(error.find("(at offset "), std::string::npos) << error;
}

TEST_F(ConfigTest, AnEmptyFileFailsToLoad) {
    same::spill(at("empty.json"), "");

    Config config;
    std::string error;

    EXPECT_FALSE(config.load(at("empty.json"), &error));
    EXPECT_FALSE(error.empty());
}

TEST_F(ConfigTest, ARootThatIsNotAnObjectFailsToLoad) {
    same::spill(at("array.json"), "[1, 2]");

    Config config;
    std::string error;

    EXPECT_FALSE(config.load(at("array.json"), &error));
    EXPECT_EQ(error, "root value is not an object");
}

TEST_F(ConfigTest, AFailedLoadLeavesTheConfigAlone) {
    same::spill(at("array.json"), "[]");
    same::spill(at("broken.json"), "{");

    const Config made = tests::Fixtures::shaped(3, 3, 3);
    Config config = made;

    EXPECT_FALSE(config.load(at("array.json")));
    EXPECT_FALSE(config.load(at("broken.json")));
    EXPECT_FALSE(config.load(at("absent.json")));

    same::config(config, made);
}

TEST_F(ConfigTest, LoadingWithoutAnErrorStringStillFails) {
    Config config;

    EXPECT_FALSE(config.load(at("absent.json")));
}

TEST_F(ConfigTest, SavingCreatesMissingDirectories) {
    ASSERT_TRUE(Config{}.save(at("a") / "b" / "zdl.json"));

    EXPECT_TRUE(fs::is_regular_file(at("a") / "b" / "zdl.json"));
}

TEST_F(ConfigTest, SavingLeavesNoTemporaryBehind) {
    ASSERT_TRUE(Config{}.save(at("zdl.json")));

    EXPECT_FALSE(fs::exists(at("zdl.json.new")));
}

TEST_F(ConfigTest, SavingUnderAFileFailsAndSaysSo) {
    same::spill(at("blocker"), "x");

    std::string error;

    EXPECT_FALSE(Config{}.save(at("blocker") / "zdl.json", &error));
    EXPECT_TRUE(error.starts_with("cannot create directory " + at("blocker").string())) << error;
}

TEST_F(ConfigTest, SavingOverADirectoryFailsAndSaysSo) {
    fs::create_directories(at("taken.json"));

    std::string error;

    EXPECT_FALSE(Config{}.save(at("taken.json"), &error));
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(fs::exists(at("taken.json.new")));
}
