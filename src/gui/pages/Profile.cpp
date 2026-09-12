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
#include <array>
#include <string_view>
#include <utility>

#include "core/launch/Dos.h"
#include "gui/app/Filters.h"
#include "gui/draw/Glyphs.h"
#include "gui/draw/Paint.h"
#include "gui/pages/Profile.h"
#include "gui/state/State.h"
#include "gui/toolkit/Root.h"
#include "gui/toolkit/controls/Button.h"
#include "gui/toolkit/controls/Chip.h"
#include "gui/toolkit/controls/Fact.h"
#include "gui/toolkit/controls/Field.h"
#include "gui/toolkit/controls/GlyphButton.h"
#include "gui/toolkit/controls/Label.h"
#include "gui/toolkit/controls/Pill.h"
#include "gui/toolkit/controls/Segmented.h"
#include "gui/toolkit/controls/Select.h"
#include "gui/toolkit/controls/Stepper.h"
#include "gui/toolkit/controls/Toggle.h"
#include "gui/toolkit/layout/Box.h"
#include "gui/toolkit/layout/Pair.h"
#include "gui/toolkit/layout/Panel.h"
#include "gui/toolkit/layout/Scroll.h"
#include "gui/toolkit/layout/Spacer.h"
#include "gui/toolkit/layout/Wrap.h"
#include "gui/toolkit/overlays/Menu.h"
#include "gui/util/Clipboard.h"
#include "gui/util/Desktop.h"
#include "gui/util/Format.h"

namespace {

enum class ProfileMenuAction : std::uint8_t {
    Rename,
    Duplicate,
    Clear,
    CopyConfig,
    LoadZdl,
    SaveZdl,
    Delete,
};

constexpr double BLEED = 16.0;
constexpr double RUN_WIDTH = 340.0;
constexpr double SETTLING = 0.16;

constexpr std::array<std::string_view, 5> SKILLS = {"V. Easy", "Easy", "Medium", "Hard",
                                                   "V. Hard"};
constexpr std::array<std::string_view, 4> MONSTERS = {"No monsters", "Fast", "Respawn",
                                                      "Fast & respawn"};

constexpr std::array<std::string_view, 3> ROLES = {"alone", "host", "join"};
constexpr std::array<std::string_view, 3> TYPES = {"coop", "dm", "altdm"};
constexpr std::array<std::string_view, 3> MODES = {"any", "p2p", "cs"};
constexpr std::array<std::string_view, 3> DEMO_MODES = {"off", "record", "play"};
constexpr std::array<std::string_view, 3> SPEEDS = {"played", "timed", "fast"};

// The tokens a custom command can be written with.
constexpr auto TOKENS = std::to_array<std::pair<const char *, const char *>>({
    {"{source_port}", "The source port this profile is set to."},
    {"{game}", "The game this profile is set to."},
    {"{addon_n}", "An add-on from the list, counting from one."},
    {"{profile}", "The profile's own folder, which the next few sit in."},
    {"{cfgdir}", "The port config written for this profile."},
    {"{extracfg}", "The second config a vanilla port keeps."},
    {"{savedir}", "The profile's saves folder, inside {profile}."},
    {"{savefile}", "The save this profile is set to load."},
    {"{replaydir}", "The profile's replays folder, inside {profile}."},
});

template <typename List>
int indexOf(const List &list, const std::string_view wanted) {
    const auto found = std::ranges::find(list, wanted);

    return found == std::ranges::end(list)
        ? -1
        : static_cast<int>(std::ranges::distance(std::ranges::begin(list), found));
}

toolkit::Label *panelTitle(toolkit::Box *into, const std::string &text) {
    toolkit::Label *made = into->append(std::make_unique<toolkit::Label>(text));

    made->font(Theme::of().headingWeight, Theme::fontMedium)->tone(Theme::of().text);

    return made;
}

// A panel no taller than what is in it, up to a ceiling: the command line is
// usually one line and a box four deep leaves a hole under it.
class Hug : public toolkit::Panel {
public:
    explicit Hug(const double most) : _most(most) {}

    double naturalHeight(Typeface &type, const double width) override {
        return std::min(_most, Panel::naturalHeight(type, width));
    }

private:
    double _most;
};

class Rule : public toolkit::Widget {
public:
    Rule() { fixedHeight = 1.0; }

    void paint(const toolkit::Painter &painter) override {
        painter.fill(BLRect{_box.x, _box.y, _box.w, 1.0}, Theme::of().border);
    }
};

}

namespace pages {

using namespace toolkit;

// --- Fold ----------------------------------------------------------------------

// A panel whose body slides away: the heading row folds it, and whatever control
// sits at the far end of that row stays live either way.
class ProfilePage::Fold : public Panel {
public:
    Fold(std::string title, std::function<void(bool)> folded)
        : _title(std::move(title)), _folded(std::move(folded)) {
        _takesPointer = true;
        cursor = Cursor::Pointer;

        _head = append(Box::row());
        _head->spacing(10.0)->cross(Box::Place::Centre);
        _head->fixedHeight = Theme::control;

        // The title is drawn, not laid out, so the row opens with a gap its width.
        _gap = _head->append(std::make_unique<Spacer>(0.0));

        _pill = _head->append(std::make_unique<Pill>());
        _pill->setVisible(false);
        _pill->fixedHeight = 22.0;

        _said = _head->append(std::make_unique<Label>());
        _said->font(400, Theme::fontSmall)->tone(Theme::of().faint);
        _said->stretch = 1.0;

        // The whole band, so it centres in the row exactly as the drawn title does
        // rather than on a line box the layout has centred for it.
        _said->fixedHeight = Theme::control;

        _tools = _head->append(Box::row());
        _tools->spacing(10.0)->cross(Box::Place::Centre);

        _body = append(Box::column());
        _body->spacing(16.0);
    }

    [[nodiscard]] Box *tools() const { return _tools; }

    [[nodiscard]] Box *body() const { return _body; }

    [[nodiscard]] Pill *pill() const { return _pill; }

    void setOpen(const bool open) {
        if (_open == open) {
            return;
        }

        _open = open;

        // What is in the body may go the moment it is switched off, so the slide
        // closes over the height it had.
        if (!open && root() != nullptr) {
            _held = _body->naturalHeight(root()->type(), _box.w - 32.0);
        }

        _turn.run(open ? 180.0F : 0.0F, now(), 0.22, Anim::Curve::CubicOut);
        _slide.run(open ? 1.0F : 0.0F, now(), 0.22, Anim::Curve::CubicOut);

        animate();

        if (root() != nullptr) {
            root()->relayout();
        }

        _settling = true;
    }

    void setSaid(std::string text, const bool warning) {
        _said->setText(std::move(text));
        _said->tone(warning ? Theme::of().warning : Theme::of().faint);
    }

    double naturalHeight(Typeface &type, const double width) override {
        const double body = !_open && _slide.live() ? _held
                                                    : _body->naturalHeight(type, width - 32.0);

        return Theme::control + 32.0
            + (_slide.value() > 0.0 ? (body + 16.0) * _slide.value() : 0.0);
    }

    void arrange(Typeface &type) override {
        _gap->fixedWidth = titleWidth(type) + 10.0;

        _head->place(BLRect{_box.x + 16.0, _box.y + 16.0, _box.w - 32.0, Theme::control}, type);

        const double room = _box.w - 32.0;
        const double tall = _body->naturalHeight(type, room);

        _body->place(BLRect{_box.x + 16.0, _box.y + 16.0 + Theme::control + 16.0, room, tall},
                     type);

        _body->setVisible(_slide.value() > 0.0);
    }

    bool clips(BLRect &region) const override {
        region = _box;

        return true;
    }

    void paint(const Painter &painter) override {
        Panel::paint(painter);

        const Theme::Palette &palette = Theme::of();
        const BLRect head{_box.x + 16.0, _box.y + 16.0, _box.w - 32.0, Theme::control};
        const double side = Glyphs::span(1.0F);

        Glyphs::draw(painter.context(), Glyphs::Glyph::Down,
                     BLPoint{head.x, head.y + ((head.h - side) / 2.0)}, 1.0F,
                     _overHead ? palette.text : palette.faint,
                     _turn.value());

        painter.label(painter.font(palette.headingWeight, Theme::fontMedium),
                      BLRect{head.x + side + 10.0, head.y, 200.0, head.h}, Align::Start, _title,
                      palette.text);
    }

    bool press(const Pointer &at) override {
        return at.y < _box.y + 16.0 + Theme::control && at.x < _said->box().x + _said->box().w;
    }

    void release(const Pointer &at) override {
        if (at.y < _box.y + 16.0 + Theme::control && at.x < _said->box().x + _said->box().w
            && _folded) {
            _folded(!_open);
        }
    }

    void hover(const Pointer &at) override {
        const bool over = at.y < _box.y + 16.0 + Theme::control
            && at.x < _said->box().x + _said->box().w;

        if (over != _overHead) {
            _overHead = over;

            invalidate();
        }
    }

    void leave() override {
        Widget::leave();

        _overHead = false;
    }

    bool advance(const double now) override {
        _turn.advance(now);
        _slide.advance(now);

        // The panel grows, so the page under it moves: that is a relayout, but one
        // asked for per frame only while it is actually sliding.
        if (_settling && root() != nullptr) {
            root()->relayout();
        }

        invalidate();

        if (!_turn.live() && !_slide.live()) {
            _settling = false;

            return false;
        }

        return true;
    }

    // Where the heading's title ends, so the pill sits after it.
    double titleWidth(Typeface &type) const {
        return Glyphs::span(1.0F) + 10.0
            + type.width(type.at(Theme::of().headingWeight, Theme::fontMedium), _title);
    }

private:
    std::string _title;
    std::function<void(bool)> _folded;

    Box *_head = nullptr;
    Spacer *_gap = nullptr;
    Box *_tools = nullptr;
    Box *_body = nullptr;
    Pill *_pill = nullptr;

