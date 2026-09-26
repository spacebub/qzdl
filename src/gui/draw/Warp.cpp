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
#include <cmath>
#include <cstdint>
#include <cstring>

#ifdef __AVX2__
#include <immintrin.h>
#define QZDL_WARP_AVX2
#define QZDL_WARP_SSE2
#elif defined(__SSE2__) || defined(_M_X64)
#include <emmintrin.h>
#define QZDL_WARP_SSE2
#endif

#include "gui/draw/Warp.h"

namespace {

// Texel weights run to one short of this: a weight of 256 would not fit the
// lane once multiplied by a channel.
constexpr int SCALE = 256;

// The source as bytes, and where a sample may land in it. A sample is held to
// the last whole texel and its neighbour, which extends the edge the way a
// padded pattern does.
struct Texels {
    const std::uint8_t *bytes;
    std::ptrdiff_t stride;
    double lastX;
    double lastY;

    [[nodiscard]] const std::uint8_t *at(const std::ptrdiff_t offset) const {
        return bytes + offset;
    }
};

// One row of output: the source point of every pixel, then its texels. The
// source point is a ratio of two things linear in x, so the row carries three
// running sums and divides once per pixel.
struct Row {
    double sx;
    double sy;
    double sw;
    double dx;
    double dy;
    double dw;

    // The origin of the source in map coordinates, with the half taken off so
    // a texel's centre is where its weight is whole.
    double ox;
    double oy;
};

std::uint32_t read(const std::uint8_t *at) {
    std::uint32_t value = 0;

    std::memcpy(&value, at, sizeof(value));

    return value;
}

// Scalar, for the tail of a row and for builds with no vector unit.
void plain(const Texels &texels, std::uint32_t *line, const int from, const int count, const Row &row,
           const bool smooth) {
    for (int x = from; x < count; ++x) {
        const double inv = 1.0 / (row.sw + (row.dw * x));
        const double u = std::clamp(((row.sx + (row.dx * x)) * inv) - row.ox, 0.0, texels.lastX);
        const double v = std::clamp(((row.sy + (row.dy * x)) * inv) - row.oy, 0.0, texels.lastY);

        if (!smooth) {
            const auto px = static_cast<std::ptrdiff_t>(std::floor(u + 0.5));
            const auto py = static_cast<std::ptrdiff_t>(std::floor(v + 0.5));

            line[x] = read(texels.at((py * texels.stride) + (px * 4)));

            continue;
        }

        const double fx = std::floor(u);
        const double fy = std::floor(v);
        const int wx = static_cast<int>((u - fx) * SCALE);
        const int wy = static_cast<int>((v - fy) * SCALE);
        const std::uint8_t *top = texels.at((static_cast<std::ptrdiff_t>(fy) * texels.stride)
                                            + (static_cast<std::ptrdiff_t>(fx) * 4));
        const std::uint8_t *bottom = top + texels.stride;

        std::uint32_t out = 0;

        for (unsigned shift = 0; shift < 32; shift += 8) {
            const auto channel = [shift](const std::uint32_t v) { return static_cast<int>((v >> shift) & 0xffU); };

            const int above = ((channel(read(top)) * (SCALE - wx)) + (channel(read(top + 4)) * wx)) >> 8;
            const int below = ((channel(read(bottom)) * (SCALE - wx)) + (channel(read(bottom + 4)) * wx)) >> 8;
            const int mixed = ((above * (SCALE - wy)) + (below * wy)) >> 8;

            out |= static_cast<std::uint32_t>(mixed) << shift;
        }

        line[x] = out;
    }
}

#ifdef QZDL_WARP_SSE2

// A weight for each of four pixels, spread over a pair of them as one lane per
// channel: [w0 w0 w0 w0 w1 w1 w1 w1] for the first pair.
struct Spread {
    __m128i first;
    __m128i second;
};

Spread spread(const __m128i weights) {
    const __m128i packed = _mm_packs_epi32(weights, weights);
    const __m128i doubled = _mm_unpacklo_epi16(packed, packed);

    return {.first = _mm_unpacklo_epi32(doubled, doubled), .second = _mm_unpackhi_epi32(doubled, doubled)};
}

// One texel row of two pixels blended across, one channel a lane. A pixel's two
// texels sit side by side, so each comes in as one read, and premultiplied
// colour blends straight.
__m128i across(const std::uint8_t *left, const std::uint8_t *right, const __m128i keep, const __m128i take) {
    const __m128i zero = _mm_setzero_si128();
    const __m128i both = _mm_unpacklo_epi32(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(left)),
                                            _mm_loadl_epi64(reinterpret_cast<const __m128i *>(right)));
    const __m128i first = _mm_unpacklo_epi8(both, zero);
    const __m128i second = _mm_unpackhi_epi8(both, zero);

