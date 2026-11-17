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
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/system/Env.h"
#include "ttk/system/Paths.h"
#include "ttk/system/Process.h"
#include "ttk/system/Text.h"

#include "Launch.h"
#include "core/launch/Arguments.h"
#include "core/launch/Command.h"
#include "core/launch/Dialect.h"
#include "core/launch/FlatpakRun.h"
#include "core/launch/KnownPorts.h"
#include "core/launch/Launcher.h"
#include "core/launch/Storage.h"
#include "core/ports/Flatpak.h"
#include "support/FlatpakApp.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

using Arguments::Access;
using Arguments::Reached;
using launch::Strings;

constexpr const char *MISSING = "The source port is a Flatpak, and flatpak is not on the PATH.";
constexpr const char *REFUSED = "This Flatpak only shows a notice. Install UZDoom instead.";

std::vector<Reached> paths(const std::vector<std::pair<const char *, Access>> &listed) {
    std::vector<Reached> out;

    for (const auto &[path, access] : listed) {
        out.push_back({.path = path, .access = access});
    }

    return out;
}

Strings granted(const std::vector<Reached> &reached) {
    Strings out;

    for (const Reached &each : reached) {
        out.push_back(FlatpakRun::grant(each));
    }

    return out;
}

bool reaches(const std::vector<Reached> &reached, const fs::path &path, const Access access) {
    return std::ranges::any_of(
            reached, [&path, access](const Reached &each) { return each.path == path && each.access == access; });
}

Strings led(const fs::path &first, const Strings &rest) {
    Strings out = {first.string()};

    out.insert(out.end(), rest.begin(), rest.end());

    return out;
}

std::string ro(const fs::path &path) {
    return "--filesystem=" + path.string() + ":ro";
}

std::string rw(const fs::path &folder) {
    return "--filesystem=" + folder.string();
}

class FlatpakTest : public ::testing::Test {
protected:
    fs::path scratch;
    fs::path record;
    fs::path iwads;
    Config config;

    void SetUp() override {
        fs::remove_all(launch::profiles());
        ttk::Env::set("DOOMWADPATH", "");
        ttk::Env::set("DOOMWADDIR", "");

        scratch = tests::Sandbox::scratch("flatpak");
        record = scratch / "ran";
        iwads = scratch / "iwads";
        config = launch::single(exported("org.zdoom.UZDoom"));
        config.iwads.front().file = launch::wad(iwads / "DOOM2.WAD", "IWAD", {"MAP01"}).string();
    }

    void TearDown() override {
        for (const fs::path &placed : onPath) {
            fs::remove(placed);
        }
    }

    [[nodiscard]] fs::path exported(const std::string &app) const {
        const fs::path file = scratch / "flatpak" / "exports" / "bin" / app;

        launch::write(file, "#!/bin/sh\n");

        return file;
    }

    void runs(const std::string &app) { config.engines.front().file = exported(app).string(); }

    fs::path placeOnPath(const std::string &name) {
        onPath.push_back(launch::recorder(tests::Sandbox::root() / "bin" / name, record));

        return onPath.back();
    }

    bool launched(std::string *error = nullptr) {
        ttk::Process::Id id = 0;

        if (!Launcher::launch(config, &id, nullptr, error)) {
            return false;
        }

        EXPECT_EQ(launch::finish(id), ttk::Process::State::Finished);

        return true;
    }

    [[nodiscard]] Strings arguments() const { return launch::lines(record.string() + ".args"); }

    // DOOMWADDIR, DOOMWADPATH and QZDL_PROBE as the started process got them.
    [[nodiscard]] Strings environment() const { return launch::lines(record.string() + ".env"); }

    [[nodiscard]] fs::path game() const { return iwads / "DOOM2.WAD"; }

    [[nodiscard]] std::string wadFolder() const { return "--env=DOOMWADDIR=" + iwads.string(); }

    // The grants on the line, in order.
    [[nodiscard]] Strings grants() const {
        Strings out;

        for (const std::string &argument : Launcher::program(config).arguments) {
            if (argument.starts_with("--filesystem=")) {
                out.push_back(argument);
            }
        }

        return out;
    }

    Profile &profile() { return config.activeProfile(); }

    void typed(const std::string &command) {
        profile().set(Switch::customCommand, true);
        profile().command = command;
    }

    void ownFolder() {
        config.general.profileConfigs = true;
        profile().config = "alpha.cfg";
    }

