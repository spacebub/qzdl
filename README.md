# ZDL

## 1. License

Copyright (c) 2023 spacebub  
Copyright (c) 2018-2019 Lcferrum  
Copyright (c) 2004-2012 ZDL Software Foundation

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

You should have received a copy of the GNU General Public License along with
this program. If not, see <http://www.gnu.org/licenses/>.

This program uses SimpleWFA and miniz libraries. See `AUTHORS` and associated
`LICENSE` files for details.

## 2. About

ZDL is a frontend ("launcher") for most popular Doom engine source ports. It
provides a convenient interface to launch Doom engine games and their third-party
modifications (WADs) using the selected source port. Launch configurations can be
saved on disc to be later used by ZDL to quickly launch selected game or
modification. ZDL can also be used to launch multiplayer sessions.

## 3. Where to get

You can compile binary by yourself (refer to `COMPILE` that comes with the
sources) or download binary distribution from GitHub releases section. Project
homepage at GitHub:

<https://github.com/spacebub/qzdl>

## 4. Installation and usage

ZDL is distributed as a single portable executable. Just copy it somewhere and
run it. On Windows executable additionally requires Microsoft Visual C++ 2010
SP1 x86 redistributable package to be installed on the machine. If it's not
already installed, download it from here:

<https://www.microsoft.com/en-us/download/details.aspx?id=8328>

Linux distribution of ZDL is tested to be working with most popular modern
distros. If it's not working for you, check that all binary dependencies are
met (listed in `DEPENDS` file). If dependencies are satisfied but binary still
fails to run, the only option is to compile ZDL from the sources.

ZDL is just a frontend, you should get Doom source port and needed game assets
(IWADs and PWADs) separately.

ZDL interface should be self-explanatory to someone familiar with Doom source
ports. There are two pages: Launch is used to configure and start a game, and
Settings is used to manage the collection of IWADs and source ports. The
Multiplayer panel on the Launch page folds open when you want it. For the
quickstart:

1) Switch to Settings and add IWADs and source ports

   a) Add IWAD files to the IWAD list. Each one is named after the game
   it holds, worked out from the file itself

   b) Add source port executables to the source port list

2) Switch to Launch and configure launch options

   a) Select a source port and an IWAD

   b) If necessary, add any additional files (PWADs, configs, patches) to
   the external file list. They are passed to the source port in the
   order they are listed, and each one can be switched off without
   being removed

3) Click 'Launch' to launch the source port using the current configuration

4) ZDL automatically saves IWADs and source ports lists and, by default,
   also remembers launch configuration

Launch configurations are kept as named profiles, selected from the drop-down
at the top of the Launch page. Use the button beside it to create, duplicate,
rename or delete them.

The interface follows the desktop's light or dark setting. The button in the
title bar switches between following it, light and dark.

A profile is nothing more than the launch options you had set while it was
selected: source port, IWAD, external files, map, skill and the rest. Nothing
is tied to a particular game, so a profile can be a game, a single mod, a
multiplayer setup or whatever else you want to come back to. Switching
profiles is always your own doing, done from that drop-down; picking an IWAD
just sets the IWAD on the profile you are already in.

Everything above is what ZDL passes to the source port; the port's own
settings -- controls, video, sound -- normally live in one file it shares
between every launch. Switch on "A config file per profile" on the Settings
page and each profile is instead given a config file of its own, which the
port is pointed at with `-config`. Every source port in wide use understands
that; the ports in the Chocolate Doom family, which split their settings over
two files, are also handed `-extraconfig`. The files are written by the port
itself, in `$XDG_DATA_HOME/qzdl` (`~/.local/share/qzdl` by default) or
`%APPDATA%\qZDL`, and they are named after the profile they belong to. Renaming
a profile afterwards leaves its config where it is, so its settings follow it.

A profile that should not have its own -- one that is only a different set of
WADs, and wants the settings you already have -- can be switched back with
"Use the port's own config", beside the source port on the Launch page.

Settings are stored in `zdl.json`. On Windows it sits next to the executable, so
ZDL stays a self-contained portable install; if an older ZDL already kept a
config under AppData, that folder carries on being used instead. Elsewhere it
goes in `$XDG_CONFIG_HOME/qzdl` (`~/.config/qzdl` by default), and a `zdl.json`
placed next to the executable still takes precedence if you want a portable
setup there too. A config from an older ZDL is found in its previous location,
converted automatically the first time this version runs, and left in place.
Individual launch configurations can still be exported to and imported from
`.zdl` files.
