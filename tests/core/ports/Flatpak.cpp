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
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/system/Paths.h"

#include "core/config/Config.h"
#include "core/launch/Dialect.h"
#include "core/ports/Detect.h"
#include "core/ports/Flatpak.h"
#include "support/FlatpakApp.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

using Strings = std::vector<std::string>;

TEST(FlatpakExport, AnExportLivesInFlatpakExportsBin) {
    EXPECT_TRUE(Flatpak::exported("/var/lib/flatpak/exports/bin/org.zdoom.UZDoom"));
    EXPECT_TRUE(Flatpak::exported("/home/player/.local/share/flatpak/exports/bin/org.zdoom.UZDoom"));
    EXPECT_FALSE(Flatpak::exported("/var/lib/flatpak/app/org.zdoom.UZDoom"));
    EXPECT_FALSE(Flatpak::exported("/usr/bin/uzdoom"));
}

TEST(FlatpakExport, OnlyAWholeAppIdFromFlathubRunsAKnownExecutable) {
    EXPECT_EQ(Flatpak::executable("io.github.fabiangreffrath.Doom"), "crispy-doom");
    EXPECT_EQ(Flatpak::executable("org.zdoom.UZDoom"), "uzdoom");
    EXPECT_EQ(Flatpak::executable("org.chocolate_doom.ChocolateDoom"), "chocolate-doom");
    EXPECT_EQ(Flatpak::executable("com.zandronum.Zandronum"), "zandronum");
    EXPECT_EQ(Flatpak::executable("io.github.fabiangreffrath.Doom.Heretic"), "");
    EXPECT_EQ(Flatpak::executable("Doom"), "");
}

TEST(FlatpakExport, TheGzdoomAppStartsNoGame) {
    EXPECT_EQ(Flatpak::executable("org.zdoom.GZDoom"), "");
    EXPECT_EQ(Flatpak::refusal("org.zdoom.GZDoom"), "This Flatpak only shows a notice. Install UZDoom instead.");
    EXPECT_EQ(Flatpak::refusal("org.zdoom.UZDoom"), "");
    EXPECT_EQ(Flatpak::refusal("org.example.Doom"), "");
}

TEST(FlatpakExport, DetectionNeverOffersAnAppThatStartsNoGame) {
    const fs::path exports = ttk::Paths::home_directory() / ".local" / "share" / "flatpak" / "exports" / "bin";

    fs::create_directories(exports);
    std::ofstream(exports / "org.zdoom.GZDoom") << "#!/bin/sh\n";
    std::ofstream(exports / "org.zdoom.UZDoom") << "#!/bin/sh\n";

    const std::span<const Detect::Found> found = Detect::ports();

    EXPECT_FALSE(std::ranges::any_of(
            found, [&exports](const Detect::Found &each) { return each.file == exports / "org.zdoom.GZDoom"; }));
    EXPECT_TRUE(std::ranges::any_of(
            found, [&exports](const Detect::Found &each) { return each.file == exports / "org.zdoom.UZDoom"; }));
}

TEST(FlatpakExport, AnAppThatStartsNoGameIsNotOffered) {
    const fs::path exports = ttk::Paths::home_directory() / ".local" / "share" / "flatpak" / "exports" / "bin";

    const Strings placed = {"org.zdoom.GZDoom", "org.zdoom.UZDoom", "com.zandronum.Zandronum", "noid"};

    for (const std::string &app : placed) {
        fs::create_directories(exports);
        std::ofstream(exports / app) << "#!/bin/sh\n";
    }

    Strings offered;

    for (const Flatpak::Export &app : Flatpak::exports()) {
        if (app.file.parent_path() == exports && std::ranges::contains(placed, app.file.filename().string())) {
            offered.push_back(app.file.filename().string() + " " + app.name);
        }
    }

    std::ranges::sort(offered);

    EXPECT_EQ(offered, (Strings{"com.zandronum.Zandronum zandronum", "org.zdoom.UZDoom uzdoom"}));
}

TEST(FlatpakExport, AnExportKeepsItsHomeInVarApp) {
    EXPECT_EQ(Flatpak::home("/var/lib/flatpak/exports/bin/org.zdoom.UZDoom"),
              ttk::Paths::home_directory() / ".var" / "app" / "org.zdoom.UZDoom");
    EXPECT_EQ(Flatpak::home("/usr/bin/uzdoom"), fs::path());
}

constexpr const char *METAINFO = R"(<?xml version="1.0" encoding="UTF-8"?>
<component type="desktop-application">
  <id>org.zdoom.UZDoom</id>
  <releases>
    <!-- <release version="9.9.9"/> is planned -->
    <release date="2026-09-23" version="5.0.3">
      <url>https://example.org/5.0.3</url>
    </release>
    <release version="5.0.2" date="2026-09-22"/>
  </releases>
</component>
)";

void put(const fs::path &file, const std::string &text) {
    fs::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary) << text;
}