    double _held = 0.0;
    Label *_said = nullptr;

    Anim::Tween _turn;
    Anim::Tween _slide;

    bool _open = false;
    bool _overHead = false;
    bool _settling = false;
};

// --- Files ---------------------------------------------------------------------

// The add-on list: a row per file, reordered by its grip.
class ProfilePage::Files : public Scroll {
public:
    explicit Files(Reach *reach) : _reach(reach) { _takesPointer = true; }

    [[nodiscard]] Cursor cursorAt(const double x, const double y) const override {
        if (rowOf(y) < 0) {
            return Cursor::Default;
        }

        return x < _box.x + 22.0 ? Cursor::Resize : Cursor::Pointer;
    }

    [[nodiscard]] static double rowHeight() { return State::get().cfg.showPaths ? 46.0 : 34.0; }

    void arrange(Typeface & /*type*/) override {
        setReach(static_cast<double>(State::get().cfg.files.size()) * rowHeight());
    }

    void paint(const Painter &painter) override {
        const Theme::Palette &palette = Theme::of();
        const std::vector<State::FileRow> &files = State::get().cfg.files;

        if (files.empty()) {
            const BLFont &heading = painter.font(600, Theme::fontMedium);
            const double wide = _box.w - 48.0;
            double y = _box.y + ((_box.h - 70.0) / 2.0);

            painter.label(heading, BLRect{_box.x + 24.0, y, wide, painter.lineHeight(heading)},
                          Align::Start, "Nothing loaded", palette.muted);

            y += painter.lineHeight(heading) + 6.0;

            painter.paragraph(painter.font(400, Theme::fontSmall),
                              BLRect{_box.x + 24.0, y, wide, 0.0},
                              "WADs, PK3s, DEH and BEX patches, demos and configs go here. They "
                              "are passed to the source port in the order they are listed.",
                              palette.faint);

            return;
        }

        painter.push(_box);

        const double step = rowHeight();
        const double wide = _box.w - (scrollable() ? Theme::lane : 0.0);

        for (size_t index = 0; index < files.size(); ++index) {
            const State::FileRow &row = files[index];
            const double shift = std::cmp_equal(index, _carrying) ? _carryY.value()
                                                                      : shiftOf(index, step);
            const BLRect line{_box.x, _box.y - offset() + (static_cast<double>(index) * step)
                                          + shift,
                              wide, step};

            if (!painter.needed(line)) {
                continue;
            }

            if (std::cmp_equal(index, _carrying)) {
                painter.round(line, Theme::radiusSmall - 2.0, palette.mutedSoft);
            } else if (std::cmp_equal(index, _over)) {
                painter.round(line, Theme::radiusSmall - 2.0, palette.hover);
            }

            const double grip = Glyphs::span(1.0F);

            Glyphs::draw(painter.context(), Glyphs::Glyph::Grip,
                         BLPoint{line.x + ((22.0 - grip) / 2.0), line.y + ((line.h - grip) / 2.0)},
                         1.0F,
                         std::cmp_equal(index, _overGrip) ? palette.muted : palette.border);

            const BLRect check{line.x + 24.0, line.y + ((line.h - 18.0) / 2.0), 18.0, 18.0};

            painter.round(check, 5.0, row.loaded ? palette.accent : palette.field);
            painter.outline(check, 5.0, 1.0, row.loaded ? palette.accent : palette.border);

            if (row.loaded) {
                const double tick = Glyphs::span(0.85F);

                Glyphs::draw(painter.context(), Glyphs::Glyph::Check,
                             BLPoint{check.x + ((check.w - tick) / 2.0),
                                     check.y + ((check.h - tick) / 2.0)},
                             0.85F, palette.accentText);
            }

            const double left = line.x + 52.0;
            const double right = line.x + line.w - 38.0;

            const BLFont &face = painter.font(400, Theme::fontBody);
            const double taken = std::min(painter.width(face, row.name), right - left - 60.0);

            painter.label(face,
                          BLRect{left, State::get().cfg.showPaths ? line.y + 6.0 : line.y,
                                 right - left, State::get().cfg.showPaths ? 20.0 : line.h},
                          Align::Start, row.name,
                          row.missing  ? palette.danger
                          : row.loaded ? palette.text
                                       : palette.faint);

            if (!row.loaded) {
                // Blend2D strikes nothing through; the line is drawn.
                const double middle = (State::get().cfg.showPaths ? line.y + 16.0
                                                                  : line.y + (line.h / 2.0));

                painter.fill(BLRect{left, middle, taken, 1.0}, palette.faint);
            }

            if (row.missing) {
                painter.label(painter.font(600, Theme::fontTiny),
                              BLRect{left + taken + 7.0,
                                     State::get().cfg.showPaths ? line.y + 6.0 : line.y, 60.0,
                                     State::get().cfg.showPaths ? 20.0 : line.h},
                              Align::Start, "missing", palette.danger);
            }

            if (State::get().cfg.showPaths) {
                const BLFont &mono = painter.font(Typeface::mono, Theme::fontTiny);
                const double unit = painter.width(mono, "M");
                const int room = unit > 0.0 ? static_cast<int>((right - left) / unit) : 0;

                painter.label(mono, BLRect{left, line.y + 25.0, right - left, 16.0}, Align::Start,
                              Format::fitPath(row.directory, room), palette.faint);
            }

            if (std::cmp_equal(index, _over)) {
                const double cross = Glyphs::span(1.2F);

                Glyphs::draw(painter.context(), Glyphs::Glyph::Cross,
                             BLPoint{line.x + line.w - 32.0 + ((26.0 - cross) / 2.0),
                                     line.y + ((line.h - cross) / 2.0)},
                             1.2F, _overShut ? palette.danger : palette.muted);
            }
        }

        painter.pop();

        Scroll::paint(painter);
    }

    void hover(const Pointer &at) override {
        const int row = rowAt(at.y);
        const bool grip = at.x < _box.x + 22.0;
        const bool shut = at.x >= _box.x + _box.w - 38.0;

        if (row != _over || grip != (_overGrip == row) || shut != _overShut) {
            _over = row;
            _overGrip = grip ? row : -1;
            _overShut = shut;

            invalidate();
        }
    }

    void leave() override {
        Widget::leave();

        _over = -1;
        _overGrip = -1;
        _overShut = false;
    }

    bool press(const Pointer &at) override {
        if (Scroll::press(at)) {
            return true;
        }

        land();

        const int row = rowAt(at.y);

        if (row < 0) {
            return false;
        }

        if (at.x < _box.x + 22.0) {
            _dragging = true;
            _carrying = row;
            _target = row;
            _grabY = at.y;

            _carryY.set(0.0F);
            animate();
        }

        return true;
    }

    void drag(const Pointer &at) override {
        Scroll::drag(at);

        if (_carrying < 0) {
            return;
        }

        const double step = rowHeight();
        const int count = static_cast<int>(State::get().cfg.files.size());

        _target = std::clamp(static_cast<int>((at.y - _box.y + offset()) / step), 0,
                             std::max(0, count - 1));

        _carryY.set(static_cast<float>(at.y - _grabY));

        invalidate();
        animate();
    }

    void release(const Pointer &at) override {
        Scroll::release(at);

        if (_carrying >= 0) {
            _dragging = false;

            // The row walks to its gap; only then does the list change.
            _carryY.run(static_cast<float>((_target - _carrying) * rowHeight()), now(), SETTLING,
                        Anim::Curve::CubicOut);

            _landing = now() + SETTLING + 0.02;

            animate();

            return;
        }

        const int row = rowAt(at.y);

        if (row < 0 || std::cmp_greater_equal(row, State::get().cfg.files.size())) {
            return;
        }

        if (at.x >= _box.x + _box.w - 38.0) {
            _reach->config.lists().removeFile(row);

            return;
        }

        if (at.x >= _box.x + 24.0 && at.x < _box.x + 42.0) {
            _reach->config.lists().setFileEnabled(
                row, !State::get().cfg.files[static_cast<size_t>(row)].loaded);
        }
    }

    bool advance(const double now) override {
        _carryY.advance(now);

        invalidate();

        if (_landing > 0.0 && now >= _landing) {
            land();

            return false;
        }

        return _carryY.live() || _landing > 0.0 || _dragging;
    }

    // The one place the list changes.
    void land() {
        _landing = 0.0;

        if (_carrying < 0) {
            return;
        }

        if (_target != _carrying) {
            _reach->config.lists().moveFile(_carrying, _target);
        }

        _carrying = -1;
        _target = -1;

        _carryY.set(0.0F);

        invalidate();
    }

    [[nodiscard]] int rowOf(const double y) const {
        const int row = static_cast<int>((y - _box.y + offset()) / rowHeight());

        return row >= 0 && std::cmp_less(row, State::get().cfg.files.size()) ? row : -1;
    }

private:
    [[nodiscard]] int rowAt(const double y) const { return rowOf(y); }

    // Rows the carried one passed close up behind it.
    [[nodiscard]] double shiftOf(const size_t index, const double step) const {
        const int at = static_cast<int>(index);

        if (_carrying < 0 || at == _carrying) {
            return 0.0;
        }

        if (_carrying < _target && at > _carrying && at <= _target) {
            return -step;
        }

        if (_carrying > _target && at >= _target && at < _carrying) {
            return step;
        }

        return 0.0;
    }

    Reach *_reach;

    int _over = -1;
    int _overGrip = -1;
    bool _overShut = false;

    int _carrying = -1;
    int _target = -1;
    bool _dragging = false;

    double _grabY = 0.0;
    double _landing = 0.0;

    Anim::Tween _carryY;
};

// --- Chooser -------------------------------------------------------------------

// The profile at the top of the page, and the list it is picked from.
class ProfilePage::Chooser : public Widget {
public:
    explicit Chooser(Reach *reach) : _reach(reach) {
        _takesPointer = true;
        cursor = Cursor::Pointer;
    }