    return _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(first, keep), _mm_mullo_epi16(second, take)), 8);
}

// Two pixels through both texel rows and down, packed to bytes.
void pair(const Texels &texels, std::uint32_t *line, const std::ptrdiff_t left, const std::ptrdiff_t right,
          const __m128i wx, const __m128i wy) {
    const __m128i full = _mm_set1_epi16(SCALE);
    const __m128i cx = _mm_sub_epi16(full, wx);
    const __m128i cy = _mm_sub_epi16(full, wy);

    const __m128i top = across(texels.at(left), texels.at(right), cx, wx);
    const __m128i bottom = across(texels.at(left + texels.stride), texels.at(right + texels.stride), cx, wx);
    const __m128i out = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(top, cy), _mm_mullo_epi16(bottom, wy)), 8);

    _mm_storel_epi64(reinterpret_cast<__m128i *>(line), _mm_packus_epi16(out, out));
}

void vector(const Texels &texels, std::uint32_t *line, const int count, const Row &row, const bool smooth,
            const int from = 0) {
    const __m128 step = _mm_set_ps(3.0F, 2.0F, 1.0F, 0.0F);
    const __m128 one = _mm_set1_ps(1.0F);
    const __m128 half = _mm_set1_ps(0.5F);
    const __m128 zero = _mm_setzero_ps();
    const __m128 bias = _mm_set1_ps(1024.0F);
    const __m128i unbias = _mm_set1_epi32(1024);
    const __m128 scale = _mm_set1_ps(static_cast<float>(SCALE));
    const __m128 lastX = _mm_set1_ps(static_cast<float>(texels.lastX));
    const __m128 lastY = _mm_set1_ps(static_cast<float>(texels.lastY));
    const __m128 four = _mm_set1_ps(4.0F);
    const __m128 stride = _mm_set1_ps(static_cast<float>(texels.stride));
    const __m128 vox = _mm_set1_ps(static_cast<float>(row.ox));
    const __m128 voy = _mm_set1_ps(static_cast<float>(row.oy));

    __m128 sx = _mm_add_ps(_mm_set1_ps(static_cast<float>(row.sx + (row.dx * from))),
                           _mm_mul_ps(step, _mm_set1_ps(static_cast<float>(row.dx))));
    __m128 sy = _mm_add_ps(_mm_set1_ps(static_cast<float>(row.sy + (row.dy * from))),
                           _mm_mul_ps(step, _mm_set1_ps(static_cast<float>(row.dy))));
    __m128 sw = _mm_add_ps(_mm_set1_ps(static_cast<float>(row.sw + (row.dw * from))),
                           _mm_mul_ps(step, _mm_set1_ps(static_cast<float>(row.dw))));

    const __m128 ax = _mm_set1_ps(static_cast<float>(row.dx * 4.0));
    const __m128 ay = _mm_set1_ps(static_cast<float>(row.dy * 4.0));
    const __m128 aw = _mm_set1_ps(static_cast<float>(row.dw * 4.0));

    alignas(16) std::int32_t offsets[4];

    int x = from;

    for (; x + 4 <= count; x += 4) {
        const __m128 inv = _mm_div_ps(one, sw);
        const __m128 u = _mm_min_ps(_mm_max_ps(_mm_sub_ps(_mm_mul_ps(sx, inv), vox), zero), lastX);
        const __m128 v = _mm_min_ps(_mm_max_ps(_mm_sub_ps(_mm_mul_ps(sy, inv), voy), zero), lastY);

        sx = _mm_add_ps(sx, ax);
        sy = _mm_add_ps(sy, ay);
        sw = _mm_add_ps(sw, aw);

        if (!smooth) {
            const __m128 px = _mm_cvtepi32_ps(_mm_sub_epi32(_mm_cvttps_epi32(_mm_add_ps(_mm_add_ps(u, half), bias)), unbias));
            const __m128 py = _mm_cvtepi32_ps(_mm_sub_epi32(_mm_cvttps_epi32(_mm_add_ps(_mm_add_ps(v, half), bias)), unbias));

            _mm_store_si128(reinterpret_cast<__m128i *>(offsets),
                            _mm_cvttps_epi32(_mm_add_ps(_mm_mul_ps(py, stride), _mm_mul_ps(px, four))));

            for (int lane = 0; lane < 4; ++lane) {
                line[x + lane] = read(texels.at(offsets[lane]));
            }

            continue;
        }

        // floor, with a bias so truncation lands right for what is just under zero.
        const __m128i fx = _mm_sub_epi32(_mm_cvttps_epi32(_mm_add_ps(u, bias)), unbias);
        const __m128i fy = _mm_sub_epi32(_mm_cvttps_epi32(_mm_add_ps(v, bias)), unbias);
        const __m128 px = _mm_cvtepi32_ps(fx);
        const __m128 py = _mm_cvtepi32_ps(fy);

        const Spread wx = spread(_mm_cvttps_epi32(_mm_mul_ps(_mm_sub_ps(u, px), scale)));
        const Spread wy = spread(_mm_cvttps_epi32(_mm_mul_ps(_mm_sub_ps(v, py), scale)));

        _mm_store_si128(reinterpret_cast<__m128i *>(offsets),
                        _mm_cvttps_epi32(_mm_add_ps(_mm_mul_ps(py, stride), _mm_mul_ps(px, four))));

        pair(texels, line + x, offsets[0], offsets[1], wx.first, wy.first);
        pair(texels, line + x + 2, offsets[2], offsets[3], wx.second, wy.second);
    }

    plain(texels, line, x, count, row, smooth);
}

