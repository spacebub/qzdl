/*
 * This file is part of qZDL
 * Copyright (C) 2018-2019  Lcferrum
 * Copyright (C) 2023  spacebub
 * 
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QCryptographicHash>
#include "wad/ZDLFileInfo.h"
#include "wad/ZDLMapFile.h"

namespace {
/* Built on first use: a namespace scope std::map would run its constructor
 * before main() and could throw where nothing can catch it. */
const std::map<std::string, std::string> &iwadHashes() {
    // MD5 to IWAD name
    static const std::map<std::string, std::string> map = {
            {"740901119ba2953e3c7f3764eca6e128", "Doom Alpha v0.2"},
            {"dae9b1eea1a8e090fdfa5707187f4a43", "Doom Alpha v0.3"},
            {"b6afa12a8b22e2726a8ff5bd249223de", "Doom Alpha v0.4"},
            {"9c877480b8ef33b7074f1f0c07ed6487", "Doom Alpha v0.5"},
            {"049e32f18d9c9529630366cfc72726ea", "Doom Press Release Beta"},
            {"90facab21eede7981be10790e3f82da2", "Doom Shareware v0.99/v1.0"},
            {"cea4989df52b65f4d481b706234a3dca", "Doom Shareware v1.1 (cancelled release)"},
            {"52cbc8882f445573ce421fa5453513c1", "Doom Shareware v1.1"},
            {"30aa5beb9e5ebfbbe1e1765561c08f38", "Doom Shareware v1.2"},
            {"17aebd6b5f2ed8ce07aa526a32af8d99", "Doom Shareware v1.25"},
            {"a21ae40c388cb6f2c3cc1b95589ee693", "Doom Shareware v1.4b"},
            {"e280233d533dcc28c1acd6ccdc7742d4", "Doom Shareware v1.5b"},
            {"762fd6d4b960d4b759730f01387a50a1", "Doom Shareware v1.6b"},
            {"c428ea394dc52835f2580d5bfd50d76f", "Doom Shareware v1.666"},
            {"5f4eb849b1af12887dec04a2a12e5e62", "Doom Shareware v1.8"},
            {"f0cefca49926d00903cf57551d901abe", "Doom Shareware v1.9"},
            {"981b03e6d1dc033301aa3095acc437ce", "Doom Registered v1.1"},
            {"792fd1fea023d61210857089a7c1e351", "Doom Registered v1.2"},
            {"464e3723a7e7f97039ac9fd057096adb", "Doom Registered v1.6b"},
            {"54978d12de87f162b9bcc011676cb3c0", "Doom Registered v1.666"},
            {"11e1cd216801ea2657723abc86ecb01f", "Doom Registered v1.8"},
            {"1cd63c5ddff1bf8ce844237f580e9cf3", "Doom Registered v1.9"},
            {"c4fe9fd920207691a9f493668e0a2083", "The Ultimate Doom v1.9"},
            {"fb35c4a5a9fd49ec29ab6e900572c524", "The Ultimate Doom v1.9 (BFG Edition)"},
            {"7912931e44c7d56e021084a256659800", "The Ultimate Doom v1.9 (Xbox 360 BFG Edition)"},
            {"0c8758f102ccafe26a3040bee8ba5021", "The Ultimate Doom v1.9 (Xbox Doom 3 bundle)"},
            {"72286ddc680d47b9138053dd944b2a3d", "The Ultimate Doom v1.9 (XBLA Doom)"},
            {"e4f120eab6fb410a5b6e11c947832357", "The Ultimate Doom v1.9 (PSN Classic Complete)"},
            {"3e410ecd27f61437d53fa5c279536e88", "The Ultimate Doom v1.9 (Doom PDA)"},
            {"dae77aff77a0491e3b7254c9c8401aa8", "The Ultimate Doom v1.9 (Doom PDA)"},
            {"8517c4e8f0eef90b82852667d345eb86", "The Ultimate Doom (Unity v1.3)"},
            {"d9153ced9fd5b898b36cc5844e35b520", "Doom II: Hell on Earth v1.666 (German release)"},
            {"30e3c2d0350b67bfbf47271970b74b2f", "Doom II: Hell on Earth v1.666"},
            {"ea74a47a791fdef2e9f2ea8b8a9da13b", "Doom II: Hell on Earth v1.7"},
            {"d7a07e5d3f4625074312bc299d7ed33f", "Doom II: Hell on Earth v1.7a"},
            {"3cb02349b3df649c86290907eed64e7b", "Doom II: Hell on Earth v1.8 (French release)"},
            {"c236745bb01d89bbb866c8fed81b6f8c", "Doom II: Hell on Earth v1.8"},
            {"25e1459ca71d321525f84628f45ca8cd", "Doom II: Hell on Earth v1.9"},
            {"c3bea40570c23e511a7ed3ebcd9865f7", "Doom II: Hell on Earth v1.9 (BFG Edition)"},
            {"f617591a6c5d07037eb716dc4863e26b", "Doom II: Hell on Earth v1.9 (Xbox 360 BFG Edition)"},
            {"a793ebcdd790afad4a1f39cc39a893bd", "Doom II: Hell on Earth v1.9 (Xbox Doom 3 bundle)"},
            {"43c2df32dc6c740cb11f34dc5ab693fa", "Doom II: Hell on Earth v1.9 (XBLA Doom II)"},
            {"4c3db5f23b145fccd24c9b84aba3b7dd", "Doom II: Hell on Earth v1.9 (PSN Classic Complete)"},
            {"9640fc4b2c8447bbd28f2080725d5c51", "Doom II: Hell on Earth v1.9 (Tapwave Zodiac)"},
            {"8ab6d0527a29efdc1ef200e5687b5cae", "Doom II: Hell on Earth (Unity v1.3)"},
            {"75c8cf89566741fa9d22447604053bd7", "Final Doom: The Plutonia Experiment v1.9"},
            {"3493be7e1e2588bc9c8b31eab2587a04", "Final Doom: The Plutonia Experiment v1.9 (id Anthology)"},
            {"b77ca6a809c4fae086162dad8e7a1335", "Final Doom: The Plutonia Experiment v1.9 (PSN Classic Complete)"},
            {"4e158d9953c79ccf97bd0663244cc6b6", "Final Doom: TNT Evilution v1.9"},
            {"677605e1a7ee75dc279373036cdb6ebb", "Final Doom: TNT Evilution v1.9 (DOOMPatcher)"},
            {"1d39e405bf6ee3df69a8d2646c8d5c49", "Final Doom: TNT Evilution v1.9 (id Anthology)"},
            {"be626c12b7c9d94b1dfb9c327566b4ff", "Final Doom: TNT Evilution v1.9 (PSN Classic Complete)"},
            {"fc7eab659f6ee522bb57acc1a946912f", "Heretic Shareware Beta"},
            {"023b52175d2f260c3bdc5528df5d0a8c", "Heretic Shareware v1.0"},
            {"ae779722390ec32fa37b0d361f7d82f8", "Heretic Shareware v1.2"},
            {"3117e399cdb4298eaa3941625f4b2923", "Heretic Registered v1.0"},
            {"1e4cb4ef075ad344dd63971637307e04", "Heretic Registered v1.2"},
            {"66d686b1ed6d35ff103f15dbd30e0341", "Heretic Registered v1.3"},
            {"9178a32a496ff5befebfe6c47dac106c", "Hexen Shareware Beta"},
            {"876a5a44c7b68f04b3bb9bc7a5bd69d6", "Hexen Shareware v1.0"},
            {"925f9f5000e17dc84b0a6a3bed3a6f31", "Hexen Shareware v1.0 (Mac)"},
            {"c88a2bb3d783e2ad7b599a8e301e099e", "Hexen Registered Beta"},
            {"b2543a03521365261d0a0f74d5dd90f0", "Hexen Registered v1.0"},
            {"abb033caf81e26f12a2103e1fa25453f", "Hexen Registered v1.1"},
            {"b68140a796f6fd7f3a5d3226a32b93be", "Hexen Registered v1.1 (Mac)"},
            {"1077432e2690d390c256ac908b5f4efa", "Hexen: Deathkings of the Dark Citadel v1.0"},
            {"78d5898e99e220e4de64edaa0e479593", "Hexen: Deathkings of the Dark Citadel v1.1"},
            {"de2c8dcad7cca206292294bdab524292", "Strife Shareware v1.0"},
            {"bb545b9c4eca0ff92c14d466b3294023", "Strife Shareware v1.1"},
            {"8f2d3a6a289f5d2f2f9c1eec02b47299", "Strife Registered v1.1"},
            {"2fed2031a5b03892106e0f117f17901f", "Strife Registered v1.2+"},
            {"06a8f99b9b756ac908917c3868b8e3bc", "Strife: Veteran Edition v1.0"},
            {"2c0a712d3e39b010519c879f734d79ae", "Strife: Veteran Edition v1.1"},
            {"47958a4fea8a54116e4b51fc155799c0", "Strife: Veteran Edition v1.2+"},
            {"25485721882b050afa96a56e5758dd52", "Chex Quest v1.0"},
            {"59c985995db55cd2623c1893550d82b3", "Chex Quest 3 v1.0"},
            {"bce163d06521f9d15f9686786e64df13", "Chex Quest 3 v1.4"},
            {"1511a7032ebc834a3884cf390d7f186e", "HacX v1.0"},
            {"b7fd2f43f3382cf012dc6b097a3cb182", "HacX v1.1"},
            {"65ed74d522bdf6649c2831b13b9e02b4", "HacX v1.2"},
            {"1914b280b0a4b517214523bc2270e758", "Action Doom 2: Urban Brawl v1.0"},
            {"c106a4e0a96f299954b073d5f97240be", "Action Doom 2: Urban Brawl v1.1"},
            {"fe2cce6713ddcf6c6d6f0e8154b0cb38", "Harmony v1.0"},
            {"48ebb49b52f6a3020d174dbcc1b9aeaf", "Harmony v1.1"},
    };
    return map;
}

