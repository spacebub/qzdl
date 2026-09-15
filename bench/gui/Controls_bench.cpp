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

#include "gui/draw/Theme.h"
#include "gui/toolkit/controls/Button.h"
#include "gui/toolkit/controls/Check.h"
#include "gui/toolkit/controls/Chip.h"
#include "gui/toolkit/controls/Fact.h"
#include "gui/toolkit/controls/Field.h"
#include "gui/toolkit/controls/GlyphButton.h"
#include "gui/toolkit/controls/Label.h"
#include "gui/toolkit/controls/Pill.h"
#include "gui/toolkit/controls/MultistateSwitch.h"
#include "gui/toolkit/controls/Select.h"
#include "gui/toolkit/controls/StatusIndicator.h"
#include "gui/toolkit/controls/Stepper.h"
#include "gui/toolkit/controls/TextBox.h"
#include "gui/toolkit/controls/TextView.h"
#include "gui/toolkit/controls/Toggle.h"
#include "support/Canvas.h"
#include "support/Fixtures.h"

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

std::unique_ptr<toolkit::Button> button(const toolkit::Button::Kind kind) {
    auto made = std::make_unique<toolkit::Button>("Launch", [] {});

    made->kind(kind);

    return made;
}

std::unique_ptr<toolkit::Select> select(const int count) {
    auto made = std::make_unique<toolkit::Select>("Source port", [](int) {});

    made->setOptions(options(count));
    made->setCurrent(count / 2);

    return made;
}

std::unique_ptr<toolkit::MultistateSwitch> multistateSwitch() {
    auto made = std::make_unique<toolkit::MultistateSwitch>([](int) {});

    made->setOptions({{.value = 0, .label = "Profiles", .badge = false},
                      {.value = 1, .label = "Games", .badge = true}});
    made->setCurrent(0);

    return made;
}

std::unique_ptr<toolkit::Field> field() {
    auto made = std::make_unique<toolkit::Field>("Command line", [](const std::string &) {});

    made->placeholder("Leave empty for the default")
        ->value("gzdoom -iwad DOOM2.WAD -file eviternity.wad")
        ->note("Placeholders are expanded at launch");

    return made;
}

std::unique_ptr<toolkit::TextView> textView(const int rows) {
    auto made = std::make_unique<toolkit::TextView>();

    made->setRows(bench::Fixtures::lines(rows));

    return made;
}

void Button_build(benchmark::State &state) {
    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(
            bench::mount(sheet(), button(toolkit::Button::Kind::Primary)));
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
    toolkit::Button *made = bench::mount(sheet(), button(toolkit::Button::Kind::Default));

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(made->naturalWidth(sheet().type()));
    }
}

BENCHMARK(Button_naturalWidth);

void Label_naturalWidth(benchmark::State &state) {
    toolkit::Label *made = bench::mount(sheet(), std::make_unique<toolkit::Label>("Ultra-Violence"));

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(made->naturalWidth(sheet().type()));
    }
}

BENCHMARK(Label_naturalWidth);

void Label_wrapHeight(benchmark::State &state) {
    auto held = std::make_unique<toolkit::Label>(bench::Fixtures::paragraph(8));

    held->wrap();

    toolkit::Label *made = bench::mount(sheet(), std::move(held), 420.0);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(made->naturalHeight(sheet().type(), 420.0));
    }
}

BENCHMARK(Label_wrapHeight);

void MultistateSwitch_naturalWidth(benchmark::State &state) {
    toolkit::MultistateSwitch *made = bench::mount(sheet(), multistateSwitch());

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(made->naturalWidth(sheet().type()));
    }
}

BENCHMARK(MultistateSwitch_naturalWidth);

void Select_naturalWidth(benchmark::State &state) {
    toolkit::Select *made = bench::mount(sheet(), select(64), 280.0);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(made->naturalWidth(sheet().type()));
    }
}

BENCHMARK(Select_naturalWidth);