    // A profile folder, add-ons in two folders and one nested, a recorded demo and screenshots elsewhere.
    void typical() {
        ownFolder();
        profile().files = {
                {.file = launch::wad(scratch / "mods" / "a.wad", "PWAD", {"MAP02"}).string()},
                {.file = launch::wad(scratch / "mods" / "more" / "b.wad", "PWAD", {"MAP03"}).string()},
                {.file = launch::wad(scratch / "maps" / "c.wad", "PWAD", {"MAP04"}).string()},
        };
        launch::write(scratch / "mods" / "fix.deh", "Patch File for DeHackEd v3.0\n");
        profile().files.push_back({.file = (scratch / "mods" / "fix.deh").string()});
        profile().replay = {.mode = ReplayMode::Record, .file = "run1"};
        profile().skill = 3;
        profile().screenshots = Screenshots::Custom;
        profile().screenshotFolder = (scratch / "shots").string();
    }

    [[nodiscard]] fs::path own() const { return Storage::configFile(config).parent_path(); }

    void installForUser(const std::string &app) {
        const fs::path root = ttk::Paths::home_directory() / ".local" / "share" / "flatpak";

        config.engines.front().file = tests::deployFlatpak(root, app, "beta").exported.string();
    }

    // The paths the arguments reach for a port that is no Flatpak.
    std::vector<Reached> reachedBy(const char *port) {
        config.engines.front().file = (fs::path("/games") / port).string();

        return Arguments::withReached(config).reached;
    }

private:
    std::vector<fs::path> onPath;
};

}

TEST(FlatpakDialect, AnExportOfUzdoomKeepsNetgameSavesApart) {
    EXPECT_TRUE(Dialect::of("/var/lib/flatpak/exports/bin/org.zdoom.UZDoom").has(Dialect::Feature::netgameFolder));
}

TEST(FlatpakDialect, AnExportAnswersOnlyToAPortThatTakesTheWholeWord) {
    using Port::Match;

    for (const Port::Known &known : Port::known()) {
        for (const std::string_view word : known.words) {
            if (word.empty() || known.match == Match::contains) {
                continue;
            }

            const Dialect::Traits traits = Dialect::of("/var/lib/flatpak/exports/bin/net.example." + std::string(word));

            EXPECT_EQ(traits.port == known.port, known.match == Match::whole) << word;
        }
    }
}

TEST(FlatpakGrants, EachPathIsGrantedOnce) {
    EXPECT_EQ(granted(FlatpakRun::grants(paths({{"/wads", Access::reads}, {"/wads", Access::reads}}))),
              (Strings{"--filesystem=/wads:ro"}));
}

TEST(FlatpakGrants, WritingWinsWhenOneFolderIsBoth) {
    EXPECT_EQ(granted(FlatpakRun::grants(paths({{"/p", Access::reads}, {"/p", Access::writes}}))),
              (Strings{"--filesystem=/p"}));
}

TEST(FlatpakGrants, APathInsideAGrantedFolderGoes) {
    const std::vector<Reached> asked = paths({
            {"/p", Access::writes},
            {"/p/saves", Access::writes},
            {"/p/replays/old.lmp", Access::reads},
            {"/mods", Access::reads},
            {"/mods/a.wad", Access::reads},
            {"/wads/DOOM2.WAD", Access::reads},
    });

    EXPECT_EQ(granted(FlatpakRun::grants(asked)),
              (Strings{"--filesystem=/p", "--filesystem=/mods:ro", "--filesystem=/wads/DOOM2.WAD:ro"}));
}

TEST(FlatpakGrants, AReadFileBesideAnotherIsGrantedOnItsOwn) {
    EXPECT_EQ(granted(FlatpakRun::grants(paths({{"/wads/a.wad", Access::reads}, {"/wads/b.wad", Access::reads}}))),
              (Strings{"--filesystem=/wads/a.wad:ro", "--filesystem=/wads/b.wad:ro"}));
}

TEST(FlatpakGrants, AWrittenFolderInsideAReadOneKeepsItsOwnGrant) {
    EXPECT_EQ(granted(FlatpakRun::grants(paths({{"/games", Access::reads}, {"/games/profile", Access::writes}}))),
              (Strings{"--filesystem=/games:ro", "--filesystem=/games/profile"}));
}

TEST(FlatpakGrants, AFolderIsComparedByItsParts) {
    EXPECT_EQ(granted(FlatpakRun::grants(paths({{"/wads", Access::reads}, {"/wads2", Access::reads}}))),
              (Strings{"--filesystem=/wads:ro", "--filesystem=/wads2:ro"}));
    EXPECT_EQ(granted(FlatpakRun::grants(paths({{"/a/b/", Access::reads}, {"/a/./b", Access::reads}}))),
              (Strings{"--filesystem=/a/b:ro"}));
}

TEST(FlatpakGrants, TheRootIsNeverGranted) {
    EXPECT_EQ(granted(FlatpakRun::grants(paths({{"/", Access::reads}, {"/wads", Access::reads}}))),
              (Strings{"--filesystem=/wads:ro"}));
}

