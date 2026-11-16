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
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "ttk/system/Text.h"

#include "Launch.h"
#include "core/launch/Arguments.h"
#include "core/launch/Command.h"
#include "core/launch/Dos.h"
#include "support/Corpus.h"
#include "support/Sandbox.h"

namespace {

namespace fs = std::filesystem;

using launch::Strings;

class CommandTest : public ::testing::Test {
protected:
    void SetUp() override { fs::remove_all(launch::profiles()); }

    static Config typed(const std::string &command, const char *program = "/games/gzdoom") {
        Config config = launch::single(program);

        config.activeProfile().set(Switch::customCommand, true);
        config.activeProfile().command = command;

        return config;
    }

    static std::string iwad() { return tests::Corpus::iwad().string(); }

    static fs::path own() { return launch::profiles() / "alpha"; }
};

}

TEST_F(CommandTest, AnEmptyCommandHasNothingToRun) {
    std::string error;

    EXPECT_TRUE(Command::custom(typed("   "), &error).empty());
    EXPECT_EQ(error, "There is nothing here to run.");
}

TEST_F(CommandTest, PlainTokensPassThroughSplitAsAShellWould) {
    EXPECT_EQ(Command::custom(typed("run 'two words' -x")), (Strings{"run", "two words", "-x"}));
}

TEST_F(CommandTest, ThePortAndGameAreFilledIn) {
    EXPECT_EQ(Command::custom(typed("{source_port} -iwad {game}")), (Strings{"/games/gzdoom", "-iwad", iwad()}));
}

TEST_F(CommandTest, APlaceholderInsideATokenIsFilledInPlace) {
    EXPECT_EQ(Command::custom(typed("--game={game}")), (Strings{"--game=" + iwad()}));
}

TEST_F(CommandTest, SeveralPlaceholdersInOneTokenAreAllFilled) {
    EXPECT_EQ(Command::custom(typed("{source_port}:{game}")), (Strings{"/games/gzdoom:" + iwad()}));
}

TEST_F(CommandTest, AnUnclosedBraceIsKeptAsWritten) {
    EXPECT_EQ(Command::custom(typed("run {game")), (Strings{"run", "{game"}));
}

TEST_F(CommandTest, AddonsCountFromOne) {
    Config config = typed("{addon_2} {addon_1}");

    config.activeProfile().files = {{.file = "/mods/a.wad"}, {.file = "/mods/b.wad", .enabled = false}};

    EXPECT_EQ(Command::custom(config), (Strings{"/mods/b.wad", "/mods/a.wad"}));
}

TEST_F(CommandTest, AddonZeroIsRefused) {
    Config config = typed("{addon_0}");
    std::string error;

    config.activeProfile().files = {{.file = "/mods/a.wad"}};

    EXPECT_TRUE(Command::custom(config, &error).empty());
    EXPECT_EQ(error, "{addon_0} has to end in a number, counting from one.");
}

TEST_F(CommandTest, AnAddonWithoutANumberIsRefused) {
    std::string error;

    EXPECT_TRUE(Command::custom(typed("{addon_x}"), &error).empty());
    EXPECT_EQ(error, "{addon_x} has to end in a number, counting from one.");
}

TEST_F(CommandTest, AnAddonPastTheListIsRefusedWithTheCount) {
    Config config = typed("{addon_3}");
    std::string error;

    config.activeProfile().files = {{.file = "/mods/a.wad"}, {.file = "/mods/b.wad"}};

    EXPECT_TRUE(Command::custom(config, &error).empty());
    EXPECT_EQ(error, "This profile has 2 add-ons, so there is nothing for {addon_3}.");
}

TEST_F(CommandTest, AnAddonOfAnEmptyListIsRefused) {
    std::string error;

    EXPECT_TRUE(Command::custom(typed("{addon_1}"), &error).empty());
    EXPECT_EQ(error, "This profile has no add-ons, so there is nothing for {addon_1}.");
}

