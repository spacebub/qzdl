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
#include <fstream>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "core/util/Md5.h"
#include "support/Corpus.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

// The read buffer of md5File, so sizes around it cross a refill.
constexpr std::size_t BUFFER = static_cast<std::size_t>(64 * 1024);

constexpr std::size_t MILLION = 1000000;

std::string pattern(const std::size_t size) {
    std::string out(size, '\0');

    for (std::size_t at = 0; at < size; ++at) {
        out[at] = static_cast<char>((at * 31U + 7U) & 0xFFU);
    }

    return out;
}

fs::path spill(const char *name, const std::string_view bytes) {
    const fs::path path = tests::Sandbox::scratch("md5") / name;
    std::ofstream file(path, std::ios::binary | std::ios::trunc);

    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));

    return path;
}

}

TEST(Md5Text, MatchesTheRfc1321Vectors) {
    EXPECT_EQ(md5Text(""), "d41d8cd98f00b204e9800998ecf8427e");
    EXPECT_EQ(md5Text("a"), "0cc175b9c0f1b6a831c399e269772661");
    EXPECT_EQ(md5Text("abc"), "900150983cd24fb0d6963f7d28e17f72");
    EXPECT_EQ(md5Text("message digest"), "f96b697d7cb7938d525a2f31aaf161d0");
    EXPECT_EQ(md5Text("abcdefghijklmnopqrstuvwxyz"), "c3fcd3d76192e4007dfb496cca67e13b");
    EXPECT_EQ(md5Text("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"),
              "d174ab98d277d9f5a5611c2c9f419d9f");
    EXPECT_EQ(md5Text("12345678901234567890123456789012345678901234567890123456789012345678901234567890"),
              "57edf4a22be3c955ac49da2e2107b67a");
}

TEST(Md5Text, MatchesAMillionAs) {
    EXPECT_EQ(md5Text(std::string(MILLION, 'a')), "7707d6ae4e027c70eea2a935c2296f21");
}

TEST(Md5Text, HashesEmbeddedNulBytes) {
    EXPECT_NE(md5Text(std::string_view("a\0b", 3)), md5Text("ab"));
    EXPECT_NE(md5Text(std::string_view("\0", 1)), md5Text(""));
}

TEST(Md5Text, IsThirtyTwoLowercaseHexDigits) {
    const std::string digest = md5Text("The quick brown fox jumps over the lazy dog");

    EXPECT_EQ(digest, "9e107d9d372bb6826bd81d3542a419d6");
    ASSERT_EQ(digest.size(), 32U);

    for (const char each : digest) {
        EXPECT_TRUE((each >= '0' && each <= '9') || (each >= 'a' && each <= 'f')) << digest;
    }
}

TEST(Md5File, AgreesWithTheTextDigestAcrossTheBufferEdge) {
    for (const std::size_t size : {std::size_t{0},
                                   std::size_t{1},
                                   std::size_t{55},
                                   std::size_t{64},
                                   BUFFER - 1,
                                   BUFFER,
                                   BUFFER + 1,
                                   (BUFFER * 3) + 17}) {
        const std::string bytes = pattern(size);
        const fs::path path = spill("sized.bin", bytes);

        EXPECT_EQ(md5File(path), md5Text(bytes)) << "size " << size;
    }
}

TEST(Md5File, HashesAnEmptyFileAsTheEmptyDigest) {
    EXPECT_EQ(md5File(spill("empty.bin", "")), "d41d8cd98f00b204e9800998ecf8427e");
}

TEST(Md5File, HashesTheCorpusIwadStably) {
    const std::string first = md5File(tests::Corpus::iwad());

    EXPECT_EQ(first.size(), 32U);
    EXPECT_EQ(md5File(tests::Corpus::iwad()), first);
    EXPECT_NE(md5File(tests::Corpus::pk3()), first);
}

TEST(Md5File, AMissingFileHasNoDigest) {
    EXPECT_EQ(md5File(tests::Sandbox::scratch("md5") / "absent.wad"), "");
}

TEST(Md5File, ADirectoryHasNoDigest) {
    EXPECT_EQ(md5File(tests::Sandbox::scratch("md5")), "");
}
