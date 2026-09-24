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

#include <memory>
#include <string>
#include <vector>

#include <benchmark/benchmark.h>

#include "ttk/draw/Theme.h"
#include "ttk/toolkit/controls/Button.h"
#include "ttk/toolkit/controls/Check.h"
#include "ttk/toolkit/controls/Chip.h"
#include "ttk/toolkit/controls/Fact.h"
#include "ttk/toolkit/controls/Field.h"
#include "ttk/toolkit/controls/GlyphButton.h"
#include "ttk/toolkit/controls/Label.h"
#include "ttk/toolkit/controls/MultistateSwitch.h"
#include "ttk/toolkit/controls/Pill.h"
#include "ttk/toolkit/controls/Select.h"
#include "ttk/toolkit/controls/StatusIndicator.h"
#include "ttk/toolkit/controls/Stepper.h"
#include "ttk/toolkit/controls/TextBox.h"
#include "ttk/toolkit/controls/TextView.h"
#include "ttk/toolkit/controls/Toggle.h"

#include "support/Canvas.h"
#include "support/Fixtures.h"

using namespace ttk;

namespace {

bench::Canvas &sheet() {
    static bench::Canvas made(1280, 800);

    return made;
}

std::vector<std::string> options(const int count) {
    std::vector<std::string> out;

    out.reserve(static_cast<size_t>(count));

    for (int at = 0; at < count; ++at) {
        out.push_back("Option " + std::to_string(at) + " " + bench::Fixtures::words(2, static_cast<unsigned>(at)));
    }

    return out;
}

std::unique_ptr<ttk::Button> button(const ttk::Button::Kind kind) {
    auto made = std::make_unique<ttk::Button>("Launch", [] {});

    made->kind(kind);

    return made;
}

std::unique_ptr<ttk::Select> select(const int count) {
    auto made = std::make_unique<ttk::Select>("Source port", [](int) {});

    made->set_options(options(count));
    made->set_current(count / 2);

    return made;
}

std::unique_ptr<ttk::MultistateSwitch> multistateSwitch() {
    auto made = std::make_unique<ttk::MultistateSwitch>([](int) {});

    made->set_options({{.value = 0, .label = "Profiles", .badge = false},
                      {.value = 1, .label = "Games", .badge = true}});
    made->set_current(0);

    return made;
}

std::unique_ptr<ttk::Field> field() {
    auto made = std::make_unique<ttk::Field>("Command line", [](const std::string &) {});

    made->placeholder("Leave empty for the default")
        ->value("gzdoom -iwad DOOM2.WAD -file eviternity.wad")
        ->note("Placeholders are expanded at launch");

    return made;
}

std::unique_ptr<ttk::TextView> textView(const int rows) {
    auto made = std::make_unique<ttk::TextView>();

    made->set_rows(bench::Fixtures::lines(rows));

    return made;
}

void Button_build(benchmark::State &state) {
    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(
            bench::mount(sheet(), button(ttk::Button::Kind::Primary)));
    }
}

BENCHMARK(Button_build);

void Field_build(benchmark::State &state) {
    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::mount(sheet(), field(), 560.0));
    }
}

BENCHMARK(Field_build);

void Select_build(benchmark::State &state) {
    const auto count = static_cast<int>(state.range(0));

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::mount(sheet(), select(count), 280.0));
    }
}

BENCHMARK(Select_build)->Arg(8)->Arg(128);

void TextView_build(benchmark::State &state) {
    const auto rows = static_cast<int>(state.range(0));

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::mount(sheet(), textView(rows), 900.0, 600.0));
    }

    state.SetItemsProcessed(state.iterations() * rows);
}

BENCHMARK(TextView_build)->Arg(64)->Arg(1024);

void Button_naturalWidth(benchmark::State &state) {
    ttk::Button *made = bench::mount(sheet(), button(ttk::Button::Kind::Default));

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(made->natural_width(sheet().type()));
    }
}

BENCHMARK(Button_naturalWidth);

