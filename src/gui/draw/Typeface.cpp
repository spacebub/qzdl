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

#include "gui/draw/Typeface.h"
#include "gui/util/Cache.h"

namespace {

// Candidate faces per weight, in order of preference. The first that loads wins.
//
// There is no font enumeration here and Blend2D offers none: it reads a file and
// that is the whole of it. On Windows that is easy, and everywhere else it means
// guessing at paths, because the alternative is linking fontconfig. The lists
// below cover Debian, Ubuntu, Arch and Fedora layouts.
struct Face {
    int weight;
    const char *path;
};

const Face FACES[] = {
#ifdef _WIN32
    {Typeface::regular, "C:/Windows/Fonts/segoeui.ttf"},
    {Typeface::semibold, "C:/Windows/Fonts/seguisb.ttf"},
    {Typeface::bold, "C:/Windows/Fonts/segoeuib.ttf"},
    {Typeface::mono, "C:/Windows/Fonts/consola.ttf"},
    {Typeface::monoBold, "C:/Windows/Fonts/consolab.ttf"},
#elifdef __APPLE__
    {Typeface::regular, "/System/Library/Fonts/SFNS.ttf"},
    {Typeface::regular, "/System/Library/Fonts/Helvetica.ttc"},
    {Typeface::bold, "/System/Library/Fonts/SFNS.ttf"},
    {Typeface::mono, "/System/Library/Fonts/Menlo.ttc"},
    {Typeface::mono, "/System/Library/Fonts/Monaco.ttf"},
#else
    {.weight = Typeface::regular, .path = "/usr/share/fonts/noto/NotoSans-Regular.ttf"},
    {.weight = Typeface::regular, .path = "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf"},
    {.weight = Typeface::regular, .path = "/usr/share/fonts/cantarell/Cantarell-Regular.otf"},
    {.weight = Typeface::regular, .path = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"},
    {.weight = Typeface::regular, .path = "/usr/share/fonts/TTF/DejaVuSans.ttf"},
    {.weight = Typeface::regular, .path = "/usr/share/fonts/dejavu/DejaVuSans.ttf"},
    {.weight = Typeface::regular, .path = "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"},
    {.weight = Typeface::regular, .path = "/usr/share/fonts/liberation-sans/LiberationSans-Regular.ttf"},

    // Nothing here ships a true semibold, so the bold face stands in for both.
    {.weight = Typeface::semibold, .path = "/usr/share/fonts/noto/NotoSans-SemiBold.ttf"},
    {.weight = Typeface::semibold, .path = "/usr/share/fonts/truetype/noto/NotoSans-SemiBold.ttf"},
    {.weight = Typeface::semibold, .path = "/usr/share/fonts/noto/NotoSans-Bold.ttf"},
    {.weight = Typeface::semibold, .path = "/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf"},
    {.weight = Typeface::semibold, .path = "/usr/share/fonts/cantarell/Cantarell-Bold.otf"},
    {.weight = Typeface::semibold, .path = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"},
    {.weight = Typeface::semibold, .path = "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf"},
    {.weight = Typeface::semibold, .path = "/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf"},
    {.weight = Typeface::semibold, .path = "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf"},
    {.weight = Typeface::semibold, .path = "/usr/share/fonts/liberation-sans/LiberationSans-Bold.ttf"},

    {.weight = Typeface::bold, .path = "/usr/share/fonts/noto/NotoSans-Bold.ttf"},
    {.weight = Typeface::bold, .path = "/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf"},
    {.weight = Typeface::bold, .path = "/usr/share/fonts/cantarell/Cantarell-Bold.otf"},
    {.weight = Typeface::bold, .path = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"},
    {.weight = Typeface::bold, .path = "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf"},
    {.weight = Typeface::bold, .path = "/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf"},
    {.weight = Typeface::bold, .path = "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf"},
    {.weight = Typeface::bold, .path = "/usr/share/fonts/liberation-sans/LiberationSans-Bold.ttf"},

    {.weight = Typeface::mono, .path = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"},
    {.weight = Typeface::mono, .path = "/usr/share/fonts/TTF/DejaVuSansMono.ttf"},
    {.weight = Typeface::mono, .path = "/usr/share/fonts/dejavu/DejaVuSansMono.ttf"},
    {.weight = Typeface::mono, .path = "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf"},
    {.weight = Typeface::mono, .path = "/usr/share/fonts/liberation-mono/LiberationMono-Regular.ttf"},
    {.weight = Typeface::mono, .path = "/usr/share/fonts/noto/NotoSansMono-Regular.ttf"},
    {.weight = Typeface::mono, .path = "/usr/share/fonts/truetype/noto/NotoSansMono-Regular.ttf"},

    {.weight = Typeface::monoBold, .path = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf"},
    {.weight = Typeface::monoBold, .path = "/usr/share/fonts/TTF/DejaVuSansMono-Bold.ttf"},
    {.weight = Typeface::monoBold, .path = "/usr/share/fonts/dejavu/DejaVuSansMono-Bold.ttf"},
    {.weight = Typeface::monoBold, .path = "/usr/share/fonts/truetype/liberation/LiberationMono-Bold.ttf"},
    {.weight = Typeface::monoBold, .path = "/usr/share/fonts/liberation-mono/LiberationMono-Bold.ttf"},
    {.weight = Typeface::monoBold, .path = "/usr/share/fonts/noto/NotoSansMono-Bold.ttf"},
    {.weight = Typeface::monoBold, .path = "/usr/share/fonts/truetype/noto/NotoSansMono-Bold.ttf"},
#endif
};

// A key for the font cache: the size to a quarter of a pixel is finer than
// anything here asks for.
// The byte length of the UTF-8 character starting at `at`.
size_t step(const std::string_view run, const size_t at) {
    const auto lead = static_cast<unsigned char>(run[at]);
    const size_t wide = lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;

    return std::min(wide, run.size() - at);
}

// How many runs and answers are kept before the coldest quarter goes.
constexpr size_t SHAPED = 4096;
constexpr size_t ELIDED = 4096;

long long key(const int weight, const float size) {
    return (static_cast<long long>(weight) * 100000)
           + static_cast<long long>(std::lround(size * 4.0F));
}

}

bool Typeface::load() {
    for (const Face &face : FACES) {
        // The first candidate for a weight that loads is the one kept.
        if (_faces.contains(face.weight)) {
            continue;
        }

        BLFontFace loaded;

        if (loaded.create_from_file(face.path) != BL_SUCCESS) {
            continue;
        }

        _faces[face.weight] = loaded;
    }

    if (_faces.empty()) {
        return false;
    }

    // A weight with nothing of its own falls back to one that did load, so a
    // machine carrying a single face still draws every label.
    for (const int weight : {regular, semibold, bold, mono, monoBold}) {
        if (!_faces.contains(weight)) {
            _faces[weight] = _faces.begin()->second;
        }
    }

    return true;
}

int Typeface::pick(const int weight, const bool fixed) {
    if (!fixed) {
        return weight;
    }

    return weight >= semibold ? monoBold : mono;
}

const BLFont &Typeface::at(const int weight, const float size) {
    const long long at = key(weight, size);
    const auto found = _fonts.find(at);

    if (found != _fonts.end()) {
        return found->second;
    }

    BLFont font;

    font.create_from_face(_faces[weight], size);

    return _fonts.emplace(at, std::move(font)).first->second;
}

Typeface::Shaped &Typeface::shaped(const BLFont &font, const std::string_view run) {
    if (_maskBytes >= (32U << 20U)) {
        _shaped.clear();
        _maskBytes = 0;
    } else {
        Cache::evictOldest(_shaped, SHAPED, [this](const Shaped &gone) {
            _maskBytes -= std::min(_maskBytes, static_cast<size_t>(gone.mask.width())
                                                   * static_cast<size_t>(gone.mask.height()));
        });
    }

    const auto at = reinterpret_cast<std::uintptr_t>(&font);
    std::string key;

    key.reserve(sizeof(at) + run.size());
    key.append(reinterpret_cast<const char *>(&at), sizeof(at));
    key.append(run);

    const auto [held, fresh] = _shaped.try_emplace(std::move(key));

    held->second.used = ++_asked;

    if (fresh) {
        Shaped &made = held->second;
        BLTextMetrics metrics{};

        made.buffer.set_utf8_text(run.data(), run.size());
        font.shape(made.buffer);

        if (font.get_text_metrics(made.buffer, metrics) == BL_SUCCESS) {
            made.width = static_cast<float>(metrics.advance.x);
            made.ink = metrics.bounding_box;
        }
    }

    return held->second;
}

namespace {

// Blend2D fills a mask only where it lands square on the pixels.
bool maskable(const BLContext &context, const BLPoint origin) {
    const BLMatrix2D &at = context.final_transform();

    if (at.type() > BL_TRANSFORM_TYPE_TRANSLATE) {
        return false;
    }

    const double x = std::lround(origin.x) + at.m20;
    const double y = std::lround(origin.y) + at.m21;

    return x == std::floor(x) && y == std::floor(y);
}

}

void Typeface::lay(BLContext &context, const BLFont &font, Shaped &made, const BLPoint origin,
                   const BLRgba32 tone) {
    if (!maskable(context, origin)) {
        context.fill_glyph_run(origin, font, made.buffer.glyph_run(), tone);

        return;
    }

    if (!made.masked) {
        made.masked = true;

        // The metrics box is only ever sideways; up and down come from the face,
        // whichever of its ascent and its tallest glyph reaches further. A pixel of
        // room each way for the antialiasing to fall into.
        const BLFontMetrics face = font.metrics();
        const double rise = std::max({static_cast<double>(face.ascent),
                                      std::abs(static_cast<double>(face.y_min)),
                                      std::abs(static_cast<double>(face.y_max))});
        const double fall = std::max({static_cast<double>(face.descent),
                                      std::abs(static_cast<double>(face.y_min)),
                                      std::abs(static_cast<double>(face.y_max))});

        const int left = static_cast<int>(std::floor(made.ink.x0)) - 1;
        const int top = -static_cast<int>(std::ceil(rise)) - 1;
        const int wide = static_cast<int>(std::ceil(made.ink.x1)) - left + 1;
        const int tall = static_cast<int>(std::ceil(fall)) - top + 1;

        if (wide > 0 && tall > 0 && made.mask.create(wide, tall, BL_FORMAT_A8) == BL_SUCCESS) {
            BLContext into(made.mask);

            into.clear_all();
            into.fill_glyph_run(BLPoint{static_cast<double>(-left), static_cast<double>(-top)},
                                font, made.buffer.glyph_run(), BLRgba32(0xffffffff));
            into.end();

            made.maskAt = BLPointI{left, top};
            _maskBytes += static_cast<size_t>(wide) * static_cast<size_t>(tall);
        }
    }

    if (made.mask.is_empty()) {
        return;
    }

    // Whole pixels: the mask was rendered against an origin on one, and a run a
    // fraction over lands blurred rather than a fraction over.
    context.fill_mask(BLPointI{static_cast<int>(std::lround(origin.x)) + made.maskAt.x,
                               static_cast<int>(std::lround(origin.y)) + made.maskAt.y},
                      made.mask, tone);
}

float Typeface::width(const BLFont &font, const std::string_view run) {
    return run.empty() ? 0.0F : shaped(font, run).width;
}

float Typeface::widthOnce(const BLFont &font, const std::string_view run) {
    if (run.empty()) {
        return 0.0F;
    }

    BLTextMetrics metrics{};

    _scratch.set_utf8_text(run.data(), run.size());
    font.shape(_scratch);

    return font.get_text_metrics(_scratch, metrics) == BL_SUCCESS
        ? static_cast<float>(metrics.advance.x)
        : 0.0F;
}

std::string Typeface::elide(const BLFont &font, const std::string_view run, const float room,
                            const float tracking) {
    // The answer is kept, not the candidates: a label elides to the same string every
    // paint, and putting the candidates in the shape cache would evict what is drawn.
    Cache::evictOldest(_elided, ELIDED);

    const auto face = reinterpret_cast<std::uintptr_t>(&font);
    const auto wide = static_cast<int>(std::lround(room * 4.0));
    const auto space = static_cast<int>(std::lround(tracking * 16.0));

    std::string asked;

    asked.reserve(sizeof(face) + sizeof(wide) + sizeof(space) + run.size());
    asked.append(reinterpret_cast<const char *>(&face), sizeof(face));
    asked.append(reinterpret_cast<const char *>(&wide), sizeof(wide));
    asked.append(reinterpret_cast<const char *>(&space), sizeof(space));
    asked.append(run);

    if (const auto found = _elided.find(asked); found != _elided.end()) {
        found->second.used = ++_asked;

        return found->second.text;
    }

    std::string answer = elideOnce(font, run, room, tracking);

    _elided.emplace(std::move(asked), Elided{.text = answer, .used = ++_asked});

    return answer;
}

std::string Typeface::elideOnce(const BLFont &font, const std::string_view run, const float room,
                                const float tracking) {
    // Candidates are measured but never drawn, so they are not kept: eliding a path
    // would otherwise fill the shape cache with runs nothing asks for again.
    const auto measure = [&](const std::string_view text) {
        return tracking > 0.0F ? widthTracked(font, text, tracking) : widthOnce(font, text);
    };

    if ((tracking > 0.0F ? widthTracked(font, run, tracking) : width(font, run)) <= room) {
        return std::string(run);
    }

    // Where a character may be cut. A prefix only grows wider as it lengthens, so
    // the longest one that fits is found by bisection rather than one cut at a time.
    _cuts.clear();
    _cuts.push_back(0);

    for (size_t at = 0; at < run.size(); at += step(run, at)) {
        _cuts.push_back(at + step(run, at));
    }

    std::string candidate;
    size_t low = 0;
    size_t high = _cuts.size();
    size_t best = _cuts.size();

    while (low < high) {
        const size_t mid = low + ((high - low) / 2);

        candidate.assign(run.substr(0, _cuts[mid]));
        candidate += "\xe2\x80\xa6";

        if (measure(candidate) <= room) {
            best = mid;
            low = mid + 1;
        } else {
            high = mid;
        }
    }

    if (best == _cuts.size()) {
        return {};
    }

    candidate.assign(run.substr(0, _cuts[best]));
    candidate += "\xe2\x80\xa6";

    return candidate;
}

float Typeface::widthTracked(const BLFont &font, const std::string_view run, const float tracking) {
    float total = 0.0F;

    for (size_t at = 0; at < run.size();) {
        const size_t wide = step(run, at);

        total += width(font, run.substr(at, wide)) + tracking;
        at += wide;
    }

    // The tracking after the last character is not part of the run.
    return run.empty() ? 0.0F : total - tracking;
}

void Typeface::drawTracked(BLContext &context, const BLFont &font, const BLPoint top,
                       const std::string_view run, const BLRgba32 tone, const float tracking) {
    const BLFontMetrics metrics = font.metrics();
    const double baseline = std::floor(top.y + metrics.ascent + 0.5);
    double x = top.x;

    for (size_t at = 0; at < run.size();) {
        const size_t wide = step(run, at);
        Shaped &one = shaped(font, run.substr(at, wide));

        lay(context, font, one, BLPoint{x, baseline}, tone);

        x += one.width + tracking;
        at += wide;
    }
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static) -- see the header.
float Typeface::lineHeight(const BLFont &font) const {
    const BLFontMetrics metrics = font.metrics();

    return metrics.ascent + metrics.descent;
}

void Typeface::draw(BLContext &context, const BLFont &font, const BLPoint top,
                const std::string_view run, const BLRgba32 tone) {
    if (run.empty()) {
        return;
    }

    const BLFontMetrics metrics = font.metrics();

    lay(context, font, shaped(font, run), BLPoint{top.x, std::floor(top.y + metrics.ascent + 0.5)},
        tone);
}

void Typeface::drawCentred(BLContext &context, const BLFont &font, const BLPoint top,
                       const float height, const std::string_view run, const BLRgba32 tone) {
    if (run.empty()) {
        return;
    }

    const BLFontMetrics metrics = font.metrics();
    const double baseline =
        top.y + ((static_cast<double>(height) + metrics.ascent - metrics.descent) * 0.5);

    lay(context, font, shaped(font, run), BLPoint{top.x, std::floor(baseline + 0.5)}, tone);
}
