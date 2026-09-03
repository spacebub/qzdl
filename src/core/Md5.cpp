/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
 *
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

#include "core/Md5.h"

namespace {

// RFC 1321, section 3.4: how far each round rotates, and the sine table.
constexpr std::array<std::uint32_t, 64> SHIFTS = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
};

constexpr std::array<std::uint32_t, 64> SINES = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
    0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
    0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
    0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
    0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
    0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

std::uint32_t rotate(const std::uint32_t value, const std::uint32_t by) {
    return (value << by) | (value >> (32 - by));
}

/** One 64 byte block, folded into the running state. */
void mix(std::array<std::uint32_t, 4> &state, const unsigned char *block) {
    std::array<std::uint32_t, 16> words{};

    for (size_t index = 0; index < words.size(); ++index) {
        words[index] = static_cast<std::uint32_t>(block[index * 4])
            | static_cast<std::uint32_t>(block[index * 4 + 1]) << 8
            | static_cast<std::uint32_t>(block[index * 4 + 2]) << 16
            | static_cast<std::uint32_t>(block[index * 4 + 3]) << 24;
    }

    std::uint32_t a = state[0];
    std::uint32_t b = state[1];
    std::uint32_t c = state[2];
    std::uint32_t d = state[3];

    for (std::uint32_t step = 0; step < 64; ++step) {
        std::uint32_t mixed = 0;
        std::uint32_t word = 0;

        if (step < 16) {
            mixed = (b & c) | (~b & d);
            word = step;
        } else if (step < 32) {
            mixed = (d & b) | (~d & c);
            word = (5 * step + 1) % 16;
        } else if (step < 48) {
            mixed = b ^ c ^ d;
            word = (3 * step + 5) % 16;
        } else {
            mixed = c ^ (b | ~d);
            word = (7 * step) % 16;
        }

        const std::uint32_t rotated = a + mixed + SINES[step] + words[word];

        a = d;
        d = c;
        c = b;
        b += rotate(rotated, SHIFTS[step]);
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

}

namespace Md5 {

std::string ofFile(const std::filesystem::path &file) {
    std::ifstream stream(file, std::ios::binary);

    if (!stream) {
        return {};
    }

    std::array<std::uint32_t, 4> state = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};
    std::vector<char> buffer(64 * 1024);
    std::array<unsigned char, 64> tail{};
    size_t held = 0;
    std::uint64_t total = 0;

    while (stream) {
        stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

        const auto read = static_cast<size_t>(stream.gcount());

        if (read == 0) {
            break;
        }

        total += read;

        size_t offset = 0;

        // Whatever was left over from the last read is completed first.
        if (held != 0) {
            const size_t wanted = std::min(tail.size() - held, read);

            std::memcpy(tail.data() + held, buffer.data(), wanted);

            held += wanted;
            offset = wanted;

            if (held < tail.size()) {
                continue;
            }

            mix(state, tail.data());
            held = 0;
        }

        while (read - offset >= tail.size()) {
            mix(state, reinterpret_cast<const unsigned char *>(buffer.data() + offset));
            offset += tail.size();
        }

        held = read - offset;

        if (held != 0) {
            std::memcpy(tail.data(), buffer.data() + offset, held);
        }
    }

    if (stream.bad()) {
        return {};
    }

    // The padding: a one bit, zeroes, and the length in bits as eight bytes.
    std::array<unsigned char, 128> last{};

    std::memcpy(last.data(), tail.data(), held);

    last[held] = 0x80;

    const size_t padded = held + 1 <= 56 ? 64 : 128;
    const std::uint64_t bits = total * 8;

    for (size_t index = 0; index < 8; ++index) {
        last[padded - 8 + index] = static_cast<unsigned char>(bits >> (index * 8));
    }

    mix(state, last.data());

    if (padded == 128) {
        mix(state, last.data() + 64);
    }

    static constexpr char DIGITS[] = "0123456789abcdef";
    std::string digest;
    digest.reserve(32);

    for (const std::uint32_t word : state) {
        for (size_t index = 0; index < 4; ++index) {
            const auto byte = static_cast<unsigned char>(word >> (index * 8));

            digest.push_back(DIGITS[byte >> 4]);
            digest.push_back(DIGITS[byte & 0x0f]);
        }
    }

    return digest;
}

}