TEST_F(CommandTest, NoPortIsRefused) {
    Config config = typed("{source_port}");
    std::string error;

    config.activeProfile().port.clear();

    EXPECT_TRUE(Command::custom(config, &error).empty());
    EXPECT_EQ(error, "This profile has no source port, so there is nothing for {source_port}.");
}

TEST_F(CommandTest, NoGameIsRefused) {
    Config config = typed("{game}");
    std::string error;

    config.activeProfile().iwad.clear();

    EXPECT_TRUE(Command::custom(config, &error).empty());
    EXPECT_EQ(error, "This profile has no game, so there is nothing for {game}.");
}

TEST_F(CommandTest, AnUnknownPlaceholderNamesTheKnownOnes) {
    std::string error;

    EXPECT_TRUE(Command::custom(typed("{nonsense}"), &error).empty());
    EXPECT_TRUE(error.starts_with("{nonsense} is not one ZDL4 knows."));
    EXPECT_TRUE(error.contains("{replaydir}"));
}

TEST_F(CommandTest, EveryPlaceholderHasOneEntryWithItsOwnText) {
    std::vector<std::string_view> seen;

    for (int kind = 0; kind <= static_cast<int>(Command::Placeholder::ReplayDir); ++kind) {
        const auto placeholder = static_cast<Command::Placeholder>(kind);

        EXPECT_EQ(std::ranges::count(Command::tokens(), placeholder, &Command::Token::kind), 1) << kind;
        EXPECT_FALSE(Command::text(placeholder).empty()) << kind;
    }

    for (const Command::Token &token : Command::tokens()) {
        EXPECT_EQ(std::ranges::count(seen, token.text), 0) << token.text;
        EXPECT_TRUE(token.text.starts_with('{') && token.text.ends_with('}')) << token.text;
        EXPECT_FALSE(token.about.empty()) << token.text;

        seen.push_back(token.text);
    }
}

TEST_F(CommandTest, EveryOfferedPlaceholderFillsIn) {
    Config config = typed("");

    config.general.profileConfigs = true;
    config.activeProfile().config = "alpha.cfg";
    config.activeProfile().save = {.enabled = false, .file = "/saves/save1.zds"};
    config.activeProfile().files = {{.file = "/mods/a.wad"}};

    for (const Command::Token &token : Command::tokens()) {
        const std::string written =
                token.kind == Command::Placeholder::Addon ? std::string("{addon_1}") : std::string(token.text);
        std::string error;

        config.activeProfile().command = written;

        EXPECT_EQ(Command::custom(config, &error).size(), 1U) << written << ": " << error;
        EXPECT_TRUE(Command::uses(written, token.kind)) << written;
    }
}

TEST_F(CommandTest, ARefusedPlaceholderNamesEveryOfferedOne) {
    std::string error;

    EXPECT_TRUE(Command::custom(typed("{nonsense}"), &error).empty());

    for (const Command::Token &token : Command::tokens()) {
        const std::string_view named = token.kind == Command::Placeholder::Addon ? "{addon_1} upwards" : token.text;

        EXPECT_TRUE(error.contains(named)) << named;
    }
}

TEST_F(CommandTest, TheExampleRuns) {
    Config config = typed(Command::example());

    config.activeProfile().files = {{.file = "/mods/a.wad"}};

    EXPECT_EQ(Command::custom(config), (Strings{"/games/gzdoom", "-iwad", iwad(), "-file", "/mods/a.wad"}));
}

TEST_F(CommandTest, TheProfileFoldersAreFilledIn) {
    Config config = typed("{profile} {cfgdir} {extracfg} {savedir} {replaydir}");

    config.general.profileConfigs = true;
    config.activeProfile().config = "alpha.cfg";

    EXPECT_EQ(Command::custom(config),
              (Strings{own().string(),
                       (own() / "alpha.cfg").string(),
                       (own() / "alpha-extra.cfg").string(),
                       (own() / "saves").string(),
                       (own() / "replays").string()}));
}

