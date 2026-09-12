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

#include <utility>

#include "gui/draw/Glyphs.h"
#include "gui/draw/Typeface.h"
#include "gui/model/Picker.h"
#include "gui/toolkit/controls/Button.h"
#include "gui/toolkit/controls/GlyphButton.h"
#include "gui/toolkit/controls/TextBox.h"
#include "gui/toolkit/controls/Toggle.h"
#include "gui/toolkit/layout/Scroll.h"
#include "gui/toolkit/overlays/Sheet.h"

class App;

namespace components {

class PickSheet : public toolkit::Sheet {
public:
    explicit PickSheet(Picker &picker);

    void sync() override;

    bool closing() override;

    void arrange(Typeface &type) override;

    [[nodiscard]] toolkit::Cursor cursorAt(double x, double y) const override;

    void hover(const toolkit::Pointer &at) override;

    void leave() override;

    bool press(const toolkit::Pointer &at) override;

    void release(const toolkit::Pointer &at) override;

protected:
    void paintOver(const toolkit::Painter &painter) override;

private:
    // A framed strip the sheet's own controls sit inside. It is a child rather than
    // something the sheet paints, so that what goes in it is drawn over it, not under.
    class Slab : public toolkit::Widget {
    public:
        BLRgba32 fill{};
        BLRgba32 edge{};
        double rounding = Theme::radiusSmall;

        void paint(const toolkit::Painter &painter) override {
            painter.round(_box, rounding, fill);
            painter.outline(_box, rounding, 1.0, edge);
        }
    };

    static std::string said(const State::PickState &pick);

    void chose();

    void paintCrumbs(const toolkit::Painter &painter);

    static void paintCrumb(const toolkit::Painter &painter, const BLRect &box, bool lit);

    // -2 for nothing, -1 for the root, otherwise the part's index.
    [[nodiscard]] int crumbAt(double x, double y) const;

    // The directory listing, drawn straight: a folder here may hold thousands.
    class Rows : public toolkit::Scroll {
    public:
        explicit Rows(PickSheet *sheet) : _sheet(sheet) {
            _takesPointer = true;
            cursor = toolkit::Cursor::Pointer;
        }

        static constexpr double ROW = 34.0;

        void arrange(Typeface & /*type*/) override {
            setReach(static_cast<double>(State::get().pick.entries.size()) * ROW);
        }

        void paint(const toolkit::Painter &painter) override {
            const Theme::Palette &palette = Theme::of();
            const State::PickState &pick = State::get().pick;

            painter.push(_box);

            const double wide = _box.w - (scrollable() ? Theme::lane : 0.0);
            double y = _box.y - offset();

            for (size_t index = 0; index < pick.entries.size(); ++index) {
                const State::DirEntry &entry = pick.entries[index];
                const BLRect line{_box.x, y, wide, ROW};

                y += ROW;

                if (!painter.needed(line)) {
                    continue;
                }

                if (entry.marked) {
                    painter.round(BLRect{line.x, line.y, line.w - 6.0, line.h},
                                  Theme::radiusSmall - 2.0, palette.accentSoft);
                } else if (std::cmp_equal(index, _over)) {
                    painter.round(BLRect{line.x, line.y, line.w - 6.0, line.h},
                                  Theme::radiusSmall - 2.0, palette.hover);
                }

                constexpr double side = 15.0 * 1.2;

                Glyphs::draw(painter.context(), entry.directory ? "file-folder" : "file",
                             BLPoint{line.x + 10.0, line.y + ((line.h - side) / 2.0)}, 1.2F,
                             entry.directory ? palette.accent : palette.faint);

                painter.label(painter.font(Typeface::pick(entry.marked ? 600 : 400, true),
                                           Theme::fontBody),
                              BLRect{line.x + 10.0 + side + 9.0, line.y, line.w - 70.0, line.h},
                              toolkit::Align::Start, entry.name,
                              entry.marked   ? palette.accent
                              : entry.hidden ? palette.muted
                                             : palette.text);

                if (entry.marked) {
                    const double tick = Glyphs::span(1.2F);

                    Glyphs::draw(painter.context(), "check",
                                 BLPoint{line.x + line.w - 16.0 - tick,
                                         line.y + ((line.h - tick) / 2.0)},
                                 1.2F, palette.accent);
                } else if (pick.folders && entry.directory) {
                    // Always shown: the only sign a folder can be taken.
                    const double plus = Glyphs::span(1.2F);
                    const bool lit = std::cmp_equal(index, _over) && _onEdge;

                    Glyphs::draw(painter.context(), "plus",
                                 BLPoint{line.x + line.w - 16.0 - plus,
                                         line.y + ((line.h - plus) / 2.0)},
                                 1.2F,
                                 Theme::alpha(lit ? palette.accent : palette.faint,
                                              lit ? 1.0 : 0.5));
                }
            }

            painter.pop();

            toolkit::Scroll::paint(painter);
        }

        void hover(const toolkit::Pointer &at) override {
            const int over = rowAt(at.y);
            const bool edge = at.x >= _box.x + _box.w - 44.0;

            if (over != _over || edge != _onEdge) {
                _over = over;
                _onEdge = edge;

                invalidate();
            }
        }

        void leave() override {
            toolkit::Widget::leave();

            _over = -1;
        }

        bool press(const toolkit::Pointer &at) override {
            return toolkit::Scroll::press(at) || holds(at.x, at.y);
        }

        void release(const toolkit::Pointer &at) override {
            toolkit::Scroll::release(at);

            const State::PickState &pick = State::get().pick;
            const int row = rowAt(at.y);

            if (row < 0 || std::cmp_greater_equal(row, pick.entries.size())) {
                return;
            }

            const State::DirEntry entry = pick.entries[static_cast<size_t>(row)];

            // The row enters, its edge takes: one click cannot mean both.
            if (pick.folders && entry.directory && at.x >= _box.x + _box.w - 44.0) {
                _sheet->_picker.mark(entry.path);

                return;
            }

            if (entry.directory) {
                _sheet->_picker.go(entry.path);
            } else if (pick.saving) {
                _sheet->_named->setText(entry.name);
                _sheet->_picker.named(entry.name);
            } else {
                _sheet->_picker.mark(entry.path);
            }
        }

    private:
        [[nodiscard]] int rowAt(const double y) const {
            const int row = static_cast<int>((y - _box.y + offset()) / ROW);

            return row >= 0 ? row : -1;
        }

        PickSheet *_sheet;

        int _over = -1;
        bool _onEdge = false;
    };

    Picker &_picker;

    Slab *_whereSlab = nullptr;
    Slab *_nameSlab = nullptr;
    Slab *_listSlab = nullptr;

    toolkit::GlyphButton *_shut = nullptr;
    toolkit::GlyphButton *_up = nullptr;
    toolkit::GlyphButton *_typer = nullptr;
    toolkit::TextBox *_typed = nullptr;
    toolkit::TextBox *_named = nullptr;
    Rows *_rows = nullptr;
    toolkit::Toggle *_hidden = nullptr;
    toolkit::Toggle *_option = nullptr;
    toolkit::Button *_use = nullptr;

    BLRect _where{};
    BLRect _crumbs{};
    BLRect _naming{};
    BLRect _panel{};

    std::vector<BLRect> _crumbBoxes;

    // -2 for nothing, -1 for the root, otherwise the part under the pointer.
    int _overCrumb = -2;

    // The directory the list is showing, so only a change to it moves the view.
    std::string _shownPath;

    int _seeded = 0;
};

}