fs::path share(const fs::path &root, const std::string &app, const std::string &commit = "0123abcd") {
    return root / "app" / app / "x86_64" / "stable" / commit / "files" / "share";
}

// An update as flatpak makes it: a new commit folder, and active moved onto it.
void update(const fs::path &root, const std::string &app, const std::string &commit) {
    const fs::path stable = root / "app" / app / "x86_64" / "stable";

    put(stable / commit / "export" / "bin" / app, "#!/bin/sh\n");
    fs::remove(stable / "active");
    fs::create_directory_symlink(commit, stable / "active");
}

std::string release(const std::string &version) {
    return R"(<releases><release version=")" + version + R"("/></releases>)";
}

TEST(FlatpakDeploy, AnExportGivesTheBranchAndArchItLinksTo) {
    const fs::path root = tests::Sandbox::scratch("flatpak-deploy") / "flatpak";
    const Flatpak::Deploy deployed = Flatpak::deploy(tests::deployFlatpak(root, "org.zdoom.UZDoom").exported);

    EXPECT_EQ(deployed.installation, Flatpak::Installation::other);
    EXPECT_EQ(deployed.folder, fs::canonical(root) / "app" / "org.zdoom.UZDoom" / "x86_64" / "stable" / "0123abcd");
    EXPECT_EQ(deployed.arch, "x86_64");
    EXPECT_EQ(deployed.branch, "stable");
}

TEST(FlatpakDeploy, TheExportFolderNamesTheInstallation) {
    const fs::path user = ttk::Paths::home_directory() / ".local" / "share" / "flatpak";
    const fs::path exported = tests::deployFlatpak(user, "org.chocolate_doom.ChocolateDoom").exported;

    EXPECT_EQ(Flatpak::deploy(exported).installation, Flatpak::Installation::user);
    EXPECT_EQ(Flatpak::deploy(exported).branch, "stable");
    EXPECT_EQ(Flatpak::deploy("/var/lib/flatpak/exports/bin/org.example.Missing").installation,
              Flatpak::Installation::system);
}

TEST(FlatpakDeploy, AnExportLinkedNowhereGivesNoBranch) {
    const fs::path root = tests::Sandbox::scratch("flatpak-deploy-loose") / "flatpak";

    put(root / "exports" / "bin" / "org.zdoom.UZDoom", "#!/bin/sh\n");

    const Flatpak::Deploy deployed = Flatpak::deploy(root / "exports" / "bin" / "org.zdoom.UZDoom");

    EXPECT_EQ(deployed.folder, fs::path());
    EXPECT_EQ(deployed.arch, "");
    EXPECT_EQ(deployed.branch, "");
    EXPECT_EQ(Flatpak::deploy("/var/lib/flatpak/exports/bin/org.example.Missing").folder, fs::path());
}

TEST(FlatpakVersion, TheFirstReleaseIsTheNewest) {
    EXPECT_EQ(Flatpak::firstRelease(METAINFO), "5.0.3");
}

TEST(FlatpakVersion, AVersionIsTrimmed) {
    EXPECT_EQ(Flatpak::firstRelease(R"(<releases><release version="3.0.0 " date="2017-12-30"/></releases>)"), "3.0.0");
    EXPECT_EQ(Flatpak::firstRelease("<release\n  version = '2.1'>"), "2.1");
}

TEST(FlatpakVersion, TextWithoutAReleaseHasNoVersion) {
    EXPECT_EQ(Flatpak::firstRelease("<releases></releases>"), "");
    EXPECT_EQ(Flatpak::firstRelease(R"(<release date="2020-01-01"/><release version="1.0"/>)"), "");
    EXPECT_EQ(Flatpak::firstRelease(R"(<release subversion="1.0"/>)"), "");
    EXPECT_EQ(Flatpak::firstRelease("<!-- <release version=\"1.0\"/>"), "");
}

TEST(FlatpakVersion, AFlatpakExportReadsTheMetainfoOfItsApp) {
    const fs::path root = tests::Sandbox::scratch("file-version-flatpak") / "flatpak";
    const fs::path exported = tests::deployFlatpak(root, "org.zdoom.UZDoom").exported;

    put(share(root, "org.zdoom.UZDoom") / "metainfo" / "org.zdoom.UZDoom.metainfo.xml", METAINFO);

    EXPECT_EQ(Flatpak::version(exported), "5.0.3");
}

TEST(FlatpakVersion, AnAppdataFileStandsInForMetainfo) {
    const fs::path root = tests::Sandbox::scratch("file-version-appdata") / "flatpak";
    const fs::path exported = tests::deployFlatpak(root, "org.chocolate_doom.ChocolateDoom").exported;

    put(share(root, "org.chocolate_doom.ChocolateDoom") / "appdata" / "org.chocolate_doom.ChocolateDoom.appdata.xml",
        R"(<releases><release version="3.0.1" date="2020-06-25"/> <release version="3.0.0 "/></releases>)");

    EXPECT_EQ(Flatpak::version(exported), "3.0.1");
}

