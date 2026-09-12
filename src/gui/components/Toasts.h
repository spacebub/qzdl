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

#include <functional>
#include <vector>

#include "gui/draw/Anim.h"
#include "gui/state/State.h"
#include "gui/toolkit/controls/GlyphButton.h"

namespace components {

// The toasts, newest at the bottom.
class Toasts : public toolkit::Widget {
public:
    explicit Toasts(std::function<void(int)> dismissed);

    void setMessages(const std::vector<State::Buzz> &messages);

    void arrange(Typeface &type) override;

    bool advance(double now) override;

private:
    std::function<void(int)> _dismissed;

    std::vector<int> _shown;
};

// One toast.
class Toast : public toolkit::Widget {
public:
    Toast(State::Buzz message, std::function<void()> close);

    [[nodiscard]] int id() const { return _message.id; }

    double naturalHeight(Typeface &type, double width) override;

    void arrange(Typeface &type) override;

    void paint(const toolkit::Painter &painter) override;

    bool press(const toolkit::Pointer &at) override { return holds(at.x, at.y); }

    void enter() override;
    void leave() override;

    bool advance(double now) override;

    // Starts the fade out; the stack drops it when it finishes.
    void close();

protected:
    void moved() override;

private:
    [[nodiscard]] BLRgba32 tone() const;
    [[nodiscard]] BLRgba32 wash() const;

    static constexpr double WIDTH = 392.0;

    State::Buzz _message;
    std::function<void()> _close;

    toolkit::GlyphButton *_shut = nullptr;

    double _left = 0.0;
    double _ticked = 0.0;

    bool _going = false;
    bool _started = false;

    Anim::Tween _here;
};

}
