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

#include <bit>
#include <cstdint>
#include <string>
#include <string_view>

#ifdef __AVX2__
#include <immintrin.h>
#define QZDL_ANSI_AVX2
#define QZDL_ANSI_SIMD
#elif defined(__SSE2__) || defined(_M_X64)
#include <emmintrin.h>
#define QZDL_ANSI_SIMD
#endif

#include "core/util/Ansi.h"

namespace {
    constexpr char ESCAPE = '\x1b';
    constexpr char BELL = '\x07';
    constexpr char DELETE = '\x7f';

    bool printable(const char c) {
        return static_cast<unsigned char>(c) >= 0x20 && c != DELETE;
    }

#ifdef QZDL_ANSI_SIMD
    // The scan goes a vector at a time: thirty two bytes under AVX2 and sixteen
    // under SSE2, with the bits of a byte comparison read as a mask.
    struct Bytes {
#ifdef QZDL_ANSI_AVX2
        using V = __m256i;

        static constexpr size_t WIDE = 32;

        static V load(const char *at) { return _mm256_loadu_si256(reinterpret_cast<const V *>(at)); }
        static V set1(const char value) { return _mm256_set1_epi8(value); }
        static V atMost(const V v, const V limit) { return _mm256_cmpeq_epi8(_mm256_max_epu8(v, limit), limit); }
        static V equal(const V a, const V b) { return _mm256_cmpeq_epi8(a, b); }
        static V either(const V a, const V b) { return _mm256_or_si256(a, b); }
        static uint32_t bits(const V v) { return static_cast<uint32_t>(_mm256_movemask_epi8(v)); }
#else
        using V = __m128i;

        static constexpr size_t WIDE = 16;

        static V load(const char *at) { return _mm_loadu_si128(reinterpret_cast<const V *>(at)); }
        static V set1(const char value) { return _mm_set1_epi8(value); }
        static V atMost(const V v, const V limit) { return _mm_cmpeq_epi8(_mm_max_epu8(v, limit), limit); }
        static V equal(const V a, const V b) { return _mm_cmpeq_epi8(a, b); }
        static V either(const V a, const V b) { return _mm_or_si128(a, b); }
        static uint32_t bits(const V v) { return static_cast<uint32_t>(_mm_movemask_epi8(v)); }
#endif
    };
#endif

    // The first control byte at or after from, or the end. A byte at most 0x1f is
    // control and so is delete, and anything above is printable, including every
    // byte of a UTF-8 sequence.
    size_t control(const std::string_view text, size_t from) {
        const size_t size = text.size();

#ifdef QZDL_ANSI_SIMD
        const Bytes::V floor = Bytes::set1(0x1f);
        const Bytes::V erased = Bytes::set1(DELETE);

        for (; from + Bytes::WIDE <= size; from += Bytes::WIDE) {
            const Bytes::V v = Bytes::load(text.data() + from);
            const uint32_t found = Bytes::bits(Bytes::either(Bytes::atMost(v, floor), Bytes::equal(v, erased)));

            if (found != 0) {
                return from + static_cast<size_t>(std::countr_zero(found));
            }
        }

        // The rest through one vector over the end, its bits shifted past what the
        // loop already saw.
        if (from < size && size >= Bytes::WIDE) {
            const size_t start = size - Bytes::WIDE;
            const Bytes::V v = Bytes::load(text.data() + start);
            const uint32_t found = Bytes::bits(Bytes::either(Bytes::atMost(v, floor), Bytes::equal(v, erased)))
                >> (from - start);

            return found != 0 ? from + static_cast<size_t>(std::countr_zero(found)) : size;
        }
#endif

        for (; from < size && printable(text[from]); ++from) {
        }

        return from;
    }

    // The end of the printable run starting at from, with a carriage return or a
    // tab let through when breaks says so.
    size_t run(const std::string_view text, size_t from, const bool breaks) {
        while (from < text.size()) {
            const size_t stop = control(text, from);

            if (stop == text.size() || !breaks || (text[stop] != '\r' && text[stop] != '\t')) {
                return stop;
            }

            from = stop + 1;
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
