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

#include <cctype>
#include <filesystem>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "Zip.h"
#include "core/ports/Catalog.h"
#include "core/ports/Install.h"
#include "core/util/Sha256.h"
#include "support/Corpus.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

// A port whose build is the same pattern on every platform.
constexpr Catalog::Port FAKE{
        .id = "fakeport",
        .name = "Fake Port",
        .blurb = "",
        .homepage = "",
        .repository = "fake/port",
        .file = "",
        .version = "",
        .sha256 = "",
        .windowsBuild = "fake+.zip",
        .linuxBuild = "fake+.zip",
        .program = "fakeport",
        .dos = false,
};

constexpr Catalog::Port FAKE_DOS{
        .id = "fakedos",
        .name = "Fake DOS",
        .blurb = "",
        .homepage = "",
        .repository = "",
        .file = "https://example.invalid/fakedos.zip",
        .version = "1.0",
        .sha256 = "",
        .windowsBuild = "fakedos.zip",
        .linuxBuild = "fakedos.zip",
        .program = "fakedos",
        .dos = true,
};

std::string release(const std::string &tag, const std::string &assets) {
    return R"({"tag_name":")" + tag + R"(","assets":[)" + assets + "]}";
}

std::string asset(const std::string &name, const long long size = 100, const std::string &digest = {}) {
    return R"({"name":")" + name + R"(","browser_download_url":"https://example.invalid/download/)" + name
           + R"(","size":)" + std::to_string(size) + (digest.empty() ? "" : R"(,"digest":")" + digest + "\"") + "}";
}

constexpr std::string_view HASH = "1c7fd0afe9b1ab9517ab900fc4b4974282430e50cb57fe3291d6218f125ccd31";

std::string upper(std::string text) {
    for (char &each : text) {
        each = static_cast<char>(std::toupper(static_cast<unsigned char>(each)));
    }

    return text;
}

class InstallTest : public ::testing::Test {
protected:
    fs::path scratch;

    void SetUp() override {
        scratch = tests::Sandbox::scratch("install");

        fs::remove_all(Catalog::directory());
        fs::remove_all(Catalog::downloads());
    }

    [[nodiscard]] fs::path archive(const std::string &name, const std::vector<ports::Entry> &entries) const {
        ports::write(scratch / name, ports::zip(entries));

        return scratch / name;
    }
};

}

TEST(InstallParseRelease, TheVersionIsTheTagFromItsFirstDigit) {
    EXPECT_EQ(Install::parseRelease(release("v4.14.2", ""), FAKE).version, "4.14.2");
    EXPECT_EQ(Install::parseRelease(release("release-0.29.3", ""), FAKE).version, "0.29.3");
    EXPECT_EQ(Install::parseRelease(release("15.0.0", ""), FAKE).version, "15.0.0");
}

TEST(InstallParseRelease, ATagWithoutDigitsIsTheVersionAsIs) {
    EXPECT_EQ(Install::parseRelease(release("nightly", ""), FAKE).version, "nightly");
}

TEST(InstallParseRelease, TheFirstAssetMatchingTheBuildIsPicked) {
    const Install::Release found = Install::parseRelease(
            release("v1.0",
                    asset("fake-source.tar.gz") + "," + asset("fake-1.0.zip", 4096) + "," + asset("fake-old.zip")),
            FAKE);

    EXPECT_EQ(found.asset, "fake-1.0.zip");
    EXPECT_EQ(found.url, "https://example.invalid/download/fake-1.0.zip");
    EXPECT_EQ(found.size, 4096);
}

TEST(InstallParseRelease, NoMatchingAssetLeavesTheUrlEmpty) {
    const Install::Release found = Install::parseRelease(release("v1.0", asset("other.tar.gz")), FAKE);

    EXPECT_EQ(found.version, "1.0");
    EXPECT_TRUE(found.url.empty());
    EXPECT_TRUE(found.asset.empty());
}

TEST(InstallParseRelease, NoAssetsLeavesTheUrlEmpty) {
    EXPECT_TRUE(Install::parseRelease(R"({"tag_name":"v1"})", FAKE).url.empty());
}