void Button_paint(benchmark::State &state) {
    toolkit::Button *made =
        bench::mount(sheet(), button(static_cast<toolkit::Button::Kind>(state.range(0))));

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(Button_paint)
    ->Arg(static_cast<int>(toolkit::Button::Kind::Default))
    ->Arg(static_cast<int>(toolkit::Button::Kind::Primary))
    ->Arg(static_cast<int>(toolkit::Button::Kind::Danger))
    ->Arg(static_cast<int>(toolkit::Button::Kind::Ghost));

void Check_paint(benchmark::State &state) {
    toolkit::Check *made = bench::mount(sheet(), std::make_unique<toolkit::Check>([](bool) {}));

    made->checked = true;

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(Check_paint);

void Chip_paint(benchmark::State &state) {
    toolkit::Chip *made =
        bench::mount(sheet(), std::make_unique<toolkit::Chip>("-complevel 9", "Compatibility"));

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(Chip_paint);

void Fact_paint(benchmark::State &state) {
    toolkit::Fact *made = bench::mount(
        sheet(), std::make_unique<toolkit::Fact>("Config", "/config/qzdl/doom2.ini"),
        420.0);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(Fact_paint);

void Field_paint(benchmark::State &state) {
    toolkit::Field *made = bench::mount(sheet(), field(), 560.0);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(Field_paint);

void GlyphButton_paint(benchmark::State &state) {
    toolkit::GlyphButton *made = bench::mount(
        sheet(), std::make_unique<toolkit::GlyphButton>(Glyphs::Glyph::Cog, [] {}));

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(GlyphButton_paint);

void Label_paint(benchmark::State &state) {
    toolkit::Label *made =
        bench::mount(sheet(), std::make_unique<toolkit::Label>("Knee Deep in the Dead"));

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(Label_paint);

void Label_paintWrapped(benchmark::State &state) {
    auto held = std::make_unique<toolkit::Label>(bench::Fixtures::paragraph(8));

    held->wrap();

    toolkit::Label *made = bench::mount(sheet(), std::move(held), 420.0);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(Label_paintWrapped);

void Pill_paint(benchmark::State &state) {
    auto held = std::make_unique<toolkit::Pill>("Running");

    held->kind(toolkit::Pill::Kind::Success)->dot(true);

    toolkit::Pill *made = bench::mount(sheet(), std::move(held));

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(Pill_paint);

void MultistateSwitch_paint(benchmark::State &state) {
    toolkit::MultistateSwitch *made = bench::mount(sheet(), multistateSwitch());

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(MultistateSwitch_paint);

void Select_paint(benchmark::State &state) {
    toolkit::Select *made = bench::mount(sheet(), select(64), 280.0);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(Select_paint);

void StatusIndicator_paint(benchmark::State &state) {
    auto held = std::make_unique<toolkit::StatusIndicator>();

    held->set(toolkit::StatusIndicator::Status::Running);

    toolkit::StatusIndicator *made = bench::mount(sheet(), std::move(held));

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(StatusIndicator_paint);

void Stepper_paint(benchmark::State &state) {
    auto held = std::make_unique<toolkit::Stepper>("Skill", [](int) {});

    held->range(1, 5);
    held->setValue(4);

    toolkit::Stepper *made = bench::mount(sheet(), std::move(held), 220.0);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(Stepper_paint);

void TextBox_paint(benchmark::State &state) {
    auto held = std::make_unique<toolkit::TextBox>([](const std::string &) {});

    held->setText("gzdoom -iwad DOOM2.WAD -file eviternity.wad -skill 4");

    toolkit::TextBox *made = bench::mount(sheet(), std::move(held), 420.0, 32.0);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(TextBox_paint);

// The run log, which is a full-window view of monospaced rows.
void TextView_paint(benchmark::State &state) {
    toolkit::TextView *made =
        bench::mount(sheet(), textView(static_cast<int>(state.range(0))), 900.0, 600.0);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(TextView_paint)->Arg(64)->Arg(1024);

void Toggle_paint(benchmark::State &state) {
    auto held = std::make_unique<toolkit::Toggle>("Close ZDL when a game starts", [](bool) {});

    held->setChecked(true);

    toolkit::Toggle *made = bench::mount(sheet(), std::move(held), 420.0);

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(bench::paintOnce(sheet(), *made));
    }
}

BENCHMARK(Toggle_paint);

void Button_pressRelease(benchmark::State &state) {
    toolkit::Button *made = bench::mount(sheet(), button(toolkit::Button::Kind::Primary));

    const toolkit::Pointer at{.x = 40.0, .y = 20.0};

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(made->press(at));

        made->release(at);
    }
}

BENCHMARK(Button_pressRelease);

void Button_enterLeave(benchmark::State &state) {
    toolkit::Button *made = bench::mount(sheet(), button(toolkit::Button::Kind::Primary));

    for ([[maybe_unused]] auto step : state) {
        made->enter();
        made->leave();
    }
}

BENCHMARK(Button_enterLeave);

void TextBox_wrote(benchmark::State &state) {
    auto held = std::make_unique<toolkit::TextBox>([](const std::string &) {});

    toolkit::TextBox *made = bench::mount(sheet(), std::move(held), 420.0, 32.0);

    for ([[maybe_unused]] auto step : state) {
        made->wrote("a");

        if (made->text().size() > 256) {
            made->setText({});
        }
    }
}

BENCHMARK(TextBox_wrote);

void TextBox_caretMove(benchmark::State &state) {
    auto held = std::make_unique<toolkit::TextBox>([](const std::string &) {});

    held->setText("gzdoom -iwad DOOM2.WAD -file eviternity.wad -skill 4");

    toolkit::TextBox *made = bench::mount(sheet(), std::move(held), 420.0, 32.0);

    const toolkit::Key left{.code = toolkit::Code::Left};
    const toolkit::Key right{.code = toolkit::Code::Right};

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(made->key(right));
        benchmark::DoNotOptimize(made->key(left));
    }
}

BENCHMARK(TextBox_caretMove);

void MultistateSwitch_hover(benchmark::State &state) {
    toolkit::MultistateSwitch *made = bench::mount(sheet(), multistateSwitch());

    double x = 0.0;

    for ([[maybe_unused]] auto step : state) {
        x = x > made->box().w ? 0.0 : x + 3.0;

        made->hover(toolkit::Pointer{.x = x, .y = 20.0});
    }
}

BENCHMARK(MultistateSwitch_hover);

void Select_openClose(benchmark::State &state) {
    toolkit::Select *made = bench::mount(sheet(), select(64), 280.0);

    // The caption sits above the frame. The press has to land on the frame.
    const toolkit::Pointer at{.x = made->box().w / 2.0, .y = made->box().h - 16.0};

    for ([[maybe_unused]] auto step : state) {
        benchmark::DoNotOptimize(made->press(at));

        made->release(at);

        benchmark::DoNotOptimize(made->open());

        made->close();
    }
}

BENCHMARK(Select_openClose);

}
