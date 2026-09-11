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
#include <functional>
#include <string>

#include "gui/draw/Anim.h"
#include "gui/toolkit/Widget.h"

namespace toolkit {

class Button : public Widget {
public:
    enum class Kind : std::uint8_t {
        Default,
        Primary,
        Danger,
        Ghost,
    };

    Button(std::string text, std::function<void()> clicked);

    void setText(std::string text);

    Button *kind(Kind value);
    Button *glyph(std::string name);
    Button *compact(bool value = true);
    Button *busy(bool value);
    Button *tip(std::string text);

    // Never takes a row's spare width.
    double naturalWidth(Typeface &type) override;
    double naturalHeight(Typeface &type, double width) override;

    void paint(const Painter &painter) override;

    bool press(const Pointer &at) override;
    void release(const Pointer &at) override;
    void enter() override;
    void leave() override;

    [[nodiscard]] bool takesFocus() const override { return enabled(); }
    bool key(const Key &pressed) override;

    bool advance(double now) override;

private:
    [[nodiscard]] BLRgba32 ink() const;

    std::string _text;
    std::string _glyph;
    std::function<void()> _clicked;

    Kind _kind = Kind::Default;
    bool _compact = false;
    bool _busy = false;

    Anim::Tween _lit;
    Anim::Tween _give;

    // The three dots, while busy.
    int _tick = 0;
    double _ticked = 0.0;
};

}
