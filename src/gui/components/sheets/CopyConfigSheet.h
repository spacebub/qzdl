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
#include "gui/model/State.h"
#include "gui/toolkit/controls/Button.h"
#include "gui/toolkit/layout/Scroll.h"
#include "gui/toolkit/overlays/Sheet.h"
#include "gui/util/Format.h"

class App;

namespace components {

class CopyConfigSheet : public toolkit::Sheet {
public:
    explicit CopyConfigSheet(std::function<void(const std::string &)> picked);

    void sync() override;

    void arrange(Typeface &type) override;

    void paintOver(const toolkit::Painter &painter) override;

private:
    // The donors, drawn straight rather than held as widgets.
    class Rows : public toolkit::Scroll {
    public:
        explicit Rows(CopyConfigSheet *sheet) : _sheet(sheet) {
            _takesPointer = true;
            cursor = toolkit::Cursor::Pointer;
        }

        static constexpr double ROW = 46.0;

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

            for (const State::ConfigDonor &donor : donors) {
                const BLRect line{_box.x, y, _box.w - (scrollable() ? Theme::lane : 0.0), ROW};

                y += ROW;

                if (!painter.needed(line)) {
                    continue;
                }

                const bool picked = donor.id == _sheet->_chosen;

                if (picked) {
                    painter.round(line, Theme::radiusSmall, palette.accentSoft);
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

                    Glyphs::draw(painter.context(), "check",
                                 BLPoint{line.x + line.w - 16.0 - side,
                                         line.y + ((line.h - side) / 2.0)},
                                 1.2F, palette.accent);
                }
            }

            painter.pop();

            toolkit::Scroll::paint(painter);
        }

        bool press(const toolkit::Pointer &at) override {
            if (toolkit::Scroll::press(at)) {
                return true;
            }

            return holds(at.x, at.y);
        }

        void release(const toolkit::Pointer &at) override {
            toolkit::Scroll::release(at);

            const std::vector<State::ConfigDonor> &donors = State::get().cfg.configDonors;
            const auto row = static_cast<size_t>((at.y - _box.y + offset()) / ROW);

            if (row < donors.size()) {
                _sheet->_chosen = donors[row].id;

                _sheet->_accept->setEnabled(true);

                invalidate();
            }
        }

    private:
        CopyConfigSheet *_sheet;
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