TEST(FlatpakGrants, NothingAtOrInsideTheAppHomeIsGranted) {
    const std::vector<Reached> asked = paths({
            {"/home/player/.var/app/org.zdoom.UZDoom", Access::writes},
            {"/home/player/.var/app/org.zdoom.UZDoom/data/games/uzdoom", Access::reads},
            {"/home/player/.var/app/org.zdoom.UZDoomer", Access::reads},
            {"/home/player/wads", Access::reads},
    });

    EXPECT_EQ(
            granted(FlatpakRun::grants(asked, "/home/player/.var/app/org.zdoom.UZDoom")),
            (Strings{"--filesystem=/home/player/.var/app/org.zdoom.UZDoomer:ro", "--filesystem=/home/player/wads:ro"}));
}

TEST(FlatpakGrants, ARelativePathIsNeverGranted) {
    EXPECT_EQ(granted(FlatpakRun::grants(
                      paths({{"wads", Access::reads}, {"./mods", Access::writes}, {"/maps", Access::reads}}))),
              (Strings{"--filesystem=/maps:ro"}));
}

TEST(FlatpakInstallation, EachInstallationIsNamedWithTheBranchAndArch) {
    const fs::path commit = "/var/lib/flatpak/app/org.zdoom.UZDoom/x86_64/stable/0123abcd";
    const auto on = [&commit](const Flatpak::Installation installation) {
        return FlatpakRun::installationArguments(
                {.installation = installation, .folder = commit, .arch = "x86_64", .branch = "stable"});
    };

    EXPECT_EQ(on(Flatpak::Installation::system), (Strings{"--system", "--branch=stable", "--arch=x86_64"}));
    EXPECT_EQ(on(Flatpak::Installation::user), (Strings{"--user", "--branch=stable", "--arch=x86_64"}));
    EXPECT_EQ(on(Flatpak::Installation::other), (Strings{"--branch=stable", "--arch=x86_64"}));
    EXPECT_EQ(FlatpakRun::installationArguments(
                      {.installation = Flatpak::Installation::system, .folder = {}, .arch = {}, .branch = {}}),
              (Strings{"--system"}));
}

TEST(FlatpakGrants, AReadPathIsGrantedReadOnly) {
    EXPECT_EQ(FlatpakRun::grant({.path = "/wads", .access = Access::reads}), "--filesystem=/wads:ro");
    EXPECT_EQ(FlatpakRun::grant({.path = "/p", .access = Access::writes}), "--filesystem=/p");
}

TEST(FlatpakGrants, AColonInThePathIsEscaped) {
    EXPECT_EQ(FlatpakRun::grant({.path = "/a:b", .access = Access::reads}), R"(--filesystem=/a\:b:ro)");
    EXPECT_EQ(FlatpakRun::grant({.path = R"(/a\b)", .access = Access::writes}), R"(--filesystem=/a\\b)");
}

TEST_F(FlatpakTest, ATypicalLaunchGrantsEachFileItReadsAndEachFolderItWrites) {
    placeOnPath("flatpak");
    typical();

    ASSERT_TRUE(launched());

    const Strings args = arguments();
    const Strings expected = {
            "run",
            rw(own()),
            ro(iwads),
            ro(scratch / "mods" / "a.wad"),
            ro(scratch / "mods" / "more" / "b.wad"),
            ro(scratch / "maps" / "c.wad"),
            ro(scratch / "mods" / "fix.deh"),
            rw(scratch / "shots"),
            wadFolder(),
            "--command=uzdoom",
            "org.zdoom.UZDoom",
    };

    ASSERT_GE(args.size(), expected.size());
    EXPECT_EQ(Strings(args.begin(), args.begin() + static_cast<std::ptrdiff_t>(expected.size())), expected);
    EXPECT_EQ(Strings(args.begin() + static_cast<std::ptrdiff_t>(expected.size()), args.end()),
              Arguments::of(config, scratch / "flatpak" / "exports" / "bin"));
}

TEST_F(FlatpakTest, TheWholeLineCarriesOnlyWhatTheProfileNeeds) {
    const fs::path flatpak = placeOnPath("flatpak");

    placeOnPath("wrapper");

    installForUser("org.zdoom.UZDoom");
    ownFolder();

    for (const char *other : {"TNT.WAD", "PLUTONIA.WAD", "HERETIC.WAD"}) {
        config.iwads.push_back(
                {.name = other,
                 .file = launch::wad(scratch / fs::path(other).stem() / other, "IWAD", {"MAP01"}).string()});
    }

    profile().environment = {{.name = "QZDL_PROBE", .value = "sdf"}};
    profile().prefix = "wrapper";

    const Strings expected = {
            "wrapper",
            flatpak.string(),
            "run",
            "--user",
            "--branch=beta",
            "--arch=x86_64",
            rw(own()),
            ro(iwads),
            wadFolder(),
            "--env=QZDL_PROBE=sdf",
            "--command=uzdoom",
            "org.zdoom.UZDoom",
            "-config",
            Storage::configFile(config).string(),
            "-savedir",
            Storage::saveDirectory(config).string(),
            "-iwad",
            game().string(),
    };

    EXPECT_EQ(ttk::Text::parse_arguments(Command::line(config)), expected) << Command::line(config);

    ASSERT_TRUE(launched());

    EXPECT_EQ(arguments(), Strings(expected.begin() + 1, expected.end()));
    EXPECT_EQ(environment(), (Strings{"", "", ""}));
}

