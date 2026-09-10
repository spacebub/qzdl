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

#include "gui/Convert.h"

namespace {

constexpr std::string_view REPLACEMENT = "\xEF\xBF\xBD";

// Length of the character at `at`, or 0 if invalid by Slint's rules.
size_t character(const std::string_view value, const size_t at) {
    const auto byte = [value](const size_t index) {
        return static_cast<unsigned char>(value[index]);
    };

    const unsigned char lead = byte(at);

    if (lead == 0x00) {
        return 0;
    }

    if (lead < 0x80) {
        return 1;
    }

    size_t length = 0;
    unsigned char low = 0x80;
    unsigned char high = 0xBF;

    if (lead >= 0xC2 && lead <= 0xDF) {
        length = 2;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
        length = 3;

        if (lead == 0xE0) {
            low = 0xA0;
        } else if (lead == 0xED) {
            high = 0x9F;
        }
    } else if (lead >= 0xF0 && lead <= 0xF4) {
        length = 4;

        if (lead == 0xF0) {
            low = 0x90;
        } else if (lead == 0xF4) {
            high = 0x8F;
        }
    } else {
        return 0;
    }

    if (value.size() - at < length) {
        return 0;
    }

    if (byte(at + 1) < low || byte(at + 1) > high) {
        return 0;
    }

    for (size_t index = 2; index < length; index++) {
        if (byte(at + index) < 0x80 || byte(at + index) > 0xBF) {
            return 0;
        }
    }

    return length;
}

bool whole(const std::string_view value) {
    for (size_t at = 0; at < value.size();) {
        const size_t length = character(value, at);

        if (length == 0) {
            return false;
        }

        at += length;
    }

    return true;
}

std::string repair(const std::string_view value) {
    std::string out;

    out.reserve(value.size());

    for (size_t at = 0; at < value.size();) {
        const size_t length = character(value, at);

        if (length == 0) {
            out.append(REPLACEMENT);
            at++;

            continue;
        }

        out.append(value.substr(at, length));
        at += length;
    }

    return out;
}

}

namespace Convert {

slint::SharedString text(const std::string_view value) {
    return whole(value) ? slint::SharedString(value) : slint::SharedString(repair(value));
}

}