TEST(InstallParseRelease, MalformedJsonGivesAnEmptyRelease) {
    const Install::Release found = Install::parseRelease(R"({"tag_name": "v1", "assets": [)", FAKE);

    EXPECT_TRUE(found.version.empty());
    EXPECT_TRUE(found.url.empty());
}

TEST(InstallParseRelease, AnEmptyBodyGivesAnEmptyRelease) {
    EXPECT_TRUE(Install::parseRelease("", FAKE).version.empty());
}

TEST(InstallParseRelease, APortWithoutABuildForThisPlatformMatchesNothing) {
    Catalog::Port none = FAKE;

    none.windowsBuild = "";
    none.linuxBuild = "";

    EXPECT_TRUE(Install::parseRelease(release("v1", asset("fake-1.zip")), none).url.empty());
}

TEST(InstallParseRelease, TheCatalogsOwnPatternPicksThisPlatformsBuild) {
    const Catalog::Port *uzdoom = Catalog::find("uzdoom");

    ASSERT_NE(uzdoom, nullptr);

    const Install::Release found = Install::parseRelease(release("4.14.3",
                                                                 asset("UZDoom-windows-4.14.3.zip") + ","
                                                                         + asset("UZDoom-linux-4.14.3-x86_64.AppImage")
                                                                         + "," + asset("UZDoom-macOS-4.14.3.dmg")),
                                                         *uzdoom);

#ifdef _WIN32
    EXPECT_EQ(found.asset, "UZDoom-windows-4.14.3.zip");
#else
    EXPECT_EQ(found.asset, "UZDoom-linux-4.14.3-x86_64.AppImage");
#endif
}

TEST(InstallParseRelease, TheAssetsDigestIsItsChecksum) {
    const Install::Release found =
            Install::parseRelease(release("v1", asset("fake-1.zip", 100, "sha256:" + std::string(HASH))), FAKE);

    EXPECT_EQ(found.sha256, HASH);
}

TEST(InstallParseRelease, ADigestInUpperCaseIsLowered) {
    const Install::Release found = Install::parseRelease(
            release("v1", asset("fake-1.zip", 100, "sha256:" + upper(std::string(HASH)))), FAKE);

    EXPECT_EQ(found.sha256, HASH);
}

TEST(InstallParseRelease, NoDigestIsNoChecksum) {
    EXPECT_EQ(Install::parseRelease(release("v1", asset("fake-1.zip")), FAKE).sha256, "");
}

TEST(InstallParseRelease, ADigestThatIsNotASha256IsNoChecksum) {
    const auto checksum = [](const std::string &digest) {
        return Install::parseRelease(release("v1", asset("fake-1.zip", 100, digest)), FAKE).sha256;
    };

    EXPECT_EQ(checksum("md5:9e107d9d372bb6826bd81d3542a419d6"), "");
    EXPECT_EQ(checksum(std::string(HASH)), "");
    EXPECT_EQ(checksum("sha256:" + std::string(HASH).substr(1)), "");
    EXPECT_EQ(checksum("sha256:" + std::string(HASH) + "0"), "");
    EXPECT_EQ(checksum("sha256:" + std::string(64, 'z')), "");
    EXPECT_EQ(checksum("sha256:"), "");
}

TEST(InstallSecure, OnlyHttpsIsFetched) {
    EXPECT_TRUE(Install::secure("https://example.invalid/port.zip"));
    EXPECT_FALSE(Install::secure("http://example.invalid/port.zip"));
    EXPECT_FALSE(Install::secure("ftp://example.invalid/port.zip"));
    EXPECT_FALSE(Install::secure("file:///tmp/port.zip"));
    EXPECT_FALSE(Install::secure(""));
}

TEST(InstallFileNameOf, TheNameIsTheLastPartOfTheUrl) {
    EXPECT_EQ(Install::fileNameOf("https://example.invalid/a/b/port-1.0.zip"), "port-1.0.zip");
}

TEST(InstallFileNameOf, AQueryOrFragmentIsDropped) {
    EXPECT_EQ(Install::fileNameOf("https://example.invalid/port.zip?raw=true"), "port.zip");
    EXPECT_EQ(Install::fileNameOf("https://example.invalid/port.zip#top"), "port.zip");
}

