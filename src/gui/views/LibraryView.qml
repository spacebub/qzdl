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
pragma ComponentBehavior: Bound

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

    // A filtered shelf is not the list itself, so there is nothing to reorder
    // there: what is between two cards on screen is not what is between them.
    readonly property bool reorderable: page.filter === ""

    // Whichever of the two the shelf is showing, which is the one a card
    // dropped on another is moved in.
    readonly property var shelved: page.showingProfiles ? App.config.profiles : App.config.iwads

    function matches(name) {
        return page.filter === ""
               || name.toLowerCase().indexOf(page.filter.toLowerCase()) >= 0
    }

    function sift(list) {
        if (page.filter === "") {
            return list
        }

        return list.filter(function (each) { return page.matches(each.name) })
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

                move: Transition {
                    NumberAnimation {
                        properties: "x,y"
                        duration: 160
                        easing.type: Easing.OutQuad
                    }
                }

                Repeater {
                    model: page.showingProfiles ? App.config.profiles : null

                    // The card leaves the grid while it is being dragged, so
                    // an empty slot holds its place and catches the drop.
                    delegate: Slot {
                        id: slot

                        required property var profile

                        width: page.cell
                        height: Theme.cardArt + 94

                        // Filtering hides cards rather than taking them out of
                        // the model, which would be a different order to drag.
                        visible: page.matches(slot.profile.name)

                        LibraryCard {
                            id: card

                            readonly property bool loadedAll:
                                slot.profile.loaded === slot.profile.files

                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.verticalCenter: parent.verticalCenter
                            width: slot.width
                            draggable: page.reorderable
                            origin: slot

                            states: State {
                                when: card.dragging

                                ParentChange { target: card; parent: shelf }

                                AnchorChanges {
                                    target: card
                                    anchors.horizontalCenter: undefined
                                    anchors.verticalCenter: undefined
                                }

                                PropertyChanges { card.z: 3 }
                            }

                            title: slot.profile.name
                            artFile: slot.profile.iwadFile

                            caption: slot.profile.iwad === ""
                                ? "NO GAME" : slot.profile.iwad.toUpperCase()

                            subtitle: slot.profile.port === ""
                                ? "No source port" : slot.profile.port

                            playable: slot.profile.ready
                            status: App.runs.states[slot.profile.key] || ""

                            // Read through the map so it is worked out again
                            // whenever a run moves.
                            statusReason: App.runs.states[slot.profile.key]
                                ? App.runs.reason(slot.profile.key) : ""
                            primary: "open"

                            playHint: slot.profile.ready
                                ? "Launch " + slot.profile.name
                                : "This profile has no source port to run"

                            badges: {
                                const shown = []

                                if (slot.profile.dosPort) {
                                    shown.push({ text: "DOS" })
                                }

                                if (!slot.profile.ready) {
                                    shown.push({
                                        text: "No port",
                                        tone: Theme.warning,
                                        wash: Theme.warningSoft
                                    })
                                } else if (slot.profile.files > 0) {
                                    shown.push({
                                        text: card.loadedAll
                                            ? page.count(slot.profile.files, "file")
                                            : slot.profile.loaded + " of "
                                              + slot.profile.files + " loaded"
                                    })
                                }

                                /*
                                None of the multiplayer settings reach a DOS
                                port's command line, so the card does not claim
                                that profile is in a game with anyone.
                                */
                                if (slot.profile.netRole !== 0 && !slot.profile.dosPort) {
                                    shown.push({
                                        text: slot.profile.netRole === 1 ? "Hosting" : "Multiplayer"
                                    })
                                }

                                return shown
                            }

                            actions: [
                                { action: "open", label: "Set this one up", glyph: "edit" },
                                { action: "launch", label: "Launch it", glyph: "play",
                                  enabled: slot.profile.ready },
                                { separator: true },
                                { action: "duplicate", label: "Duplicate", glyph: "extract" },
                                { action: "rename", label: "Rename…", glyph: "edit" },
                                { separator: true },
                                { action: "delete", label: "Delete", glyph: "trash", danger: true }
                            ]

                            onPlayed: page.launch(slot.profile.index)
                            onOpened: page.open(slot.profile.index)
                            onLogRequested: App.runs.show(slot.profile.key)
                            onTriggered: function (action) {
                                page.profileAction(action, slot.profile.index)
                            }
                        }

                        DropArea {
                            anchors.fill: parent

                            onEntered: function (event) {
                                App.config.profiles.moveTo((event.source as Slot).index,
                                                           slot.index)
                            }
                        }
                    }
                }

                Repeater {
                    model: page.showingProfiles ? null : App.config.iwads

                    delegate: Slot {
                        id: box

                        required property string name
                        required property string file
                        required property string directory
                        required property string kind
                        required property bool missing

                        readonly property string key: App.config.gameKey(box.name)

                        width: page.cell
                        height: Theme.cardArt + 94
                        visible: page.matches(box.name)

                        LibraryCard {
                            id: game

                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.verticalCenter: parent.verticalCenter
                            width: box.width
                            draggable: page.reorderable
                            origin: box

                            states: State {
                                when: game.dragging

                                ParentChange { target: game; parent: shelf }

                                AnchorChanges {
                                    target: game
                                    anchors.horizontalCenter: undefined
                                    anchors.verticalCenter: undefined
                                }

                                PropertyChanges { game.z: 3 }
                            }

                            title: box.name
                            caption: page.kindOf(box.kind)
                            artFile: box.missing ? "" : box.file
                            subtitle: App.prettyPath(box.directory)
                            playable: !box.missing
                            primary: "play"
                            status: App.runs.states[box.key] || ""

                            statusReason: App.runs.states[box.key]
                                ? App.runs.reason(box.key) : ""

                            playHint: {
                                if (box.missing) {
                                    return "This file is not where the library says it is"
                                }

                                // Read so the line is worked out again when the
                                // shelf is pointed at another port.
                                App.config.gamePort

                                return App.config.gameCommandLine(box.name)
                            }

                            badges: box.missing
                                ? [ { text: "Missing", tone: Theme.danger, wash: Theme.dangerSoft } ]
                                : []

                            actions: [
                                { action: "play", label: "Play it", glyph: "play",
                                  enabled: !box.missing },
                                { action: "use", label: "Use it in this profile", glyph: "check" },
                                { separator: true },
                                { action: "edit", label: "Rename…", glyph: "edit" },
                                { action: "reveal", label: "Show the folder it is in",
                                  glyph: "folder" },
                                { separator: true },
                                { action: "remove", label: "Remove from the library",
                                  glyph: "trash", danger: true }
                            ]

                            onPlayed: page.playGame(box.name)
                            onOpened: page.playGame(box.name)
                            onLogRequested: App.runs.show(box.key)
                            onTriggered: function (action) {
                                page.gameAction(action, box.index, box.name, box.file)
                            }
                        }

                        DropArea {
                            anchors.fill: parent

                            onEntered: function (event) {
                                App.config.iwads.moveTo((event.source as Slot).index,
                                                        box.index)
                            }
                        }
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

                    // It sits where the end of the list is, which is what
                    // dropping a card on it asks for.
                    DropArea {
                        anchors.fill: parent
                        enabled: page.reorderable

                        onEntered: function (event) {
                            page.shelved.moveTo((event.source as Slot).index,
                                                page.shelved.count - 1)
                        }
                    }
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

    function gameAction(action, index, name, file) {
        if (action === "play") {
            page.playGame(name)
        } else if (action === "use") {
            App.config.iwad = name
            App.notify.success("\"" + App.config.profileName + "\" now plays " + name + ".")
        } else if (action === "edit") {
            page.entry.ask("Edit " + name, App.config.iwads, App.wadFilters, "wad", name, file,
                           function (named, at) { App.config.iwads.update(index, named, at) })
        } else if (action === "reveal") {
            App.reveal(App.directoryOf(file))
        } else if (action === "remove") {
            page.confirm.ask("Remove \"" + name + "\"?",
                             "It goes out of the library and out of every profile that named it. "
                             + "The file itself is left where it is.",
                             "Remove", true,
                             function () { App.config.iwads.remove(index) })
        }
    }
}
