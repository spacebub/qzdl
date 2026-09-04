/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
 *
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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

#include <QVariant>
#include <QtQml/qqmlregistration.h>

#include "gui/FileList.h"
#include "gui/NameList.h"
#include "gui/Notifier.h"
#include "gui/Runs.h"

class ConfigBridge : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Reached through App.config")

    /* Which profile is being edited, and what there is to choose from. */
    Q_PROPERTY(QStringList profileNames READ profileNames NOTIFY profilesChanged)
    Q_PROPERTY(int profileIndex READ profileIndex WRITE setProfileIndex NOTIFY profileChanged)
    Q_PROPERTY(QString profileName READ profileName NOTIFY profileChanged)

    /** What the active profile's runs are filed under in App.runs. */
    Q_PROPERTY(QString profileKey READ profileKey NOTIFY profileChanged)

    /** Every profile as the library draws it, all of them at once. */
    Q_PROPERTY(QVariantList profileCards READ profileCards NOTIFY profilesChanged)

    /* The active profile. Every one of these is a field on the launch page. */
    Q_PROPERTY(QString iwad READ iwad WRITE setIwad NOTIFY profileChanged)
    Q_PROPERTY(QString port READ port WRITE setPort NOTIFY profileChanged)
    Q_PROPERTY(int skill READ skill WRITE setSkill NOTIFY profileChanged)
    Q_PROPERTY(int monsters READ monsters WRITE setMonsters NOTIFY profileChanged)
    Q_PROPERTY(QString warp READ warp WRITE setWarp NOTIFY profileChanged)
    Q_PROPERTY(QString extra READ extra WRITE setExtra NOTIFY profileChanged)

    /** Whether the multiplayer panel is open, which is remembered per profile. */
    Q_PROPERTY(bool multiplayerOpen READ multiplayerOpen WRITE setMultiplayerOpen NOTIFY profileChanged)

    /** Launches this profile on the port's own config rather than its own. */
    Q_PROPERTY(bool sharedConfig READ sharedConfig WRITE setSharedConfig NOTIFY profileChanged)

    /** Whether this profile's runs keep what they print. */
    Q_PROPERTY(bool captureOutput READ captureOutput WRITE setCaptureOutput NOTIFY profileChanged)

    /**
     * Where this profile's own source port config is kept. It reads the same
     * whether or not anything is launched with it, so the page can say where
     * the settings would go before they are asked for.
     */
    Q_PROPERTY(QString configFile READ configFile NOTIFY profileChanged)

    /* The multiplayer panel. It only reaches the command line when the game
     * type is something other than singleplayer. */
    Q_PROPERTY(int gameType READ gameType WRITE setGameType NOTIFY multiplayerChanged)
    Q_PROPERTY(int players READ players WRITE setPlayers NOTIFY multiplayerChanged)
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY multiplayerChanged)
    Q_PROPERTY(QString netPort READ netPort WRITE setNetPort NOTIFY multiplayerChanged)
    Q_PROPERTY(QString fragLimit READ fragLimit WRITE setFragLimit NOTIFY multiplayerChanged)
    Q_PROPERTY(QString timeLimit READ timeLimit WRITE setTimeLimit NOTIFY multiplayerChanged)
    Q_PROPERTY(QString dmflags READ dmflags WRITE setDmflags NOTIFY multiplayerChanged)
    Q_PROPERTY(QString dmflags2 READ dmflags2 WRITE setDmflags2 NOTIFY multiplayerChanged)
    Q_PROPERTY(int extratic READ extratic WRITE setExtratic NOTIFY multiplayerChanged)
    Q_PROPERTY(int netmode READ netmode WRITE setNetmode NOTIFY multiplayerChanged)
    Q_PROPERTY(int dup READ dup WRITE setDup NOTIFY multiplayerChanged)
    Q_PROPERTY(QString savegame READ savegame WRITE setSavegame NOTIFY multiplayerChanged)

    /* Settings that outlive any one profile. */
    Q_PROPERTY(QString alwaysAdd READ alwaysAdd WRITE setAlwaysAdd NOTIFY generalChanged)
    Q_PROPERTY(bool autoClose READ autoClose WRITE setAutoClose NOTIFY generalChanged)
    Q_PROPERTY(bool launchZdlImmediately READ launchZdlImmediately
               WRITE setLaunchZdlImmediately NOTIFY generalChanged)
    Q_PROPERTY(bool rememberFileList READ rememberFileList WRITE setRememberFileList NOTIFY generalChanged)
    Q_PROPERTY(bool showPaths READ showPaths WRITE setShowPaths NOTIFY generalChanged)
    Q_PROPERTY(bool profileConfigs READ profileConfigs WRITE setProfileConfigs NOTIFY generalChanged)

    Q_PROPERTY(FileList *files READ files CONSTANT)
    Q_PROPERTY(NameList *iwads READ iwads CONSTANT)
    Q_PROPERTY(NameList *ports READ ports CONSTANT)

    /**
     * Every map the active profile could warp to. Reading it walks the IWAD
     * and every file switched on, so it is worked out once and held until one
     * of those changes.
     */
    Q_PROPERTY(QStringList maps READ maps NOTIFY mapsChanged)

    /** What the source port would be handed, as one line. */
    Q_PROPERTY(QString commandLine READ commandLine NOTIFY commandLineChanged)

    /** Which file all of this is being read from and written to. */
    Q_PROPERTY(QString path READ path NOTIFY pathChanged)

    /** True when the config in use is the per user one rather than some other. */
    Q_PROPERTY(bool userConfig READ userConfig NOTIFY pathChanged)

