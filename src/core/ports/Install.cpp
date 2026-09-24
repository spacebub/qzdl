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
#include <fstream>

#include "ttk/system/Json.h"
#include "ttk/system/Text.h"

#include "core/ports/Archive.h"
#include "core/ports/Install.h"

using namespace ttk;

namespace {

constexpr auto STAMP = ".zdl-version";

std::string versionOf(const std::string &tag) {
    for (size_t at = 0; at < tag.size(); at++) {
        if (std::isdigit(static_cast<unsigned char>(tag[at])) != 0) {
            return tag.substr(at);
        }
    }

    return tag;
}

}

namespace Install {

Release parseRelease(const std::string &body, const Catalog::Port &port) {
    std::string trouble;
    const Json::Doc release = Json::read_data(body, &trouble);
    const std::string_view pattern = Catalog::pattern(port);
    Release out;

    out.version = versionOf(Json::obj_get_string(release.root(), "tag_name"));

    if (const yyjson_val *assets = Json::obj_get(release.root(), "assets"); assets != nullptr) {
        size_t index = 0;
        size_t count = 0;
        yyjson_val *asset = nullptr;

        yyjson_arr_foreach(assets, index, count, asset) {
            const std::string name = Json::obj_get_string(asset, "name");

            if (Catalog::matches(name, pattern)) {
                out.asset = name;
                out.url = Json::obj_get_string(asset, "browser_download_url");
                out.size = Json::obj_get_int(asset, "size");

                break;
            }
        }
    }

    return out;
}

std::string fileNameOf(const std::string_view url) {
    const size_t cut = url.find_last_of('/');
    const std::string_view last = cut == std::string_view::npos ? url : url.substr(cut + 1);
    const size_t query = last.find_first_of("?#");

    return std::string(query == std::string_view::npos ? last : last.substr(0, query));
}

std::string downloadName(const Catalog::Port &port, const std::string_view asset) {
    std::string out;

    for (const char each : asset) {
        if (std::isalnum(static_cast<unsigned char>(each)) != 0
            || each == '.' || each == '-' || each == '_') {
            out.push_back(each);
        }
    }

    return out.empty() || out.starts_with(".") ? std::string(port.id) : out;
}

Placed place(const std::filesystem::path &archive, const Catalog::Port &port) {
    const std::filesystem::path where = Catalog::directory(port);
    const std::string name = archive.filename().string();
    Placed out;
    std::error_code code;

    std::filesystem::create_directories(where, code);

    if (code) {
        out.trouble = "Could not make a directory for it: " + code.message();

        return out;
    }

    if (Text::iends_with(name, ".zip")) {
        if (!Archive::extract(archive, where, &out.trouble)) {
            out.headline = "Could not unpack " + name;

            return out;
        }

        out.program = Catalog::program(where, port.program, port.dos);
    } else {
        const std::filesystem::path target = where / name;

        std::filesystem::remove(target, code);
        std::filesystem::copy_file(archive, target,
                                   std::filesystem::copy_options::overwrite_existing, code);

        if (code) {
            out.trouble = "Could not put it in place";

            return out;
        }

        out.program = target;
    }

    // A zip packed on Windows carries no permission bits.
    if (!out.program.empty()) {
        std::filesystem::permissions(out.program,
                                     std::filesystem::perms::owner_exec
                                     | std::filesystem::perms::group_exec
                                     | std::filesystem::perms::others_exec,
                                     std::filesystem::perm_options::add, code);
    }

    return out;
}

std::string stampedVersion(const Catalog::Port &port) {
    const std::filesystem::path where = Catalog::directory(port);
    std::string version;

    if (where.empty()) {
        return {};
    }

    if (std::ifstream stamp(where / STAMP); stamp) {
        std::getline(stamp, version);
    }

    return Text::trim(version);
}

void stamp(const Catalog::Port &port, const std::string &version) {
    const std::filesystem::path where = Catalog::directory(port);

    if (where.empty()) {
        return;
    }

    if (std::ofstream stamp(where / STAMP, std::ios::trunc); stamp) {
        stamp << version;
    }
}

long long shelfBytes() {
    const std::filesystem::path shelf = Catalog::downloads();
    std::error_code code;
    long long held = 0;

    for (std::filesystem::directory_iterator walk(shelf, code), end;
         walk != end && !code; walk.increment(code)) {
        std::error_code asked;

        if (walk->is_regular_file(asked)) {
            held += static_cast<long long>(walk->file_size(asked));
        }
    }

    return held;
}

void clearShelf() {
    const std::filesystem::path shelf = Catalog::downloads();
    std::error_code code;

    for (std::filesystem::directory_iterator walk(shelf, code), end;
         walk != end && !code; walk.increment(code)) {
        std::error_code asked;

        std::filesystem::remove_all(walk->path(), asked);
    }
}

}