    std::function<BLImage(const std::string &)> artwork;

    void paint(const Painter &painter) override {
        const Theme::Palette &palette = Theme::of();
        const State::Cfg &cfg = State::get().cfg;

        if (hovered() || _open) {
            painter.round(_box, Theme::radius, _open ? palette.mutedSoft : palette.hover);
        }

        const BLRect thumb{_box.x + 8.0, _box.y + ((_box.h - 66.0) / 2.0), 116.0, 66.0};

        painter.round(thumb, Theme::radiusSmall, palette.artMiddle);

        const BLImage shot = artwork ? artwork(ProfileBridge::artKey()) : BLImage();

        if (!shot.is_empty()) {
            Paint::cover(painter.context(), thumb, shot, Theme::radiusSmall);
        }

        const double left = thumb.x + thumb.w + 14.0;
        const BLFont &name = painter.font(palette.headingWeight, Theme::fontDisplay);

        painter.label(name, BLRect{left, _box.y + 12.0, _box.w - left + _box.x, 30.0},
                      Align::Start, cfg.profileName, palette.text);

        painter.label(painter.font(400, Theme::fontSmall),
                      BLRect{left, _box.y + 44.0, _box.w - left + _box.x, 18.0}, Align::Start,
                      _said, _ready ? palette.faint : palette.warning);
    }

    void setSaid(std::string said, const bool ready) {
        _said = std::move(said);
        _ready = ready;

        invalidate();
    }

    bool press(const Pointer & /*at*/) override { return true; }

    void release(const Pointer &at) override {
        if (holds(at.x, at.y)) {
            show();
        }
    }

private:
    void show();

    Reach *_reach;

    std::string _said;
    bool _ready = true;
    bool _open = false;

    Widget *_list = nullptr;
};

namespace {

// The profile list the chooser drops, with a row per profile and a way to add one.
class Profiles : public Widget {
public:
    static constexpr double ROW = 52.0;
    static constexpr double ADDER = 38.0;

    Profiles(Reach *reach, std::function<void()> chose,
             std::function<BLImage(const std::string &)> artwork)
        : _reach(reach), _chose(std::move(chose)), _artwork(std::move(artwork)) {
        _takesPointer = true;
        cursor = Cursor::Pointer;

        _scroll = append(std::make_unique<Scroll>());
    }

    static double heightOf(const size_t count) {
        return (static_cast<double>(std::min<size_t>(count, 8)) * ROW) + 10.0 + ADDER;
    }

    void arrange(Typeface &type) override {
        _scroll->place(BLRect{_box.x + 5.0, _box.y + 5.0, _box.w - 10.0,
                              _box.h - 10.0 - ADDER},
                       type);

        _scroll->setReach(static_cast<double>(State::get().cfg.profileCards.size()) * ROW);
    }

    void settle() {
        _scroll->scrollTo((State::get().cfg.profileIndex * ROW) - _scroll->box().h + ROW);
    }

    void paint(const Painter &painter) override {
        const Theme::Palette &palette = Theme::of();
        const std::vector<State::ProfileCard> &cards = State::get().cfg.profileCards;

        painter.round(_box, Theme::radiusSmall, palette.raised);
        painter.outline(_box, Theme::radiusSmall, 1.0, palette.borderStrong);

        const BLRect view = _scroll->box();

        painter.push(view);

        double y = view.y - _scroll->offset();

        for (size_t index = 0; index < cards.size(); ++index) {
            const State::ProfileCard &card = cards[index];
            const BLRect line{view.x, y, view.w - (_scroll->scrollable() ? Theme::lane : 0.0),
                              ROW};

            y += ROW;

            if (!painter.needed(line)) {
                continue;
            }

            const bool current = card.index == State::get().cfg.profileIndex;

            if (current) {
                painter.round(line, Theme::radiusSmall, palette.accentSoft);
            } else if (std::cmp_equal(index, _over)) {
                painter.round(line, Theme::radiusSmall, palette.hover);
            }

            const BLRect chip{line.x + 8.0, line.y + ((line.h - 36.0) / 2.0), 60.0, 36.0};

            painter.round(chip, Theme::radiusSmall - 3.0, palette.artMiddle);

            const BLImage shot = _artwork ? _artwork(card.artKey) : BLImage();

            if (!shot.is_empty()) {
                Paint::cover(painter.context(), chip, shot, Theme::radiusSmall - 3.0);
            }

            const double left = chip.x + chip.w + 10.0;

            painter.label(painter.font(current ? 600 : 400, Theme::fontBody),
                          BLRect{left, line.y + 8.0, line.x + line.w - 10.0 - left, 18.0},
                          Align::Start, card.name, current ? palette.accent : palette.text);

            painter.label(painter.font(400, Theme::fontTiny),
                          BLRect{left, line.y + 27.0, line.x + line.w - 10.0 - left, 16.0},
                          Align::Start,
                          card.port.empty()
                              ? "No source port"
                              : card.port + " · " + (card.iwad.empty() ? "no game" : card.iwad),
                          palette.faint);
        }

        painter.pop();

        _scroll->paint(painter);

        // The row that makes one.
        const BLRect adder{_box.x, _box.y + _box.h - ADDER, _box.w, ADDER};

        painter.fill(BLRect{adder.x + 4.0, adder.y, adder.w - 8.0, 1.0}, palette.border);

        if (_onAdder) {
            painter.round(BLRect{adder.x + 4.0, adder.y + 5.0, adder.w - 8.0, adder.h - 9.0},
                          Theme::radiusSmall, palette.hover);
        }

        const double side = Glyphs::span(1.0F);

        Glyphs::draw(painter.context(), Glyphs::Glyph::Plus,
                     BLPoint{adder.x + 14.0, adder.y + ((adder.h - side) / 2.0)}, 1.0F,
                     _onAdder ? palette.accent : palette.faint);

        painter.label(painter.font(400, Theme::fontBody),
                      BLRect{adder.x + 14.0 + side + 9.0, adder.y, adder.w - 60.0, adder.h},
                      Align::Start, "New profile…", _onAdder ? palette.accent : palette.text);
    }

    bool wheel(const double steps, const Pointer &at) override {
        return _scroll->wheel(steps, at);
    }

    void hover(const Pointer &at) override {
        const bool adder = at.y >= _box.y + _box.h - ADDER;
        const int row = adder ? -1 : rowAt(at.y);

        if (adder != _onAdder || row != _over) {
            _onAdder = adder;
            _over = row;

            invalidate();
        }
    }

    void leave() override {
        Widget::leave();

        _over = -1;
        _onAdder = false;
    }

    // The rows are this widget's, not the scroller's, so it answers the pointer
    // itself; the scroller only wants its lane.
    Widget *at(const double x, const double y) override {
        if (!visible() || !holds(x, y)) {
            return nullptr;
        }

        return _scroll->scrollable() && x >= _box.x + _box.w - 5.0 - Theme::lane
            ? static_cast<Widget *>(_scroll)
            : this;
    }

    bool press(const Pointer &at) override { return holds(at.x, at.y); }

    void release(const Pointer &at) override {
        // Read out first: closing the list frees this widget.
        Reach *reach = _reach;
        const std::function<void()> close = _chose;

        if (at.y >= _box.y + _box.h - ADDER) {
            if (close) {
                close();
            }

            reach->prompt("New profile", "Name", "New profile", "Create",
                          [reach](const std::string &named) {
                              reach->config.profile().addProfile(named);
                          });

            return;
        }

        const int row = rowAt(at.y);

        if (row < 0 || std::cmp_greater_equal(row, State::get().cfg.profileCards.size())) {
            return;
        }

        const int index = State::get().cfg.profileCards[static_cast<size_t>(row)].index;

        if (close) {
            close();
        }

        reach->config.profile().setProfileIndex(index);
    }

private:
    [[nodiscard]] int rowAt(const double y) const {
        const int row = static_cast<int>((y - _scroll->box().y + _scroll->offset()) / ROW);

        return row >= 0 ? row : -1;
    }

    Reach *_reach;

    std::function<void()> _chose;
    std::function<BLImage(const std::string &)> _artwork;

    Scroll *_scroll = nullptr;