TEST_F(CommandTest, AProfileWithoutItsOwnConfigHasNoFolderToFill) {
    Config config = typed("{cfgdir}");
    std::string error;

    config.activeProfile().config = "alpha.cfg";

    EXPECT_TRUE(Command::custom(config, &error).empty());
    EXPECT_EQ(error, "This profile has no folder of its own, so there is nothing for {cfgdir}.");
}

TEST_F(CommandTest, ReplaysNeedOnlyAProfileName) {
    Config config = typed("{replaydir}");

    config.activeProfile().config = "alpha.cfg";

    EXPECT_EQ(Command::custom(config), (Strings{(own() / "replays").string()}));
}

TEST_F(CommandTest, ReplaysWithoutAProfileNameAreRefused) {
    std::string error;

    EXPECT_TRUE(Command::custom(typed("{replaydir}"), &error).empty());
    EXPECT_EQ(error, "This profile has no folder of its own, so there is nothing for {replaydir}.");
}

TEST_F(CommandTest, TheSaveFileIsFilledIn) {
    Config config = typed("{savefile}");

    config.activeProfile().save = {.enabled = false, .file = "/saves/save1.zds"};

    EXPECT_EQ(Command::custom(config), (Strings{"/saves/save1.zds"}));
}

TEST_F(CommandTest, NoSaveIsRefused) {
    std::string error;

    EXPECT_TRUE(Command::custom(typed("{savefile}"), &error).empty());
    EXPECT_EQ(error, "This profile has no save picked, so there is nothing for {savefile}.");
}

TEST_F(CommandTest, TroubleIsSilentWhenTheCommandIsNotInUse) {
    Config config = typed("{nonsense}");

    config.activeProfile().set(Switch::customCommand, false);

    EXPECT_EQ(Command::trouble(config), "");
}

TEST_F(CommandTest, TroubleSaysWhatIsWrongWithTheCommand) {
    EXPECT_EQ(Command::trouble(typed("")), "There is nothing here to run.");
    EXPECT_EQ(Command::trouble(typed("{source_port}")), "");
}

TEST_F(CommandTest, APatternPutsThePlaceholdersBack) {
    Config config = launch::single("/games/gzdoom");

    config.activeProfile().files = {{.file = "/mods/a.wad"}, {.file = "/mods/b.wad"}};

    EXPECT_EQ(Command::pattern(config), "{source_port} -iwad {game} -file {addon_1} {addon_2}");
}

TEST_F(CommandTest, APatternLeavesOffAnAddOnMarkedGoneAndKeepsTheNumbers) {
    Config config = launch::single("/games/gzdoom");

    config.activeProfile().files = {
            {.file = "/mods/a.wad", .gone = true},
            {.file = "/mods/b.wad"},
            {.file = "/mods/c.wad"},
    };

    EXPECT_EQ(Command::pattern(config), "{source_port} -iwad {game} -file {addon_2} {addon_3}");
}

TEST_F(CommandTest, AnAddonWordCountsAnAddOnMarkedGoneAndNamesItAsWritten) {
    Config config = typed("{addon_1} {addon_2}");

    config.activeProfile().files = {{.file = "/mods/a.wad", .gone = true}, {.file = "/mods/b.wad"}};

    EXPECT_EQ(Command::custom(config), (Strings{"/mods/a.wad", "/mods/b.wad"}));
}

TEST_F(CommandTest, APatternPutsTheProfileFoldersBack) {
    Config config = launch::single("/games/chocolate-doom");

    config.general.profileConfigs = true;
    config.activeProfile().config = "alpha.cfg";

    EXPECT_EQ(Command::pattern(config),
              "{source_port} -config {cfgdir} -extraconfig {extracfg} -savedir {savedir} -iwad {game}");
}

