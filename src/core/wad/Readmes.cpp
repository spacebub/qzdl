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
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "ttk/system/Text.h"

#include "core/wad/Readmes.h"
#include "external/miniz/miniz.h"

using namespace ttk;

namespace fs = std::filesystem;

namespace {

// No readme is this big, and folding more text slows every layout.
constexpr std::uintmax_t LARGEST = 1U << 20U;

// Text lumps an engine reads from the top of a pk3, by the name before the first dot.
constexpr std::array<std::string_view, 54> LUMPS = {
        "althudcf", "animated", "animdefs", "cvarinfo", "decaldef", "decorate", "defbinds", "defcvars", "dehacked",
        "dmapinfo", "dmxgus",   "doomdefs", "edfroot",  "emapinfo", "esndinfo", "esndseq",  "fontdefs", "gameinfo",
        "gldefs",   "hexndefs", "hirestex", "hticdefs", "iwadinfo", "keyconf",  "language", "loadacs",  "lockdefs",
        "mapinfo",  "menudef",  "modeldef", "musinfo",  "palvers",  "reverbs",  "rmapinfo", "s_skin",   "sbarinfo",
        "secrets",  "skininfo", "sndinfo",  "sndseq",   "strfdefs", "switches", "terrain",  "textcolo", "textures",
        "trnslate", "umapinfo", "voxeldef", "xhairs",   "zmapinfo", "zscript",  "dehsupp",  "teaminfo", "x11r6rgb",
};

// Bytes 0x80 to 0xFF.
constexpr std::u16string_view CP437 = u"ÇüéâäàåçêëèïîìÄÅÉæÆôöòûùÿÖÜ¢£¥₧ƒáíóúñÑªº¿⌐¬½¼¡«»"
                                      u"░▒▓│┤╡╢╖╕╣║╗╝╜╛┐└┴┬├─┼╞╟╚╔╩╦╠═╬╧╨╤╥╙╘╒╓╫╪┘┌█▄▌▐▀"
                                      u"αßΓπΣσµτΦΘΩδ∞φε∩≡±≥≤⌠⌡÷≈°∙·√ⁿ²■\u00A0";

// Bytes 0x80 to 0x9F. The rest of Windows-1252 is Latin-1.
constexpr std::u16string_view CP1252 = u"€\uFFFD‚ƒ„…†‡ˆ‰Š‹Œ\uFFFDŽ\uFFFD\uFFFD‘’“”•–—˜™š›œ\uFFFDžŸ";

static_assert(CP437.size() == 128 && CP1252.size() == 32);

bool textName(const std::string_view name) {
    const std::string lowered = Text::lower(name);

    return lowered.starts_with("readme") || lowered.ends_with(".txt") || lowered.ends_with(".md")
           || lowered.ends_with(".nfo");
}

bool lump(const std::string_view name) {
    return std::ranges::find(LUMPS, Text::lower(name.substr(0, name.find('.')))) != LUMPS.end();
}

bool validUtf8(const std::string_view bytes) {
    for (size_t at = 0; at < bytes.size();) {
        const auto lead = static_cast<unsigned char>(bytes[at]);
        const size_t length = lead < 0x80            ? 1
                              : (lead >> 5U) == 0x6  ? 2
                              : (lead >> 4U) == 0xE  ? 3
                              : (lead >> 3U) == 0x1E ? 4
                                                     : 0;

        if (length == 0 || at + length > bytes.size()) {
            return false;
        }

        for (size_t next = 1; next < length; ++next) {
            if ((static_cast<unsigned char>(bytes[at + next]) >> 6U) != 0x2) {
                return false;
            }
        }

        at += length;
    }

    return true;
}

void put(std::string &out, const unsigned code) {
    if (code < 0x80U) {
        out += static_cast<char>(code);
    } else if (code < 0x800U) {
        out += static_cast<char>(0xC0U | (code >> 6U));
        out += static_cast<char>(0x80U | (code & 0x3FU));
    } else {
        out += static_cast<char>(0xE0U | (code >> 12U));
        out += static_cast<char>(0x80U | ((code >> 6U) & 0x3FU));
        out += static_cast<char>(0x80U | (code & 0x3FU));
    }
}

// DOS readmes lean on the box drawing block, which in Windows-1252 is mostly capital accents.
std::string decoded(const std::string_view bytes) {
    size_t high = 0;
    size_t boxes = 0;

    for (const char each : bytes) {
        const auto byte = static_cast<unsigned char>(each);

        high += byte >= 0x80 ? 1 : 0;
        boxes += byte >= 0xB0 && byte <= 0xDF ? 1 : 0;
    }

    const bool dos = boxes * 2 > high;
    std::string out;

    out.reserve(bytes.size() + high);

    for (const char each : bytes) {
        const auto byte = static_cast<unsigned char>(each);

        put(out,
            byte < 0x80   ? static_cast<char16_t>(byte)
            : dos         ? CP437[byte - 0x80]
            : byte < 0xA0 ? CP1252[byte - 0x80]
                          : static_cast<char16_t>(byte));
    }

    return out;
}

std::string fileBytes(const fs::path &path) {
    std::ifstream in(path, std::ios::binary);

    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void loose(const fs::path &directory, const fs::path &addon, std::vector<Readmes::File> &out) {
    std::error_code error;

    for (const fs::directory_entry &entry : fs::directory_iterator(directory, error)) {
        const fs::path &path = entry.path();
        const std::string name = path.filename().string();

        if (path == addon || !textName(name)
            || (addon.empty() ? lump(name) : !Text::iequals(path.stem().string(), addon.stem().string()))
            || !entry.is_regular_file(error) || entry.file_size(error) > LARGEST) {
            continue;
        }

        out.push_back({.name = name, .inside = false, .body = Readmes::readable(fileBytes(path))});
    }
}

void archived(const fs::path &addon, std::vector<Readmes::File> &out) {
    mz_zip_archive zip{};

    if (mz_zip_reader_init_file(&zip, addon.string().c_str(), 0) == 0) {
        return;
    }

    for (mz_uint at = 0; at < mz_zip_reader_get_num_files(&zip); ++at) {
        mz_zip_archive_file_stat stat;

        if (mz_zip_reader_file_stat(&zip, at, &stat) == 0 || stat.m_is_directory != 0 || stat.m_uncomp_size > LARGEST) {
            continue;
        }

        const std::string_view path = stat.m_filename;
        const size_t slash = path.find_last_of('/');
        const std::string_view name = slash == std::string_view::npos ? path : path.substr(slash + 1);
        const bool top = slash == std::string_view::npos;

        if (!textName(name) || (top ? lump(name) : !Text::lower(name).starts_with("readme"))) {
            continue;
        }

        size_t size = 0;
        void *data = mz_zip_reader_extract_to_heap(&zip, at, &size, 0);

        if (data != nullptr) {
            out.push_back({
                    .name = std::string(path),
                    .inside = true,
                    .body = Readmes::readable({static_cast<const char *>(data), size}),
            });
            mz_free(data);
        }
    }

    mz_zip_reader_end(&zip);
}

}

namespace Readmes {

std::vector<File> of(const fs::path &addon) {
    std::vector<File> found;
    std::error_code error;

    if (fs::is_directory(addon, error)) {
        loose(addon, {}, found);
    } else {
        loose(addon.parent_path().empty() ? "." : addon.parent_path(), addon, found);
        archived(addon, found);
    }

    std::ranges::stable_sort(found, [](const File &left, const File &right) {
        return left.inside != right.inside ? right.inside
                                           : Text::natural_less(Text::lower(left.name), Text::lower(right.name));
    });

    return found;
}

std::string readable(std::string_view bytes) {
    if (bytes.starts_with("\xEF\xBB\xBF")) {
        bytes.remove_prefix(3);
    }

    const std::string text = validUtf8(bytes) ? std::string(bytes) : decoded(bytes);
    std::string out;
    size_t column = 0;

    out.reserve(text.size());

    for (size_t at = 0; at < text.size(); ++at) {
        const char each = text[at];

        if (each == '\r' || each == '\n') {
            if (each == '\r' && at + 1 < text.size() && text[at + 1] == '\n') {
                ++at;
            }

            out += '\n';
            column = 0;
        } else if (each == '\t') {
            out.append(8 - (column % 8), ' ');
            column += 8 - (column % 8);
        } else if (static_cast<unsigned char>(each) >= 0x20 && each != 0x7F) {
            out += each;
            column += (static_cast<unsigned char>(each) >> 6U) == 0x2 ? 0 : 1;
        }
    }

    return out;
}

}