    int _over = -1;
    bool _onAdder = false;
};

}

void ProfilePage::Chooser::show() {
    if (_open || root() == nullptr) {
        return;
    }

    const double tall = Profiles::heightOf(State::get().cfg.profileCards.size());
    const double wide = std::clamp(_box.w, 280.0, 460.0);

    auto made = std::make_unique<Profiles>(_reach, [this] { root()->dismiss(); }, artwork);
    Profiles *raw = made.get();

    _list = root()->layer(Root::POPUPS)->add(std::move(made));
    _open = true;

    _list->place(BLRect{_box.x, _box.y + _box.h + 6.0, wide, tall}, root()->type());

    raw->settle();

    root()->setDismiss([this] {
        if (_list != nullptr) {
            const BLRect was = _list->box();

            root()->layer(Root::POPUPS)->erase(_list);

            _list = nullptr;
            _open = false;

            root()->damage(was);

            // The chooser draws itself differently while the list is down.
            invalidate();
        }
    }, this);

    _list->invalidate();
    invalidate();
}

// --- the page ------------------------------------------------------------------

bool ProfilePage::ready() {
    const State::Cfg &cfg = State::get().cfg;

    return cfg.commandOverride ? cfg.commandTrouble.empty() : !cfg.port.empty();
}

std::string ProfilePage::summary() {
    const State::Cfg &cfg = State::get().cfg;

    if (cfg.commandOverride) {
        return cfg.commandTrouble.empty() ? "Launches the command written on this page"
                                          : cfg.commandTrouble;
    }

    if (!ready()) {
        return "No source port · this profile cannot be launched yet";
    }

    std::string said = cfg.port + " · " + (cfg.iwad.empty() ? "no game" : cfg.iwad);

    if (!cfg.warp.empty()) {
        said += " · " + cfg.warp;
    }

    if (!cfg.files.empty()) {
        said += " · " + std::to_string(cfg.files.size())
            + (cfg.files.size() == 1 ? " file" : " files");
    }

    return said;
}

std::string ProfilePage::netSummary() {
    const State::Cfg &cfg = State::get().cfg;

    if (cfg.netRole == 0) {
        return "Off · this profile launches a single player game";
    }

    const bool unsupported = (cfg.netRole == 1 && !cfg.netHosts)
        || (cfg.netRole == 2 && !cfg.netJoins);

    if (unsupported) {
        return "not a side this port takes";
    }

    if (cfg.netRole == 1) {
        return (cfg.netPlayers ? "for " + std::to_string(cfg.players) + " players"
                               : "open to others")
            + (cfg.netPort.empty() ? "" : ", on port " + cfg.netPort);
    }

    if (cfg.host.empty()) {
        return "no address yet";
    }

    return cfg.host + (cfg.netPort.empty() ? "" : ":" + cfg.netPort);
}

std::string ProfilePage::saveSummary() {
    const State::Cfg &cfg = State::get().cfg;

    if (!cfg.saveEnabled) {
        return "Off · every launch starts a new game";
    }

    if (cfg.saveFile.empty()) {
        return "nothing picked yet";
    }

    return "from " + Format::fitPath(cfg.savePath, 30);
}

std::string ProfilePage::replaySummary() {
    const State::Cfg &cfg = State::get().cfg;

    if (cfg.replayMode == 0) {
        return "Off · nothing is recorded and nothing is played back";
    }

    if (cfg.replayFile.empty()) {
        return cfg.replayMode == 1 ? "no name yet" : "nothing picked yet";
    }

    return (cfg.replayMode == 1 ? "into " : "") + Format::fitPath(cfg.replayPath, 30);
}

std::string ProfilePage::tuningSummary() {
    const State::Cfg &cfg = State::get().cfg;

    const int mode = cfg.hasNetmode ? cfg.netmode : -1;
    const int dup = cfg.netDup ? cfg.dup : 0;
    const bool extra = cfg.netExtratic && cfg.extratic == 1;

    if (mode == -1 && dup <= 0 && !extra) {
        return "left to the port";
    }

    std::string said = mode == -1 ? "" : (mode == 0 ? "peer to peer" : "client/server");

    if (dup > 0) {
        said += (said.empty() ? "" : " · ") + std::to_string(dup) + "× tics";
    }

    if (extra) {
        said += (said.empty() ? "" : " · ") + std::string("extra tic");
    }

    return said;
}

ProfilePage::ProfilePage(Reach *reach) : _reach(reach) {
    Box *column = append(Box::column());

    column->spacing(16.0);

    // --- the head ---

    Box *head = column->append(Box::row());

    head->fixedHeight = 74.0;
    head->spacing(14.0)->pad(BLEED, 0.0, BLEED, 0.0)->cross(Box::Place::Centre);

    _chooser = head->append(std::make_unique<Chooser>(reach));
    _chooser->stretch = 1.0;
    _chooser->fixedHeight = 74.0;
    _chooser->artwork = [this](const std::string &key) { return _reach->art.of(key); };

    _terminal = head->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Terminal, [this] {
        _reach->runs.show(State::get().cfg.profileKey);
    }));

    _terminal->size(Theme::control)->outlined();
    _terminal->fixedWidth = Theme::control;
    _terminal->fixedHeight = Theme::control;

    _cog = head->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Cog, [this] { showMenu(); }));
    _cog->size(Theme::control)->outlined()->tooltip("What else can be done with this profile");
    _cog->fixedWidth = Theme::control;
    _cog->fixedHeight = Theme::control;

    _launch = head->append(std::make_unique<Button>("Launch", [this] {
        _reach->config.profile().launch();
    }));

    _launch->kind(Button::Kind::Primary)->glyph(Glyphs::Glyph::Play);

    // --- the body ---

    _scroll = column->append(std::make_unique<Scroll>());
    _scroll->stretch = 1.0;

    _body = static_cast<Box *>(_scroll->hold(Box::column()));
    _body->spacing(16.0)->pad(BLEED, 0.0, BLEED, 8.0);

    // Add-ons floor, gap, and the run panel: under this the page is cut, not
    // squeezed.
    _body->minWidth = 260.0 + 16.0 + RUN_WIDTH + (BLEED * 2.0);

    Box *top = _body->append(Box::row());

    top->spacing(16.0);

    // A floor, not a ceiling: the row grows with whatever The run holds.
    top->minHeight = 340.0;

    // Add-ons.
    Panel *addons = top->append(std::make_unique<Panel>());

    addons->stretch = 1.0;
    addons->minWidth = 260.0;

    Box *inside = addons->append(Box::column());

    inside->pad(16.0)->spacing(12.0);

    Box *addonHead = inside->append(Box::row());

    addonHead->spacing(8.0)->cross(Box::Place::Centre);
    addonHead->fixedHeight = Theme::controlSmall;

    panelTitle(addonHead, "Add-ons");

    addonHead->append(std::make_unique<Spacer>());

    _loaded = addonHead->append(std::make_unique<Pill>());
    _loaded->kind(Pill::Kind::Muted)->dot(false);

    _addFiles = addonHead->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Plus, [this] {
        _reach->picker.open(FilePicker::Action::AddFiles, "Add files", Filters::wad(), false, true, true,
                            FilePicker::Slot::Wad);
    }));

    _addFiles->tooltip("Add files");
    _addFiles->fixedWidth = Theme::controlSmall;

    _clearFiles = addonHead->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Trash, [this] {
        _reach->ask("Clear the file list?",
                  "Every file in this profile's list is removed. The files themselves are left "
                  "alone, and the rest of the profile is untouched.",
                  "Clear", true, [this] { _reach->config.lists().clearFiles(); });
    }));

    _clearFiles->tone(Theme::of().muted, Theme::of().danger)
        ->tooltip("Remove every file from this profile");
    _clearFiles->fixedWidth = Theme::controlSmall;

    Panel *trough = inside->append(std::make_unique<Panel>());

    trough->inset = true;
    trough->stretch = 1.0;

    Box *lane = trough->append(Box::column());

    lane->pad(6.0);

    _files = lane->append(std::make_unique<Files>(reach));
    _files->stretch = 1.0;

    // The run.
    Panel *runPanel = top->append(std::make_unique<Panel>());

    runPanel->fixedWidth = RUN_WIDTH;

    Box *run = runPanel->append(Box::column());

    run->pad(16.0)->spacing(14.0);

    buildRun(run);

    buildReplay(_body);
    buildSaves(_body);
    buildNet(_body);
    buildCommand(_body);

    // --- no profiles ---

    _none = append(Box::column());
    _none->spacing(18.0)->align(Box::Place::Centre)->cross(Box::Place::Centre);
    _none->setVisible(false);

    Label *title = _none->append(std::make_unique<Label>("No profiles"));

    title->font(600, Theme::fontMedium)->tone(Theme::of().muted);
    title->fixedWidth = 420.0;

    Label *said = _none->append(std::make_unique<Label>(
        "A profile holds a game, the port it runs on and whatever is loaded over them, under a "
        "name. This config has none."));

    said->font(400, Theme::fontSmall)->tone(Theme::of().faint)->wrap();
    said->fixedWidth = 420.0;

    Box *buttons = _none->append(Box::row());

    buttons->spacing(8.0);
    buttons->fixedWidth = 420.0;
    buttons->fixedHeight = Theme::control;

    buttons->append(std::make_unique<Button>("New profile", [this] {
        _reach->prompt("New profile", "Name", "New profile", "Create",
                     [this](const std::string &named) {
                         _reach->config.profile().addProfile(named);
                     });
    }))->kind(Button::Kind::Primary)->glyph(Glyphs::Glyph::Plus);

    buttons->append(std::make_unique<Button>("Import a .zdl", [this] {
        _reach->picker.open(FilePicker::Action::LoadZdl, "Load a .zdl launch config", Filters::zdl(), false,
                            false, false, FilePicker::Slot::Zdl);
    }))->kind(Button::Kind::Ghost)->glyph(Glyphs::Glyph::Download)
        ->tooltip("Read a .zdl launch config in as a profile of its own");

    buttons->append(std::make_unique<Spacer>());
}

