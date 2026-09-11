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
#include <string>

#include "gui/draw/Anim.h"
#include "gui/toolkit/controls/Label.h"
#include "gui/toolkit/layout/Box.h"
#include "gui/toolkit/layout/Panel.h"

namespace toolkit {

// A scrim over the window with a card in the middle of it.
//
// The card is a Panel the view fills; the sheet itself only darkens what is under
// it, catches the press that dismisses it, and keeps the rest of the window from
// answering the pointer while it is up.
class Sheet : public Widget {
public:
    Sheet();

    [[nodiscard]] Panel *card() const { return _card; }

    // What the card would like, before the window's own room is taken into account.
    double wanted = 460.0;
    double tall = 0.0;

    std::function<void()> dismissed;

    // False when the sheet dealt with the dismissal itself and means to stay up.
    virtual bool closing() { return true; }

    // Called once it is up and can reach the tree, for whatever wants the keyboard.
    virtual void opened() {}

    // Called each turn while it is up, for a sheet whose content moves under it.
    virtual void sync() {}

protected:
    // Drawn over the card's children, under the same scale while the card grows.
    virtual void paintOver(const Painter & /*painter*/) {}

    // The heading and the paragraph under it, which every sheet opens with.
    static Label *heading(Box *into, const std::string &text);
    static Label *body(Box *into, const std::string &text);

public:

    [[nodiscard]] bool open() const { return _open; }

protected:
    void setOpen(bool open);

public:

    void arrange(Typeface &type) override;

    void paint(const Painter &painter) override;

    bool press(const Pointer &at) override;
    void release(const Pointer &at) override;

    Widget *at(double x, double y) override;

    bool advance(double now) override;

private:
    Panel *_card = nullptr;

    bool _open = false;
    bool _onScrim = false;

    // The grow waits for the first layout, the first time the frame clock is in reach.
    bool _grow = false;

    Anim::Tween _grown;
};

}
