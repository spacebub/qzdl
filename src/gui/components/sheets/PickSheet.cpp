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

#include <algorithm>
#include <utility>

#include "gui/components/sheets/PickSheet.h"
#include "gui/draw/Typeface.h"
#include "gui/toolkit/Root.h"
#include "gui/toolkit/controls/Button.h"
#include "gui/toolkit/controls/GlyphButton.h"
#include "gui/toolkit/controls/TextBox.h"
#include "gui/toolkit/controls/Toggle.h"
#include "gui/toolkit/overlays/Sheet.h"

namespace components {

using namespace toolkit;

bool PickSheet::closing() {
    const bool editing = State::get().pick.editing;

    _picker.dismiss();

    return !editing;
}

PickSheet::PickSheet(Picker &picker) : _picker(picker) {
    wanted = 700.0;
    tall = 540.0;

    _whereSlab = card()->append(std::make_unique<Slab>());
    _nameSlab = card()->append(std::make_unique<Slab>());

    _listSlab = card()->append(std::make_unique<Slab>());
    _listSlab->rounding = Theme::radius;

    _shut = card()->append(std::make_unique<GlyphButton>("cross", [this] {
        _picker.dismiss();
    }));

    _shut->size(26.0);

    _up = card()->append(std::make_unique<GlyphButton>("up", [this] {
        _picker.up();
    }));

    _up->size(30.0)->tip("Go up one directory");

    _typer = card()->append(std::make_unique<GlyphButton>("edit", [this] {
        State::PickState &pick = State::get().pick;

        pick.editing = !pick.editing;

        if (pick.editing) {
            _typed->setText(pick.drives ? std::string() : pick.path);

            if (root() != nullptr) {
                root()->focus(_typed);
                _typed->selectAll();
            }
        }

        State::get().touch();
    }));

    _typer->size(30.0);

    _typed = card()->append(std::make_unique<TextBox>([](const std::string &) {}));
    _typed->mono();

    _typed->accepted = [this] { _picker.typed(_typed->text()); };

    _named = card()->append(std::make_unique<TextBox>([this](const std::string &value) {
        _picker.named(value);
    }));

    _named->mono();

    // Enter never overwrites.
    _named->accepted = [this] { _picker.save(_named->text(), false); };

    _rows = card()->append(std::make_unique<Rows>(this));

    _hidden = card()->append(std::make_unique<Toggle>("Hidden", [this](const bool value) {
        _picker.showHidden(value);
    }));

    _hidden->hint = "Show what the filesystem keeps out of the way";

    _option = card()->append(std::make_unique<Toggle>("", [this](const bool value) {
        State::get().pick.optionSet = value;

        _option->setChecked(value);
    }));

    _use = card()->append(std::make_unique<Button>("", [this] { chose(); }));
    _use->kind(Button::Kind::Primary)->compact();
}

void PickSheet::sync() {
    State::PickState  const&pick = State::get().pick;

    // Switching directory starts at the top; marking a file leaves the list
    // where the eye left it.
    if (pick.path != _shownPath) {
        _shownPath = pick.path;

        _rows->scrollTo(0.0);
    }

    _up->setVisible(!pick.editing && !pick.drives);
    _up->setEnabled(pick.rooted || !pick.parts.empty());
    _typer->glyph(pick.editing ? "cross" : "edit");
    _typer->tip(pick.editing ? "Back to browsing" : "Type a path");
    _typed->setVisible(pick.editing);

    _named->setVisible(pick.saving);
    _hidden->setChecked(pick.hiddenShown);

    _option->setVisible(!pick.option.empty());
    _option->setText(pick.option);
    _option->hint = pick.optionHint;
    _option->setChecked(pick.optionSet);

    if (pick.saving) {
        _use->setText(pick.replacing ? "Replace" : "Save");
        _use->kind(pick.replacing ? Button::Kind::Danger : Button::Kind::Primary);
        _use->setEnabled(!pick.target.empty());
        _use->setVisible(!pick.drives);
    } else if (pick.directories) {
        _use->setText("Use this directory");
        _use->kind(Button::Kind::Primary);
        _use->setEnabled(true);
        _use->setVisible(!pick.drives);
    } else if (pick.multiple) {
        _use->setText(said(pick));
        _use->kind(Button::Kind::Primary);
        _use->setEnabled(pick.marked > 0);
        _use->setVisible(true);
    } else {
        _use->setText("Use this file");
        _use->kind(Button::Kind::Primary);
        _use->setEnabled(pick.marked > 0);
        _use->setVisible(true);
    }

    // Pushed, not bound: the first keystroke would break it. The count makes
    // the same name twice distinct.
    if (pick.nameSeed != _seeded) {
        _seeded = pick.nameSeed;

        _named->setText(pick.name);

        if (pick.saving && root() != nullptr) {
            root()->focus(_named);
        }
    }

    // A listing, a crumb or a button's width has changed under the layout.
    if (root() != nullptr) {
        root()->relayout();
    }
}

void PickSheet::arrange(Typeface &type) {
    Sheet::arrange(type);

    const State::PickState &pick = State::get().pick;
    const BLRect box = card()->box();

    _shut->place(BLRect{box.x + box.w - 18.0 - 26.0, box.y + 18.0, 26.0, 26.0}, type);

    _where = BLRect{box.x + 18.0, box.y + 56.0, box.w - 36.0, Theme::control};

    _whereSlab->fill = Theme::of().field;
    _whereSlab->edge = pick.editing ? Theme::of().accent : Theme::of().borderStrong;
    _whereSlab->place(_where, type);

    _up->place(BLRect{_where.x + 4.0, _where.y + ((_where.h - 30.0) / 2.0), 30.0, 30.0}, type);
    _typer->place(BLRect{_where.x + _where.w - 34.0, _where.y + ((_where.h - 30.0) / 2.0), 30.0,
                         30.0},
                  type);

    _typed->place(BLRect{_where.x + 12.0, _where.y + 6.0, _where.w - 12.0 - 38.0,
                         _where.h - 12.0},
                  type);

    _crumbs = BLRect{_where.x + 38.0, _where.y, _where.w - 38.0 - 38.0, _where.h};

    _naming = BLRect{box.x + 18.0, _where.y + _where.h + (pick.saving ? 10.0 : 0.0),
                     box.w - 36.0, pick.saving ? Theme::control : 0.0};

    _nameSlab->fill = Theme::of().field;
    _nameSlab->edge = pick.replacing      ? Theme::of().danger
                    : _named->focused()   ? Theme::of().accent
                                          : Theme::of().borderStrong;
    _nameSlab->place(_naming, type);

    _named->place(BLRect{_naming.x + 58.0, _naming.y + 6.0, _naming.w - 70.0,
                         std::max(0.0, _naming.h - 12.0)},
                  type);

    _panel = BLRect{box.x + 18.0, _naming.y + _naming.h + 12.0, box.w - 36.0, 0.0};
    _panel.h = box.y + box.h - 18.0 - Theme::controlSmall - 12.0 - _panel.y;

    _listSlab->fill = Theme::of().sunken;
    _listSlab->edge = Theme::of().border;
    _listSlab->place(_panel, type);

    _rows->place(BLRect{_panel.x + 6.0, _panel.y + 6.0, _panel.w - 12.0, _panel.h - 12.0},
                 type);

    const double bottom = box.y + box.h - 18.0 - Theme::controlSmall;
    double right = box.x + box.w - 18.0;

    if (_use->visible()) {
        const double wide = _use->naturalWidth(type);

        _use->place(BLRect{right - wide, bottom, wide, Theme::controlSmall}, type);

        right -= wide + 10.0;
    }

    if (_option->visible()) {
        const double wide = _option->naturalWidth(type);

        _option->place(BLRect{right - wide, bottom + ((Theme::controlSmall - 24.0) / 2.0),
                              wide, 24.0},
                       type);

        right -= wide + 10.0;
    }

    const double wide = _hidden->naturalWidth(type);

    _hidden->place(BLRect{right - wide, bottom + ((Theme::controlSmall - 24.0) / 2.0), wide,
                          24.0},
                   type);

    _crumbBoxes.clear();
}

void PickSheet::paintOver(const Painter &painter) {
    const Theme::Palette &palette = Theme::of();
    const State::PickState &pick = State::get().pick;
    const BLRect box = card()->box();

    painter.label(painter.font(palette.headingWeight, Theme::fontLarge),
                  BLRect{box.x + 18.0, box.y + 18.0, box.w - 36.0 - 30.0, 26.0}, Align::Start,
                  pick.title, palette.text);

    if (!pick.editing) {
        paintCrumbs(painter);
    }

    if (pick.saving) {
        painter.label(painter.font(600, Theme::fontSmall),
                      BLRect{_naming.x + 12.0, _naming.y, 40.0, _naming.h}, Align::Start,
                      "Name", palette.faint);
    }

    const double bottom = box.y + box.h - 18.0 - Theme::controlSmall;

    if (pick.entries.empty()) {
        painter.label(painter.font(400, Theme::fontSmall),
                      BLRect{box.x + 18.0, bottom, 260.0, Theme::controlSmall}, Align::Start,
                      pick.nothing, palette.faint);
    }

    if (pick.replacing) {
        painter.label(painter.font(400, Theme::fontSmall),
                      BLRect{_use->box().x - 110.0, bottom, 100.0, Theme::controlSmall},
                      Align::End, "Already there", palette.danger);
    }
}

Cursor PickSheet::cursorAt(const double x, const double y) const {
    return crumbAt(x, y) >= -1 ? Cursor::Pointer : Cursor::Default;
}

void PickSheet::hover(const Pointer &at) {
    Sheet::hover(at);

    if (const int over = crumbAt(at.x, at.y); over != _overCrumb) {
        _overCrumb = over;

        invalidate(_crumbs);
    }
}

void PickSheet::leave() {
    Sheet::leave();

    _overCrumb = -2;
}

bool PickSheet::press(const Pointer &at) {
    if (crumbAt(at.x, at.y) >= 0) {
        return true;
    }

    return Sheet::press(at);
}

void PickSheet::release(const Pointer &at) {
    if (const int crumb = crumbAt(at.x, at.y); crumb >= -1) {
        if (crumb == -1) {
            if (State::get().sys.windows) {
                _picker.showDrives();
            } else {
                _picker.go("/");
            }
        } else {
            _picker.upTo(crumb);
        }

        return;
    }

    Sheet::release(at);
}

std::string PickSheet::said(const State::PickState &pick) {
    if (pick.markedFolders == 0) {
        return pick.marked == 1 ? "Add 1 file"
                                : "Add " + std::to_string(pick.marked) + " files";
    }

    if (pick.markedFolders == pick.marked) {
        return pick.marked == 1 ? "Add 1 folder"
                                : "Add " + std::to_string(pick.marked) + " folders";
    }

    return "Add " + std::to_string(pick.marked) + " selected";
}

void PickSheet::chose() {
    const State::PickState &pick = State::get().pick;

    if (pick.saving) {
        _picker.save(_named->text(), true);
    } else if (pick.directories) {
        _picker.go(pick.path);
        _picker.choose({pick.path});
    } else {
        _picker.chooseMarked();
    }
}

void PickSheet::paintCrumbs(const Painter &painter) {
    const Theme::Palette &palette = Theme::of();
    const State::PickState &pick = State::get().pick;
    const BLFont &face = painter.font(Typeface::mono, Theme::fontSmall);

    _crumbBoxes.clear();

    painter.push(_crumbs);

    const std::string root = State::get().sys.windows ? "This PC" : "/";

    double x = _crumbs.x;

    // An overrunning path is read from its far end.
    double total = painter.width(face, root) + 14.0;

    if (!pick.drives) {
        for (const std::string &part : pick.parts) {
            total += painter.width(face, "/") + painter.width(face, part) + 16.0;
        }
    }

    if (total > _crumbs.w) {
        x -= total - _crumbs.w;
    }

    BLRect const first{x, _crumbs.y, painter.width(face, root) + 14.0, _crumbs.h};

    paintCrumb(painter, first, _overCrumb == -1);
    painter.label(face, first, Align::Centre, root,
                  _overCrumb == -1 ? palette.accent : palette.muted);

    _crumbBoxes.push_back(first);

    x += first.w;

    if (pick.drives) {
        painter.pop();

        return;
    }

    for (size_t index = 0; index < pick.parts.size(); ++index) {
        const std::string &part = pick.parts[index];
        const double slash = painter.width(face, "/");

        painter.label(face, BLRect{x, _crumbs.y, slash + 2.0, _crumbs.h}, Align::Start, "/",
                      palette.faint);

        x += slash + 1.0;

        const bool last = index + 1 == pick.parts.size();
        const bool lit = std::cmp_equal(_overCrumb ,index);
        const BLRect box{x, _crumbs.y, painter.width(face, part) + 14.0, _crumbs.h};

        paintCrumb(painter, box, lit);
        painter.label(painter.font(Typeface::pick(last ? 600 : 400, true), Theme::fontSmall),
                      box, Align::Centre, part,
                      last  ? palette.text
                      : lit ? palette.accent
                            : palette.muted);

        _crumbBoxes.push_back(box);

        x += box.w;
    }

    painter.pop();
}

void PickSheet::paintCrumb(const Painter &painter, const BLRect &box, const bool lit) {
    if (lit) {
        painter.round(BLRect{box.x, box.y + 3.0, box.w, box.h - 6.0},
                      Theme::radiusSmall - 2.0, Theme::of().hover);
    }
}

int PickSheet::crumbAt(const double x, const double y) const {
    if (State::get().pick.editing || y < _crumbs.y || y >= _crumbs.y + _crumbs.h
        || x < _crumbs.x || x >= _crumbs.x + _crumbs.w) {
        return -2;
    }

    for (size_t index = 0; index < _crumbBoxes.size(); ++index) {
        if (x >= _crumbBoxes[index].x && x < _crumbBoxes[index].x + _crumbBoxes[index].w) {
            return static_cast<int>(index) - 1;
        }
    }

    return -2;
}

}