TEST_F(CommandTest, APatternQuotesWhatItCannotName) {
    Config config = launch::single("/games/gzdoom");

    config.activeProfile().extra = "+name 'Doom Guy'";

    EXPECT_EQ(Command::pattern(config), "{source_port} -iwad {game} +name 'Doom Guy'");
}

TEST_F(CommandTest, APatternReadsBackAsTheLaunchItCameFrom) {
    Config config = launch::single("/games/gzdoom");

    config.activeProfile().extra = R"('C:\maps\e1.wad' 'costs $5')";

    Strings generated = Arguments::of(config);

    generated.insert(generated.begin(), "/games/gzdoom");
    config.activeProfile().set(Switch::customCommand, true);
    config.activeProfile().command = Command::pattern(config);

    EXPECT_EQ(Command::custom(config), generated) << config.activeProfile().command;
}

TEST_F(CommandTest, ALineReadsBackWithShellRules) {
    Config config = launch::single("/games/gzdoom");

    config.activeProfile().extra = R"('C:\maps\e1.wad' 'costs $5')";

    Strings generated = Arguments::of(config);

    generated.insert(generated.begin(), "/games/gzdoom");

    EXPECT_EQ(ttk::Text::parse_arguments(Command::line(config)), generated) << Command::line(config);
}

TEST_F(CommandTest, APatternNamesTheSaveOnlyForAPortHandedItsPath) {
    Config config = launch::single("/games/zdoom");

    config.general.profileConfigs = true;
    config.activeProfile().config = "alpha.cfg";
    config.activeProfile().save = {.enabled = true, .file = "save1.zds"};
    launch::write(launch::profiles() / "alpha" / "saves" / "save1.zds", "save");

    EXPECT_TRUE(Command::pattern(config).ends_with("-loadgame {savefile}"));

    config.ports.front().file = "/games/gzdoom";

    EXPECT_TRUE(Command::pattern(config).ends_with("-loadgame save1.zds"));
}

TEST_F(CommandTest, ALinePutsThePortFirstAndQuotesEachArgument) {
    Config config = launch::single("/games/gzdoom");

    config.activeProfile().extra = "+name 'Doom Guy'";

    EXPECT_EQ(Command::line(config), "/games/gzdoom -iwad " + iwad() + " +name 'Doom Guy'");
}

TEST_F(CommandTest, ALineFindsRelativeFilesBesideThePort) {
    const fs::path port = tests::Sandbox::scratch("command-line");

    launch::write(port / "mod.wad", "PWAD");

    Config config = launch::single(port / "gzdoom");

    config.activeProfile().files = {{.file = "mod.wad"}};

    EXPECT_TRUE(Command::line(config).ends_with("-file " + (port / "mod.wad").string()));
}

TEST_F(CommandTest, ALineWithNoPortIsOnlyTheArguments) {
    Config config = launch::single("/games/gzdoom");

    config.activeProfile().port.clear();

    EXPECT_EQ(Command::line(config), "-iwad " + iwad());
}

TEST_F(CommandTest, ALineOfATypedCommandIsTheCommand) {
    EXPECT_EQ(Command::line(typed("{source_port} \"a b\"")), "/games/gzdoom 'a b'");
}

TEST_F(CommandTest, ALineOfABrokenTypedCommandIsEmpty) {
    EXPECT_EQ(Command::line(typed("{nonsense}")), "");
}

TEST_F(CommandTest, ALineOfADosLaunchStartsWithDosbox) {
    const fs::path scratch = tests::Sandbox::scratch("command-dos");

    launch::write(scratch / "dosbox", "");

    Config config = launch::single(scratch / "port" / "DOOM2.EXE", true);

    config.general.dosbox = (scratch / "dosbox").string();

    EXPECT_TRUE(Command::line(config).starts_with((scratch / "dosbox").string() + " -c "));
}