#endif

#ifdef QZDL_WARP_AVX2

// The pair helpers over both halves of a wide register: pixels 0 and 1 in the
// low half, 4 and 5 in the high, since the unpacks work within a half.
struct Spread8 {
    __m256i first;
    __m256i second;
};

Spread8 spread8(const __m256i weights) {
    const __m256i packed = _mm256_packs_epi32(weights, weights);
    const __m256i doubled = _mm256_unpacklo_epi16(packed, packed);

    return {.first = _mm256_unpacklo_epi32(doubled, doubled), .second = _mm256_unpackhi_epi32(doubled, doubled)};
}

__m256i across4(const Texels &texels, const std::int32_t *offsets, const std::ptrdiff_t down, const __m256i keep,
                const __m256i take) {
    const auto both = [&](const int a, const int b) {
        return _mm_unpacklo_epi32(
            _mm_loadl_epi64(reinterpret_cast<const __m128i *>(texels.at(offsets[a] + down))),
            _mm_loadl_epi64(reinterpret_cast<const __m128i *>(texels.at(offsets[b] + down))));
    };

    const __m256i zero = _mm256_setzero_si256();
    const __m256i all = _mm256_inserti128_si256(_mm256_castsi128_si256(both(0, 1)), both(4, 5), 1);
    const __m256i first = _mm256_unpacklo_epi8(all, zero);
    const __m256i second = _mm256_unpackhi_epi8(all, zero);

    return _mm256_srli_epi16(_mm256_add_epi16(_mm256_mullo_epi16(first, keep), _mm256_mullo_epi16(second, take)), 8);
}

// Pixels 0, 1, 4 and 5 of eight, with `offsets` pointing at their texels.
void quad(const Texels &texels, std::uint32_t *line, const std::int32_t *offsets, const __m256i wx,
          const __m256i wy) {
    const __m256i full = _mm256_set1_epi16(SCALE);
    const __m256i cx = _mm256_sub_epi16(full, wx);
    const __m256i cy = _mm256_sub_epi16(full, wy);

    const __m256i top = across4(texels, offsets, 0, cx, wx);
    const __m256i bottom = across4(texels, offsets, texels.stride, cx, wx);
    const __m256i out = _mm256_srli_epi16(_mm256_add_epi16(_mm256_mullo_epi16(top, cy), _mm256_mullo_epi16(bottom, wy)), 8);
    const __m256i packed = _mm256_packus_epi16(out, out);

    _mm_storel_epi64(reinterpret_cast<__m128i *>(line), _mm256_castsi256_si128(packed));
    _mm_storel_epi64(reinterpret_cast<__m128i *>(line + 4), _mm256_extracti128_si256(packed, 1));
}