/* Built on first use: a namespace scope std::map would run its constructor
 * before main() and could throw where nothing can catch it. */
const std::map<std::string, std::string> &iwadFiles() {
    // file name to IWAD name
    static const std::map<std::string, std::string> map = {
            {"action2.wad",       "Action Doom 2: Urban Brawl"},
            {"bfgdoom.wad",       "The Ultimate Doom (BFG Edition)"},
            {"bfgdoom2.wad",      "Doom II: Hell on Earth (BFG Edition)"},
            {"blasphem.wad",      "Blasphemer"},
            {"blasphemer.wad",    "Blasphemer"},
            {"chex.wad",          "Chex Quest"},
            {"chex3.wad",         "Chex Quest 3"},
            {"delaweare.wad",     "Delaweare"},
            {"doom_complete.pk3", "DOOM Complete (WADSmoosh)"},
            {"doom.wad",          "Doom Registered"},
            {"doom1.wad",         "Doom Shareware"},
            {"doom2.wad",         "Doom II: Hell on Earth"},
            {"doom2bfg.wad",      "Doom II: Hell on Earth (BFG Edition)"},
            {"doom2f.wad",        "Doom II: Hell on Earth (French release)"},
            {"doombfg.wad",       "The Ultimate Doom (BFG Edition)"},
            {"doomu.wad",         "The Ultimate Doom"},
            {"freedm.wad",        "FreeDM"},
            {"freedoom.wad",      "Freedoom: Phase 2"},
            {"freedoom1.wad",     "Freedoom: Phase 1"},
            {"freedoom2.wad",     "Freedoom: Phase 2"},
            {"freedoomu.wad",     "Freedoom: Phase 1"},
            {"hacx.wad",          "HacX v2.0"},
            {"hacx2.wad",         "HacX v2.0"},
            {"harm1.wad",         "Harmony"},
            {"heretic.wad",       "Heretic Registered"},
            {"heretic1.wad",      "Heretic Shareware"},
            {"hereticsr.wad",     "Heretic Registered"},
            {"hexdd.wad",         "Hexen: Deathkings of the Dark Citadel"},
            {"hexdemo.wad",       "Hexen Shareware"},
            {"hexen.wad",         "Hexen Registered"},
            {"hexendemo.wad",     "Hexen Shareware"},
            {"plutonia.wad",      "Final Doom: The Plutonia Experiment"},
            {"rotwb.wad",         "Rise Of The Wool Ball"},
            {"square.pk3",        "The Adventures of Square"},
            {"square1.pk3",       "The Adventures of Square"},
            {"srb2.srb",          "Sonic Robo Blast 2"},
            {"strife.wad",        "Strife Registered"},
            {"strife0.wad",       "Strife Shareware"},
            {"strife1.wad",       "Strife Registered"},
            {"sve.wad",           "Strife: Veteran Edition"},
            {"tnt.wad",           "Final Doom: TNT Evilution"},
            {"tntyk.wad",         "Final Doom: TNT Evilution (DOOMPatcher)"},
            {"voices.wad",        "Strife Voices WAD"}
    };
    return map;
}

