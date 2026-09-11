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

#include "gui/draw/Anim.h"

namespace {

// CSS's cubic-bezier(0.33, 1, 0.68, 1), to within the width of a pixel:
// 1 - (1 - t)^3.
float cubicOut(const float at) {
    const float left = 1.0F - at;

    return 1.0F - (left * left * left);
}

// The usual ease-out-back constants; the overshoot is about 10%.
float backOut(const float at) {
    constexpr float over = 1.70158F;
    const float left = at - 1.0F;

    return 1.0F + ((over + 1.0F) * left * left * left) + (over * left * left);
}

}

float Anim::shape(const Curve curve, const float at) {
    if (at <= 0.0F) {
        return 0.0F;
    }

    if (at >= 1.0F) {
        return 1.0F;
    }

    switch (curve) {
        case Curve::CubicOut: return cubicOut(at);
        case Curve::BackOut:  return backOut(at);
        default:              return at;
    }
}

void Anim::Tween::set(const float value) {
    _value = value;
    _from = value;
    _to = value;
    _running = false;
}

void Anim::Tween::run(const float value, const double now, const double seconds,
                      const Curve curve) {
    if (seconds <= 0.0) {
        set(value);

        return;
    }

    _from = _value;
    _to = value;
    _start = now;
    _span = seconds;
    _curve = curve;
    _running = true;
}

void Anim::Tween::toward(const float value, const double now, const double seconds,
                         const Curve curve) {
    if (_running ? _to == value : _value == value) {
        return;
    }

    run(value, now, seconds, curve);
}

void Anim::Tween::advance(const double now) {
    if (!_running) {
        return;
    }

    const double through = (now - _start) / _span;

    if (through >= 1.0) {
        _value = _to;
        _running = false;

        return;
    }

    _value = _from + ((_to - _from) * shape(_curve, static_cast<float>(through)));
}
