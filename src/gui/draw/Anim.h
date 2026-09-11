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
#pragma once

#include <cstdint>

// A value on its way somewhere. Every animated property in the interface is one of
// these, which is also what tells the frame loop whether there is anything to
// redraw: when nothing is live, nothing is drawn and nothing is presented.
namespace Anim {

enum class Curve : std::uint8_t {
    Linear,
    // cubic-bezier(0.33, 1, 0.68, 1).
    CubicOut,
    // ease-out-back: overshoots and comes back.
    BackOut,
};

float shape(Curve curve, float at);

class Tween {
public:
    // Jumps straight there, with nothing in flight.
    void set(float value);

    // Starts a run to `value`, from wherever the tween has got to.
    void run(float value, double now, double seconds, Curve curve);

    // Same, except a run already headed there is left alone -- otherwise a hover
    // that arrives every frame would restart the animation every frame.
    void toward(float value, double now, double seconds, Curve curve);

    void advance(double now);

    [[nodiscard]] float value() const { return _value; }

    [[nodiscard]] bool live() const { return _running; }

private:
    float _value = 0.0F;
    float _from = 0.0F;
    float _to = 0.0F;

    double _start = 0.0;
    double _span = 0.0;

    Curve _curve = Curve::Linear;
    bool _running = false;
};

}