TEST_F(FlatpakTest, APortThatIsNoFlatpakKeepsTheVariablesAndTheSearchPath) {
    const fs::path port = launch::recorder(scratch / "port" / "uzdoom", record);

    ownFolder();
    config.iwads.push_back(
            {.name = "Other", .file = launch::wad(scratch / "other" / "TNT.WAD", "IWAD", {"MAP01"}).string()});
    config.engines.front().file = port.string();
    profile().environment = {{.name = "QZDL_PROBE", .value = "sdf"}};

    const Strings line = ttk::Text::parse_arguments(Command::line(config));

    ASSERT_GE(line.size(), 2U);
    EXPECT_EQ(Strings(line.begin(), line.begin() + 2), (Strings{"QZDL_PROBE=sdf", port.string()}));
    EXPECT_FALSE(std::ranges::any_of(line, [](const std::string &word) { return word.starts_with("--"); }));

    ASSERT_TRUE(launched());

    const Strings set = environment();

    ASSERT_EQ(set.size(), 3U);
    EXPECT_EQ(set.at(0), iwads.string());
    EXPECT_TRUE(set.at(1).contains((scratch / "other").string())) << set.at(1);
    EXPECT_EQ(set.at(2), "sdf");
}

TEST_F(FlatpakTest, AnInstalledAppRunsOnTheInstallationBranchAndArchOfItsExport) {
    placeOnPath("flatpak");
    installForUser("org.chocolate_doom.ChocolateDoom");

    ASSERT_TRUE(launched());

    EXPECT_EQ(arguments(),
              (Strings{"run",
                       "--user",
                       "--branch=beta",
                       "--arch=x86_64",
                       ro(iwads),
                       wadFolder(),
                       "--command=chocolate-doom",
                       "org.chocolate_doom.ChocolateDoom",
                       "-iwad",
                       game().string()}));
}

TEST_F(FlatpakTest, TheZandronumAppRunsTheGameAndNotDoomseeker) {
    const fs::path flatpak = placeOnPath("flatpak");

    runs("com.zandronum.Zandronum");

    EXPECT_EQ(ttk::Text::parse_arguments(Command::line(config)),
              led(flatpak,
                  {"run",
                   ro(iwads),
                   wadFolder(),
                   "--command=zandronum",
                   "com.zandronum.Zandronum",
                   "-iwad",
                   game().string()}));
}

TEST_F(FlatpakTest, AnAppOffFlathubRunsItsOwnCommand) {
    const fs::path flatpak = placeOnPath("flatpak");

    runs("io.github.fabiangreffrath.woof");

    EXPECT_EQ(
            ttk::Text::parse_arguments(Command::line(config)),
            led(flatpak, {"run", ro(iwads), wadFolder(), "io.github.fabiangreffrath.woof", "-iwad", game().string()}));
}

TEST_F(FlatpakTest, TheGzdoomAppIsRefusedBeforeAnythingIsMade) {
    std::string error;

    placeOnPath("flatpak");
    runs("org.zdoom.GZDoom");
    typical();

    EXPECT_FALSE(launched(&error));
    EXPECT_EQ(error, REFUSED);
    EXPECT_FALSE(fs::exists(own()));
    EXPECT_FALSE(fs::exists(scratch / "shots"));
    EXPECT_EQ(Command::line(config), "");
    EXPECT_EQ(Command::shown(config).trouble, REFUSED);
}

TEST_F(FlatpakTest, ARemovedExportIsRefusedBeforeItRuns) {
    const std::string gone = "The Flatpak export is gone. Reinstall the app or pick another engine.";
    std::string error;

    placeOnPath("flatpak");
    config.engines.front().file = (scratch / "flatpak" / "exports" / "bin" / "org.example.Gone").string();

    EXPECT_FALSE(launched(&error));
    EXPECT_EQ(error, gone);
    EXPECT_FALSE(fs::exists(record.string() + ".args"));
    EXPECT_EQ(Command::shown(config).trouble, gone);
}

TEST_F(FlatpakTest, ThePortRunsInTheGrantedProfileFolder) {
    placeOnPath("flatpak");
    typical();

    ASSERT_TRUE(launched());

    EXPECT_EQ(fs::path(launch::lines(record.string() + ".cwd").at(0)), fs::canonical(own()));
}

