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
#include <cstdlib>
#include <numbers>

#include "gui/draw/Svg.h"

namespace {

// A cursor over the command string. SVG lets separators be commas, whitespace or
// nothing at all where a sign makes the break unambiguous, so everything goes
// through here rather than through sscanf.
class Reader {
public:
    explicit Reader(const char *at) : _at(at) {}

    void skip() {
        while (*_at == ',' || (*_at != 0 && (std::isspace(static_cast<unsigned char>(*_at)) != 0))) {
            ++_at;
        }
    }

    bool done() {
        skip();

        return *_at == 0;
    }

    // The next character if it names a command, else a nul.
    char command() {
        skip();

        return (std::isalpha(static_cast<unsigned char>(*_at)) != 0) ? *_at++ : static_cast<char>(0);
    }

    bool number(double &out) {
        skip();

        char *end = nullptr;
        const double value = std::strtod(_at, &end);

        if (end == _at) {
            return false;
        }

        _at = end;
        out = value;

        return true;
    }

    // A flag inside an arc is a single digit, and nothing separates it from what
    // follows: "a 5 5 0 1 1 2 3" has to read 1 and 1, not 11.
    bool flag(bool &out) {
        skip();

        if (*_at != '0' && *_at != '1') {
            return false;
        }

        out = *_at++ == '1';

        return true;
    }

private:
    const char *_at;
};

struct Pen {
    BLPoint at{};

    // Where the subpath began, which is where Z returns to.
    BLPoint home{};

    // The last control point, which S and T reflect. A smooth command following
    // anything else reflects about the current point instead, so which kind of
    // curve was last is part of the state.
    BLPoint control{};
    char last = 0;
};

BLPoint reflect(const BLPoint &about, const BLPoint &control) {
    return {(about.x * 2.0) - control.x, (about.y * 2.0) - control.y};
}

}

bool Svg::parse(const char *commands, BLPath &out) {
    out.clear();

    if (commands == nullptr) {
        return false;
    }

    Reader reader(commands);
    Pen pen;

    char verb = 0;

    while (!reader.done()) {
        if (const char next = reader.command(); next != 0) {
            verb = next;
        } else if (verb == 0) {
            return false;
        } else if (verb == 'M') {
            // A second coordinate pair after a moveto is a lineto, per the grammar.
            verb = 'L';
        } else if (verb == 'm') {
            verb = 'l';
        }

        const bool relative = std::islower(static_cast<unsigned char>(verb)) != 0;
        const BLPoint from = relative ? pen.at : BLPoint{0.0, 0.0};
        const char kind = static_cast<char>(std::toupper(static_cast<unsigned char>(verb)));

        double a = 0.0;
        double b = 0.0;
        double c = 0.0;
        double d = 0.0;
        double e = 0.0;
        double f = 0.0;

        switch (kind) {
            case 'M':
                if (!reader.number(a) || !reader.number(b)) {
                    return false;
                }

                pen.at = {from.x + a, from.y + b};
                pen.home = pen.at;

                out.move_to(pen.at);

                break;

            case 'L':
                if (!reader.number(a) || !reader.number(b)) {
                    return false;
                }

                pen.at = {from.x + a, from.y + b};

                out.line_to(pen.at);

                break;

            case 'H':
                if (!reader.number(a)) {
                    return false;
                }

                pen.at = {from.x + a, pen.at.y};

                out.line_to(pen.at);

                break;

            case 'V':
                if (!reader.number(a)) {
                    return false;
                }

                pen.at = {pen.at.x, from.y + a};

                out.line_to(pen.at);

                break;

            case 'C': {
                if (!reader.number(a) || !reader.number(b) || !reader.number(c)
                    || !reader.number(d) || !reader.number(e) || !reader.number(f)) {
                    return false;
                }

                const BLPoint second{from.x + c, from.y + d};

                pen.at = {from.x + e, from.y + f};

                out.cubic_to({from.x + a, from.y + b}, second, pen.at);

                pen.control = second;

                break;
            }

            case 'S': {
                if (!reader.number(a) || !reader.number(b) || !reader.number(c)
                    || !reader.number(d)) {
                    return false;
                }

                const bool smooth = pen.last == 'C' || pen.last == 'S';
                const BLPoint first = smooth ? reflect(pen.at, pen.control) : pen.at;
                const BLPoint second{from.x + a, from.y + b};

                pen.at = {from.x + c, from.y + d};

                out.cubic_to(first, second, pen.at);

                pen.control = second;

                break;
            }

            case 'Q': {
                if (!reader.number(a) || !reader.number(b) || !reader.number(c)
                    || !reader.number(d)) {
                    return false;
                }

                const BLPoint control{from.x + a, from.y + b};

                pen.at = {from.x + c, from.y + d};

                out.quad_to(control, pen.at);

                pen.control = control;

                break;
            }

            case 'T': {
                if (!reader.number(a) || !reader.number(b)) {
                    return false;
                }

                const bool smooth = pen.last == 'Q' || pen.last == 'T';
                const BLPoint control = smooth ? reflect(pen.at, pen.control) : pen.at;

                pen.at = {from.x + a, from.y + b};

                out.quad_to(control, pen.at);

                pen.control = control;

                break;
            }

            case 'A': {
                bool large = false;
                bool sweep = false;

                if (!reader.number(a) || !reader.number(b) || !reader.number(c)
                    || !reader.flag(large) || !reader.flag(sweep)
                    || !reader.number(e) || !reader.number(f)) {
                    return false;
                }

                pen.at = {from.x + e, from.y + f};

                // SVG states the x-axis rotation in degrees; Blend2D takes radians.
                out.elliptic_arc_to({a, b}, c * std::numbers::pi / 180.0, large, sweep, pen.at);

                break;
            }

            case 'Z':
                pen.at = pen.home;

                out.close();

                break;

            default:
                return false;
        }

        pen.last = kind;
    }

    return true;
}

BLPath Svg::glyph(const char *commands, const float box, const float size) {
    BLPath path;

    if (!parse(commands, path)) {
        return path;
    }

    const double scale = static_cast<double>(size) / static_cast<double>(box);

    path.transform(BLMatrix2D::make_scaling(scale, scale));

    return path;
}