void ProfilePage::buildRun(Box *into) {
    panelTitle(into, "The run");

    _addPort = into->append(std::make_unique<Button>("Add a source port…", [this] {
        _reach->go(State::Page::Engines);
    }));

    _addPort->glyph(Glyphs::Glyph::Plus)->tooltip("Ports are set up on the Engines page");

    _port = into->append(std::make_unique<Select>("Source port", [this](const int index) {
        const std::vector<std::string> &names = State::get().cfg.portNames;

        _reach->config.profile().setPort(
            index < 0 || std::cmp_greater_equal(index, names.size())
                ? std::string()
                : names[static_cast<size_t>(index)]);
    }));

    _port->placeholder("None selected")->clearable()
        ->tooltip("What actually runs. Add ports on the Engines page.");

    _addGame = into->append(std::make_unique<Button>("Add a game…", [this] {
        State::get().nav.shelf = State::Shelf::Games;

        _reach->go(State::Page::Library);
    }));

    _addGame->glyph(Glyphs::Glyph::Plus)->tooltip("Games are added on the library's games shelf");

    _iwad = into->append(std::make_unique<Select>("Game", [this](const int index) {
        const std::vector<std::string> &names = State::get().cfg.iwadNames;

        _reach->config.profile().setIwad(
            index < 0 || std::cmp_greater_equal(index, names.size())
                ? std::string()
                : names[static_cast<size_t>(index)]);
    }));

    _iwad->placeholder("None selected")->clearable()
        ->tooltip("The IWAD itself. Add games from the library.");

    _map = into->append(std::make_unique<Select>("Map", [this](const int index) {
        const std::vector<std::string> &maps = State::get().cfg.maps;

        _reach->config.profile().setWarp(
            index < 0 || std::cmp_greater_equal(index, maps.size())
                ? std::string()
                : maps[static_cast<size_t>(index)]);
    }));

    _map->clearable()->tooltip("Read out of the game and everything loaded on top of it");

    // Half the row each, whatever the panel has come down to: a width taken off
    // the page's own runs the second one off the edge as soon as it is narrower.
    Box *pair = into->append(std::make_unique<Pair>(0.0));

    pair->spacing(12.0);

    _skill = pair->append(std::make_unique<Select>("Skill", [this](const int index) {
        _reach->config.profile().setSkill(index + 1);
    }));

    _skill->clearable();
    _skill->setOptions(std::vector<std::string>(SKILLS.begin(), SKILLS.end()));

    _monsters = pair->append(std::make_unique<Select>("Monsters", [this](const int index) {
        _reach->config.profile().setMonsters(index + 1);
    }));

    _monsters->clearable();
    _monsters->setOptions(std::vector<std::string>(MONSTERS.begin(), MONSTERS.end()));

    into->append(std::make_unique<Rule>());

    _capture = into->append(std::make_unique<Toggle>("Log the game's output",
                                                     [this](const bool on) {
        _reach->config.profile().setCaptureOutput(on);
    }));

    _fullscreen = into->append(std::make_unique<Toggle>("Full screen", [this](const bool on) {
        _reach->config.profile().setDosFullscreen(on);
    }));

    _fullscreen->hint = "Give DOSBox the whole screen rather than a window";

    _levelstat = into->append(std::make_unique<Toggle>("Level stats", [this](const bool on) {
        _reach->config.profile().setLevelstat(on);
    }));

    _levelstat->hint = "Writes a levelstat.txt with the time taken on each map, which is what a "
                       "run is submitted with";

    _sharedConfig = into->append(std::make_unique<Toggle>("Use the port's config",
                                                          [this](const bool on) {
        _reach->config.profile().setSharedConfig(on);
    }));

    _sharedConfig->hint = "Launch on the settings the source port keeps for itself, shared with "
                          "everything else that uses them";

    _directory = into->append(std::make_unique<Fact>("Directory", ""));
    _directory->path()->onClick("Open the folder this profile keeps its files in", [] {
        const std::string where = State::get().cfg.profileDirectory;
        std::error_code made;

        std::filesystem::create_directories(std::filesystem::path(where), made);

        Desktop::open(where);
    });
}

void ProfilePage::buildReplay(Box *into) {
    _replay = into->append(std::make_unique<Fold>("Replay", [this](const bool open) {
        _reach->config.panels().setReplayOpen(open);
    }));

    _replayMode = _replay->tools()->append(std::make_unique<Segmented>(
        [this](const std::string &key) {
            _reach->config.panels().setReplayMode(indexOf(DEMO_MODES, key));
            _reach->config.panels().setReplayOpen(key != "off");
        }));

    _replayMode->setOptions({{.key = "off", .label = "Off"},
                             {.key = "record", .label = "Record"},
                             {.key = "play", .label = "Play"}});

    // Last, so it sits against the far edge of the heading.
    _replayReset = _replay->tools()->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Refresh, [this] {
        _reach->ask("Reset the replay settings?",
                  "The profile goes back to recording nothing and playing nothing back. The "
                  "demos already in its replays folder are left where they are.",
                  "Reset", true, [this] { _reach->config.panels().clearReplay(); });
    }));

    _replayReset->size(Theme::control)->outlined();
    _replayReset->fixedWidth = Theme::control;

    Box *body = _replay->body();

    body->append(std::make_unique<Rule>());

    _replayNote = body->append(std::make_unique<Label>());
    _replayNote->font(400, Theme::fontSmall)->tone(Theme::of().faint)->wrap();

    // Recording.
    _replayRecord = body->append(Box::column());
    _replayRecord->spacing(10.0);

    _replayName = _replayRecord->append(std::make_unique<Field>("Record it as",
                                                                [this](const std::string &value) {
        _reach->config.panels().setReplayFile(value);
    }));

    _replayName->placeholder("A name for the demo")->mono()
        ->note(".lmp goes on the end by itself, and the file lands in the profile's replays "
               "folder");

    // Playing back.
    _replayPlay = body->append(Box::column());
    _replayPlay->spacing(10.0);

    Box *pick = _replayPlay->append(Box::row());

    pick->spacing(8.0)->cross(Box::Place::End);

    _replayFile = pick->append(std::make_unique<Select>("Replay", [this](const int index) {
        _reach->config.panels().setReplayIndex(index);
    }));

    _replayFile->clearable()->tooltip("The demos in this profile's replays folder, newest first");
    _replayFile->fixedWidth = 340.0;

    _replayRefresh = pick->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Refresh, [this] {
        _reach->config.panels().refreshReplays();
    }));

    _replayRefresh->size(Theme::control)->outlined()->tooltip("Refresh folder");
    _replayRefresh->fixedWidth = Theme::control;

    _replayBrowse = pick->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Folder, [this] {
        _reach->picker.open(FilePicker::Action::Replay, "Select a replay", Filters::replay(), false, false,
                            false, FilePicker::Slot::Replay);
    }));

    _replayBrowse->size(Theme::control)->outlined()->tooltip("Play a demo from somewhere else");
    _replayBrowse->fixedWidth = Theme::control;

    pick->append(std::make_unique<Spacer>());

    Box *speed = _replayPlay->append(Box::column());

    speed->spacing(6.0);

    speed->append(std::make_unique<Label>("How it plays"))->section();

    _replaySpeed = speed->append(std::make_unique<Segmented>([this](const std::string &key) {
        _reach->config.panels().setReplayPlayback(indexOf(SPEEDS, key));
    }));

    _replayPath = body->append(std::make_unique<Label>());
    _replayPath->font(400, Theme::fontTiny)->tone(Theme::of().faint)->path();

    // Compatibility, while recording.
    _replayTune = body->append(Box::column());
    _replayTune->spacing(12.0);

    _replayTune->append(std::make_unique<Rule>());
    _replayTune->append(std::make_unique<Label>("Compatibility"))->section();

    Box *tune = _replayTune->append(Box::row());

    tune->spacing(20.0)->cross(Box::Place::End);

    _complevel = tune->append(std::make_unique<Select>("Complevel", [this](const int index) {
        _reach->config.panels().setReplayComplevel(index);
    }));

    _complevel->tooltip("The rules the demo is recorded under, and what it has to be played back "
                    "under");
    _complevel->fixedWidth = 220.0;

    _longtics = tune->append(std::make_unique<Toggle>("Long tics", [this](const bool on) {
        _reach->config.panels().setReplayLongtics(on);
    }));

    _longtics->hint = "Records turns at the port's own precision rather than vanilla's. A demo "
                      "made this way needs a port that reads them";

    _soloNet = tune->append(std::make_unique<Toggle>("Solo net", [this](const bool on) {
        _reach->config.panels().setReplaySoloNet(on);
    }));

    _soloNet->hint = "Plays alone under a netgame's rules, which is what a recorded run is "
                     "judged under";

    tune->append(std::make_unique<Spacer>());
}

void ProfilePage::buildSaves(Box *into) {
    _saves = into->append(std::make_unique<Fold>("Saves", [this](const bool open) {
        _reach->config.panels().setSaveOpen(open);
    }));

    _saveOn = _saves->tools()->append(std::make_unique<Segmented>(
        [this](const std::string &key) {
            _reach->config.panels().setSaveEnabled(key == "on");
            _reach->config.panels().setSaveOpen(key == "on");
        }));

    _saveOn->setOptions({{.key = "off", .label = "Off"}, {.key = "on", .label = "On"}});

    Box *body = _saves->body();

    body->append(std::make_unique<Rule>());

    Box *pick = body->append(Box::row());

    pick->spacing(8.0)->cross(Box::Place::End);

    _saveFile = pick->append(std::make_unique<Select>("Save", [this](const int index) {
        _reach->config.panels().setSaveIndex(index);
    }));

    _saveFile->clearable();
    _saveFile->fixedWidth = 340.0;

    _saveRefresh = pick->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Refresh, [this] {
        _reach->config.panels().refreshSaves();
    }));

    _saveRefresh->size(Theme::control)->outlined()->tooltip("Refresh folder");
    _saveRefresh->fixedWidth = Theme::control;

    pick->append(std::make_unique<Spacer>());

    _saveNote = body->append(std::make_unique<Label>());
    _saveNote->font(400, Theme::fontSmall)->tone(Theme::of().faint)->wrap();

    _savePath = body->append(std::make_unique<Label>());
    _savePath->font(400, Theme::fontTiny)->tone(Theme::of().faint)->path();
    _savePath->onClick([] { Desktop::open(State::get().cfg.saveFolder); });
    _savePath->hint = "Open the folder this profile's saves go in";
}

