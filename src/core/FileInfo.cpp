/*
 * This file is part of qZDL
 * Copyright (C) 2018-2019  Lcferrum
 * Copyright (C) 2023-2026  spacebub
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
#include <span>
#include <string_view>
#include <utility>

#include "core/FileInfo.h"

#include "Md5.h"
#include "core/MapFile.h"
#include "core/Text.h"

namespace {

using Row = std::pair<std::string_view, std::string_view>;

// Sorted, so a lookup is a search rather than a walk, and in the binary rather
// than built on first use: a map would be four hundred heap strings kept all
// session to answer two questions.
std::string_view lookUp(const std::span<const Row> rows, const std::string_view key) {
    const auto found = std::ranges::lower_bound(rows, key, {}, &Row::first);

    return found != rows.end() && found->first == key ? found->second : std::string_view();
}

// MD5 to IWAD name.
constexpr std::array IWAD_HASHES = std::to_array<Row>({
    Row{"023b52175d2f260c3bdc5528df5d0a8c", "Heretic Shareware v1.0"},
    Row{"049e32f18d9c9529630366cfc72726ea", "Doom Press Release Beta"},
    Row{"06a8f99b9b756ac908917c3868b8e3bc", "Strife: Veteran Edition v1.0"},
    Row{"0c8758f102ccafe26a3040bee8ba5021", "The Ultimate Doom v1.9 (Xbox Doom 3 bundle)"},
    Row{"1077432e2690d390c256ac908b5f4efa", "Hexen: Deathkings of the Dark Citadel v1.0"},
    Row{"11e1cd216801ea2657723abc86ecb01f", "Doom Registered v1.8"},
    Row{"1511a7032ebc834a3884cf390d7f186e", "HacX v1.0"},
    Row{"17aebd6b5f2ed8ce07aa526a32af8d99", "Doom Shareware v1.25"},
    Row{"1914b280b0a4b517214523bc2270e758", "Action Doom 2: Urban Brawl v1.0"},
    Row{"1cd63c5ddff1bf8ce844237f580e9cf3", "Doom Registered v1.9"},
    Row{"1d39e405bf6ee3df69a8d2646c8d5c49", "Final Doom: TNT Evilution v1.9 (id Anthology)"},
    Row{"1e4cb4ef075ad344dd63971637307e04", "Heretic Registered v1.2"},
    Row{"25485721882b050afa96a56e5758dd52", "Chex Quest v1.0"},
    Row{"25e1459ca71d321525f84628f45ca8cd", "Doom II: Hell on Earth v1.9"},
    Row{"2c0a712d3e39b010519c879f734d79ae", "Strife: Veteran Edition v1.1"},
    Row{"2fed2031a5b03892106e0f117f17901f", "Strife Registered v1.2+"},
    Row{"30aa5beb9e5ebfbbe1e1765561c08f38", "Doom Shareware v1.2"},
    Row{"30e3c2d0350b67bfbf47271970b74b2f", "Doom II: Hell on Earth v1.666"},
    Row{"3117e399cdb4298eaa3941625f4b2923", "Heretic Registered v1.0"},
    Row{"3493be7e1e2588bc9c8b31eab2587a04", "Final Doom: The Plutonia Experiment v1.9 (id Anthology)"},
    Row{"3cb02349b3df649c86290907eed64e7b", "Doom II: Hell on Earth v1.8 (French release)"},
    Row{"3e410ecd27f61437d53fa5c279536e88", "The Ultimate Doom v1.9 (Doom PDA)"},
    Row{"43c2df32dc6c740cb11f34dc5ab693fa", "Doom II: Hell on Earth v1.9 (XBLA Doom II)"},
    Row{"464e3723a7e7f97039ac9fd057096adb", "Doom Registered v1.6b"},
    Row{"47958a4fea8a54116e4b51fc155799c0", "Strife: Veteran Edition v1.2+"},
    Row{"48ebb49b52f6a3020d174dbcc1b9aeaf", "Harmony v1.1"},
    Row{"4c3db5f23b145fccd24c9b84aba3b7dd", "Doom II: Hell on Earth v1.9 (PSN Classic Complete)"},
    Row{"4e158d9953c79ccf97bd0663244cc6b6", "Final Doom: TNT Evilution v1.9"},
    Row{"52cbc8882f445573ce421fa5453513c1", "Doom Shareware v1.1"},
    Row{"54978d12de87f162b9bcc011676cb3c0", "Doom Registered v1.666"},
    Row{"59c985995db55cd2623c1893550d82b3", "Chex Quest 3 v1.0"},
    Row{"5f4eb849b1af12887dec04a2a12e5e62", "Doom Shareware v1.8"},
    Row{"65ed74d522bdf6649c2831b13b9e02b4", "HacX v1.2"},
    Row{"66d686b1ed6d35ff103f15dbd30e0341", "Heretic Registered v1.3"},
    Row{"677605e1a7ee75dc279373036cdb6ebb", "Final Doom: TNT Evilution v1.9 (DOOMPatcher)"},
    Row{"72286ddc680d47b9138053dd944b2a3d", "The Ultimate Doom v1.9 (XBLA Doom)"},
    Row{"740901119ba2953e3c7f3764eca6e128", "Doom Alpha v0.2"},
    Row{"75c8cf89566741fa9d22447604053bd7", "Final Doom: The Plutonia Experiment v1.9"},
    Row{"762fd6d4b960d4b759730f01387a50a1", "Doom Shareware v1.6b"},
    Row{"78d5898e99e220e4de64edaa0e479593", "Hexen: Deathkings of the Dark Citadel v1.1"},
    Row{"7912931e44c7d56e021084a256659800", "The Ultimate Doom v1.9 (Xbox 360 BFG Edition)"},
    Row{"792fd1fea023d61210857089a7c1e351", "Doom Registered v1.2"},
    Row{"8517c4e8f0eef90b82852667d345eb86", "The Ultimate Doom (Unity v1.3)"},
    Row{"876a5a44c7b68f04b3bb9bc7a5bd69d6", "Hexen Shareware v1.0"},
    Row{"8ab6d0527a29efdc1ef200e5687b5cae", "Doom II: Hell on Earth (Unity v1.3)"},
    Row{"8f2d3a6a289f5d2f2f9c1eec02b47299", "Strife Registered v1.1"},
    Row{"90facab21eede7981be10790e3f82da2", "Doom Shareware v0.99/v1.0"},
    Row{"9178a32a496ff5befebfe6c47dac106c", "Hexen Shareware Beta"},
    Row{"925f9f5000e17dc84b0a6a3bed3a6f31", "Hexen Shareware v1.0 (Mac)"},
    Row{"9640fc4b2c8447bbd28f2080725d5c51", "Doom II: Hell on Earth v1.9 (Tapwave Zodiac)"},
    Row{"981b03e6d1dc033301aa3095acc437ce", "Doom Registered v1.1"},
    Row{"9c877480b8ef33b7074f1f0c07ed6487", "Doom Alpha v0.5"},
    Row{"a21ae40c388cb6f2c3cc1b95589ee693", "Doom Shareware v1.4b"},
    Row{"a793ebcdd790afad4a1f39cc39a893bd", "Doom II: Hell on Earth v1.9 (Xbox Doom 3 bundle)"},
    Row{"abb033caf81e26f12a2103e1fa25453f", "Hexen Registered v1.1"},
    Row{"ae779722390ec32fa37b0d361f7d82f8", "Heretic Shareware v1.2"},
    Row{"b2543a03521365261d0a0f74d5dd90f0", "Hexen Registered v1.0"},
    Row{"b68140a796f6fd7f3a5d3226a32b93be", "Hexen Registered v1.1 (Mac)"},
    Row{"b6afa12a8b22e2726a8ff5bd249223de", "Doom Alpha v0.4"},
    Row{"b77ca6a809c4fae086162dad8e7a1335", "Final Doom: The Plutonia Experiment v1.9 (PSN Classic Complete)"},
    Row{"b7fd2f43f3382cf012dc6b097a3cb182", "HacX v1.1"},
    Row{"bb545b9c4eca0ff92c14d466b3294023", "Strife Shareware v1.1"},
    Row{"bce163d06521f9d15f9686786e64df13", "Chex Quest 3 v1.4"},
    Row{"be626c12b7c9d94b1dfb9c327566b4ff", "Final Doom: TNT Evilution v1.9 (PSN Classic Complete)"},
    Row{"c106a4e0a96f299954b073d5f97240be", "Action Doom 2: Urban Brawl v1.1"},
    Row{"c236745bb01d89bbb866c8fed81b6f8c", "Doom II: Hell on Earth v1.8"},
    Row{"c3bea40570c23e511a7ed3ebcd9865f7", "Doom II: Hell on Earth v1.9 (BFG Edition)"},
    Row{"c428ea394dc52835f2580d5bfd50d76f", "Doom Shareware v1.666"},
    Row{"c4fe9fd920207691a9f493668e0a2083", "The Ultimate Doom v1.9"},
    Row{"c88a2bb3d783e2ad7b599a8e301e099e", "Hexen Registered Beta"},
    Row{"cea4989df52b65f4d481b706234a3dca", "Doom Shareware v1.1 (cancelled release)"},
    Row{"d7a07e5d3f4625074312bc299d7ed33f", "Doom II: Hell on Earth v1.7a"},
    Row{"d9153ced9fd5b898b36cc5844e35b520", "Doom II: Hell on Earth v1.666 (German release)"},
    Row{"dae77aff77a0491e3b7254c9c8401aa8", "The Ultimate Doom v1.9 (Doom PDA)"},
    Row{"dae9b1eea1a8e090fdfa5707187f4a43", "Doom Alpha v0.3"},
    Row{"de2c8dcad7cca206292294bdab524292", "Strife Shareware v1.0"},
    Row{"e280233d533dcc28c1acd6ccdc7742d4", "Doom Shareware v1.5b"},
    Row{"e4f120eab6fb410a5b6e11c947832357", "The Ultimate Doom v1.9 (PSN Classic Complete)"},
    Row{"ea74a47a791fdef2e9f2ea8b8a9da13b", "Doom II: Hell on Earth v1.7"},
    Row{"f0cefca49926d00903cf57551d901abe", "Doom Shareware v1.9"},
    Row{"f617591a6c5d07037eb716dc4863e26b", "Doom II: Hell on Earth v1.9 (Xbox 360 BFG Edition)"},
    Row{"fb35c4a5a9fd49ec29ab6e900572c524", "The Ultimate Doom v1.9 (BFG Edition)"},
    Row{"fc7eab659f6ee522bb57acc1a946912f", "Heretic Shareware Beta"},
    Row{"fe2cce6713ddcf6c6d6f0e8154b0cb38", "Harmony v1.0"},
});

static_assert(std::ranges::is_sorted(IWAD_HASHES, {}, &Row::first));

// File name to IWAD name.
constexpr std::array IWAD_FILES = std::to_array<Row>({
    Row{"action2.wad",       "Action Doom 2: Urban Brawl"},
    Row{"bfgdoom.wad",       "The Ultimate Doom (BFG Edition)"},
    Row{"bfgdoom2.wad",      "Doom II: Hell on Earth (BFG Edition)"},
    Row{"blasphem.wad",      "Blasphemer"},
    Row{"blasphemer.wad",    "Blasphemer"},
    Row{"chex.wad",          "Chex Quest"},
    Row{"chex3.wad",         "Chex Quest 3"},
    Row{"delaweare.wad",     "Delaweare"},
    Row{"doom.wad",          "Doom Registered"},
    Row{"doom1.wad",         "Doom Shareware"},
    Row{"doom2.wad",         "Doom II: Hell on Earth"},
    Row{"doom2bfg.wad",      "Doom II: Hell on Earth (BFG Edition)"},
    Row{"doom2f.wad",        "Doom II: Hell on Earth (French release)"},
    Row{"doom_complete.pk3", "DOOM Complete (WADSmoosh)"},
    Row{"doombfg.wad",       "The Ultimate Doom (BFG Edition)"},
    Row{"doomu.wad",         "The Ultimate Doom"},
    Row{"freedm.wad",        "FreeDM"},
    Row{"freedoom.wad",      "Freedoom: Phase 2"},
    Row{"freedoom1.wad",     "Freedoom: Phase 1"},
    Row{"freedoom2.wad",     "Freedoom: Phase 2"},
    Row{"freedoomu.wad",     "Freedoom: Phase 1"},
    Row{"hacx.wad",          "HacX v2.0"},
    Row{"hacx2.wad",         "HacX v2.0"},
    Row{"harm1.wad",         "Harmony"},
    Row{"heretic.wad",       "Heretic Registered"},
    Row{"heretic1.wad",      "Heretic Shareware"},
    Row{"hereticsr.wad",     "Heretic Registered"},
    Row{"hexdd.wad",         "Hexen: Deathkings of the Dark Citadel"},
    Row{"hexdemo.wad",       "Hexen Shareware"},
    Row{"hexen.wad",         "Hexen Registered"},
    Row{"hexendemo.wad",     "Hexen Shareware"},
    Row{"plutonia.wad",      "Final Doom: The Plutonia Experiment"},
    Row{"rotwb.wad",         "Rise Of The Wool Ball"},
    Row{"square.pk3",        "The Adventures of Square"},
    Row{"square1.pk3",       "The Adventures of Square"},
    Row{"srb2.srb",          "Sonic Robo Blast 2"},
    Row{"strife.wad",        "Strife Registered"},
    Row{"strife0.wad",       "Strife Shareware"},
    Row{"strife1.wad",       "Strife Registered"},
    Row{"sve.wad",           "Strife: Veteran Edition"},
    Row{"tnt.wad",           "Final Doom: TNT Evilution"},
    Row{"tntyk.wad",         "Final Doom: TNT Evilution (DOOMPatcher)"},
    Row{"voices.wad",        "Strife Voices WAD"},
});

static_assert(std::ranges::is_sorted(IWAD_FILES, {}, &Row::first));

// Executable name to source port name.
constexpr std::array SOURCE_PORTS = std::to_array<Row>({
    Row{"boom",               "Boom"},
    Row{"chocolate-doom",     "Chocolate Doom"},
    Row{"chocolate-heretic",  "Chocolate Heretic"},
    Row{"chocolate-hexen",    "Chocolate Hexen"},
    Row{"chocolate-strife",   "Chocolate Strife"},
    Row{"cndoom",             "CnDoom (Doom)"},
    Row{"cnheretic",          "CnDoom (Heretic)"},
    Row{"cnhexen",            "CnDoom (Hexen)"},
    Row{"cnserver",           "CnDoom (Server)"},
    Row{"cnstrife",           "CnDoom (Strife)"},
    Row{"crispy-doom",        "Crispy Doom"},
    Row{"crispy-server",      "Crispy Doom (Server)"},
    Row{"doom",               "Doom"},
    Row{"doom2",              "Doom II: Hell on Earth"},
    Row{"doom3d",             "Doom3D"},
    Row{"doom95",             "Doom 95"},
    Row{"doomcl",             "csDoom"},
    Row{"doomgl",             "DoomGL"},
    Row{"doomlegacy",         "Doom Legacy"},
    Row{"doomplus",           "Doom Plus"},
    Row{"doomretro",          "Doom Retro"},
    Row{"doomsday",           "Doomsday Engine"},
    Row{"doomsday-server",    "Doomsday Engine (Server)"},
    Row{"dosdoom",            "DOSDoom"},
    Row{"edge",               "EDGE"},
    Row{"eternity",           "Eternity Engine"},
    Row{"glboom",             "PrBoom (OpenGL)"},
    Row{"glboom-plus",        "PrBoom+ (OpenGL)"},
    Row{"gldoom",             "GLDoom"},
    Row{"gzdoom",             "GZDoom"},
    Row{"heretic",            "Heretic"},
    Row{"hexen",              "Hexen"},
    Row{"hexendk",            "Hexen: Deathkings of the Dark Citadel"},
    Row{"jdoom",              "jDoom (Doom)"},
    Row{"jheretic",           "jDoom (Heretic)"},
    Row{"jhexen",             "jDoom (Hexen)"},
    Row{"linuxsdoom",         "Linux Doom (SVGAlib)"},
    Row{"linuxxdoom",         "Linux Doom (X)"},
    Row{"lxdoom",             "LxDoom"},
    Row{"lxdoom-game-server", "LxDoom (Server)"},
    Row{"lzdoom",             "LZDoom"},
    Row{"mbf",                "Marine's Best Friend (MBF)"},
    Row{"mochadoom",          "Mocha Doom"},
    Row{"mochadoom7",         "Mocha Doom (Win 7)"},
    Row{"odamex",             "Odamex"},
    Row{"odasrv",             "Odamex (Server)"},
    Row{"prboom",             "PrBoom"},
    Row{"prboom-plus",        "PrBoom+"},
    Row{"prboom-plus_server", "PrBoom+ (Server)"},
    Row{"prboom_server",      "PrBoom (Server)"},
    Row{"qzdoom",             "QZDoom"},
    Row{"remood",             "ReMooD"},
    Row{"risen3d",            "Risen3D"},
    Row{"skulltag",           "Skulltag"},
    Row{"smmu",               "Smack My Marine Up (SMMU)"},
    Row{"strawberry-doom",    "Strawberry Doom"},
    Row{"strawberry-server",  "Strawberry Doom (Server)"},
    Row{"strife",             "Strife (Shareware)"},
    Row{"strife1",            "Strife"},
    Row{"tasdoom",            "TASDoom"},
    Row{"vavoom",             "Vavoom"},
    Row{"vavoom-dedicated",   "Vavoom (Server)"},
    Row{"wdmp",               "WDMP"},
    Row{"wdmp32s",            "WDMP (Win32s)"},
    Row{"windoom",            "WinDoom"},
    Row{"zandronum",          "Zandronum"},
    Row{"zdaemon",            "ZDaemon"},
    Row{"zdaemongl",          "ZDaemonGL"},
    Row{"zdoom",              "ZDoom"},
    Row{"zdoom32",            "ZDoom32"},
    Row{"zdoom32_N",          "ZDoom32 (MinGW)"},
    Row{"zdoom32_SSE2",       "ZDoom32 (SSE2)"},
    Row{"zdoom98",            "ZDoom LE (Win 9x)"},
    Row{"zdoomgl",            "ZDoomGL"},
    Row{"zserv32",            "ZDaemon (Server)"},
});

static_assert(std::ranges::is_sorted(SOURCE_PORTS, {}, &Row::first));

}

namespace FileInfo {

std::string describeIwad(const std::filesystem::path &file) {
    if (const std::string digest = md5File(file); !digest.empty()) {
        if (const std::string_view found = lookUp(IWAD_HASHES, digest); !found.empty()) {
            return std::string(found);
        }
    }

    if (const std::unique_ptr<MapFile> map = MapFile::open(file)) {
        if (std::string name = map->iwadinfoName(); !name.empty()) {
            return name;
        }
    }

    const std::string name = Text::lower(file.filename().string());

    if (const std::string_view found = lookUp(IWAD_FILES, name); !found.empty()) {
        return std::string(found);
    }

    return Text::upper(file.filename().string());
}

std::string describePort(const std::filesystem::path &file) {
    std::string stem = file.stem().string();

    if (const std::string_view found = lookUp(SOURCE_PORTS, Text::lower(stem)); !found.empty()) {
        return std::string(found);
    }

    return stem;
}
}