TEST(FlatpakVersion, AnAppWithoutMetainfoHasNoVersion) {
    const fs::path root = tests::Sandbox::scratch("file-version-bare") / "flatpak";

    EXPECT_EQ(Flatpak::version(tests::deployFlatpak(root, "org.zdoom.UZDoom").exported), "");
    EXPECT_EQ(Flatpak::version(root / "exports" / "bin" / "org.zdoom.GZDoom"), "");
}

TEST(FlatpakVersion, AnExportNotLinkedIntoADeployHasNoVersion) {
    const fs::path root = tests::Sandbox::scratch("file-version-loose") / "flatpak";

    put(root / "exports" / "bin" / "org.zdoom.UZDoom", "#!/bin/sh\n");
    put(root / "exports" / "files" / "share" / "metainfo" / "org.zdoom.UZDoom.metainfo.xml", METAINFO);

    EXPECT_EQ(Flatpak::version(root / "exports" / "bin" / "org.zdoom.UZDoom"), "");
}

TEST(FlatpakVersion, AFlatpakWithoutAPickedVersionRunsAsTheReleaseItInstalled) {
    const fs::path root = tests::Sandbox::scratch("flatpak-live") / "flatpak";
    const fs::path exported = tests::deployFlatpak(root, "org.zdoom.UZDoom").exported;

    put(share(root, "org.zdoom.UZDoom") / "metainfo" / "org.zdoom.UZDoom.metainfo.xml", release("4.14.3"));

    const Engine engine{.name = "UZDoom", .file = exported.string()};
    const Dialect::Traits traits = Dialect::of(engine);

    EXPECT_FALSE(traits.has(Dialect::Feature::netgameFolder));
    EXPECT_TRUE(traits.has(Dialect::Feature::netmode));
    EXPECT_FALSE(Dialect::needsVersion(engine));
    EXPECT_EQ(Dialect::suggestedVersion(engine, Flatpak::version), "");
}

TEST(FlatpakVersion, AVersionPickedByHandWinsOverTheReleaseInstalled) {
    const fs::path root = tests::Sandbox::scratch("flatpak-picked") / "flatpak";
    const fs::path exported = tests::deployFlatpak(root, "org.zdoom.UZDoom").exported;

    put(share(root, "org.zdoom.UZDoom") / "metainfo" / "org.zdoom.UZDoom.metainfo.xml", release("4.14.3"));

    const Engine engine{.name = "UZDoom", .file = exported.string(), .version = "5.0.0"};

    EXPECT_TRUE(Dialect::of(engine).has(Dialect::Feature::netgameFolder));
    EXPECT_FALSE(Dialect::needsVersion(engine));
}

TEST(FlatpakVersion, AnUpdatedFlatpakRunsAsTheNewRelease) {
    const fs::path root = tests::Sandbox::scratch("flatpak-updated") / "flatpak";
    const fs::path exported = tests::deployFlatpak(root, "org.zdoom.UZDoom").exported;
    const Engine engine{.name = "UZDoom", .file = exported.string()};

    put(share(root, "org.zdoom.UZDoom") / "metainfo" / "org.zdoom.UZDoom.metainfo.xml", release("4.14.3"));

    EXPECT_EQ(Flatpak::version(exported), "4.14.3");
    EXPECT_FALSE(Dialect::of(engine).has(Dialect::Feature::netgameFolder));

    update(root, "org.zdoom.UZDoom", "4567cdef");
    put(share(root, "org.zdoom.UZDoom", "4567cdef") / "metainfo" / "org.zdoom.UZDoom.metainfo.xml", METAINFO);

    EXPECT_EQ(Flatpak::version(exported), "5.0.3");
    EXPECT_TRUE(Dialect::of(engine).has(Dialect::Feature::netgameFolder));
    EXPECT_FALSE(Dialect::needsVersion(engine));
}

TEST(FlatpakVersion, AFlatpakThatNamesNoReleaseNeedsAVersion) {
    const fs::path root = tests::Sandbox::scratch("flatpak-unnamed") / "flatpak";
    const Engine engine{.name = "UZDoom", .file = tests::deployFlatpak(root, "org.zdoom.UZDoom").exported.string()};

    EXPECT_TRUE(Dialect::needsVersion(engine));
}

TEST(FlatpakVersion, ADetectedFlatpakIsListedAsItsPortWithNoVersionStored) {
    const fs::path root = tests::Sandbox::scratch("file-version-detected") / "flatpak";
    const fs::path exported = tests::deployFlatpak(root, "org.zdoom.UZDoom").exported;

    put(share(root, "org.zdoom.UZDoom") / "metainfo" / "org.zdoom.UZDoom.metainfo.xml", METAINFO);

    const Engine engine =
            Detect::engine({.port = Port::Port::uzdoom, .name = "UZDoom", .file = exported, .dos = false});

    EXPECT_EQ(engine.port, Port::Port::uzdoom);
    EXPECT_EQ(engine.version, "");
    EXPECT_FALSE(engine.managed);
    EXPECT_FALSE(Dialect::needsVersion(engine));
}

}