void ProfilePage::buildNet(Box *into) {
    _net = into->append(std::make_unique<Fold>("Multiplayer", [this](const bool open) {
        _reach->config.panels().setMultiplayerOpen(open);
    }));

    _role = _net->tools()->append(std::make_unique<Segmented>([this](const std::string &key) {
        _reach->config.panels().setNetRole(indexOf(ROLES, key));
        _reach->config.panels().setMultiplayerOpen(key != "alone");
    }));

    _role->setOptions({{.key = "alone", .label = "Off"},
                       {.key = "host", .label = "Host"},
                       {.key = "join", .label = "Join"}});

    // Last, so it sits against the far edge of the heading.
    _netReset = _net->tools()->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Refresh, [this] {
        _reach->ask("Reset the multiplayer settings?",
                  "The side this profile is on, the game it opens and every address, limit and "
                  "flag under it go back to their defaults. The rest of the profile is "
                  "untouched.",
                  "Reset", true, [this] { _reach->config.panels().clearMultiplayer(); });
    }));

    _netReset->size(Theme::control)->outlined();
    _netReset->fixedWidth = Theme::control;

    Box *body = _net->body();

    body->append(std::make_unique<Rule>());

    _netNote = body->append(std::make_unique<Label>());
    _netNote->font(400, Theme::fontSmall)->tone(Theme::of().faint)->wrap();

    // Hosting.
    _hosting = body->append(Box::row());
    _hosting->spacing(20.0)->cross(Box::Place::End);

    Box *type = _hosting->append(Box::column());

    type->spacing(6.0);
    type->append(std::make_unique<Label>("Game type"))->section();

    _gameType = type->append(std::make_unique<Segmented>([this](const std::string &key) {
        _reach->config.panels().setGameType(indexOf(TYPES, key) + 1);
    }));

    _gameType->setOptions({{.key = "coop", .label = "Co-op"},
                           {.key = "dm", .label = "Deathmatch"},
                           {.key = "altdm", .label = "Alt deathmatch"}});

    _players = _hosting->append(std::make_unique<Stepper>("Players", [this](const int value) {
        _reach->config.panels().setPlayers(value);
    }));

    _players->range(1, 8)->tooltip("How many the game is opened for, this machine included");
    _players->fixedWidth = 170.0;

    _netPort = _hosting->append(std::make_unique<Field>("Listen on port",
                                                        [this](const std::string &value) {
        _reach->config.panels().setNetPort(value);
    }));

    _netPort->placeholder("Default")->mono();
    _netPort->fixedWidth = 160.0;

    Box *listing = _hosting->append(Box::column());

    listing->spacing(6.0);
    listing->append(std::make_unique<Label>("Listing"))->section();

    _listed = listing->append(std::make_unique<Toggle>("Public", [this](const bool on) {
        _reach->config.panels().setListed(on);
    }));

    _listed->hint = "Puts the game on the master server's list, where anyone can find it. Off "
                    "keeps it to whoever has the address";
    _listed->fixedHeight = Theme::control;

    _hosting->append(std::make_unique<Spacer>());

    // Joining.
    _joining = body->append(Box::row());
    _joining->spacing(12.0)->cross(Box::Place::End);

    _host = _joining->append(std::make_unique<Field>("Address of the game",
                                                     [this](const std::string &value) {
        _reach->config.panels().setHost(value);
    }));

    _host->placeholder("A host name or an address")->mono();
    _host->stretch = 1.0;

    _joinPort = _joining->append(std::make_unique<Field>("Port",
                                                         [this](const std::string &value) {
        _reach->config.panels().setNetPort(value);
    }));

    _joinPort->placeholder("Default")->mono()->note("");
    _joinPort->fixedWidth = 160.0;

    // The rules of a hosted game.
    _rules = body->append(Box::column());
    _rules->spacing(12.0);

    _rules->append(std::make_unique<Rule>());
    _rules->append(std::make_unique<Label>("Rules of the game"))->section();

    Box *limits = _rules->append(Box::row());

    limits->spacing(12.0)->cross(Box::Place::End);

    _fragLimit = limits->append(std::make_unique<Field>("Frag limit",
                                                        [this](const std::string &value) {
        _reach->config.panels().setFragLimit(value);
    }));

    _fragLimit->placeholder("None")->mono();
    _fragLimit->stretch = 1.0;

    _timeLimit = limits->append(std::make_unique<Field>("Time limit",
                                                        [this](const std::string &value) {
        _reach->config.panels().setTimeLimit(value);
    }));

    _timeLimit->placeholder("None")->mono();
    _timeLimit->stretch = 1.0;

    _dmflags = limits->append(std::make_unique<Field>("dmflags",
                                                      [this](const std::string &value) {
        _reach->config.panels().setDmflags(value);
    }));

    _dmflags->placeholder("None")->mono();
    _dmflags->stretch = 1.0;

    _dmflags2 = limits->append(std::make_unique<Field>("dmflags2",
                                                       [this](const std::string &value) {
        _reach->config.panels().setDmflags2(value);
    }));

    _dmflags2->placeholder("None")->mono();
    _dmflags2->stretch = 1.0;

    _savegame = _rules->append(std::make_unique<Field>("Start from a save",
                                                       [this](const std::string &value) {
        _reach->config.panels().setSavegame(value);
    }));

    _savegame->placeholder("None · the game starts at its first map")->mono()
        ->note("Everyone joining drops into the host's saved game");

    _savegame->icon(Glyphs::Glyph::Folder, "Browse", [this] {
        _reach->picker.open(FilePicker::Action::Savegame, "Select a save game", Filters::save(), false, false,
                            false, FilePicker::Slot::Save);
    });

    // The connection, which folds on its own.
    _tuning = body->append(Box::column());
    _tuning->spacing(12.0);

    _tuning->append(std::make_unique<Rule>());

    Box *tuningHead = _tuning->append(Box::row());

    tuningHead->spacing(8.0)->cross(Box::Place::Centre);
    tuningHead->fixedHeight = 18.0;

    Label *said = tuningHead->append(std::make_unique<Label>("Connection"));

    said->section();
    said->onClick([this] {
        State::get().nav.tuning = !State::get().nav.tuning;

        _reach->touch();
    });

    _tuningSaid = tuningHead->append(std::make_unique<Label>());
    _tuningSaid->font(400, Theme::fontTiny)->tone(Theme::of().faint);

    tuningHead->append(std::make_unique<Spacer>());

    Box *knobs = _tuning->append(Box::row());

    knobs->spacing(20.0)->cross(Box::Place::End);

    Box *mode = knobs->append(Box::column());

    mode->spacing(6.0);
    mode->append(std::make_unique<Label>("Net mode"))->section();

    _netmode = mode->append(std::make_unique<Segmented>([this](const std::string &key) {
        _reach->config.panels().setNetmode(indexOf(MODES, key) - 1);
    }));

    _netmode->setOptions({{.key = "any", .label = "The port's own"},
                          {.key = "p2p", .label = "Peer to peer"},
                          {.key = "cs", .label = "Client/server"}});

    _dup = knobs->append(std::make_unique<Stepper>("Duplicate tics", [this](const int value) {
        _reach->config.panels().setDup(value);
    }));

    _dup->range(1, 9)->clearable(0, "Off")
        ->tooltip("Sends each tic more than once, which trades bandwidth for a connection that "
              "drops packets");
    _dup->fixedWidth = 170.0;

    Box *extra = knobs->append(Box::column());

    extra->spacing(6.0);
    extra->append(std::make_unique<Label>("Extra tic"))->section();

    _extratic = extra->append(std::make_unique<Segmented>([this](const std::string &key) {
        _reach->config.panels().setExtratic(key == "yes" ? 1 : 0);
    }));

    _extratic->setOptions({{.key = "no", .label = "Off"}, {.key = "yes", .label = "On"}});

    knobs->append(std::make_unique<Spacer>());
}

void ProfilePage::buildCommand(Box *into) {
    Panel *panel = into->append(std::make_unique<Panel>());
    Box *line = panel->append(Box::column());

    line->pad(16.0)->spacing(12.0);

    Box *head = line->append(Box::row());

    head->spacing(12.0)->cross(Box::Place::Centre);
    head->fixedHeight = Theme::control;

    panelTitle(head, "Command line");

    _budget = head->append(std::make_unique<Chip>("", "DOSBox runs only the first eleven "
        "commands it is given with -c and silently drops the rest."));

    _budget->plain();

    head->append(std::make_unique<Spacer>());

    _override = head->append(std::make_unique<Toggle>("Override", [this](const bool on) {
        _reach->config.profile().setCommandOverride(on);
    }));

    _override->hint = "Use a custom command instead of the generated one";

    _extra = line->append(std::make_unique<Field>("Extra arguments",
                                                  [this](const std::string &value) {
        _reach->config.profile().setExtra(value);
    }));

    _extra->placeholder("Passed to the source port as typed")->mono();

    _command = line->append(std::make_unique<Field>("The command",
                                                    [this](const std::string &value) {
        _reach->config.profile().setCommand(value);
    }));

    _command->placeholder("{source_port} -iwad {game} -file {addon_1}")->mono();

    _tokens = line->append(std::make_unique<Wrap>());
    _tokens->spacing(2.0, 2.0);

    for (const auto &[word, about] : TOKENS) {
        _tokens->append(std::make_unique<Chip>(word, about));
    }

    Panel *resolved = line->append(std::make_unique<Hug>(108.0));

    resolved->inset = true;
    resolved->hoverable = true;

    Box *inside = resolved->append(Box::row());

    inside->pad(12.0)->spacing(10.0)->cross(Box::Place::Start);

    _resolved = inside->append(std::make_unique<Label>());
    _resolved->stretch = 1.0;
    _resolved->font(Typeface::mono, Theme::fontSmall)->tone(Theme::of().muted)->wrap();
    _resolved->hint = "See the whole of it";
    _resolved->onClick([this] {
        _reach->showCommand();

        _reach->touch();
    });

    _copy = inside->append(std::make_unique<GlyphButton>(Glyphs::Glyph::Extract, [this] {
        Clipboard::write(State::get().cfg.commandLine);
        _reach->notify.success("The command line is on the clipboard.");
    }));

    _copy->size(26.0)->tooltip("Copy it");
    _copy->fixedWidth = 26.0;
    _copy->fixedHeight = 26.0;
}

// --- keeping up with the state -------------------------------------------------