TEST_F(FlatpakTest, ThePreviewIsTheLineThatRuns) {
    const fs::path flatpak = placeOnPath("flatpak");
    const fs::path odd = scratch / "it's a:dir";

    placeOnPath("wrapper");
    typical();
    profile().files.push_back({.file = launch::wad(odd / "d e.wad", "PWAD", {"MAP05"}).string()});
    profile().prefix = "wrapper";
    profile().environment = {{.name = "A", .value = "'x y'"}};

    ASSERT_TRUE(launched());

    Strings ran = arguments();

    ASSERT_FALSE(ran.empty());
    EXPECT_EQ(ran.front(), flatpak.string());
    EXPECT_TRUE(
            std::ranges::contains(ran, R"(--filesystem=)" + (scratch / "it's a\\:dir" / "d e.wad").string() + ":ro"));
    EXPECT_TRUE(std::ranges::contains(ran, "--env=A=x y"));

    ran.insert(ran.begin(), "wrapper");

    EXPECT_EQ(ttk::Text::parse_arguments(Command::line(config)), ran) << Command::line(config);
}

TEST_F(FlatpakTest, AProfileWithoutAFolderGrantsOnlyWhatTheLineNames) {
    placeOnPath("flatpak");

    ASSERT_TRUE(launched());

    EXPECT_EQ(
            arguments(),
            (Strings{"run", ro(iwads), wadFolder(), "--command=uzdoom", "org.zdoom.UZDoom", "-iwad", game().string()}));
}

TEST_F(FlatpakTest, AFlatpakGetsNoSearchPathAndNoOtherGameFolder) {
    placeOnPath("flatpak");
    ownFolder();
    config.iwads.push_back(
            {.name = "Other", .file = launch::wad(scratch / "other" / "TNT.WAD", "IWAD", {"MAP01"}).string()});

    EXPECT_EQ(grants(), (Strings{rw(own()), ro(iwads)}));

    ASSERT_TRUE(launched());

    Strings set;

    std::ranges::copy_if(
            arguments(), std::back_inserter(set), [](const std::string &each) { return each.starts_with("--env="); });

    EXPECT_EQ(set, (Strings{wadFolder()}));
    EXPECT_EQ(environment(), (Strings{"", "", ""}));
}

TEST_F(FlatpakTest, AFileInTheAppHomeIsNotGranted) {
    const fs::path inside = ttk::Paths::home_directory() / ".var" / "app" / "org.zdoom.UZDoom" / "data";

    placeOnPath("flatpak");
    profile().files = {
            {.file = launch::wad(inside / "games" / "uzdoom" / "a.wad", "PWAD", {"MAP02"}).string()},
            {.file = launch::wad(scratch / "mods" / "b.wad", "PWAD", {"MAP03"}).string()},
    };

    EXPECT_EQ(grants(), (Strings{ro(iwads), ro(scratch / "mods" / "b.wad")}));
}

TEST_F(FlatpakTest, AnAddOnFolderIsGrantedReadOnly) {
    placeOnPath("flatpak");
    launch::write(scratch / "mods" / "pack" / "MAPINFO", "map MAP01 \"One\" {}\n");
    profile().files = {{.file = (scratch / "mods" / "pack").string()}};

    EXPECT_EQ(grants(), (Strings{ro(iwads), ro(scratch / "mods" / "pack")}));
}

TEST_F(FlatpakTest, AnAddOnInsideTheProfileFolderNeedsNoGrant) {
    placeOnPath("flatpak");
    ownFolder();
    profile().files = {{.file = launch::wad(own() / "a.wad", "PWAD", {"MAP02"}).string()}};

    EXPECT_EQ(grants(), (Strings{rw(own()), ro(iwads)}));
}

// flatpak binds the target of a granted link and lays the link over it.
TEST_F(FlatpakTest, ALinkedFileIsGrantedAsTheLink) {
    const fs::path target = launch::wad(scratch / "real" / "a.wad", "PWAD", {"MAP02"});

    placeOnPath("flatpak");
    fs::create_directories(scratch / "links");
    fs::create_symlink(target, scratch / "links" / "a.wad");
    profile().files = {{.file = (scratch / "links" / "a.wad").string()}};

    EXPECT_EQ(grants(), (Strings{ro(iwads), ro(scratch / "links" / "a.wad")}));
}

TEST_F(FlatpakTest, TheProfileVariablesGoOnlyIntoTheSandbox) {
    placeOnPath("flatpak");
    profile().environment = {{.name = "QZDL_PROBE", .value = "'one two'"}, {.name = "", .value = "lost"}};

    ASSERT_TRUE(launched());

    const Strings args = arguments();

    ASSERT_GE(args.size(), 6U);
    EXPECT_EQ(Strings(args.begin(), args.begin() + 6),
              (Strings{"run",
                       ro(iwads),
                       wadFolder(),
                       "--env=QZDL_PROBE=one two",
                       "--command=uzdoom",
                       "org.zdoom.UZDoom"}));
    EXPECT_EQ(environment().at(2), "");
    EXPECT_FALSE(Command::line(config).contains("QZDL_PROBE='one two' ")) << Command::line(config);
}

