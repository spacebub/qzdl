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

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "ttk/draw/Glyphs.h"
#include "ttk/draw/Theme.h"
#include "ttk/draw/Typeface.h"
#include "ttk/toolkit/controls/Button.h"
#include "ttk/toolkit/controls/GlyphButton.h"
#include "ttk/toolkit/controls/TextView.h"
#include "ttk/toolkit/layout/Panel.h"
#include "ttk/toolkit/layout/Scroll.h"
#include "ttk/toolkit/overlays/Dialog.h"

#include "gui/dialogs/ReadmeDialog.h"

using namespace ttk;

namespace dialogs {

class ReadmeDialog::Rows : public Scroll {
public:
    explicit Rows(ReadmeDialog *dialog) : _dialog(dialog) { _takesPointer = true; }

    static constexpr double ROW = 46.0;

    [[nodiscard]] Cursor cursor_at(const double x, const double y) const override {
        return rowAt(x, y) == LIST ? Cursor::Default : Cursor::Pointer;
    }

    void arrange(Typeface & /*type*/) override { set_reach(static_cast<double>(_dialog->_texts.size()) * ROW); }

    void hover(const Pointer &at) override { setHovered(rowAt(at.x, at.y)); }

    void leave() override {
        Widget::leave();

        setHovered(LIST);
    }

    bool press(const Pointer &at) override {
        _scrolling = Scroll::press(at);

        return true;
    }

    void release(const Pointer &at) override {
        Scroll::release(at);

        if (const size_t row = rowAt(at.x, at.y); !std::exchange(_scrolling, false) && row != LIST) {
            _dialog->show(row);
        }
    }

    void paint(const Painter &painter) override {
        const Theme::Palette &palette = Theme::palette();

        painter.push(_box);

        double y = _box.y - offset();

        for (size_t which = 0; which < _dialog->_texts.size(); ++which) {
            const Readmes::Text &text = _dialog->_texts[which];
            const BLRect line{_box.x, y, _box.w - (scrollable() ? Theme::lane : 0.0), ROW};

            y += ROW;

            if (!painter.needed(line)) {
                continue;
            }

            if (which == _hovered) {
                painter.round(line, Theme::radiusSmall, palette.hover);
            }

            painter.label(painter.font(400, Theme::fontBody),
                          BLRect{line.x + 12.0, line.y + 6.0, line.w - 24.0, 18.0},
                          Align::Start,
                          text.name,
                          palette.text);
            painter.label(painter.font(400, Theme::fontSmall),
                          BLRect{line.x + 12.0, line.y + 25.0, line.w - 24.0, 16.0},
                          Align::Start,
                          text.inside ? "Inside the add-on" : "Next to the add-on",
                          palette.faint);
        }

        painter.pop();

        Scroll::paint(painter);
    }

private:
    [[nodiscard]] size_t rowAt(const double x, const double y) const {
        if (x < _box.x || x >= _box.x + _box.w - (scrollable() ? Theme::lane : 0.0) || y < _box.y
            || y >= _box.y + _box.h) {
            return LIST;
        }

        const auto row = static_cast<size_t>((y - _box.y + offset()) / ROW);

        return row < _dialog->_texts.size() ? row : LIST;
    }

    void setHovered(const size_t row) {
        if (row != _hovered) {
            _hovered = row;
            invalidate();
        }
    }

    ReadmeDialog *_dialog;
    size_t _hovered = LIST;
    bool _scrolling = false;
};

ReadmeDialog::ReadmeDialog(std::string addon, std::vector<Readmes::Text> texts)
    : _addon(std::move(addon)), _texts(std::move(texts)) {
    wanted = 760.0;
    tall = 560.0;

    card()->fills = false;

    _shut = card()->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Close, [this] {
        if (dismissed) {
            dismissed();
        }
    }));

    _shut->size(26.0);

    _well = card()->append(std::make_unique<Panel>());

    _well->inset = true;
    _well->rounding = Theme::radius;

    _scroll = card()->append(std::make_unique<Scroll>());
    _view = static_cast<TextView *>(_scroll->hold(std::make_unique<TextView>()));

    _rows = card()->append(std::make_unique<Rows>(this));

    _back = card()->append(std::make_unique<Button>("All files", [this] { show(LIST); }));

    _close = card()->append(std::make_unique<Button>("Close", [this] {
        if (dismissed) {
            dismissed();
        }
    }));

    _close->kind(Button::Kind::Primary);

    show(_texts.size() == 1 ? 0 : LIST);
}

void ReadmeDialog::show(const size_t which) {
    _shown = which;

    const bool listing = which == LIST;

    _rows->set_visible(listing);
    _scroll->set_visible(!listing);
    _back->set_visible(!listing && _texts.size() > 1);

    if (!listing) {
        _view->set_run(_texts[which].body);
        _scroll->scroll_to(0.0);
    }

    invalidate();
}

void ReadmeDialog::arrange(Typeface &type) {
    Dialog::arrange(type);

    const BLRect box = card()->box();

    _shut->place(BLRect{box.x + box.w - 22.0 - 26.0, box.y + 22.0, 26.0, 26.0}, type);

    const BLRect panel{box.x + 22.0, box.y + 82.0, box.w - 44.0, box.h - 82.0 - 14.0 - Theme::control - 22.0};

    _well->place(panel, type);
    _scroll->place(BLRect{panel.x + 14.0, panel.y + 14.0, panel.w - 28.0, panel.h - 28.0}, type);
    _rows->place(BLRect{panel.x + 6.0, panel.y + 6.0, panel.w - 12.0, panel.h - 12.0}, type);

    const double bottom = box.y + box.h - 22.0 - Theme::control;

    _close->place(BLRect{box.x + box.w - 22.0 - Theme::buttonWidth, bottom, Theme::buttonWidth, Theme::control}, type);
    _back->place(BLRect{box.x + 22.0, bottom, Theme::buttonWidth, Theme::control}, type);
}

void ReadmeDialog::paint_over(const Painter &painter) {
    const Theme::Palette &palette = Theme::palette();
    const BLRect box = card()->box();
    const bool listing = _shown == LIST;

    painter.label(painter.font(palette.headingWeight, Theme::fontLarge),
                  BLRect{box.x + 22.0, box.y + 22.0, box.w - 44.0 - 30.0, 26.0},
                  Align::Start,
                  listing ? _addon : _texts[_shown].name,
                  palette.text);

    painter.label(painter.font(400, Theme::fontSmall),
                  BLRect{box.x + 22.0, box.y + 52.0, box.w - 44.0, 18.0},
                  Align::Start,
                  listing                 ? std::to_string(_texts.size()) + " text files"
                  : _texts[_shown].inside ? "Inside " + _addon
                                          : "Next to " + _addon,
                  palette.muted);
}

}
