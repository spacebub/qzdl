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

#include "gui/draw/Glyphs.h"
#include "gui/draw/Typeface.h"
#include "gui/state/State.h"
#include "gui/toolkit/controls/Button.h"
#include "gui/toolkit/layout/Scroll.h"
#include "gui/toolkit/overlays/Dialog.h"
#include "gui/util/Format.h"

namespace dialogs {

class CopyConfigDialog : public toolkit::Dialog {
public:
    explicit CopyConfigDialog(std::function<void(const std::string &)> picked);

    void sync() override;

    void arrange(Typeface &type) override;

protected:
    void paintOver(const toolkit::Painter &painter) override;

private:
    // The donors, drawn straight rather than held as widgets.
    class Rows : public toolkit::Scroll {
    public:
        explicit Rows(CopyConfigDialog *dialog) : _sheet(dialog) { _takesPointer = true; }

        static constexpr double ROW = 46.0;
        static constexpr size_t NONE = static_cast<size_t>(-1);

        // Which donor is under the point, past the bar and the empty tail.
        [[nodiscard]] size_t rowAt(const double x, const double y) const {
            const std::vector<State::ConfigDonor> &donors = State::get().cfg.configDonors;
            const double wide = _box.w - (scrollable() ? Theme::lane : 0.0);

            if (x < _box.x || x >= _box.x + wide || y < _box.y || y >= _box.y + _box.h) {
                return NONE;
            }

            const auto row = static_cast<size_t>((y - _box.y + offset()) / ROW);

            return row < donors.size() ? row : NONE;
        }

        [[nodiscard]] toolkit::Cursor cursorAt(const double x, const double y) const override {
            return rowAt(x, y) == NONE ? toolkit::Cursor::Default : toolkit::Cursor::Pointer;
        }

        void hover(const toolkit::Pointer &at) override { setHovered(rowAt(at.x, at.y)); }

        void leave() override {
            Widget::leave();

            setHovered(NONE);
        }

        bool wheel(const double steps, const toolkit::Pointer &at) override {
            const bool took = Scroll::wheel(steps, at);

            setHovered(rowAt(at.x, at.y));

            return took;
        }

        void arrange(Typeface & /*type*/) override {
            setReach(static_cast<double>(State::get().cfg.configDonors.size()) * ROW);
        }

        void paint(const toolkit::Painter &painter) override {
            const Theme::Palette &palette = Theme::of();
            const std::vector<State::ConfigDonor> &donors = State::get().cfg.configDonors;

            if (donors.empty()) {
                const BLFont &face = painter.font(600, Theme::fontMedium);

                painter.label(face, BLRect{_box.x + 10.0, _box.y + 10.0, _box.w - 20.0, 22.0},
                              toolkit::Align::Start, "No other profile on this port", palette.muted);
                painter.paragraph(painter.font(400, Theme::fontSmall),
                                  BLRect{_box.x + 10.0, _box.y + 36.0, _box.w - 20.0, 0.0},
                                  "A profile writes its own engine config the first time it "
                                  "launches without \"Use the port's config\" turned on. Once "
                                  "one has, it can be copied here.",
                                  palette.faint);

                return;
            }

            painter.push(_box);

            double y = _box.y - offset();

            for (size_t which = 0; which < donors.size(); ++which) {
                const State::ConfigDonor &donor = donors[which];
                const BLRect line{_box.x, y, _box.w - (scrollable() ? Theme::lane : 0.0), ROW};

                y += ROW;

                if (!painter.needed(line)) {
                    continue;
                }

                const bool picked = donor.id == _sheet->_chosen;

                if (picked) {
                    painter.round(line, Theme::radiusSmall, palette.accentSoft);
                } else if (which == _hovered) {
                    painter.round(line, Theme::radiusSmall, palette.hover);
                }

                painter.label(painter.font(picked ? 600 : 400, Theme::fontBody),
                              BLRect{line.x + 12.0, line.y + 6.0, line.w - 46.0, 18.0},
                              toolkit::Align::Start,
                              donor.shared ? donor.name + " (launches on the port's config)"
                                           : donor.name,
                              picked ? palette.accent : palette.text);

                painter.label(painter.font(Typeface::mono, Theme::fontTiny),
                              BLRect{line.x + 12.0, line.y + 24.0, line.w - 46.0, 16.0},
                              toolkit::Align::Start, Format::prettyPath(donor.file), palette.faint);

                if (picked) {
                    const double side = Glyphs::span(1.2F);

                    Glyphs::draw(painter.context(), Glyphs::Glyph::Check,
                                 BLPoint{line.x + line.w - 16.0 - side,
                                         line.y + ((line.h - side) / 2.0)},
                                 1.2F, palette.accent);
                }
            }

            painter.pop();

            Scroll::paint(painter);
        }

        bool press(const toolkit::Pointer &at) override {
            _scrolling = Scroll::press(at);

            return _scrolling || holds(at.x, at.y);
        }

        void release(const toolkit::Pointer &at) override {
            Scroll::release(at);

            if (_scrolling) {
                _scrolling = false;

                return;
            }

            const size_t row = rowAt(at.x, at.y);

            if (row != NONE) {
                _sheet->_chosen = State::get().cfg.configDonors[row].id;

                _sheet->_accept->setEnabled(true);

                invalidate();
            }
        }

    private:
        void setHovered(const size_t row) {
            if (_hovered == row) {
                return;
            }

            _hovered = row;

            invalidate();
        }

        CopyConfigDialog *_sheet;

        size_t _hovered = NONE;
        bool _scrolling = false;
    };

    std::function<void(const std::string &)> _picked;

    Rows *_rows = nullptr;
    toolkit::Button *_cancel = nullptr;
    toolkit::Button *_accept = nullptr;

    BLRect _panel{};

    // Empty is nothing picked.
    std::string _chosen;
};

}