TEST_F(FlatpakTest, AProfileDoomwaddirGoesOnlyIntoTheSandbox) {
    placeOnPath("flatpak");
    profile().environment = {{.name = "DOOMWADDIR", .value = "/elsewhere"}};

    ASSERT_TRUE(launched());

    const Strings args = arguments();

    EXPECT_EQ(
            std::ranges::count_if(args, [](const std::string &each) { return each.starts_with("--env=DOOMWADDIR="); }),
            1);
    EXPECT_TRUE(std::ranges::contains(args, "--env=DOOMWADDIR=/elsewhere"));
    EXPECT_EQ(environment().at(0), "");
}

TEST_F(FlatpakTest, APrefixStaysInFrontOfFlatpak) {
    const fs::path flatpak = placeOnPath("flatpak");

    placeOnPath("wrapper");
    profile().prefix = "wrapper";

    ASSERT_TRUE(launched());

    EXPECT_EQ(arguments().at(0), flatpak.string());
    EXPECT_EQ(arguments().at(1), "run");
    EXPECT_TRUE(Command::line(config).starts_with("wrapper " + flatpak.string() + " run ")) << Command::line(config);
}

TEST_F(FlatpakTest, WithoutFlatpakTheLaunchIsRefusedWithWhy) {
    std::string error;

    EXPECT_FALSE(launched(&error));
    EXPECT_EQ(error, MISSING);
    EXPECT_EQ(Command::line(config), "");
    EXPECT_EQ(Command::shown(config).trouble, MISSING);
}

TEST_F(FlatpakTest, APortThatIsNoFlatpakRunsAsItIs) {
    const fs::path port = launch::recorder(scratch / "port" / "uzdoom", record);

    config.engines.front().file = port.string();

    EXPECT_EQ(Command::shown(config).trouble, "");

    ASSERT_TRUE(launched());

    EXPECT_EQ(arguments(), (Strings{"-iwad", game().string()}));
    EXPECT_EQ(Command::line(config), port.string() + " -iwad " + game().string());
}

TEST_F(FlatpakTest, ACommandStartedFromAFlatpakRunsWhatTheLaunchRuns) {
    const fs::path flatpak = placeOnPath("flatpak");

    runs("com.zandronum.Zandronum");
    typical();
    profile().environment = {{.name = "QZDL_PROBE", .value = "sdf"}};

    const std::string pattern = Command::pattern(config);

    EXPECT_TRUE(pattern.starts_with("{source_port} -")) << pattern;
    EXPECT_TRUE(pattern.contains(" {addon_1} ")) << pattern;
    EXPECT_FALSE(pattern.contains("--filesystem")) << pattern;
    EXPECT_FALSE(pattern.contains("--command")) << pattern;

    const Strings generated = Launcher::program(config).arguments;
    const std::string line = Command::line(config);

    typed(pattern);

    EXPECT_EQ(Command::custom(config), led(flatpak, generated));
    EXPECT_EQ(Command::line(config), line);
    ASSERT_TRUE(launched());
    EXPECT_EQ(arguments(), generated);
    EXPECT_EQ(environment().at(2), "");
}

TEST_F(FlatpakTest, ACommandStartedFromTheGzdoomAppIsRefusedAsTheLaunchIs) {
    std::string error;

    placeOnPath("flatpak");
    runs("org.zdoom.GZDoom");

    EXPECT_EQ(Command::shown(config).trouble, REFUSED);
    EXPECT_EQ(Command::pattern(config), "{source_port} -iwad {game}");

    typed(Command::pattern(config));

    EXPECT_EQ(Command::shown(config).trouble, REFUSED);
    EXPECT_EQ(Command::line(config), "");
    EXPECT_FALSE(launched(&error));
    EXPECT_EQ(error, REFUSED);
    EXPECT_FALSE(fs::exists(record.string() + ".args"));
}