TEST(InstallFileNameOf, AUrlWithoutASlashIsItsOwnName) {
    EXPECT_EQ(Install::fileNameOf("port.zip"), "port.zip");
    EXPECT_EQ(Install::fileNameOf("https://example.invalid/"), "");
}

TEST(InstallDownloadName, UnsafeLettersAreDropped) {
    EXPECT_EQ(Install::downloadName(FAKE, "port 1.0 (x64).zip"), "port1.0x64.zip");
    EXPECT_EQ(Install::downloadName(FAKE, "a/b\\c.zip"), "abc.zip");
    EXPECT_EQ(Install::downloadName(FAKE, "Port_1-2.AppImage"), "Port_1-2.AppImage");
}

TEST(InstallDownloadName, NothingLeftOrALeadingDotIsThePortsId) {
    EXPECT_EQ(Install::downloadName(FAKE, "!!!"), "fakeport");
    EXPECT_EQ(Install::downloadName(FAKE, ""), "fakeport");
    EXPECT_EQ(Install::downloadName(FAKE, ".hidden"), "fakeport");
    EXPECT_EQ(Install::downloadName(FAKE, "../../evil.zip"), "fakeport");
}

TEST_F(InstallTest, AZipIsUnpackedIntoThePortsDirectoryAndItsProgramFound) {
    const Install::Placed placed = Install::place(
            archive("fake-1.0.zip", {{.name = "fake/fakeport", .bytes = "#!/bin/sh\n", .mode = 0644}}), FAKE);

    EXPECT_EQ(placed.trouble, "");
    EXPECT_EQ(placed.headline, "");
    EXPECT_EQ(placed.program, Catalog::directory(FAKE) / "fake" / "fakeport");
    EXPECT_TRUE(ports::executable(placed.program)) << "a zip packed on Windows carries no permission bits";
}

TEST_F(InstallTest, AZipWithoutTheProgramPlacesItsFilesAndFindsNothing) {
    fs::copy_file(tests::Corpus::pk3(), scratch / "content.zip");

    const Install::Placed placed = Install::place(scratch / "content.zip", FAKE);

    EXPECT_EQ(placed.trouble, "");
    EXPECT_TRUE(placed.program.empty());
    EXPECT_TRUE(fs::is_regular_file(Catalog::directory(FAKE) / "maps" / "MAP01.wad"));
}

TEST_F(InstallTest, ADosZipIsSearchedForItsExe) {
    const Install::Placed placed = Install::place(
            archive("fakedos.zip", {{.name = "FAKEDOS.EXE", .bytes = "MZ"}, {.name = "fakedos", .bytes = "x"}}),
            FAKE_DOS);

    EXPECT_EQ(placed.program, Catalog::directory(FAKE_DOS) / "FAKEDOS.EXE");
}

TEST_F(InstallTest, AZipThatCannotBeReadSaysSo) {
    ports::write(scratch / "broken.zip", "not a zip");

    const Install::Placed placed = Install::place(scratch / "broken.zip", FAKE);

    EXPECT_TRUE(placed.program.empty());
    EXPECT_EQ(placed.trouble, "it is not a zip file");
    EXPECT_EQ(placed.headline, "Could not unpack broken.zip");
}

TEST_F(InstallTest, AnythingElseIsCopiedInAsTheProgram) {
    ports::write(scratch / "Fake-1.0-x86_64.AppImage", "ELF");

    const Install::Placed placed = Install::place(scratch / "Fake-1.0-x86_64.AppImage", FAKE);

    EXPECT_EQ(placed.trouble, "");
    EXPECT_EQ(placed.program, Catalog::directory(FAKE) / "Fake-1.0-x86_64.AppImage");
    EXPECT_EQ(ports::slurp(placed.program), "ELF");
    EXPECT_TRUE(ports::executable(placed.program));
}

TEST_F(InstallTest, PlacingAgainReplacesTheCopy) {
    ports::write(scratch / "fake.AppImage", "first");
    ASSERT_FALSE(Install::place(scratch / "fake.AppImage", FAKE).program.empty());

    ports::write(scratch / "fake.AppImage", "second");

    EXPECT_EQ(ports::slurp(Install::place(scratch / "fake.AppImage", FAKE).program), "second");
}