void ProfilePage::syncRun() {
    const State::Cfg &cfg = State::get().cfg;

    _addPort->setVisible(cfg.ports.empty());
    _port->setVisible(!cfg.ports.empty());

    if (_port->visible()) {
        _port->setOptions(cfg.portNames);
        _port->setBadges(cfg.portBadges);
        _port->setCurrent(indexOf(cfg.portNames, cfg.port));
    }

    _addGame->setVisible(cfg.iwads.empty());
    _iwad->setVisible(!cfg.iwads.empty());

    if (_iwad->visible()) {
        _iwad->setOptions(cfg.iwadNames);
        _iwad->setCurrent(indexOf(cfg.iwadNames, cfg.iwad));
    }

    _map->setOptions(cfg.maps);
    _map->setCurrent(indexOf(cfg.maps, cfg.warp));

    _skill->setCurrent(cfg.skill - 1);
    _monsters->setCurrent(cfg.monsters - 1);

    // A DOS port prints into DOSBox's own window.
    _capture->setVisible(!cfg.dosPort);
    _capture->setChecked(cfg.captureOutput && !cfg.autoClose);
    _capture->setEnabled(!cfg.autoClose);
    _capture->hint = cfg.autoClose
        ? "Nothing to record while ZDL4 closes on launch: the log goes with the window. Turn "
          "that off in Settings."
        : "Takes what the source port prints into a log along the bottom of the window. Off, "
          "nothing is piped at all.";

    _fullscreen->setVisible(cfg.dosPort);
    _fullscreen->setChecked(cfg.dosFullscreen);

    _levelstat->setVisible(cfg.hasLevelstat);
    _levelstat->setChecked(cfg.levelstat);

    _sharedConfig->setVisible(cfg.profileConfigs && !cfg.dosPort);
    _sharedConfig->setChecked(cfg.sharedConfig);

    _directory->setValue(cfg.profileDirectory);

    _loaded->setVisible(!cfg.files.empty());
    _loaded->setText(std::cmp_equal(cfg.enabledCount ,cfg.files.size())
                         ? std::to_string(cfg.files.size()) + " loaded"
                         : std::to_string(cfg.enabledCount) + " of "
                               + std::to_string(cfg.files.size()) + " loaded");

    _clearFiles->setEnabled(!cfg.files.empty());

    _terminal->setVisible(cfg.captureOutput && !cfg.dosPort && !cfg.autoClose);
    _terminal->setEnabled(indexOf(State::get().runs.logged, cfg.profileKey) >= 0);
    _terminal->tooltip(_terminal->enabled() ? "Show what this profile printed"
                                        : "Nothing has been launched from this profile yet");

    _launch->setEnabled(ready());
    _launch->tooltip(ready()               ? cfg.commandLine
                 : cfg.commandOverride ? cfg.commandTrouble
                                       : "Pick a source port first");
}

void ProfilePage::syncReplay() {
    const State::Cfg &cfg = State::get().cfg;

    _replay->setVisible(cfg.replayRecords);

    if (!cfg.replayRecords) {
        return;
    }

    const bool broken = cfg.replayMode != 0 && cfg.replayFile.empty();
    const bool wrong = broken || !cfg.replayTrouble.empty();

    _replay->setOpen(cfg.replayOpen);
    _replay->setSaid(replaySummary(), wrong);

    _replay->pill()->setVisible(cfg.replayMode != 0);
    _replay->pill()->setText(cfg.replayMode == 1 ? "Recording" : "Playing");
    _replay->pill()->kind(wrong ? Pill::Kind::Warning : Pill::Kind::None);

    _replayMode->setCurrent(
        std::string(DEMO_MODES[static_cast<size_t>(std::clamp(cfg.replayMode, 0, 2))]));
    _replayReset->setEnabled(cfg.replaySet);
    _replayReset->tooltip(cfg.replaySet ? "Put every replay setting back to its default"
                                    : "Nothing here has been set");

    _replayRecord->setVisible(cfg.replayMode == 1);
    _replayPlay->setVisible(cfg.replayMode == 2);

    if (cfg.replayMode == 0) {
        _replayNote->setVisible(true);
        _replayNote->setText("This profile neither records nor plays anything back. Record "
                             "writes what is played into the profile's own replays folder; Play "
                             "runs one of them back.");
        _replayNote->tone(Theme::of().faint);
    } else if (!cfg.replayTrouble.empty()) {
        _replayNote->setVisible(true);
        _replayNote->setText(cfg.replayTrouble);
        _replayNote->tone(Theme::of().danger);
    } else if (broken) {
        _replayNote->setVisible(true);
        _replayNote->setText(
            cfg.replayMode == 1
                ? "Without a name there is nothing to record into, and the profile launches "
                  "without recording."
                : cfg.replayFiles.empty()
                    ? "This profile has recorded nothing yet, so there is nothing to play back. "
                      "Record writes a demo the next time it is launched."
                    : "Without a demo picked there is nothing to play back, and the profile "
                      "launches an ordinary game.");
        _replayNote->tone(Theme::of().warning);
    } else if (cfg.replayMode == 1 && cfg.replayNameTaken) {
        _replayNote->setVisible(true);
        _replayNote->setText("A demo by that name is in the folder already. Some ports record "
                             "into a numbered name beside it, others write over it.");
        _replayNote->tone(Theme::of().warning);
    } else {
        _replayNote->setVisible(false);
    }

    if (_replayName->text() != cfg.replayFile && cfg.replayMode == 1) {
        _replayName->setText(cfg.replayFile);
    }

    _replayFile->setOptions(cfg.replayFiles);
    _replayFile->setCurrent(cfg.replayIndex);
    _replayFile->setEnabled(!cfg.replayFiles.empty());
    _replayFile->placeholder(cfg.replayFiles.empty() ? "Nothing recorded yet" : "Nothing picked");

    std::vector<Segmented::Choice> speeds = {{.key = "played", .label = "As recorded"},
                                             {.key = "timed", .label = "Timed"}};

    if (cfg.replayFast) {
        speeds.push_back({.key = "fast", .label = "As fast as it draws"});
    }

    _replaySpeed->setOptions(std::move(speeds));
    _replaySpeed->setCurrent(
        std::string(SPEEDS[static_cast<size_t>(std::clamp(cfg.replayPlayback, 0, 2))]));
    _replaySpeed->parent()->setVisible(cfg.replayTimed);

    _replayPath->setVisible(!broken && cfg.replayMode != 0);
    _replayPath->setText(cfg.replayPath);

    const bool tunable = cfg.replayHasComplevel || cfg.replayHasLongtics || cfg.replayHasSoloNet;

    _replayTune->setVisible(cfg.replayMode == 1 && tunable);

    _complevel->setVisible(cfg.replayHasComplevel);
    _complevel->setOptions(cfg.replayComplevels);
    _complevel->setBadges(cfg.replayComplevelNumbers);
    _complevel->setCurrent(cfg.replayComplevel);

    _longtics->setVisible(cfg.replayHasLongtics);
    _longtics->setChecked(cfg.replayLongtics);

    _soloNet->setVisible(cfg.replayHasSoloNet);
    _soloNet->setChecked(cfg.replaySoloNet);
}

void ProfilePage::syncSaves() {
    const State::Cfg &cfg = State::get().cfg;

    _saves->setVisible(cfg.saveLoads);

    if (!cfg.saveLoads) {
        return;
    }

    const bool broken = cfg.saveEnabled && cfg.saveFile.empty();
    const bool homeless = cfg.saveFolder.empty();

    _saves->setOpen(cfg.saveOpen);
    _saves->setSaid(saveSummary(), broken || !cfg.saveTrouble.empty());

    _saveOn->setCurrent(cfg.saveEnabled ? "on" : "off");

    _saveFile->setOptions(cfg.saveFiles);
    _saveFile->setBadges(cfg.saveSlotLabels);
    _saveFile->setCurrent(cfg.saveIndex);
    _saveFile->setEnabled(!cfg.saveFiles.empty());
    _saveFile->placeholder(cfg.saveFiles.empty() ? "Nothing saved yet" : "Nothing picked");
    _saveFile->tooltip(cfg.saveSlots
                       ? "The saves in this profile's folder, newest first. The port is handed "
                         "the slot it sits in"
                       : "The saves in this profile's folder, newest first");

    if (!cfg.saveTrouble.empty()) {
        _saveNote->setVisible(true);
        _saveNote->setText(cfg.saveTrouble);
        _saveNote->tone(Theme::of().danger);
    } else if (homeless) {
        _saveNote->setVisible(true);
        _saveNote->setText("This profile launches on the settings the source port keeps for "
                           "itself, so its saves are the port's own and there is nothing here "
                           "to list. Give it settings of its own to keep them apart.");
        _saveNote->tone(Theme::of().faint);
    } else if (cfg.saveFiles.empty()) {
        _saveNote->setVisible(true);
        _saveNote->setText("Nothing has been saved in this profile yet. A game saved while it "
                           "is playing lands in its saves folder and shows up here.");
        _saveNote->tone(Theme::of().faint);
    } else if (broken) {
        _saveNote->setVisible(true);
        _saveNote->setText("Without a save picked there is nothing to load, and the profile "
                           "launches a new game.");
        _saveNote->tone(Theme::of().warning);
    } else if (!cfg.saveFile.empty() && cfg.replayMode != 0) {
        _saveNote->setVisible(true);
        _saveNote->setText(cfg.replayMode == 1
                               ? "A demo is being recorded, which starts where a new game "
                                 "starts, so the save is left out of the launch."
                               : "A demo is being played back, so the save is left out of the "
                                 "launch.");
        _saveNote->tone(Theme::of().warning);
    } else {
        _saveNote->setVisible(false);
    }

    _savePath->setVisible(!cfg.saveFile.empty());
    _savePath->setText(cfg.savePath);
}

