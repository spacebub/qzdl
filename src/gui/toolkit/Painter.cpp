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
#include <vector>

#include "gui/draw/Typeface.h"
#include "gui/toolkit/Painter.h"

namespace toolkit {

std::vector<Fold> foldSpans(Typeface &type, const BLFont &font, const std::string_view run,
                            const double room) {
    std::vector<Fold> lines;
    std::string line;

    // Where the text held in `line` starts in `run`, and where it reaches to.
    size_t from = 0;
    size_t to = 0;

    const auto flush = [&] {
        lines.push_back(Fold{.text = line, .from = from, .to = to});
        line.clear();
    };

    size_t at = 0;

    while (at <= run.size()) {
        const size_t space = run.find_first_of(" \n", at);
        const std::string_view word = run.substr(at, space == std::string_view::npos
            ? std::string_view::npos : space - at);

        if (line.empty()) {
            from = at;
        }

        std::string candidate = line.empty() ? std::string(word) : line + ' ' + std::string(word);

        if (!line.empty() && type.width(font, candidate) > room) {
            flush();
            candidate = std::string(word);
            from = at;
        }

        // A single word still too wide is cut at the last character that fits.
        while (type.width(font, candidate) > room && candidate.size() > 1) {
            size_t cut = candidate.size() - 1;

            while (cut > 1 && (static_cast<unsigned char>(candidate[cut]) & 0xc0) == 0x80) {
                --cut;
            }

            line = candidate.substr(0, cut);
            candidate = candidate.substr(cut);
            to = from + cut;

            flush();

            from = to;
        }

        line = candidate;
        to = at + word.size();

        if (space == std::string_view::npos) {
            break;
        }

        if (run[space] == '\n') {
            flush();
        }

        at = space + 1;
    }

    if (!line.empty() || lines.empty()) {
        lines.push_back(Fold{.text = line, .from = from, .to = to});
    }

    return lines;
}

std::vector<std::string> fold(Typeface &type, const BLFont &font, const std::string_view run,
                              const double room) {
    std::vector<std::string> lines;

    for (Fold &line : foldSpans(type, font, run, room)) {
        lines.push_back(std::move(line.text));
    }

    return lines;
}

double wrapHeight(Typeface &type, const BLFont &font, const std::string_view run,
                  const double room) {
    if (run.empty()) {
        return 0.0;
    }

    return static_cast<double>(fold(type, font, run, room).size()) * type.lineHeight(font);
}

bool Painter::needed(const BLRect &box) const {
    return box.x < _clip.x + _clip.w && box.x + box.w > _clip.x && box.y < _clip.y + _clip.h
        && box.y + box.h > _clip.y;
}

void Painter::fill(const BLRect &box, const BLRgba32 tone) const {
    if (box.w > 0.0 && box.h > 0.0 && tone.a() != 0) {
        _context.fill_rect(box, tone);
    }
}

void Painter::round(const BLRect &box, const double radius, const BLRgba32 tone) const {
    if (box.w <= 0.0 || box.h <= 0.0 || tone.a() == 0) {
        return;
    }

    const double corner = std::min(radius, std::min(box.w, box.h) / 2.0);

    if (corner <= 0.0) {
        _context.fill_rect(box, tone);

        return;
    }

    _context.fill_round_rect(box, corner, corner, tone);
}

void Painter::outline(const BLRect &box, const double radius, const double width,
                      const BLRgba32 tone) const {
    if (box.w <= width * 2.0 || box.h <= width * 2.0 || tone.a() == 0) {
        return;
    }

    const BLRect inset{box.x + (width / 2.0), box.y + (width / 2.0), box.w - width, box.h - width};
    const double corner = std::max(0.0, std::min(radius - (width / 2.0),
                                                 std::min(inset.w, inset.h) / 2.0));

    _context.set_stroke_width(width);

    if (corner <= 0.0) {
        _context.stroke_rect(inset, tone);
    } else {
        _context.stroke_round_rect(inset, corner, corner, tone);
    }
}

void Painter::circle(const BLPoint centre, const double radius, const BLRgba32 tone) const {
    if (radius > 0.0 && tone.a() != 0) {
        _context.fill_circle(centre.x, centre.y, radius, tone);
    }
}

void Painter::path(const BLPath &shape, const BLRgba32 tone) const {
    _context.fill_path(shape, tone);
}

void Painter::stroke(const BLPath &shape, const double width, const BLRgba32 tone) const {
    _context.set_stroke_width(width);
    _context.set_stroke_cap(BL_STROKE_CAP_POSITION_START, BL_STROKE_CAP_ROUND);
    _context.set_stroke_cap(BL_STROKE_CAP_POSITION_END, BL_STROKE_CAP_ROUND);
    _context.set_stroke_join(BL_STROKE_JOIN_ROUND);
    _context.stroke_path(shape, tone);
}

const BLFont &Painter::font(const int weight, const float size) const {
    return _type.at(weight, size);
}

double Painter::width(const BLFont &font, const std::string_view run) const {
    return _type.width(font, run);
}

double Painter::lineHeight(const BLFont &font) const {
    return _type.lineHeight(font);
}

std::string Painter::elide(const BLFont &font, const std::string_view run,
                           const double room) const {
    return _type.elide(font, run, static_cast<float>(room));
}

void Painter::text(const BLFont &font, const BLPoint top, const std::string_view run,
                   const BLRgba32 tone) const {
    _type.draw(_context, font, top, run, tone);
}

void Painter::label(const BLFont &font, const BLRect &box, const Align align,
                    const std::string_view run, const BLRgba32 tone) const {
    if (run.empty() || box.w <= 0.0) {
        return;
    }

    const std::string shown = _type.elide(font, run, static_cast<float>(box.w));
    const double taken = _type.width(font, shown);

    double x = box.x;

    if (align == Align::Centre) {
        x = box.x + ((box.w - taken) / 2.0);
    } else if (align == Align::End) {
        x = box.x + box.w - taken;
    }

    _type.drawCentred(_context, font, BLPoint{x, box.y}, static_cast<float>(box.h), shown, tone);
}

void Painter::tracked(const BLFont &font, const BLPoint top, const std::string_view run,
                      const BLRgba32 tone, const double spacing) const {
    _type.drawTracked(_context, font, top, run, tone, static_cast<float>(spacing));
}

double Painter::paragraph(const BLFont &font, const BLRect &box, const std::string_view run,
                          const BLRgba32 tone) const {
    const double step = _type.lineHeight(font);
    double y = box.y;

    for (const std::string &line : fold(_type, font, run, box.w)) {
        _type.draw(_context, font, BLPoint{box.x, y}, line, tone);

        y += step;
    }

    return y - box.y;
}

double Painter::wrapHeight(const BLFont &font, const std::string_view run,
                           const double room) const {
    return toolkit::wrapHeight(_type, font, run, room);
}

void Painter::push(const BLRect &box) const {
    _context.save();
    _context.clip_to_rect(box);

    _held.push_back(_clip);

    const int left = std::max(_clip.x, static_cast<int>(std::floor(box.x)));
    const int top = std::max(_clip.y, static_cast<int>(std::floor(box.y)));
    const int right = std::min(_clip.x + _clip.w, static_cast<int>(std::ceil(box.x + box.w)));
    const int bottom = std::min(_clip.y + _clip.h, static_cast<int>(std::ceil(box.y + box.h)));

    _clip = BLRectI{left, top, std::max(0, right - left), std::max(0, bottom - top)};
}

void Painter::pop() const {
    _context.restore();

    if (!_held.empty()) {
        _clip = _held.back();

        _held.pop_back();
    }
}

}