void wide(const Texels &texels, std::uint32_t *line, const int count, const Row &row, const bool smooth) {
    const __m256 step = _mm256_set_ps(7.0F, 6.0F, 5.0F, 4.0F, 3.0F, 2.0F, 1.0F, 0.0F);
    const __m256 one = _mm256_set1_ps(1.0F);
    const __m256 half = _mm256_set1_ps(0.5F);
    const __m256 zero = _mm256_setzero_ps();
    const __m256 bias = _mm256_set1_ps(1024.0F);
    const __m256i unbias = _mm256_set1_epi32(1024);
    const __m256 scale = _mm256_set1_ps(static_cast<float>(SCALE));
    const __m256 lastX = _mm256_set1_ps(static_cast<float>(texels.lastX));
    const __m256 lastY = _mm256_set1_ps(static_cast<float>(texels.lastY));
    const __m256 four = _mm256_set1_ps(4.0F);
    const __m256 stride = _mm256_set1_ps(static_cast<float>(texels.stride));
    const __m256 vox = _mm256_set1_ps(static_cast<float>(row.ox));
    const __m256 voy = _mm256_set1_ps(static_cast<float>(row.oy));

    __m256 sx = _mm256_add_ps(_mm256_set1_ps(static_cast<float>(row.sx)), _mm256_mul_ps(step, _mm256_set1_ps(static_cast<float>(row.dx))));
    __m256 sy = _mm256_add_ps(_mm256_set1_ps(static_cast<float>(row.sy)), _mm256_mul_ps(step, _mm256_set1_ps(static_cast<float>(row.dy))));
    __m256 sw = _mm256_add_ps(_mm256_set1_ps(static_cast<float>(row.sw)), _mm256_mul_ps(step, _mm256_set1_ps(static_cast<float>(row.dw))));

    const __m256 ax = _mm256_set1_ps(static_cast<float>(row.dx * 8.0));
    const __m256 ay = _mm256_set1_ps(static_cast<float>(row.dy * 8.0));
    const __m256 aw = _mm256_set1_ps(static_cast<float>(row.dw * 8.0));

    alignas(32) std::int32_t offsets[8];

    int x = 0;

    for (; x + 8 <= count; x += 8) {
        const __m256 inv = _mm256_div_ps(one, sw);
        const __m256 u = _mm256_min_ps(_mm256_max_ps(_mm256_sub_ps(_mm256_mul_ps(sx, inv), vox), zero), lastX);
        const __m256 v = _mm256_min_ps(_mm256_max_ps(_mm256_sub_ps(_mm256_mul_ps(sy, inv), voy), zero), lastY);

        sx = _mm256_add_ps(sx, ax);
        sy = _mm256_add_ps(sy, ay);
        sw = _mm256_add_ps(sw, aw);

        if (!smooth) {
            const __m256 px = _mm256_cvtepi32_ps(_mm256_sub_epi32(_mm256_cvttps_epi32(_mm256_add_ps(_mm256_add_ps(u, half), bias)), unbias));
            const __m256 py = _mm256_cvtepi32_ps(_mm256_sub_epi32(_mm256_cvttps_epi32(_mm256_add_ps(_mm256_add_ps(v, half), bias)), unbias));

            _mm256_store_si256(reinterpret_cast<__m256i *>(offsets),
                               _mm256_cvttps_epi32(_mm256_add_ps(_mm256_mul_ps(py, stride), _mm256_mul_ps(px, four))));

            for (int lane = 0; lane < 8; ++lane) {
                line[x + lane] = read(texels.at(offsets[lane]));
            }

            continue;
        }

        const __m256i fx = _mm256_sub_epi32(_mm256_cvttps_epi32(_mm256_add_ps(u, bias)), unbias);
        const __m256i fy = _mm256_sub_epi32(_mm256_cvttps_epi32(_mm256_add_ps(v, bias)), unbias);
        const __m256 px = _mm256_cvtepi32_ps(fx);
        const __m256 py = _mm256_cvtepi32_ps(fy);

        const Spread8 wx = spread8(_mm256_cvttps_epi32(_mm256_mul_ps(_mm256_sub_ps(u, px), scale)));
        const Spread8 wy = spread8(_mm256_cvttps_epi32(_mm256_mul_ps(_mm256_sub_ps(v, py), scale)));

        _mm256_store_si256(reinterpret_cast<__m256i *>(offsets),
                           _mm256_cvttps_epi32(_mm256_add_ps(_mm256_mul_ps(py, stride), _mm256_mul_ps(px, four))));

        quad(texels, line + x, offsets, wx.first, wy.first);
        quad(texels, line + x + 2, offsets + 2, wx.second, wy.second);
    }

    // What is left of the row goes through the narrower kernel.
    vector(texels, line, count, row, smooth, x);
}

