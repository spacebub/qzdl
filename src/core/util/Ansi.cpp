/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2018-2019  Lcferrum
 * Copyright (C) 2023-2026  spacebub
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
#include <cstring>
#include <string>
#include <string_view>

#include "core/util/Ansi.h"

namespace {
    constexpr char ESCAPE = '\x1b';
    constexpr char BELL = '\x07';
    constexpr char DELETE = '\x7f';

    bool printable(const char c) {
        return static_cast<unsigned char>(c) >= 0x20 && c != DELETE;
    }

    // The end of the printable run starting at from. Eight bytes at a time until a
    // word holds a control byte, then one at a time within it.
    size_t run(const std::string_view text, size_t from, const bool breaks) {
        constexpr uint64_t ONES = 0x0101010101010101ULL;
        constexpr uint64_t HIGH = 0x8080808080808080ULL;

        while (from + sizeof(uint64_t) <= text.size()) {
            uint64_t word = 0;
            std::memcpy(&word, text.data() + from, sizeof(word));

            const uint64_t control = (word - (ONES * 0x20)) & ~word & HIGH;
            const uint64_t erased = word ^ (ONES * 0x7f);
            const uint64_t deletes = (erased - ONES) & ~erased & HIGH;

            if ((control | deletes) != 0) {
                break;
            }

            from += sizeof(word);
        }

        while (from < text.size()
               && (printable(text[from]) || (breaks && (text[from] == '\r' || text[from] == '\t')))) {
            from++;
        }

        return from;
    }
}

void Ansi::filter(const std::string_view chunk, std::string &out) {
    size_t at = 0;

    while (at < chunk.size()) {
        const char c = chunk[at];
        const auto code = static_cast<unsigned char>(c);
        bool consumed = true;

        switch (_mode) {
            case Mode::Text: {
                // Printable runs go over in one append, or are skipped in one step.
                const size_t end = run(chunk, at, !_aside);

                if (!_aside) {
                    out.append(chunk.data() + at, end - at);
                }

                if (end > at) {
                    at = end;

                    continue;
                }

                if (c == ESCAPE) {
                    _mode = Mode::Escape;
                } else if (c == '\n') {
                    // A saved cursor never outlives the line it was drawn on.
                    _aside = false;
                    out += c;
                } else if (c == '\r' || c == '\t') {
                    out += c;
                }

                break;
            }

            case Mode::Escape:
                _mode = Mode::Text;

                switch (c) {
                    case '[':
                        _mode = Mode::Csi;
                        break;
                    case ']':
                    case 'P':
                    case 'X':
                    case '^':
                    case '_':
                        _mode = Mode::String;
                        break;
                    case '(':
                    case ')':
                    case '*':
                    case '+':
                    case '-':
                    case '.':
                    case '/':
                    case '#':
                    case '%':
                        _mode = Mode::Charset;
                        break;
                    case '7':
                        _aside = true;
                        break;
                    case '8':
                        _aside = false;
                        break;
                    case ESCAPE:
                        _mode = Mode::Escape;
                        break;
                    default:
                        break;
                }

                break;

            case Mode::Csi:
                if (code >= 0x40 && code <= 0x7e) {
                    _mode = Mode::Text;
                } else if (code < 0x20 || code > 0x7e) {
                    // Malformed: the byte belongs to the text.
                    _mode = Mode::Text;
                    consumed = false;
                }

                break;

            case Mode::String:
                if (c == BELL) {
                    _mode = Mode::Text;
                } else if (c == ESCAPE) {
                    _mode = Mode::StringEnd;
                }

                break;

            case Mode::StringEnd:
                if (c == '\\') {
                    _mode = Mode::Text;
                } else {
                    _mode = Mode::Escape;
                    consumed = false;
                }

                break;

            case Mode::Charset:
                _mode = Mode::Text;
                break;
        }

        if (consumed) {
            at++;
        }
    }
}
