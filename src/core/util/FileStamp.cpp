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
#ifdef _WIN32
#include <chrono>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/stat.h>
#endif

#include "core/util/FileStamp.h"

#ifdef _WIN32
namespace {

FileStamp followed(const std::filesystem::path &path) {
    FileStamp out;
    std::error_code code;
    const std::filesystem::file_time_type time = std::filesystem::last_write_time(path, code);

    out.there = !code;

    if (out.there) {
        const auto since = time.time_since_epoch();
        const auto seconds = std::chrono::floor<std::chrono::seconds>(since);

        out.seconds = seconds.count();
        out.nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(since - seconds).count();

        if (std::filesystem::is_regular_file(path, code)) {
            out.size = std::filesystem::file_size(path, code);
        }
    }

    return out;
}

}
#endif

FileStamp FileStamp::of(const std::filesystem::path &path) {
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA data{};

    if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data) == 0) {
        return {};
    }

    // The attributes describe a link itself, and the standard calls describe what it points at.
    if ((data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        return followed(path);
    }

    constexpr std::uint64_t TICKS = 10'000'000;
    const std::uint64_t ticks = (static_cast<std::uint64_t>(data.ftLastWriteTime.dwHighDateTime) << 32U)
                                | data.ftLastWriteTime.dwLowDateTime;
    const bool folder = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

    return FileStamp{
            .there = true,
            .size = folder ? 0 : (static_cast<std::uintmax_t>(data.nFileSizeHigh) << 32U) | data.nFileSizeLow,
            .seconds = static_cast<std::int64_t>(ticks / TICKS),
            .nanoseconds = static_cast<std::int64_t>((ticks % TICKS) * 100),
    };
#else
    return of(path.native());
#endif
}

FileStamp FileStamp::of(const std::string &path) {
#ifdef _WIN32
    return of(std::filesystem::path(path));
#else
    struct stat info{};

    if (::stat(path.c_str(), &info) != 0) {
        return {};
    }

    return FileStamp{
            .there = true,
            .size = S_ISREG(info.st_mode) ? static_cast<std::uintmax_t>(info.st_size) : 0,
            .seconds = static_cast<std::int64_t>(info.st_mtim.tv_sec),
            .nanoseconds = static_cast<std::int64_t>(info.st_mtim.tv_nsec),
    };
#endif
}
