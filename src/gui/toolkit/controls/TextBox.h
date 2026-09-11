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

#include "gui/toolkit/Widget.h"

namespace toolkit {

// The editable part of a field: one line, a caret, a selection and the clipboard.
//
// The run is a std::string, the caret a byte offset into it, and the box scrolls
// sideways so the caret stays inside.
class TextBox : public Widget {
public:
    explicit TextBox(std::function<void(const std::string &)> edited);

    [[nodiscard]] const std::string &text() const { return _text; }

    // Pushed, not bound: a set while the field has focus would fight the typing.
    void setText(std::string text);

    TextBox *placeholder(std::string text);
    TextBox *mono(bool value = true);
    TextBox *readOnly(bool value = true);

    void selectAll();

    // Return pressed.
    std::function<void()> accepted;

    // Escape pressed, for a field that closes something.
    std::function<void()> cancelled;

    double naturalWidth(Typeface &type) override;
    double naturalHeight(Typeface &type, double width) override;

    void paint(const Painter &painter) override;

    bool press(const Pointer &at) override;
    void drag(const Pointer &at) override;

    bool key(const Key &pressed) override;
    void wrote(const std::string &text) override;

    [[nodiscard]] bool takesFocus() const override { return enabled(); }
    void gainedFocus() override;
    void lostFocus() override;

    bool advance(double now) override;

private:
    // The byte offset nearest `x`.
    size_t offsetAt(Typeface &type, double x) const;

    double widthTo(Typeface &type, size_t offset) const;

    void moveTo(size_t offset, bool selecting);

    void erase(size_t from, size_t to);

    void insert(const std::string &what);

    // The selection in order, or the caret twice over.
    void span(size_t &from, size_t &to) const;

    [[nodiscard]] size_t before(size_t at) const;
    [[nodiscard]] size_t after(size_t at) const;

    // The start of the word on either side, for ctrl-arrow and ctrl-backspace.
    [[nodiscard]] size_t wordLeft(size_t at) const;
    [[nodiscard]] size_t wordRight(size_t at) const;

    void keepCaret(Typeface &type);

    std::string _text;
    std::string _placeholder;

    std::function<void(const std::string &)> _edited;

    size_t _caret = 0;
    size_t _anchor = 0;

    // How far the run is pushed left so the caret stays in view.
    double _shift = 0.0;

    bool _mono = false;
    bool _readOnly = false;

    // Blinks while focused, which is the one thing that keeps the loop awake.
    double _blinked = 0.0;
    bool _showCaret = true;
};

}
