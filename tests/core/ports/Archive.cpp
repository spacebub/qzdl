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

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Zip.h"
#include "core/ports/Archive.h"
#include "support/Corpus.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

class ArchiveTest : public ::testing::Test {
protected:
    fs::path scratch;
    fs::path into;

    void SetUp() override {
        scratch = tests::Sandbox::scratch("archive");
        into = scratch / "into";
    }

    [[nodiscard]] fs::path archive(const std::vector<ports::Entry> &entries) const {
        const fs::path file = scratch / "made.zip";

        ports::write(file, ports::zip(entries));

        return file;
    }
};

}

TEST_F(ArchiveTest, EveryEntryLandsAtItsPathUnderTheTarget) {
    std::string error;

    ASSERT_TRUE(Archive::extract(archive({{.name = "readme.txt", .bytes = "hello"},
                                          {.name = "bin/port", .bytes = "binary"},
                                          {.name = "data/deep/more.pk3", .bytes = "zip"}}),
                                 into,
                                 &error))
            << error;

    EXPECT_EQ(ports::slurp(into / "readme.txt"), "hello");
    EXPECT_EQ(ports::slurp(into / "bin" / "port"), "binary");
    EXPECT_EQ(ports::slurp(into / "data" / "deep" / "more.pk3"), "zip");
}

TEST_F(ArchiveTest, TheCorpusPk3UnpacksWhole) {
    std::string error;

    ASSERT_TRUE(Archive::extract(tests::Corpus::pk3(), into, &error)) << error;

    EXPECT_TRUE(fs::is_regular_file(into / "iwadinfo.txt"));
    EXPECT_TRUE(fs::is_regular_file(into / "graphics" / "TITLE.png"));
    EXPECT_TRUE(fs::is_regular_file(into / "maps" / "MAP32.wad"));
}

TEST_F(ArchiveTest, TheTargetIsMadeWhenMissing) {
    into = scratch / "a" / "b" / "c";

    ASSERT_TRUE(Archive::extract(archive({{.name = "x", .bytes = "1"}}), into));

    EXPECT_TRUE(fs::is_regular_file(into / "x"));
}

TEST_F(ArchiveTest, ADirectoryEntryIsMadeEvenEmpty) {
    ASSERT_TRUE(Archive::extract(archive({{.name = "empty/", .bytes = ""}}), into));

    EXPECT_TRUE(fs::is_directory(into / "empty"));
}

TEST_F(ArchiveTest, AnEmptyArchiveExtractsNothing) {
    ASSERT_TRUE(Archive::extract(archive({}), into));

    EXPECT_TRUE(fs::is_empty(into));
}

TEST_F(ArchiveTest, AnEntryClimbingOutOfTheTargetIsSkipped) {
    ASSERT_TRUE(Archive::extract(
            archive({{.name = "../escaped.txt", .bytes = "out"}, {.name = "kept.txt", .bytes = "in"}}), into));

    EXPECT_FALSE(fs::exists(scratch / "escaped.txt"));
    EXPECT_EQ(ports::slurp(into / "kept.txt"), "in");
}

TEST_F(ArchiveTest, AnEntryThatClimbsOutAfterGoingInIsSkipped) {
    ASSERT_TRUE(Archive::extract(archive({{.name = "inside/../../escaped.txt", .bytes = "out"}}), into));

    EXPECT_FALSE(fs::exists(scratch / "escaped.txt"));
}

TEST_F(ArchiveTest, AnEntryThatOnlyLooksAboutIsKept) {
    ASSERT_TRUE(Archive::extract(archive({{.name = "inside/../here.txt", .bytes = "in"}}), into));

    EXPECT_EQ(ports::slurp(into / "here.txt"), "in");
}

TEST_F(ArchiveTest, AnAbsoluteEntryIsSkipped) {
    const fs::path target = scratch / "absolute.txt";

    ASSERT_TRUE(Archive::extract(archive({{.name = target.string(), .bytes = "out"}}), into));

    EXPECT_FALSE(fs::exists(target));
}

TEST_F(ArchiveTest, ExecutableBitsAreKept) {
    ASSERT_TRUE(Archive::extract(archive({{.name = "port", .bytes = "#!/bin/sh\n", .mode = 0755},
                                          {.name = "data.txt", .bytes = "x", .mode = 0644}}),
                                 into));

    EXPECT_TRUE(ports::executable(into / "port"));
    EXPECT_FALSE(ports::executable(into / "data.txt"));
}

TEST_F(ArchiveTest, AnArchiveClaimingMoreThanAnyPortUnpacksToIsRefusedUntouched) {
    std::string error;
    constexpr std::uint32_t HALF = 600U * 1024 * 1024;

    EXPECT_FALSE(Archive::extract(
            archive({{.name = "a", .bytes = "x", .claimed = HALF}, {.name = "b", .bytes = "y", .claimed = HALF}}),
            into,
            &error));
    EXPECT_EQ(error, "it claims to unpack to more than a gigabyte");
    EXPECT_FALSE(fs::exists(into / "a")) << "nothing is written before the claim is added up";
}

TEST_F(ArchiveTest, AFileThatIsNotAZipIsRefused) {
    std::string error;

    ports::write(scratch / "plain.zip", "this is plain text, not an archive at all");

    EXPECT_FALSE(Archive::extract(scratch / "plain.zip", into, &error));
    EXPECT_EQ(error, "it is not a zip file");
}

TEST_F(ArchiveTest, AMissingFileIsRefused) {
    std::string error;

    EXPECT_FALSE(Archive::extract(scratch / "absent.zip", into, &error));
    EXPECT_EQ(error, "it is not a zip file");
}

TEST_F(ArchiveTest, ATruncatedArchiveIsRefused) {
    std::string error;
    const std::string whole = ports::zip({{.name = "a.txt", .bytes = std::string(400, 'a')}});

    ports::write(scratch / "cut.zip", whole.substr(0, whole.size() / 2));

    EXPECT_FALSE(Archive::extract(scratch / "cut.zip", into, &error));
    EXPECT_FALSE(error.empty());
}

TEST_F(ArchiveTest, AnEntryWhoseBytesDoNotMatchItsChecksumFails) {
    std::string error;
    std::string bytes = ports::zip({{.name = "a.txt", .bytes = "abcdef"}});
    const size_t at = bytes.find("abcdef");

    bytes[at] = 'X';
    ports::write(scratch / "bad.zip", bytes);

    EXPECT_FALSE(Archive::extract(scratch / "bad.zip", into, &error));
    EXPECT_EQ(error, "could not write a.txt");
}

TEST_F(ArchiveTest, ATargetThatIsAFileIsRefused) {
    std::string error;

    ports::write(into, "in the way");

    EXPECT_FALSE(Archive::extract(archive({{.name = "x", .bytes = "1"}}), into, &error));
    EXPECT_FALSE(error.empty());
}

TEST_F(ArchiveTest, TheErrorIsOptional) {
    EXPECT_FALSE(Archive::extract(scratch / "absent.zip", into));
}
