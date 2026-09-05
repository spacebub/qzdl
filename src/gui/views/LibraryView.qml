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
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Zdl
import Zdl.Components

/*
The shelf, and the page the application opens on: the profiles somebody has
set up, and the games those are built on, which can also be played as they
are.
*/
Item {
    id: page

    property var pick: null
    property var confirm: null
    property var prompt: null
    property var entry: null

    // profiles | games. Where it starts is a setting; where it goes after that
    // is whatever the last thing to ask for a half asked for.
    property string mode: App.config.startView

    property string filter: ""

    signal launched()

    // Somebody wants a profile's own page rather than a launch.
    signal opened()

    // There is nothing to run with, and ports are set up on the other page.
    signal enginesRequested()

    // Room left around the shelf for the cards' shadows.
    readonly property int bleed: 16

    readonly property int gutter: 20

    // Cards are stretched to fill the row rather than left at their own width,
    // so that the last one in a row ends where the header does.
    readonly property real room: shelf.width - page.bleed * 2
    readonly property int columns: Math.max(1, Math.floor((room + gutter) / (Theme.cardWidth + gutter)))
    readonly property real cell: (room - (columns - 1) * gutter) / columns

    readonly property var profiles: page.sift(App.config.profileCards)
    readonly property var games: page.sift(App.config.iwads.entries)
    readonly property bool showingProfiles: page.mode === "profiles"

    function sift(list) {
        if (page.filter === "") {
            return list
        }

        const wanted = page.filter.toLowerCase()

        return list.filter(function (each) {
            return each.name.toLowerCase().indexOf(wanted) >= 0
        })
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 18

        // Inset by as much as the shelf, so the heading starts where the first
        // card does.
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: page.bleed
            Layout.rightMargin: page.bleed
            spacing: 14

            Column {
                Layout.fillWidth: true
                spacing: 3

                Text {
                    text: "Library"
                    color: Theme.text
                    font.pixelSize: Theme.fontDisplay
                    font.weight: Theme.headingWeight
                    textFormat: Text.PlainText
                }

                Text {
                    text: page.showingProfiles
                        ? page.count(App.config.profileCards.length, "profile")
                          + " · press a card to set one up, or the play button to run it"
                        : page.count(App.config.iwads.count, "game")
                          + " · everything the profiles are built on"
                    color: Theme.faint
                    font.pixelSize: Theme.fontSmall
                    textFormat: Text.PlainText
                }
            }

            // Nothing to choose from is not a choice, and engines are a page
            // away, so the control says so and goes there.
            AppButton {
                Layout.alignment: Qt.AlignVCenter
                visible: !page.showingProfiles && App.config.ports.count === 0
                text: "Add a port…"
                glyph: "plus"
                compact: true
                hint: "Nothing here can run until a source port is set up"
                onClicked: page.enginesRequested()
            }

            // The shelf plays a game on its own, so it needs its own answer to
            // what runs it rather than borrowing whichever profile is open.
            Picker {
                Layout.preferredWidth: 180
                Layout.alignment: Qt.AlignVCenter
                visible: !page.showingProfiles && App.config.ports.count > 0
                options: App.config.ports.names
                current: App.config.ports.indexOfName(App.config.gamePort)
                clearable: true
                placeholder: "(Profile's port)"
                hint: "What a game on this shelf launches with"
                onSelected: function (index) {
                    App.config.gamePort = index < 0 ? "" : App.config.ports.names[index]
                }
            }

            Segmented {
                Layout.alignment: Qt.AlignVCenter
                current: page.mode
                options: [
                    { key: "profiles", label: "Profiles" },
                    { key: "games", label: "Games" }
                ]
                onSelected: function (key) { page.mode = key }
            }

            Field {
                Layout.preferredWidth: 190
                Layout.alignment: Qt.AlignVCenter
                leading: "search"
                placeholder: "Filter"
                text: page.filter
                onTextChanged: page.filter = text
            }
        }

        Flickable {
            id: shelf

            Layout.fillWidth: true
            Layout.fillHeight: true

            contentWidth: width
            contentHeight: flow.y + flow.implicitHeight + 24
                           + (nothing.visible ? nothing.height + 28 : 0)
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar {
                id: bar
                visible: bar.size < 1
                width: 8

                contentItem: Rectangle {
                    radius: 4
                    color: Theme.borderStrong
                    opacity: bar.pressed ? 0.9 : 0.45
                }
            }

            Grid {
                id: flow

                x: page.bleed
                y: 10
                width: page.room
                columns: page.columns
                columnSpacing: page.gutter
                rowSpacing: page.gutter

                Repeater {
                    model: page.showingProfiles ? page.profiles : []

                    delegate: LibraryCard {
                        required property var modelData

                        width: page.cell
                        title: modelData.name
                        caption: modelData.iwad === "" ? "NO GAME" : modelData.iwad.toUpperCase()
                        artFile: modelData.iwadFile
                        subtitle: modelData.port === "" ? "No source port" : modelData.port
                        playable: modelData.ready
                        status: App.runs.states[modelData.key] || ""

                        // Read through the map so it is worked out again
                        // whenever a run moves.
                        statusReason: App.runs.states[modelData.key]
                            ? App.runs.reason(modelData.key) : ""
                        primary: "open"

                        playHint: modelData.ready
                            ? "Launch " + modelData.name
                            : "This profile has no source port to run"

                        badges: {
                            const shown = []

                            if (modelData.dosPort) {
                                shown.push({ text: "DOS" })
                            }

                            if (!modelData.ready) {
                                shown.push({
                                    text: "No port",
                                    tone: Theme.warning,
                                    wash: Theme.warningSoft
                                })
                            } else if (modelData.files > 0) {
                                shown.push({
                                    text: modelData.loaded === modelData.files
                                        ? page.count(modelData.files, "file")
                                        : modelData.loaded + " of " + modelData.files + " loaded"
                                })
                            }

                            /*
                            None of the multiplayer settings reach a DOS
                            port's command line, so the card does not claim
                            that profile is in a game with anyone.
                            */
                            if (modelData.netRole !== 0 && !modelData.dosPort) {
                                shown.push({
                                    text: modelData.netRole === 1 ? "Hosting" : "Multiplayer"
                                })
                            }

                            return shown
                        }

                        actions: [
                            { action: "open", label: "Set this one up", glyph: "edit" },
                            { action: "launch", label: "Launch it", glyph: "play",
                              enabled: modelData.ready },
                            { separator: true },
                            { action: "duplicate", label: "Duplicate", glyph: "extract" },
                            { action: "rename", label: "Rename…", glyph: "edit" },
                            { separator: true },
                            { action: "delete", label: "Delete", glyph: "trash", danger: true }
                        ]

                        onPlayed: page.launch(modelData.index)
                        onOpened: page.open(modelData.index)
                        onLogRequested: App.runs.show(modelData.key)
                        onTriggered: function (action) { page.profileAction(action, modelData.index) }
                    }
                }

                Repeater {
                    model: page.showingProfiles ? [] : page.games

                    delegate: LibraryCard {
                        required property var modelData

                        width: page.cell
                        title: modelData.name
                        caption: page.kindOf(modelData.kind)
                        artFile: modelData.missing ? "" : modelData.file
                        subtitle: App.prettyPath(modelData.directory)
                        playable: !modelData.missing
                        primary: "play"
                        status: App.runs.states[App.config.gameKey(modelData.name)] || ""
                        statusReason: App.runs.states[App.config.gameKey(modelData.name)]
                            ? App.runs.reason(App.config.gameKey(modelData.name)) : ""

                        playHint: {
                            if (modelData.missing) {
                                return "This file is not where the library says it is"
                            }

                            // Read so the line is worked out again when the
                            // shelf is pointed at another port.
                            App.config.gamePort

                            return App.config.gameCommandLine(modelData.name)
                        }

                        badges: modelData.missing
                            ? [ { text: "Missing", tone: Theme.danger, wash: Theme.dangerSoft } ]
                            : []

                        actions: [
                            { action: "play", label: "Play it", glyph: "play",
                              enabled: !modelData.missing },
                            { action: "use", label: "Use it in this profile", glyph: "check" },
                            { separator: true },
                            { action: "edit", label: "Rename…", glyph: "edit" },
                            { action: "reveal", label: "Show the folder it is in", glyph: "folder" },
                            { separator: true },
                            { action: "remove", label: "Remove from the library",
                              glyph: "trash", danger: true }
                        ]

                        onPlayed: page.playGame(modelData.name)
                        onOpened: page.playGame(modelData.name)
                        onLogRequested: App.runs.show(App.config.gameKey(modelData.name))
                        onTriggered: function (action) { page.gameAction(action, modelData) }
                    }
                }

                /*
                Not a thing but a verb, so no picture and no shadow: a hole in
                the shelf rather than something sitting on it. It is still cut
                out of the shelf though, and left the ground's own colour it
                would not be there at all.
                */
                Item {
                    width: page.cell
                    height: Theme.cardArt + 94

                    Rectangle {
                        anchors.fill: parent
                        radius: Theme.radius
                        color: hover.hovered ? Qt.tint(Theme.sunken, Theme.hover) : Theme.sunken
                        border.width: 1
                        border.color: hover.hovered ? Theme.accent : Theme.borderStrong

                        Behavior on color { ColorAnimation { duration: 130 } }
                        Behavior on border.color { ColorAnimation { duration: 130 } }
                    }

                    Column {
                        anchors.centerIn: parent
                        spacing: 10

                        Rectangle {
                            width: 46
                            height: 46
                            radius: width / 2
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: hover.hovered ? Theme.accentSoft : Theme.surface
                            border.width: 1
                            border.color: hover.hovered ? Theme.accent : Theme.borderStrong

                            Behavior on color { ColorAnimation { duration: 130 } }

                            Glyph {
                                anchors.centerIn: parent
                                name: "plus"
                                weight: 1.5
                                tone: hover.hovered ? Theme.accent : Theme.muted
                            }
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: page.showingProfiles ? "New profile" : "Add a game"
                            color: hover.hovered ? Theme.accent : Theme.muted
                            font.pixelSize: Theme.fontSmall
                            font.weight: Font.DemiBold
                            textFormat: Text.PlainText
                        }
                    }

                    HoverHandler {
                        id: hover
                        cursorShape: Qt.PointingHandCursor
                    }

                    TapHandler { onTapped: page.add() }
                }
            }

            EmptyState {
                id: nothing

                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: flow.bottom
                anchors.topMargin: 28
                width: Math.min(420, parent.width - 60)
                visible: page.filter !== ""
                         && (page.showingProfiles ? page.profiles.length : page.games.length) === 0
                title: "Nothing called that"
                body: "No " + (page.showingProfiles ? "profile" : "game")
                      + " here has \"" + page.filter + "\" in its name."
            }
        }
    }

    function count(number, thing) {
        return number + " " + thing + (number === 1 ? "" : "s")
    }

    function kindOf(extension) {
        return extension === "" ? "FILE" : extension.replace(".", "").toUpperCase()
    }

    function add() {
        if (page.showingProfiles) {
            page.prompt.ask("New profile", "Name", "New profile", "Create",
                            function (name) {
                                App.config.addProfile(name)
                                page.opened()
                            })

            return
        }

        // One step: what is picked is what is added, named after itself.
        page.pick.openMany("Add games", App.wadFilters,
                           function (paths) { App.config.iwads.addAll(paths) }, "wad")
    }

    function launch(index) {
        if (App.config.launchAt(index)) {
            page.launched()
        }
    }

    function playGame(name) {
        if (App.config.launchGame(name)) {
            page.launched()
        }
    }

    function open(index) {
        App.config.profileIndex = index
        page.opened()
    }

    function profileAction(action, index) {
        // Everything below works on the active profile, so the card that was
        // pressed becomes it first.
        App.config.profileIndex = index

        if (action === "open") {
            page.opened()
        } else if (action === "launch") {
            page.launch(index)
        } else if (action === "duplicate") {
            App.config.duplicateProfile()
        } else if (action === "rename") {
            page.prompt.ask("Rename profile", "Name", App.config.profileName, "Rename",
                            function (name) { App.config.renameProfile(name) })
        } else if (action === "delete") {
            page.confirm.ask("Delete \"" + App.config.profileName + "\"?",
                             "The profile and everything in it goes. The files it loaded are "
                             + "left alone.",
                             "Delete", true,
                             function () { App.config.removeProfile() })
        }
    }

    function gameAction(action, game) {
        if (action === "play") {
            page.playGame(game.name)
        } else if (action === "use") {
            App.config.iwad = game.name
            App.notify.success("\"" + App.config.profileName + "\" now plays " + game.name + ".")
        } else if (action === "edit") {
            page.entry.ask("Edit " + game.name, App.config.iwads, App.wadFilters, "wad",
                           game.name, game.file,
                           function (name, file) { App.config.iwads.update(game.index, name, file) })
        } else if (action === "reveal") {
            App.reveal(App.directoryOf(game.file))
        } else if (action === "remove") {
            page.confirm.ask("Remove \"" + game.name + "\"?",
                             "It goes out of the library and out of every profile that named it. "
                             + "The file itself is left where it is.",
                             "Remove", true,
                             function () { App.config.iwads.remove(game.index) })
        }
    }
}