public:
    explicit ConfigBridge(Notifier *notifier, Runs *runs, QObject *parent = nullptr);

    [[nodiscard]] static QStringList profileNames() ;
    [[nodiscard]] static int profileIndex() ;
    [[nodiscard]] static QString profileName() ;
    [[nodiscard]] static QString profileKey();
    [[nodiscard]] static QVariantList profileCards();

    [[nodiscard]] static QString iwad();
    [[nodiscard]] static QString port();
    [[nodiscard]] static int skill();
    [[nodiscard]] static int monsters();
    [[nodiscard]] static QString warp();
    [[nodiscard]] static QString extra();
    [[nodiscard]] static bool multiplayerOpen();
    [[nodiscard]] static bool sharedConfig();
    [[nodiscard]] static QString configFile();

    [[nodiscard]] static int gameType();
    [[nodiscard]] static int players();
    [[nodiscard]] static QString host();
    [[nodiscard]] static QString netPort();
    [[nodiscard]] static QString fragLimit();
    [[nodiscard]] static QString timeLimit();
    [[nodiscard]] static QString dmflags();
    [[nodiscard]] static QString dmflags2();
    [[nodiscard]] static int extratic();
    [[nodiscard]] static int netmode();
    [[nodiscard]] static int dup();
    [[nodiscard]] static QString savegame();

    [[nodiscard]] static QString alwaysAdd();
    [[nodiscard]] static bool autoClose();
    [[nodiscard]] static bool launchZdlImmediately();
    [[nodiscard]] static bool rememberFileList();
    [[nodiscard]] static bool showPaths();
    [[nodiscard]] static bool captureOutput();
    [[nodiscard]] static bool profileConfigs();

    [[nodiscard]] FileList *files() const;
    [[nodiscard]] NameList *iwads() const;
    [[nodiscard]] NameList *ports() const;

    [[nodiscard]] QStringList maps() const;
    [[nodiscard]] static QString commandLine();
    [[nodiscard]] static QString path();
    [[nodiscard]] static bool userConfig();

    void setProfileIndex(int index);
    void setIwad(const QString &value);
    void setPort(const QString &value);
    void setSkill(int value);
    void setMonsters(int value);
    void setWarp(const QString &value);
    void setExtra(const QString &value);
    void setMultiplayerOpen(bool value);
    void setSharedConfig(bool value);

    void setGameType(int value);
    void setPlayers(int value);
    void setHost(const QString &value);
    void setNetPort(const QString &value);
    void setFragLimit(const QString &value);
    void setTimeLimit(const QString &value);
    void setDmflags(const QString &value);
    void setDmflags2(const QString &value);
    void setExtratic(int value);
    void setNetmode(int value);
    void setDup(int value);
    void setSavegame(const QString &value);

    void setAlwaysAdd(const QString &value);
    void setAutoClose(bool value);
    void setLaunchZdlImmediately(bool value);
    void setRememberFileList(bool value);
    void setShowPaths(bool value);
    void setCaptureOutput(bool value);
    void setProfileConfigs(bool value);

    /* Profiles. */

    Q_INVOKABLE void addProfile(const QString &name);
    Q_INVOKABLE void duplicateProfile();
    Q_INVOKABLE void renameProfile(const QString &name);
    Q_INVOKABLE void removeProfile();

    /* Clearing, in the three sizes the old ZDL menu offered. */

    /** Empties the external file list and nothing else. */
    Q_INVOKABLE void clearFiles() const;

    /** Empties the active profile, keeping the profile itself. */
    Q_INVOKABLE void clearProfile();

    /** Throws away every profile, IWAD and source port. */
    Q_INVOKABLE void clearEverything();

    /* The file this is all kept in. */

    Q_INVOKABLE bool save() const;
    Q_INVOKABLE bool saveAs(const QString &path);
    Q_INVOKABLE bool load(const QString &path);

    /** Copies this config to the per user location and works on it there. */
    Q_INVOKABLE bool adoptAsUserConfig();

    /* .zdl launch configs, which other Doom tools also read and write. */

    Q_INVOKABLE bool loadZdl(const QString &path);
    Q_INVOKABLE bool saveZdl(const QString &path) const;

    /** Runs the port with everything the active profile works out to. */
    Q_INVOKABLE bool launch();

    /** Makes the profile at this row the active one and launches it. */
    Q_INVOKABLE bool launchAt(int index);

    /**
     * One game on its own: this IWAD on the active profile's port, none of its
     * files, and nothing written back to the profile.
     */
    Q_INVOKABLE bool launchGame(const QString &iwad);

    /** What launchGame would hand the port, for saying so before it is asked for. */
    [[nodiscard]] Q_INVOKABLE static QString gameCommandLine(const QString &iwad);

    /** The name a game launched on its own is filed under in App.runs. */
    [[nodiscard]] Q_INVOKABLE static QString gameKey(const QString &iwad);

    /** Tells every page to read the config again, after it has been replaced. */
    Q_INVOKABLE void reload();

signals:
    void profilesChanged();
    void profileChanged();
    void multiplayerChanged();
    void generalChanged();
    void mapsChanged();
    void commandLineChanged();
    void pathChanged();

    /** The port was started, so a config set to close on launch can do it. */
    void launched();

private:
    /** Anything that changes what would be launched. */
    void touch();

    /** Runs one worked out config and files what came of it under this name. */
    bool start(const QString &key, const QString &title, const Config &what);

    Notifier *_notifier;
    Runs *_runs;
    FileList *_files;
    NameList *_iwads;
    NameList *_ports;

    mutable QStringList _maps;
    mutable bool _mapsKnown{false};
};