void Label_naturalWidth(benchmark::State &state) {
    ttk::Label *made = bench::mount(sheet(), std::make_unique<ttk::Label>("Ultra-Violence"));

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(made->natural_width(sheet().type()));
    }
}

BENCHMARK(Label_naturalWidth);

void Label_wrapHeight(benchmark::State &state) {
    auto held = std::make_unique<ttk::Label>(bench::Fixtures::paragraph(8));

    held->wrap();

    ttk::Label *made = bench::mount(sheet(), std::move(held), 420.0);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(made->natural_height(sheet().type(), 420.0));
    }
}

BENCHMARK(Label_wrapHeight);

void MultistateSwitch_naturalWidth(benchmark::State &state) {
    ttk::MultistateSwitch *made = bench::mount(sheet(), multistateSwitch());

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(made->natural_width(sheet().type()));
    }
}

BENCHMARK(MultistateSwitch_naturalWidth);

void Select_naturalWidth(benchmark::State &state) {
    ttk::Select *made = bench::mount(sheet(), select(64), 280.0);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(made->natural_width(sheet().type()));
    }
}

BENCHMARK(Select_naturalWidth);

void Button_paint(benchmark::State &state) {
    ttk::Button *made =
        bench::mount(sheet(), button(static_cast<ttk::Button::Kind>(state.range(0))));

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(Button_paint)
    ->Arg(static_cast<int>(ttk::Button::Kind::Default))
    ->Arg(static_cast<int>(ttk::Button::Kind::Primary))
    ->Arg(static_cast<int>(ttk::Button::Kind::Danger))
    ->Arg(static_cast<int>(ttk::Button::Kind::Ghost));

void Check_paint(benchmark::State &state) {
    ttk::Check *made = bench::mount(sheet(), std::make_unique<ttk::Check>([](bool) {}));

    made->set_checked(true);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(Check_paint);

void Chip_paint(benchmark::State &state) {
    ttk::Chip *made =
        bench::mount(sheet(), std::make_unique<ttk::Chip>("-complevel 9", "Compatibility"));

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(Chip_paint);

void Fact_paint(benchmark::State &state) {
    ttk::Fact *made = bench::mount(
        sheet(), std::make_unique<ttk::Fact>("Config", "/config/qzdl/doom2.ini"),
        420.0);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(Fact_paint);

void Field_paint(benchmark::State &state) {
    ttk::Field *made = bench::mount(sheet(), field(), 560.0);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(Field_paint);

void GlyphButton_paint(benchmark::State &state) {
    ttk::GlyphButton *made = bench::mount(
        sheet(), std::make_unique<ttk::GlyphButton>(Glyphs::Glyph::Cog, [] {}));

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(GlyphButton_paint);

void Label_paint(benchmark::State &state) {
    ttk::Label *made =
        bench::mount(sheet(), std::make_unique<ttk::Label>("Knee Deep in the Dead"));

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(Label_paint);

void Label_paintWrapped(benchmark::State &state) {
    auto held = std::make_unique<ttk::Label>(bench::Fixtures::paragraph(8));

    held->wrap();

    ttk::Label *made = bench::mount(sheet(), std::move(held), 420.0);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(Label_paintWrapped);

void Pill_paint(benchmark::State &state) {
    auto held = std::make_unique<ttk::Pill>("Running");

    held->kind(ttk::Pill::Kind::Success)->dot(true);

    ttk::Pill *made = bench::mount(sheet(), std::move(held));

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(Pill_paint);

void MultistateSwitch_paint(benchmark::State &state) {
    ttk::MultistateSwitch *made = bench::mount(sheet(), multistateSwitch());

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(MultistateSwitch_paint);

void Select_paint(benchmark::State &state) {
    ttk::Select *made = bench::mount(sheet(), select(64), 280.0);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(Select_paint);

void StatusIndicator_paint(benchmark::State &state) {
    auto held = std::make_unique<ttk::StatusIndicator>();

    held->set(ttk::StatusIndicator::Status::Running);

    ttk::StatusIndicator *made = bench::mount(sheet(), std::move(held));

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(StatusIndicator_paint);

void Stepper_paint(benchmark::State &state) {
    auto held = std::make_unique<ttk::Stepper>("Skill", [](int) {});

    held->range(1, 5);
    held->set_value(4);

    ttk::Stepper *made = bench::mount(sheet(), std::move(held), 220.0);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(Stepper_paint);

void TextBox_paint(benchmark::State &state) {
    auto held = std::make_unique<ttk::TextBox>([](const std::string &) {});

    held->set_text("gzdoom -iwad DOOM2.WAD -file eviternity.wad -skill 4");

    ttk::TextBox *made = bench::mount(sheet(), std::move(held), 420.0, 32.0);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(TextBox_paint);

// The run log, which is a full-window view of monospaced rows.
void TextView_paint(benchmark::State &state) {
    ttk::TextView *made =
        bench::mount(sheet(), textView(static_cast<int>(state.range(0))), 900.0, 600.0);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(TextView_paint)->Arg(64)->Arg(1024);

void Toggle_paint(benchmark::State &state) {
    auto held = std::make_unique<ttk::Toggle>("Close ZDL when a game starts", [](bool) {});

    held->set_checked(true);

    ttk::Toggle *made = bench::mount(sheet(), std::move(held), 420.0);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(Toggle_paint);

void Button_pressRelease(benchmark::State &state) {
    ttk::Button *made = bench::mount(sheet(), button(ttk::Button::Kind::Primary));

    const ttk::Pointer at{.x = 40.0, .y = 20.0};

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(made->press(at));

        made->release(at);
    }
}

BENCHMARK(Button_pressRelease);

void Button_enterLeave(benchmark::State &state) {
    ttk::Button *made = bench::mount(sheet(), button(ttk::Button::Kind::Primary));

    for ([[maybe_unused]] auto step : state) {
        made->enter();
        made->leave();
    }
}

BENCHMARK(Button_enterLeave);

void TextBox_wrote(benchmark::State &state) {
    auto held = std::make_unique<ttk::TextBox>([](const std::string &) {});

    ttk::TextBox *made = bench::mount(sheet(), std::move(held), 420.0, 32.0);

    for ([[maybe_unused]] auto step : state) {
        made->wrote("a");

        if (made->text().size() > 256) {
            made->set_text({});
        }
    }
}

BENCHMARK(TextBox_wrote);

void TextBox_caretMove(benchmark::State &state) {
    auto held = std::make_unique<ttk::TextBox>([](const std::string &) {});

    held->set_text("gzdoom -iwad DOOM2.WAD -file eviternity.wad -skill 4");

    ttk::TextBox *made = bench::mount(sheet(), std::move(held), 420.0, 32.0);

    const ttk::Key left{.code = ttk::Code::Left};
    const ttk::Key right{.code = ttk::Code::Right};

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(made->key(right));
        benchmark::DoNotOptimize(made->key(left));
    }
}

BENCHMARK(TextBox_caretMove);

void MultistateSwitch_hover(benchmark::State &state) {
    ttk::MultistateSwitch *made = bench::mount(sheet(), multistateSwitch());

    double x = 0.0;

    for ([[maybe_unused]] auto step : state) {
        x = x > made->box().w ? 0.0 : x + 3.0;

        made->hover(ttk::Pointer{.x = x, .y = 20.0});
    }
}

BENCHMARK(MultistateSwitch_hover);

void Select_openClose(benchmark::State &state) {
    ttk::Select *made = bench::mount(sheet(), select(64), 280.0);

    // The caption sits above the frame. The press has to land on the frame.
    const ttk::Pointer at{.x = made->box().w / 2.0, .y = made->box().h - 16.0};

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(made->press(at));

        made->release(at);

        benchmark::DoNotOptimize(made->open());

        made->close();
    }
}

BENCHMARK(Select_openClose);

}
