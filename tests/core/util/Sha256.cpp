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

#include "core/util/Sha256.h"
#include "support/Corpus.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

// The read buffer of sha256File, so sizes around it cross a refill.
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
    const fs::path path = tests::Sandbox::scratch("sha256") / name;
    std::ofstream file(path, std::ios::binary | std::ios::trunc);

    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));

    return path;
}

}

TEST(Sha256Text, MatchesTheFips180Vectors) {
    EXPECT_EQ(sha256Text(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_EQ(sha256Text("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    EXPECT_EQ(sha256Text("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
              "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST(Sha256Text, MatchesAMillionAs) {
    EXPECT_EQ(sha256Text(std::string(MILLION, 'a')), "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST(Sha256Text, HashesEmbeddedNulBytes) {
    EXPECT_NE(sha256Text(std::string_view("a\0b", 3)), sha256Text("ab"));
    EXPECT_NE(sha256Text(std::string_view("\0", 1)), sha256Text(""));
}

TEST(Sha256Text, IsSixtyFourLowercaseHexDigits) {
    const std::string digest = sha256Text("The quick brown fox jumps over the lazy dog");

    EXPECT_EQ(digest, "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592");
    ASSERT_EQ(digest.size(), 64U);

    for (const char each : digest) {
        EXPECT_TRUE((each >= '0' && each <= '9') || (each >= 'a' && each <= 'f')) << digest;
    }
}

TEST(Sha256File, AgreesWithTheTextDigestAcrossTheBufferEdge) {
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

        EXPECT_EQ(sha256File(path), sha256Text(bytes)) << "size " << size;
    }
}

TEST(Sha256File, HashesAnEmptyFileAsTheEmptyDigest) {
    EXPECT_EQ(sha256File(spill("empty.bin", "")), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(Sha256File, HashesTheCorpusIwadStably) {
    const std::string first = sha256File(tests::Corpus::iwad());

    EXPECT_EQ(first.size(), 64U);
    EXPECT_EQ(sha256File(tests::Corpus::iwad()), first);
    EXPECT_NE(sha256File(tests::Corpus::pk3()), first);
}

TEST(Sha256File, AMissingFileHasNoDigest) {
    EXPECT_EQ(sha256File(tests::Sandbox::scratch("sha256") / "absent.wad"), "");
}

TEST(Sha256File, ADirectoryHasNoDigest) {
    EXPECT_EQ(sha256File(tests::Sandbox::scratch("sha256")), "");
}