TEST_F(CommandTest, ALineLeadsWithTheVariablesThenThePrefix) {
    Config config = launch::single("/games/gzdoom");

    config.activeProfile().prefix = "gamemoderun mangohud";
    config.activeProfile().environment = {{.name = "A", .value = "1"}, {.name = "", .value = "lost"}};

    EXPECT_EQ(Command::line(config), "A=1 gamemoderun mangohud /games/gzdoom -iwad " + iwad());
}

TEST_F(CommandTest, ALineOfATypedCommandTakesTheVariablesButNotThePrefix) {
    Config config = typed("{source_port}");

    config.activeProfile().prefix = "gamemoderun";
    config.activeProfile().environment = {{.name = "A", .value = "1"}};

    EXPECT_EQ(Command::line(config), "A=1 /games/gzdoom");
}

TEST_F(CommandTest, ALineOfADosLaunchPutsThePrefixBeforeDosbox) {
    const fs::path scratch = tests::Sandbox::scratch("command-dos-prefix");

    launch::write(scratch / "dosbox", "");

    Config config = launch::single(scratch / "port" / "DOOM2.EXE", true);

    config.general.dosbox = (scratch / "dosbox").string();
    config.activeProfile().prefix = "gamemoderun";

    EXPECT_TRUE(Command::line(config).starts_with("gamemoderun " + (scratch / "dosbox").string() + " -c "));
}

TEST_F(CommandTest, ALineWithNothingToRunStaysEmptyWhateverThePrefix) {
    Config config = typed("{nonsense}");

    config.activeProfile().environment = {{.name = "A", .value = "1"}};

    EXPECT_EQ(Command::line(config), "");
}

TEST_F(CommandTest, APatternStartsWithThePrefix) {
    Config config = launch::single("/games/gzdoom");

    config.activeProfile().prefix = "gamemoderun";

    EXPECT_EQ(Command::pattern(config), "gamemoderun {source_port} -iwad {game}");
}

#ifndef _WIN32
TEST_F(CommandTest, ALineQuotesAVariableValueAndKeepsAnEmptyOne) {
    Config config = launch::single("/games/gzdoom");

    config.activeProfile().environment = {{.name = "A", .value = "x y"}, {.name = "B", .value = ""}};

    EXPECT_TRUE(Command::line(config).starts_with("A='x y' B='' /games/gzdoom "));
}

TEST_F(CommandTest, ALineShowsEachVariableAsTheGameGetsIt) {
    Config config = launch::single("/games/gzdoom");

    config.activeProfile().environment = {
            {.name = "FOO", .value = "\"BAR BAZ\""},
            {.name = "WADS", .value = "C:\\wads"},
            {.name = "WHO", .value = "it's"},
    };

    EXPECT_TRUE(Command::line(config).starts_with("FOO='BAR BAZ' WADS='C:\\wads' WHO=\"it's\" /games/gzdoom "))
            << Command::line(config);
}

TEST_F(CommandTest, AnythingAShellWouldExpandIsQuoted) {
    Config config = launch::single("/games/gzdoom");

    config.activeProfile().extra = "-file ~/x.pk3 *.wad 'a;b' 'say `hi`' it\"'\"s";

    EXPECT_TRUE(Command::line(config).ends_with(" -file '~/x.pk3' '*.wad' 'a;b' 'say `hi`' \"it's\""))
            << Command::line(config);
}

TEST_F(CommandTest, AQuoteInsideAnArgumentShowsWithoutBackslashes) {
    Config config = launch::single("/games/gzdoom");

    config.activeProfile().extra = "+say 'he said \"hi\"'";

    EXPECT_TRUE(Command::line(config).ends_with(" +say 'he said \"hi\"'")) << Command::line(config);
}
#endif

TEST_F(CommandTest, ALineOfADosLaunchThatCannotBeBuiltIsEmpty) {
    Config config = launch::single("/games/DOOM2.EXE", true);

    config.general.dosbox = (tests::Sandbox::root() / "no-dosbox-here").string();

    EXPECT_EQ(Command::line(config), "");
}