/* Built on first use: a namespace scope std::map would run its constructor
 * before main() and could throw where nothing can catch it. */
const std::map<std::string, std::string> &sourcePorts() {
    // executable name to source port name
    static const std::map<std::string, std::string> map = {
            {"boom",               "Boom"},
            {"chocolate-doom",     "Chocolate Doom"},
            {"chocolate-heretic",  "Chocolate Heretic"},
            {"chocolate-hexen",    "Chocolate Hexen"},
            {"chocolate-strife",   "Chocolate Strife"},
            {"cndoom",             "CnDoom (Doom)"},
            {"cnheretic",          "CnDoom (Heretic)"},
            {"cnhexen",            "CnDoom (Hexen)"},
            {"cnserver",           "CnDoom (Server)"},
            {"cnstrife",           "CnDoom (Strife)"},
            {"crispy-doom",        "Crispy Doom"},
            {"crispy-server",      "Crispy Doom (Server)"},
            {"doom",               "Doom"},
            {"doom2",              "Doom II: Hell on Earth"},
            {"doom3d",             "Doom3D"},
            {"doom95",             "Doom 95"},
            {"doomcl",             "csDoom"},
            {"doomgl",             "DoomGL"},
            {"doomlegacy",         "Doom Legacy"},
            {"doomplus",           "Doom Plus"},
            {"doomretro",          "Doom Retro"},
            {"doomsday",           "Doomsday Engine"},
            {"doomsday-server",    "Doomsday Engine (Server)"},
            {"dosdoom",            "DOSDoom"},
            {"edge",               "EDGE"},
            {"eternity",           "Eternity Engine"},
            {"glboom",             "PrBoom (OpenGL)"},
            {"glboom-plus",        "PrBoom+ (OpenGL)"},
            {"gldoom",             "GLDoom"},
            {"gzdoom",             "GZDoom"},
            {"heretic",            "Heretic"},
            {"hexen",              "Hexen"},
            {"hexendk",            "Hexen: Deathkings of the Dark Citadel"},
            {"jdoom",              "jDoom (Doom)"},
            {"jheretic",           "jDoom (Heretic)"},
            {"jhexen",             "jDoom (Hexen)"},
            {"linuxsdoom",         "Linux Doom (SVGAlib)"},
            {"linuxxdoom",         "Linux Doom (X)"},
            {"lxdoom",             "LxDoom"},
            {"lxdoom-game-server", "LxDoom (Server)"},
            {"lzdoom",             "LZDoom"},
            {"mbf",                "Marine's Best Friend (MBF)"},
            {"mochadoom",          "Mocha Doom"},
            {"mochadoom7",         "Mocha Doom (Win 7)"},
            {"odamex",             "Odamex"},
            {"odasrv",             "Odamex (Server)"},
            {"prboom",             "PrBoom"},
            {"prboom_server",      "PrBoom (Server)"},
            {"prboom-plus",        "PrBoom+"},
            {"prboom-plus_server", "PrBoom+ (Server)"},
            {"qzdoom",             "QZDoom"},
            {"remood",             "ReMooD"},
            {"risen3d",            "Risen3D"},
            {"skulltag",           "Skulltag"},
            {"smmu",               "Smack My Marine Up (SMMU)"},
            {"strawberry-doom",    "Strawberry Doom"},
            {"strawberry-server",  "Strawberry Doom (Server)"},
            {"strife",             "Strife (Shareware)"},
            {"strife1",            "Strife"},
            {"tasdoom",            "TASDoom"},
            {"vavoom",             "Vavoom"},
            {"vavoom-dedicated",   "Vavoom (Server)"},
            {"wdmp",               "WDMP"},
            {"wdmp32s",            "WDMP (Win32s)"},
            {"windoom",            "WinDoom"},
            {"zandronum",          "Zandronum"},
            {"zdaemon",            "ZDaemon"},
            {"zdaemongl",          "ZDaemonGL"},
            {"zdoom",              "ZDoom"},
            {"zdoom32",            "ZDoom32"},
            {"zdoom32_N",          "ZDoom32 (MinGW)"},
            {"zdoom32_SSE2",       "ZDoom32 (SSE2)"},
            {"zdoom98",            "ZDoom LE (Win 9x)"},
            {"zdoomgl",            "ZDoomGL"},
            {"zserv32",            "ZDaemon (Server)"}
    };
    return map;
}
}  // namespace