TEST_F(InstallTest, AFileThatIsNotThereCannotBePutInPlace) {
    const Install::Placed placed = Install::place(scratch / "absent.AppImage", FAKE);

    EXPECT_TRUE(placed.program.empty());
    EXPECT_EQ(placed.trouble, "Could not put it in place");
}

TEST_F(InstallTest, AcceptWithNoPromiseTakesTheFileAsItIs) {
    const fs::path at = archive("fake-1.0.zip", {{.name = "fakeport", .bytes = "#!/bin/sh\n"}});

    const Install::Placed placed = Install::accept(at, at, {}, FAKE);

    EXPECT_EQ(placed.trouble, "");
    EXPECT_FALSE(placed.rejected);
    EXPECT_EQ(placed.program, Catalog::directory(FAKE) / "fakeport");
    EXPECT_TRUE(fs::exists(at));
}

TEST_F(InstallTest, AcceptHoldsTheDownloadToItsSize) {
    const std::string bytes = ports::zip({{.name = "fakeport", .bytes = "#!/bin/sh\n"}});
    const fs::path at = scratch / "fake-1.0.zip";

    ports::write(at, bytes);

    EXPECT_FALSE(Install::accept(at, at, {.size = static_cast<long long>(bytes.size()), .sha256 = {}}, FAKE).program.empty());

    const Install::Placed placed = Install::accept(at, at, {.size = static_cast<long long>(bytes.size()) + 1, .sha256 = {}}, FAKE);

    EXPECT_TRUE(placed.rejected);
    EXPECT_EQ(placed.trouble, "The download is not the size the release lists");
    EXPECT_TRUE(placed.program.empty());
    EXPECT_FALSE(fs::exists(at)) << "what was not promised is not kept";
}

TEST_F(InstallTest, AcceptHoldsTheDownloadToItsChecksum) {
    const std::string bytes = ports::zip({{.name = "fakeport", .bytes = "#!/bin/sh\n"}});
    const fs::path at = scratch / "fake-1.0.zip";

    ports::write(at, bytes);

    EXPECT_FALSE(Install::accept(at, at, {.size = 0, .sha256 = sha256Text(bytes)}, FAKE).program.empty());

    const Install::Placed placed = Install::accept(at, at, {.size = 0, .sha256 = sha256Text("something else")}, FAKE);

    EXPECT_TRUE(placed.rejected);
    EXPECT_EQ(placed.trouble, "The download does not match the checksum the release lists");
    EXPECT_FALSE(fs::exists(at));
}

TEST_F(InstallTest, AcceptChecksTheSizeAndTheChecksumTogether) {
    const std::string bytes = ports::zip({{.name = "fakeport", .bytes = "#!/bin/sh\n"}});
    const fs::path at = scratch / "fake-1.0.zip";

    ports::write(at, bytes);

    const Install::Placed placed = Install::accept(
            at, at, {.size = static_cast<long long>(bytes.size()), .sha256 = sha256Text(bytes)}, FAKE);

    EXPECT_FALSE(placed.rejected);
    EXPECT_EQ(placed.program, Catalog::directory(FAKE) / "fakeport");
}

TEST_F(InstallTest, AFreshDownloadMovesOntoTheShelfOnceItPasses) {
    const std::string bytes = ports::zip({{.name = "fakeport", .bytes = "#!/bin/sh\n"}});
    const fs::path into = scratch / "fake-1.0.zip";
    const fs::path partial = scratch / "fake-1.0.zip.part";

    ports::write(partial, bytes);

    const Install::Placed placed = Install::accept(partial, into, {.size = 0, .sha256 = sha256Text(bytes)}, FAKE);

    EXPECT_EQ(placed.trouble, "");
    EXPECT_EQ(placed.program, Catalog::directory(FAKE) / "fakeport");
    EXPECT_FALSE(fs::exists(partial));
    EXPECT_EQ(ports::slurp(into), bytes);
}