void ProfilePage::syncNet() {
    const State::Cfg &cfg = State::get().cfg;

    _net->setVisible(cfg.netHosts || cfg.netJoins);

    if (!_net->visible()) {
        return;
    }

    const bool unsupported = (cfg.netRole == 1 && !cfg.netHosts)
        || (cfg.netRole == 2 && !cfg.netJoins);
    const bool broken = unsupported || (cfg.netRole == 2 && cfg.host.empty());

    _net->setOpen(cfg.multiplayerOpen);
    _net->setSaid(netSummary(), broken);

    _net->pill()->setVisible(cfg.netRole != 0);
    _net->pill()->setText(cfg.netRole == 2   ? "Joining"
                          : cfg.gameType == 1 ? "Co-op"
                          : cfg.gameType == 2 ? "Deathmatch"
                                              : "Alt deathmatch");
    _net->pill()->kind(broken ? Pill::Kind::Warning : Pill::Kind::None);

    _role->setCurrent(std::string(ROLES[static_cast<size_t>(std::clamp(cfg.netRole, 0, 2))]));
    _netReset->setEnabled(cfg.multiplayerSet);
    _netReset->tooltip(cfg.multiplayerSet ? "Put every multiplayer setting back to its default"
                                      : "Nothing here has been set");

    const bool hosting = cfg.netRole == 1 && cfg.netHosts;
    const bool joining = cfg.netRole == 2 && cfg.netJoins;

    _hosting->setVisible(hosting);
    _joining->setVisible(joining);
    _rules->setVisible(hosting);

    if (cfg.netRole == 0) {
        _netNote->setVisible(true);
        _netNote->setText("This profile starts a game for one. Host opens a game other machines "
                          "can connect to; Join connects to one somebody else is running.");
        _netNote->tone(Theme::of().faint);
    } else if (unsupported) {
        _netNote->setVisible(true);
        _netNote->setText(cfg.netRole == 1
                              ? "This port opens no game of its own: a server program beside it "
                                "does, and the port joins that. Nothing under here reaches the "
                                "launch."
                              : "This port has no way to join a game, so nothing under here "
                                "reaches the launch.");
        _netNote->tone(Theme::of().warning);
    } else if (joining) {
        _netNote->setVisible(true);
        _netNote->setText(cfg.host.empty()
                              ? "Without an address there is nothing to join, and the profile "
                                "launches a single player game."
                              : "How the game is played is the host's to decide, so there is "
                                "nothing else to set on this side.");
        _netNote->tone(cfg.host.empty() ? Theme::of().warning : Theme::of().faint);
    } else {
        _netNote->setVisible(false);
    }

    _gameType->setCurrent(
        std::string(TYPES[static_cast<size_t>(std::clamp(cfg.gameType - 1, 0, 2))]));

    _players->setVisible(cfg.netPlayers);
    _players->setValue(cfg.players);

    _listed->parent()->setVisible(cfg.netListing);
    _listed->setChecked(cfg.listed);

    if (_netPort->text() != cfg.netPort) {
        _netPort->setText(cfg.netPort);
    }

    if (_host->text() != cfg.host) {
        _host->setText(cfg.host);
    }

    if (_joinPort->text() != cfg.netPort) {
        _joinPort->setText(cfg.netPort);
    }

    _fragLimit->setVisible(cfg.netFragLimit);
    _dmflags->setVisible(cfg.netFlags);
    _dmflags2->setVisible(cfg.netFlags);
    _savegame->setVisible(cfg.netSavegame);

    if (_fragLimit->text() != cfg.fragLimit) {
        _fragLimit->setText(cfg.fragLimit);
    }

    if (_timeLimit->text() != cfg.timeLimit) {
        _timeLimit->setText(cfg.timeLimit);
    }

    if (_dmflags->text() != cfg.dmflags) {
        _dmflags->setText(cfg.dmflags);
    }

    if (_dmflags2->text() != cfg.dmflags2) {
        _dmflags2->setText(cfg.dmflags2);
    }

    if (_savegame->text() != cfg.savegame) {
        _savegame->setText(cfg.savegame);
    }

    const bool tunable = cfg.netRole != 0 && !unsupported
        && (cfg.netExtratic || cfg.hasNetmode || cfg.netDup);

    _tuning->setVisible(tunable);

    _tuningSaid->setVisible(!State::get().nav.tuning);
    _tuningSaid->setText(tuningSummary());

    // The knobs are the row after the heading.
    _tuning->children().back()->setVisible(State::get().nav.tuning);

    _netmode->parent()->setVisible(cfg.hasNetmode);
    _netmode->setCurrent(
        std::string(MODES[static_cast<size_t>(std::clamp(cfg.netmode + 1, 0, 2))]));

    _dup->setVisible(cfg.netDup);
    _dup->setValue(cfg.dup);

    _extratic->parent()->setVisible(cfg.netExtratic);
    _extratic->setCurrent(cfg.extratic == 1 ? "yes" : "no");
}

void ProfilePage::syncCommand() {
    const State::Cfg &cfg = State::get().cfg;

    _override->setChecked(cfg.commandOverride);

    _extra->setVisible(!cfg.commandOverride);
    _command->setVisible(cfg.commandOverride);
    _tokens->setVisible(cfg.commandOverride);

    if (!cfg.commandOverride && _extra->text() != cfg.extra) {
        _extra->setText(cfg.extra);
    }

    if (cfg.commandOverride && _command->text() != cfg.command) {
        _command->setText(cfg.command);
    }

    _resolved->setText(!cfg.commandTrouble.empty()  ? cfg.commandTrouble
                       : !cfg.commandLine.empty()   ? cfg.commandLine
                       : cfg.dosPort && cfg.dosbox.empty() && cfg.systemDosbox.empty()
                           ? "A DOS source port, and no DOSBox to run it in. Set one in "
                             "Settings."
                           : "Nothing to run yet.");

    _resolved->tone(!cfg.commandTrouble.empty() ? Theme::of().danger
                    : ready()                   ? Theme::of().muted
                                                : Theme::of().faint);

    _copy->setEnabled(!cfg.commandLine.empty());

    // Only a launch that runs DOSBox spends anything, generated or typed.
    _budget->setVisible(cfg.dosCommands > 0);
    _budget->setTight(cfg.dosCommands >= Dos::COMMANDS);
    _budget->setText(std::to_string(cfg.dosCommands) + " / " + std::to_string(Dos::COMMANDS)
                     + " DOSBox commands");
}

void ProfilePage::sync() {
    const State::Cfg &cfg = State::get().cfg;
    const bool empty = cfg.profileIndex < 0;

    _none->setVisible(empty);
    children().front()->setVisible(!empty);

    if (empty) {
        return;
    }

    _chooser->setSaid(summary(), ready());

    syncRun();
    syncReplay();
    syncSaves();
    syncNet();
    syncCommand();
}

void ProfilePage::showMenu() {
    if (root() == nullptr) {
        return;
    }

    const State::Cfg &cfg = State::get().cfg;

    const std::vector<Menu::Row> rows = {
        Menu::item(ProfileMenuAction::Rename, "Rename", Glyphs::Glyph::Edit),
        Menu::item(ProfileMenuAction::Duplicate, "Duplicate", Glyphs::Glyph::Extract),
        Menu::item(ProfileMenuAction::Clear, "Empty this profile",
                   Glyphs::Glyph::Refresh),
        Menu::rule(),
        Menu::item(ProfileMenuAction::CopyConfig, "Copy port config", Glyphs::Glyph::Copy,
                   false, cfg.port.empty()),
        Menu::rule(),
        Menu::item(ProfileMenuAction::LoadZdl, "Import a .zdl", Glyphs::Glyph::Download),
        Menu::item(ProfileMenuAction::SaveZdl, "Save as .zdl", Glyphs::Glyph::Save),
        Menu::rule(),
        Menu::item(ProfileMenuAction::Delete, "Delete this profile", Glyphs::Glyph::Trash,
                   true),
    };

    const double tall = Menu::heightOf(rows);
    const BLRect cog = _cog->box();

    Widget *menu = root()->layer(Root::POPUPS)->add(
        std::make_unique<Menu>(rows, [this](const int action) {
            root()->dismiss();

            const State::Cfg &held = State::get().cfg;

            switch (static_cast<ProfileMenuAction>(action)) {
                case ProfileMenuAction::Rename:
                    _reach->prompt("Rename profile", "Name", held.profileName, "Rename",
                                   [this](const std::string &named) {
                                       _reach->config.profile().renameProfile(named);
                                   });
                    break;
                case ProfileMenuAction::Duplicate:
                    _reach->config.profile().duplicateProfile();
                    break;
                case ProfileMenuAction::CopyConfig:
                    _reach->copyConfig();
                    break;
                case ProfileMenuAction::Clear:
                    _reach->ask("Empty \"" + held.profileName + "\"?",
                                "Everything this profile launches is emptied: the port, the "
                                "game, the files and the multiplayer settings. The profile "
                                "itself stays.",
                                "Empty it", true,
                                [this] { _reach->config.profile().clearProfile(); });
                    break;
                case ProfileMenuAction::Delete:
                    _reach->ask("Delete \"" + held.profileName + "\"?",
                                "The profile and everything in it goes. The files it loaded are "
                                "left alone.",
                                "Delete", true, [this] {
                                    _reach->config.profile().removeProfile();
                                    _reach->go(State::Page::Library);
                                });
                    break;
                case ProfileMenuAction::LoadZdl:
                    _reach->picker.open(FilePicker::Action::LoadZdl, "Load a .zdl launch config", Filters::zdl(),
                                        false, false, false, FilePicker::Slot::Zdl);
                    break;
                case ProfileMenuAction::SaveZdl:
                    _reach->picker.openSave(FilePicker::Action::SaveZdl, "Save this profile as a .zdl",
                                            Filters::zdl(), FilePicker::Slot::Zdl,
                                            ProfileBridge::zdlFileName());
                    break;
            }
        }));

    menu->place(BLRect{cog.x + cog.w - Menu::WIDTH, cog.y + cog.h + 4.0, Menu::WIDTH, tall},
                root()->type());

    Widget  const*held = menu;

    root()->setDismiss([this, held] {
        const BLRect was = held->box();

        root()->layer(Root::POPUPS)->erase(held);
        root()->damage(was);
    }, _cog);

    menu->invalidate();
}

}