TEST_F(FlatpakTest, ASavedCommandRunsTheFlatpakWithTheGrantsOfTheLaunch) {
    const fs::path flatpak = placeOnPath("flatpak");
    const fs::path addon = launch::wad(scratch / "mods" / "a.wad", "PWAD", {"MAP02"});

    ownFolder();
    profile().files = {{.file = addon.string()}};
    typed("{source_port} -iwad {game} -file {addon_1}");

    const Strings expected = {"run",
                              rw(own()),
                              ro(iwads),
                              ro(addon),
                              wadFolder(),
                              "--command=uzdoom",
                              "org.zdoom.UZDoom",
                              "-iwad",
                              game().string(),
                              "-file",
                              addon.string()};

    EXPECT_EQ(Command::shown(config).trouble, "");
    EXPECT_EQ(ttk::Text::parse_arguments(Command::line(config)), led(flatpak, expected)) << Command::line(config);

    ASSERT_TRUE(launched());

    EXPECT_EQ(arguments(), expected);
    EXPECT_EQ(fs::path(launch::lines(record.string() + ".cwd").at(0)), fs::canonical(own()));
}

TEST_F(FlatpakTest, ASavedCommandSetsNothingOnTheProcessAndRunsWhereTheLaunchDoes) {
    placeOnPath("flatpak");
    ownFolder();
    profile().environment = {{.name = "QZDL_PROBE", .value = "sdf"}};

    ASSERT_TRUE(launched());

    const Strings ranIn = launch::lines(record.string() + ".cwd");

    EXPECT_EQ(environment(), (Strings{"", "", ""}));

    typed("{source_port} -iwad {game}");

    ASSERT_TRUE(launched());

    EXPECT_EQ(environment(), (Strings{"", "", ""}));
    EXPECT_EQ(launch::lines(record.string() + ".cwd"), ranIn);
    EXPECT_FALSE(Command::line(config).starts_with("QZDL_PROBE=")) << Command::line(config);
}

TEST_F(FlatpakTest, ASourcePortAfterAPrefixWordRunsTheFlatpak) {
    const fs::path flatpak = placeOnPath("flatpak");

    placeOnPath("gamemoderun");
    typed("gamemoderun {source_port} -iwad {game}");

    const Strings expected = led(
            flatpak, {"run", ro(iwads), wadFolder(), "--command=uzdoom", "org.zdoom.UZDoom", "-iwad", game().string()});

    EXPECT_EQ(Command::custom(config), led("gamemoderun", expected));

    ASSERT_TRUE(launched());

    EXPECT_EQ(arguments(), expected);
}

TEST_F(FlatpakTest, ASourcePortInsideALongerWordStaysTheExportAndKeepsTheVariables) {
    placeOnPath("flatpak");
    placeOnPath("wrapper");
    profile().environment = {{.name = "QZDL_PROBE", .value = "sdf"}};
    typed("wrapper --port={source_port} -iwad {game}");

    EXPECT_EQ(Command::custom(config),
              (Strings{"wrapper", "--port=" + config.engines.front().file, "-iwad", game().string()}));
    EXPECT_TRUE(Command::line(config).starts_with("QZDL_PROBE=sdf wrapper ")) << Command::line(config);

    ASSERT_TRUE(launched());

    EXPECT_EQ(environment().at(2), "sdf");
}

TEST_F(FlatpakTest, ASavedCommandWithoutFlatpakIsRefusedWithWhy) {
    std::string error;

    typed("{source_port} -iwad {game}");

    EXPECT_EQ(Command::shown(config).trouble, MISSING);
    EXPECT_EQ(Command::line(config), "");
    EXPECT_FALSE(launched(&error));
    EXPECT_EQ(error, MISSING);
}

TEST_F(FlatpakTest, ASavedCommandForARemovedExportIsRefusedBeforeItRuns) {
    const std::string gone = "The Flatpak export is gone. Reinstall the app or pick another engine.";
    std::string error;

    placeOnPath("flatpak");
    config.engines.front().file = (scratch / "flatpak" / "exports" / "bin" / "org.example.Gone").string();
    typed("{source_port} -iwad {game}");

    EXPECT_EQ(Command::shown(config).trouble, gone);
    EXPECT_FALSE(launched(&error));
    EXPECT_EQ(error, gone);
    EXPECT_FALSE(fs::exists(record.string() + ".args"));
}

TEST_F(FlatpakTest, ASavedCommandForAPortThatIsNoFlatpakRunsAsTyped) {
    const fs::path port = launch::recorder(scratch / "port" / "uzdoom", record);

    placeOnPath("flatpak");
    config.engines.front().file = port.string();
    typed("{source_port} -iwad {game}");

    EXPECT_EQ(Command::custom(config), (Strings{port.string(), "-iwad", game().string()}));
    EXPECT_EQ(Command::line(config), port.string() + " -iwad " + game().string());

    ASSERT_TRUE(launched());

    EXPECT_EQ(arguments(), (Strings{"-iwad", game().string()}));
}