TEST_F(InstallTest, AFreshDownloadReplacesAnOlderShelfCopy) {
    const std::string bytes = ports::zip({{.name = "fakeport", .bytes = "#!/bin/sh\n"}});
    const fs::path into = scratch / "fake-1.0.zip";
    const fs::path partial = scratch / "fake-1.0.zip.part";

    ports::write(into, "older");
    ports::write(partial, bytes);

    ASSERT_FALSE(Install::accept(partial, into, {}, FAKE).program.empty());

    EXPECT_EQ(ports::slurp(into), bytes);
    EXPECT_FALSE(fs::exists(partial));
}

TEST_F(InstallTest, AFreshDownloadThatFailsNeverReachesTheShelf) {
    const fs::path into = scratch / "fake-1.0.zip";
    const fs::path partial = scratch / "fake-1.0.zip.part";

    ports::write(into, "older");
    ports::write(partial, ports::zip({{.name = "fakeport", .bytes = "#!/bin/sh\n"}}));

    const Install::Placed placed = Install::accept(partial, into, {.size = 1, .sha256 = {}}, FAKE);

    EXPECT_TRUE(placed.rejected);
    EXPECT_FALSE(fs::exists(partial));
    EXPECT_EQ(ports::slurp(into), "older");
    EXPECT_FALSE(fs::exists(Catalog::directory(FAKE) / "fakeport"));
}

TEST_F(InstallTest, ADownloadThatIsNotThereIsRejected) {
    const fs::path absent = scratch / "absent.zip";

    const Install::Placed placed = Install::accept(absent, absent, {}, FAKE);

    EXPECT_TRUE(placed.rejected);
    EXPECT_EQ(placed.trouble, "Could not read the download back");
}

TEST_F(InstallTest, AcceptReportsWhatPlacingDoes) {
    const fs::path at = scratch / "broken.zip";

    ports::write(at, "not a zip");

    const Install::Placed placed = Install::accept(at, at, {}, FAKE);

    EXPECT_FALSE(placed.rejected);
    EXPECT_EQ(placed.trouble, "it is not a zip file");
    EXPECT_EQ(placed.headline, "Could not unpack broken.zip");
    EXPECT_TRUE(fs::exists(at)) << "a download that passed its check is kept for the next try";
}

TEST_F(InstallTest, TheStampedVersionReadsBackTrimmed) {
    fs::create_directories(Catalog::directory(FAKE));

    Install::stamp(FAKE, " 4.14.2\n");

    EXPECT_EQ(Install::stampedVersion(FAKE), "4.14.2");
}

TEST_F(InstallTest, NoStampIsNoVersion) {
    EXPECT_EQ(Install::stampedVersion(FAKE), "");
}

TEST_F(InstallTest, AStampReplacesTheLast) {
    fs::create_directories(Catalog::directory(FAKE));

    Install::stamp(FAKE, "1.0");
    Install::stamp(FAKE, "2.0");

    EXPECT_EQ(Install::stampedVersion(FAKE), "2.0");
}

TEST_F(InstallTest, TheShelfIsTheSizeOfTheDownloadsOnIt) {
    ports::write(Catalog::downloads() / "a.zip", std::string(1000, 'a'));
    ports::write(Catalog::downloads() / "b.zip", std::string(234, 'b'));

    EXPECT_EQ(Install::shelfBytes(), 1234);
}

TEST_F(InstallTest, AnEmptyOrMissingShelfHoldsNothing) {
    EXPECT_EQ(Install::shelfBytes(), 0);

    fs::create_directories(Catalog::downloads());

    EXPECT_EQ(Install::shelfBytes(), 0);
}

TEST_F(InstallTest, ClearingTheShelfEmptiesItAndKeepsTheShelf) {
    ports::write(Catalog::downloads() / "a.zip", "a");
    ports::write(Catalog::downloads() / "partial" / "b.part", "b");

    Install::clearShelf();

    EXPECT_TRUE(fs::is_directory(Catalog::downloads()));
    EXPECT_TRUE(fs::is_empty(Catalog::downloads()));
    EXPECT_EQ(Install::shelfBytes(), 0);
}

TEST_F(InstallTest, ClearingTheShelfLeavesInstalledPortsAlone) {
    ports::write(Catalog::directory(FAKE) / "fakeport", "x");
    ports::write(Catalog::downloads() / "a.zip", "a");

    Install::clearShelf();

    EXPECT_TRUE(fs::exists(Catalog::directory(FAKE) / "fakeport"));
}