TEST_F(CommandTest, ANativeLaunchSpendsNoDosCommands) {
    EXPECT_EQ(Command::dosSpend(launch::single("/games/gzdoom")), 0);
}

TEST_F(CommandTest, ATypedDosboxCommandSpendsItsDashCs) {
    EXPECT_EQ(Command::dosSpend(typed("/usr/bin/DOSBox-Staging -c 'mount c .' -c c: -c doom2")), 3);
}

TEST_F(CommandTest, ATypedCommandRunningSomethingElseSpendsNothing) {
    EXPECT_EQ(Command::dosSpend(typed("/usr/bin/wrapper -c one -c two")), 0);
}

TEST_F(CommandTest, ABrokenTypedCommandSpendsNothing) {
    EXPECT_EQ(Command::dosSpend(typed("dosbox -c {nonsense}")), 0);
}

TEST_F(CommandTest, AGeneratedDosLaunchSpendsWhatItsCommandCounts) {
    const fs::path scratch = tests::Sandbox::scratch("command-spend");

    launch::write(scratch / "dosbox", "");

    Config config = launch::single(scratch / "port" / "DOOM2.EXE", true);

    config.general.dosbox = (scratch / "dosbox").string();

    const int spent = Command::dosSpend(config);

    EXPECT_EQ(spent, Dos::spent(Dos::command(config)));
    EXPECT_GT(spent, 0);
    EXPECT_LE(spent, Dos::COMMANDS);
}

TEST_F(CommandTest, TheLineAndTheLaunchAgreeOnStdout) {
#ifdef _WIN32
    constexpr bool windows = true;
#else
    constexpr bool windows = false;
#endif

    const auto shows = [](const Config &config) {
        return std::ranges::count(ttk::Text::parse_arguments(Command::line(config)), "-stdout");
    };

    Config native = launch::single("/games/gzdoom");

    native.activeProfile().set(Switch::captureOutput, true);

    EXPECT_EQ(shows(native), windows ? 1 : 0);
    EXPECT_EQ(std::ranges::count(Arguments::of(native), "-stdout"), shows(native));

    Config typedOne = typed("{source_port} -iwad {game}");

    typedOne.activeProfile().set(Switch::captureOutput, true);

    EXPECT_EQ(shows(typedOne), 0) << "a typed command runs as written";
    EXPECT_EQ(Command::custom(typedOne), (Strings{"/games/gzdoom", "-iwad", iwad()}));

    const fs::path scratch = tests::Sandbox::scratch("command-stdout-dos");

    launch::write(scratch / "dosbox", "");

    Config dos = launch::single(scratch / "port" / "DOOM2.EXE", true);

    dos.general.dosbox = (scratch / "dosbox").string();
    dos.activeProfile().set(Switch::captureOutput, true);
    dos.ports.front().dialect = "gzdoom";

    EXPECT_EQ(shows(dos), 0);
    EXPECT_FALSE(Arguments::asksForStdout(dos, true));
}

TEST_F(CommandTest, APatternLeavesOutStdoutButKeepsOneTypedInExtra) {
#ifdef _WIN32
    constexpr bool windows = true;
#else
    constexpr bool windows = false;
#endif

    Config config = launch::single("/games/gzdoom");

    config.activeProfile().set(Switch::captureOutput, true);

    EXPECT_EQ(std::ranges::count(Arguments::of(config), "-stdout"), windows ? 1 : 0);
    EXPECT_EQ(std::ranges::count(Arguments::uncaptured(config), "-stdout"), 0);
    EXPECT_FALSE(Command::pattern(config).contains("-stdout"));

    config.activeProfile().extra = "-stdout";

    EXPECT_TRUE(Command::pattern(config).ends_with(" -stdout"));
}