TEST_F(FlatpakTest, APlayedDemoIsReadAndARecordedOneWritten) {
    ownFolder();
    profile().replay = {.mode = ReplayMode::Record, .file = "run1"};
    profile().skill = 3;

    EXPECT_TRUE(reaches(Arguments::withReached(config).reached, Storage::replayDirectory(config), Access::writes));

    profile().replay.mode = ReplayMode::Play;
    launch::write(Storage::replayFile(config), "demo");

    const std::vector<Reached> reached = Arguments::withReached(config).reached;

    EXPECT_TRUE(reaches(reached, Storage::replayFile(config), Access::reads));
    EXPECT_FALSE(reaches(reached, Storage::replayDirectory(config), Access::writes));
}

TEST_F(FlatpakTest, AContinuedDemoReadsTheSourceAndWritesBesideTheNewOne) {
    ownFolder();
    profile().replay = {.mode = ReplayMode::Continue, .file = "run2", .source = "/elsewhere/run1.lmp"};

    const std::vector<Reached> reached = reachedBy("prboom-plus");

    EXPECT_TRUE(reaches(reached, "/elsewhere/run1.lmp", Access::reads));
    EXPECT_TRUE(reaches(reached, Storage::replayDirectory(config), Access::writes));
}

TEST_F(FlatpakTest, ASaveLoadedByPathIsRead) {
    ownFolder();
    config.engines.front().file = "/games/zdoom";
    profile().save = {.enabled = true, .file = "save1.zds"};
    launch::write(Storage::saveDirectory(config) / "save1.zds", "save");

    EXPECT_TRUE(reaches(
            Arguments::withReached(config).reached, Storage::saveDirectory(config) / "save1.zds", Access::reads));
}

TEST_F(FlatpakTest, TheNetgameSaveAHostLoadsIsRead) {
    profile().multiplayer.gameType = GameType::Coop;
    profile().multiplayer.players = 2;
    profile().multiplayer.savegame = (scratch / "net" / "net.zds").string();

    EXPECT_TRUE(reaches(reachedBy("zdoom"), scratch / "net" / "net.zds", Access::reads));
}

TEST_F(FlatpakTest, AnAutoexecAndADemoAmongTheAddOnsAreRead) {
    launch::write(scratch / "cfgs" / "auto.cfg", "bind x y\n");
    launch::write(scratch / "demos" / "old.lmp", "demo");
    profile().files = {{.file = (scratch / "cfgs" / "auto.cfg").string()},
                       {.file = (scratch / "demos" / "old.lmp").string()}};

    const std::vector<Reached> reached = reachedBy("gzdoom");

    EXPECT_TRUE(reaches(reached, scratch / "cfgs" / "auto.cfg", Access::reads));
    EXPECT_TRUE(reaches(reached, scratch / "demos" / "old.lmp", Access::reads));
}

TEST_F(FlatpakTest, ACommandGrantsAnUntickedAddOnItNames) {
    const fs::path flatpak = placeOnPath("flatpak");
    const fs::path addon = launch::wad(scratch / "mods" / "a.wad", "PWAD", {"MAP02"});

    profile().files = {{.file = addon.string(), .enabled = false}};

    EXPECT_EQ(grants(), (Strings{ro(iwads)}));

    typed("{source_port} -iwad {game} -file {addon_1}");

    EXPECT_EQ(Command::custom(config),
              led(flatpak,
                  {"run",
                   ro(iwads),
                   ro(addon),
                   wadFolder(),
                   "--command=uzdoom",
                   "org.zdoom.UZDoom",
                   "-iwad",
                   game().string(),
                   "-file",
                   addon.string()}));
}

TEST_F(FlatpakTest, ACommandGrantsAPatchThePortTakesNoSwitchFor) {
    const fs::path flatpak = placeOnPath("flatpak");
    const fs::path patch = scratch / "mods" / "fix.deh";

    launch::write(patch, "Patch File for DeHackEd v3.0\n");
    runs("net.example.Doom");
    config.engines.front().port = Port::Port::doom;
    profile().files = {{.file = patch.string()}};

    ASSERT_FALSE(Dialect::of(config).has(Dialect::Feature::deh));
    EXPECT_FALSE(std::ranges::contains(grants(), ro(patch)));

    typed("{source_port} -deh {addon_1}");

    EXPECT_EQ(Command::custom(config),
              led(flatpak, {"run", ro(patch), ro(iwads), wadFolder(), "net.example.Doom", "-deh", patch.string()}));
}

TEST_F(FlatpakTest, ADosPortNeverRunsThroughFlatpak) {
    placeOnPath("flatpak");
    config.engines.front().dosbox = true;
    profile().environment = {{.name = "QZDL_PROBE", .value = "sdf"}};

    EXPECT_FALSE(Launcher::sandboxed(config));

    typed("{source_port} -iwad {game}");

    EXPECT_FALSE(Launcher::sandboxed(config));
    EXPECT_EQ(Command::custom(config), (Strings{config.engines.front().file, "-iwad", game().string()}));
    EXPECT_TRUE(Command::line(config).starts_with("QZDL_PROBE=sdf ")) << Command::line(config);
}