#endif

}

namespace Warp {

BLPoint Map::apply(const BLPoint at) const {
    const double x = (m[0][0] * at.x) + (m[0][1] * at.y) + m[0][2];
    const double y = (m[1][0] * at.x) + (m[1][1] * at.y) + m[1][2];
    const double w = (m[2][0] * at.x) + (m[2][1] * at.y) + m[2][2];

    return BLPoint{x / w, y / w};
}

Map Map::inverse() const {
    // The adjugate over the determinant. A projective map is the same up to
    // scale, so the determinant only has to be nonzero.
    const double a = m[0][0];
    const double b = m[0][1];
    const double c = m[0][2];
    const double d = m[1][0];
    const double e = m[1][1];
    const double f = m[1][2];
    const double g = m[2][0];
    const double h = m[2][1];
    const double i = m[2][2];

    const double det = (a * ((e * i) - (f * h))) - (b * ((d * i) - (f * g))) + (c * ((d * h) - (e * g)));
    const double over = det != 0.0 ? 1.0 / det : 0.0;

    return Map{{{((e * i) - (f * h)) * over, ((c * h) - (b * i)) * over, ((b * f) - (c * e)) * over},
                {((f * g) - (d * i)) * over, ((a * i) - (c * g)) * over, ((c * d) - (a * f)) * over},
                {((d * h) - (e * g)) * over, ((b * g) - (a * h)) * over, ((a * e) - (b * d)) * over}}};
}

bool render(const BLImage &source, const BLPointI origin, const Map &map, const BLRectI &area,
            const bool smooth, BLImage &out) {
    if (area.w <= 0 || area.h <= 0 || source.is_empty() || source.width() < 2 || source.height() < 2) {
        return false;
    }

    if ((out.width() < area.w || out.height() < area.h)
        && out.create(std::max(out.width(), area.w), std::max(out.height(), area.h), BL_FORMAT_PRGB32)
            != BL_SUCCESS) {
        return false;
    }

    BLImageData from{};
    BLImageData into{};

    if (source.get_data(&from) != BL_SUCCESS || out.make_mutable(&into) != BL_SUCCESS) {
        return false;
    }

    // A sample stops a hair short of the last texel, so its neighbour is always
    // the one before the edge.
    const Texels texels{
        .bytes = static_cast<const std::uint8_t *>(from.pixel_data),
        .stride = from.stride,
        .lastX = from.size.w - 1.0 - (1.0 / 1024.0),
        .lastY = from.size.h - 1.0 - (1.0 / 1024.0),
    };

    const Map back = map.inverse();
    const double left = area.x + 0.5;

    for (int y = 0; y < area.h; ++y) {
        const double py = area.y + y + 0.5;
        auto *line = reinterpret_cast<std::uint32_t *>(static_cast<std::uint8_t *>(into.pixel_data)
                                                       + (y * into.stride));

        const Row row{
            .sx = (back.m[0][0] * left) + (back.m[0][1] * py) + back.m[0][2],
            .sy = (back.m[1][0] * left) + (back.m[1][1] * py) + back.m[1][2],
            .sw = (back.m[2][0] * left) + (back.m[2][1] * py) + back.m[2][2],
            .dx = back.m[0][0],
            .dy = back.m[1][0],
            .dw = back.m[2][0],
            .ox = origin.x + 0.5,
            .oy = origin.y + 0.5,
        };

#ifdef QZDL_WARP_AVX2
        wide(texels, line, area.w, row, smooth);
#elifdef QZDL_WARP_SSE2
        vector(texels, line, area.w, row, smooth);
#else
        plain(texels, line, 0, area.w, row, smooth);
#endif
    }

    return true;
}

}