ZDLFileInfo::ZDLFileInfo() = default;

ZDLFileInfo::ZDLFileInfo(const QString &file) :
        QFileInfo(file) {
}

QString ZDLFileInfo::GetFileDescription() {
    return baseName();
}

ZDLIwadInfo::ZDLIwadInfo() = default;

ZDLIwadInfo::ZDLIwadInfo(const QString &file) :
        ZDLFileInfo(file) {
}

QString ZDLIwadInfo::GetFileDescription() {
    QString iwad_name;

    QFile iwad_file(filePath());
    if (iwad_file.open(QFile::ReadOnly)) {
        QCryptographicHash hash(QCryptographicHash::Md5);

        if (hash.addData(&iwad_file)) {
            std::string const hashStr = hash.result().toHex().toStdString();
            auto it = iwadHashes().find(hashStr);

            if (it != iwadHashes().end()) {
                iwad_name = QString::fromStdString(it->second);
            }
        }

        iwad_file.close();
    }

    if (iwad_name.isEmpty()) {
        ZDLMapFile *mapfile = ZDLMapFile::getMapFile(filePath());

        if (mapfile != nullptr) {
            iwad_name = mapfile->getIwadinfoName();
            delete mapfile;
        }
    }

    if (iwad_name.isEmpty()) {
        std::string const wad = fileName().toLower().toStdString();
        auto it = iwadFiles().find(wad);

        if (it != iwadFiles().end()) {
            iwad_name = QString::fromStdString(it->second);
        }
    }

    return iwad_name.isEmpty() ? fileName().toUpper() : iwad_name;
}

ZDLAppInfo::ZDLAppInfo() = default;

ZDLAppInfo::ZDLAppInfo(const QString &file) :
        ZDLFileInfo(file) {
}

QString ZDLAppInfo::GetFileDescription() {
    std::string const file = baseName().toLower().toStdString();
    auto it = sourcePorts().find(file);

    if (it != sourcePorts().end()) {
        return QString::fromStdString(it->second);
    }

    return baseName();
}
