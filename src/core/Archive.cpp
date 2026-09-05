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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <algorithm>
#include <system_error>

#include "core/Archive.h"
#include "external/miniz/miniz.h"

namespace {

bool escapes(const std::filesystem::path &relative) {
    return relative.is_absolute() || relative.has_root_name()
        || std::ranges::any_of(relative, [](const std::filesystem::path &part) {
               return part == "..";
           });
}

void fail(std::string *error, std::string text) {
    if (error != nullptr) {
        *error = std::move(text);
    }
}

}

bool Archive::extract(const std::filesystem::path &file, const std::filesystem::path &into,
                      std::string *error) {
    mz_zip_archive archive = {};

    if (mz_zip_reader_init_file(&archive, file.string().c_str(), 0) == 0) {
        fail(error, "it is not a zip file");

        return false;
    }

    std::error_code code;

    std::filesystem::create_directories(into, code);

    if (code) {
        mz_zip_reader_end(&archive);
        fail(error, code.message());

        return false;
    }

    bool done = true;
    const mz_uint count = mz_zip_reader_get_num_files(&archive);

    for (mz_uint index = 0; index < count; index++) {
        mz_zip_archive_file_stat stat;

        if (mz_zip_reader_file_stat(&archive, index, &stat) == 0) {
            fail(error, "the archive could not be read");
            done = false;

            break;
        }

        const std::filesystem::path relative =
            std::filesystem::path(stat.m_filename).lexically_normal();

        if (escapes(relative)) {
            continue;
        }

        const std::filesystem::path target = into / relative;

        if (mz_zip_reader_is_file_a_directory(&archive, index) != 0) {
            std::filesystem::create_directories(target, code);

            continue;
        }

        std::filesystem::create_directories(target.parent_path(), code);

        if (mz_zip_reader_extract_to_file(&archive, index, target.string().c_str(), 0) == 0) {
            fail(error, "could not write " + relative.string());
            done = false;

            break;
        }

        // The exec bit a zip packed on a unix carries, which is the difference
        // between a program and a file that only looks like one.
        if (const mz_uint mode = stat.m_external_attr >> 16U; (mode & 0111U) != 0) {
            std::filesystem::permissions(target,
                                         std::filesystem::perms::owner_exec
                                         | std::filesystem::perms::group_exec
                                         | std::filesystem::perms::others_exec,
                                         std::filesystem::perm_options::add, code);
        }
    }

    mz_zip_reader_end(&archive);

    return done;
}
