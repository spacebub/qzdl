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
#include <cstring>
#include <filesystem>
#include <fstream>

#include "core/Catalog.h"
#include "core/Paths.h"
#include "core/Releases.h"

namespace {

constexpr auto CACHE = ".releases";
constexpr char MAGIC[4] = {'Z', 'D', 'L', 'R'};
constexpr std::uint32_t VERSION = 1;

// A string longer than this is a file that got scrambled, not an answer.
constexpr std::uint32_t LONGEST = 4096;

// How long an answer stands for.
constexpr std::chrono::minutes KEEP{30};

std::filesystem::path cachePath() {
    const std::filesystem::path data = Paths::dataDirectory();

    return data.empty() ? std::filesystem::path() : data / CACHE;
}

template <typename Number>
void putNumber(std::ostream &out, const Number value) {
    out.write(reinterpret_cast<const char *>(&value), sizeof value);
}

template <typename Number>
bool getNumber(std::istream &in, Number *value) {
    return static_cast<bool>(in.read(reinterpret_cast<char *>(value), sizeof *value));
}

void putText(std::ostream &out, const std::string_view value) {
    putNumber(out, static_cast<std::uint32_t>(value.size()));
    out.write(value.data(), static_cast<std::streamsize>(value.size()));
}

bool getText(std::istream &in, std::string *value) {
    std::uint32_t size = 0;

    if (!getNumber(in, &size) || size > LONGEST) {
        return false;
    }

    value->resize(size);

    return size == 0 || static_cast<bool>(in.read(value->data(), size));
}

}

long long Releases::now() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

bool Releases::fresh(const long long checked) {
    const long long age = now() - checked;

    // A clock that went backwards would otherwise make an answer stand for years.
    return checked > 0 && age >= 0
        && age < std::chrono::duration_cast<std::chrono::seconds>(KEEP).count();
}

std::vector<Releases::Answer> Releases::read() {
    const std::filesystem::path where = cachePath();

    if (where.empty()) {
        return {};
    }

    std::vector<Answer> held;
    bool whole = false;

    {
        std::ifstream file(where, std::ios::binary);
        char magic[sizeof MAGIC]{};
        std::uint32_t version = 0;
        std::uint32_t count = 0;

        if (!file) {
            return {};
        }

        if (file.read(magic, sizeof magic) && std::memcmp(magic, MAGIC, sizeof MAGIC) == 0
            && getNumber(file, &version) && version == VERSION && getNumber(file, &count)
            && count <= Catalog::ports().size()) {
            whole = true;

            for (std::uint32_t each = 0; each < count && whole; each++) {
                Answer one;

                whole = getText(file, &one.portId) && getNumber(file, &one.checked)
                    && getText(file, &one.version) && getText(file, &one.url)
                    && getText(file, &one.asset) && getNumber(file, &one.size)
                    && getText(file, &one.verdict) && getText(file, &one.note);

                held.push_back(std::move(one));
            }
        }
    }

    if (!whole) {
        std::error_code code;

        std::filesystem::remove(where, code);

        return {};
    }

    return held;
}

void Releases::write(const std::span<const Answer> answers) {
    const std::filesystem::path where = cachePath();

    if (where.empty()) {
        return;
    }

    std::error_code code;

    std::filesystem::create_directories(where.parent_path(), code);

    if (code) {
        return;
    }

    std::ofstream file(where, std::ios::binary | std::ios::trunc);

    if (!file) {
        return;
    }

    file.write(MAGIC, sizeof MAGIC);
    putNumber(file, VERSION);
    putNumber(file, static_cast<std::uint32_t>(answers.size()));

    for (const Answer &one : answers) {
        putText(file, one.portId);
        putNumber(file, one.checked);
        putText(file, one.version);
        putText(file, one.url);
        putText(file, one.asset);
        putNumber(file, one.size);
        putText(file, one.verdict);
        putText(file, one.note);
    }
}
