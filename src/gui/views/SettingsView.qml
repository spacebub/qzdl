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

// Everything that is the application's rather than any one profile's: the
// engines, how launching behaves, and the file all of it is kept in.
Item {
    id: page

    property var pick: null
    property var confirm: null
    property var about: null

    // The same inset the other pages use, so they line up as they swap.
    readonly property int bleed: 16

    // What the downloads add up to is counted when the page is looked at
    // rather than kept up to date behind its back.
    onVisibleChanged: {
        if (visible) {
            App.browse.measure()
        }
    }

    function measure(bytes) {
        return bytes >= 1048576 ? (bytes / 1048576).toFixed(1) + " MB"
             : bytes > 0 ? Math.max(1, Math.round(bytes / 1024)) + " KB"
             : "Nothing"
    }


    ColumnLayout {
        anchors.fill: parent
        spacing: 16

        Column {
            Layout.fillWidth: true
            Layout.leftMargin: page.bleed
            Layout.rightMargin: page.bleed
            spacing: 3

            Text {
                text: "Settings"
                color: Theme.text
                font.pixelSize: Theme.fontDisplay
                font.weight: Theme.headingWeight
                textFormat: Text.PlainText
            }

            Text {
                text: "How launching behaves, and where all of it is kept"
                color: Theme.faint
                font.pixelSize: Theme.fontSmall
                textFormat: Text.PlainText
            }
        }

        Flickable {
            id: sheet

            Layout.fillWidth: true
            Layout.fillHeight: true

            contentWidth: width
            contentHeight: body.implicitHeight + 8
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

            ColumnLayout {
                id: body

                x: page.bleed
                width: sheet.width - page.bleed * 2 - (bar.visible ? bar.width : 0)
                spacing: 16

                // Two columns while there is room for two, one when there is not.
                readonly property bool wide: body.width >= 760

                readonly property real half: body.wide
                    ? (body.width - 32 - 20) / 2
                    : body.width - 32

                Surface {
                    Layout.fillWidth: true
                    Layout.preferredHeight: behaviour.implicitHeight + 32

                    Column {
                        id: behaviour

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 16
                        spacing: 16

                        Text {
                            text: "Behaviour"
                            color: Theme.text
                            font.pixelSize: Theme.fontMedium
                            font.weight: Theme.headingWeight
                            textFormat: Text.PlainText
                        }

                        Grid {
                            width: parent.width
                            columns: body.wide ? 2 : 1
                            columnSpacing: 20
                            rowSpacing: 16

                            // One of them carries a pill beside its label and
                            // is that much taller for it, so they are lined up
                            // by the boxes rather than by the labels.
                            verticalItemAlignment: Grid.AlignBottom

                            Field {
                                width: body.half
                                label: "Always add these arguments"
                                placeholder: "Added to every launch, whatever the profile"
                                text: App.config.alwaysAdd
                                mono: true
                                onTextChanged: App.config.alwaysAdd = text
                            }

                            PathField {
                                width: body.half
                                label: "DOSBox"
                                placeholder: "Only for source ports that are DOS programs"

                                /*
                                The one this machine already has is filled in
                                rather than described, so it can be seen and
                                typed over like any other.
                                */
                                text: App.config.dosbox !== ""
                                    ? App.config.dosbox
                                    : App.config.systemDosbox

                                pick: page.pick
                                filters: App.portFilters
                                remember: "src"
                                browseTitle: "Select DOSBox"
                                onTextChanged: App.config.dosbox = text

                                /*
                                Where the path in the field came from: the one
                                this machine already had, one named by hand, or
                                none at all. One that is not there any more is
                                the state worth the colour.
                                */
                                labelBadge: Pill {
                                    id: badge

                                    readonly property string path: App.config.dosbox !== ""
                                        ? App.config.dosbox
                                        : App.config.systemDosbox

                                    readonly property string kind: badge.path === "" ? "none"
                                        : !App.isFile(badge.path) ? "missing"
                                        : badge.path === App.config.systemDosbox ? "system"
                                        : "custom"

                                    height: 22

                                    text: badge.kind === "none" ? "Not found"
                                        : badge.kind === "missing" ? "Missing"
                                        : badge.kind === "system" ? "System"
                                        : "Custom"

                                    tone: badge.kind === "none" ? Theme.warning
                                        : badge.kind === "missing" ? Theme.danger
                                        : badge.kind === "system" ? Theme.success
                                        : Theme.accent

                                    wash: badge.kind === "none" ? Theme.warningSoft
                                        : badge.kind === "missing" ? Theme.dangerSoft
                                        : badge.kind === "system" ? Theme.successSoft
                                        : Theme.accentSoft
                                }
                            }
                        }

                        Rectangle {
                            width: parent.width
                            height: 1
                            color: Theme.border
                        }

                        Grid {
                            width: parent.width
                            columns: body.wide ? 2 : 1
                            columnSpacing: 20
                            rowSpacing: 18

                            Toggle {
                                width: body.half
                                text: "Close on launch"
                                checked: App.config.autoClose
                                hint: "Quit ZDL as soon as the source port has started"
                                onToggled: function (value) { App.config.autoClose = value }
                            }

                            Toggle {
                                width: body.half
                                text: "Show file paths"
                                checked: App.config.showPaths
                                hint: "Show the directory a file came from underneath its name"
                                onToggled: function (value) { App.config.showPaths = value }
                            }

                            Toggle {
                                width: body.half
                                text: "Launch .zdl files at once"
                                checked: App.config.launchZdlImmediately
                                hint: "A .zdl given on the command line launches without showing "
                                    + "this window"
                                onToggled: function (value) {
                                    App.config.launchZdlImmediately = value
                                }
                            }

                            Toggle {
                                width: body.half
                                text: "A config file per profile"
                                checked: App.config.profileConfigs
                                hint: "Each profile keeps the source port's own settings -- "
                                    + "controls, video, sound -- in a file of its own, instead of "
                                    + "every profile sharing one"
                                onToggled: function (value) { App.config.profileConfigs = value }
                            }
                        }

                        Rectangle {
                            width: parent.width
                            height: 1
                            color: Theme.border
                        }

                        // The one of them that is not a switch, and the only
                        // thing on its line, so it takes the whole width.
                        Row {
                            width: parent.width
                            spacing: 12

                            Text {
                                width: parent.width - view.width - parent.spacing
                                text: "Open the library on"
                                color: Theme.text
                                font.pixelSize: Theme.fontBody
                                elide: Text.ElideRight
                                anchors.verticalCenter: parent.verticalCenter
                                textFormat: Text.PlainText
                            }

                            Segmented {
                                id: view

                                anchors.verticalCenter: parent.verticalCenter
                                current: App.config.startView
                                options: [
                                    { key: "profiles", label: "Profiles" },
                                    { key: "games", label: "Games" }
                                ]
                                onSelected: function (key) { App.config.startView = key }
                            }
                        }
                    }
                }

                Surface {
                    Layout.fillWidth: true
                    Layout.preferredHeight: kept.implicitHeight + 32

                    Column {
                        id: kept

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 16
                        spacing: 16

                        Text {
                            text: "This config"
                            color: Theme.text
                            font.pixelSize: Theme.fontMedium
                            font.weight: Theme.headingWeight
                            textFormat: Text.PlainText
                        }

                        Fact {
                            label: "Configuration file"
                            value: App.config.path
                            path: true
                            clickable: true
                            hint: "Open the directory it is in"
                            maximumWidth: parent.width - 40
                            onActivated: App.reveal(App.directoryOf(App.config.path))
                        }

                        Flow {
                            width: parent.width
                            spacing: 8

                            AppButton {
                                text: "Open a config…"
                                glyph: "folder"
                                compact: true
                                hint: "Work on a different config file from here on"
                                onClicked: page.pick.open(
                                    "Open a config file", App.configFilters, false,
                                    function (path) { App.config.load(path) }, "config")
                            }

                            AppButton {
                                text: "Save as…"
                                glyph: "up"
                                compact: true
                                hint: "Write this config somewhere else and work on it there "
                                    + "from now on"
                                onClicked: page.pick.open(
                                    "Save the config into", [ "*" ], true,
                                    function (directory) {
                                        App.config.saveAs(directory + "/zdl.json")
                                    }, "config")
                            }

                            AppButton {
                                text: "Use as the user config"
                                glyph: "check"
                                compact: true
                                enabled: !App.config.userConfig
                                hint: App.config.userConfig
                                    ? "This is already the one ZDL opens by default"
                                    : "Make this the one ZDL opens by default"
                                onClicked: page.confirm.ask(
                                    "Use this as the user config?",
                                    "This config replaces the one ZDL opens by default, at "
                                    + App.prettyPath(App.config.path) + ".",
                                    "Replace it", false,
                                    function () { App.config.adoptAsUserConfig() })
                            }

                            AppButton {
                                text: "Import a .zdl…"
                                glyph: "download"
                                compact: true
                                hint: "Read a .zdl launch config in as a profile of its own"
                                onClicked: page.pick.open(
                                    "Load a .zdl launch config", App.zdlFilters, false,
                                    function (path) { App.config.loadZdl(path) }, "zdl")
                            }

                            AppButton {
                                text: "Clear everything"
                                glyph: "trash"
                                variant: "danger"
                                compact: true
                                hint: "Empties this config: every profile with the files and "
                                    + "settings in it, every game, and every source port. The "
                                    + "wads and the ports themselves are left where they are"
                                onClicked: page.confirm.ask(
                                    "Clear everything?",
                                    "Every profile, every game and every source port is removed. "
                                    + "Nothing on disk is touched, but this config is emptied and "
                                    + "cannot be got back.",
                                    "Clear everything", true,
                                    function () { App.config.clearEverything() })
                            }
                        }

                        Rectangle {
                            width: parent.width
                            height: 1
                            color: Theme.border
                        }

                        /*
                        The flag lives in the user config rather than this one,
                        and an older ZDL could set it with no way back. It is
                        shown here so it can be turned off again.
                        */
                        Toggle {
                            width: parent.width
                            text: "Skip the user config at startup"
                            checked: App.config.ignoreUserConfig
                            hint: "ZDL loads a portable config kept next to its program "
                                + "instead. If there is none there, it falls back to the "
                                + "user config anyway"
                            onToggled: function (value) { App.config.ignoreUserConfig = value }
                        }
                    }
                }

                Surface {
                    Layout.fillWidth: true
                    Layout.preferredHeight: shelf.implicitHeight + 32

                    Column {
                        id: shelf

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 16
                        spacing: 14

                        Text {
                            text: "Downloads"
                            color: Theme.text
                            font.pixelSize: Theme.fontMedium
                            font.weight: Theme.headingWeight
                            textFormat: Text.PlainText
                        }

                        Row {
                            width: parent.width
                            spacing: 14

                            Column {
                                width: parent.width - empty.width - parent.spacing
                                spacing: 6
                                anchors.verticalCenter: parent.verticalCenter

                                Fact {
                                    label: "Where they are kept"
                                    value: App.browse.downloads
                                    path: true
                                    clickable: true
                                    hint: "Open the directory"
                                    maximumWidth: parent.width
                                    onActivated: App.reveal(App.browse.downloads)
                                }

                                Text {
                                    width: parent.width
                                    text: page.measure(App.browse.cached)
                                        + " kept · what a port was fetched from stays here, so "
                                        + "fetching it again does not bring it down twice"
                                    color: Theme.faint
                                    font.pixelSize: Theme.fontSmall
                                    elide: Text.ElideRight
                                    textFormat: Text.PlainText
                                }
                            }

                            AppButton {
                                id: empty

                                text: "Empty it"
                                glyph: "trash"
                                variant: "danger"
                                compact: true
                                enabled: App.browse.cached > 0
                                anchors.verticalCenter: parent.verticalCenter

                                hint: App.browse.cached > 0
                                    ? "Delete what was downloaded. The ports already unpacked "
                                      + "are left alone"
                                    : "There is nothing being kept"

                                onClicked: page.confirm.ask(
                                    "Empty the downloads?",
                                    "The archives ZDL fetched are deleted. Every port already "
                                    + "unpacked stays where it is, and anything fetched after "
                                    + "this comes down the wire afresh.",
                                    "Empty it", true,
                                    function () { App.browse.clearDownloads() })
                            }
                        }
                    }
                }

                Surface {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 76

                    Row {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: 16
                        spacing: 14

                        Image {
                            width: 40
                            height: 40
                            sourceSize.width: 128
                            sourceSize.height: 128
                            source: "qrc:/qzdl-128.png"
                            smooth: true
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Column {
                            width: parent.width - 40 - 14 - marks.width - 28
                            spacing: 2
                            anchors.verticalCenter: parent.verticalCenter

                            Text {
                                text: "ZDL " + App.version
                                color: Theme.text
                                font.pixelSize: Theme.fontBody
                                font.weight: Font.DemiBold
                                textFormat: Text.PlainText
                            }

                            Text {
                                text: "A launcher for ZDoom based Doom engine source ports · Qt "
                                    + App.qtVersion
                                color: Theme.faint
                                font.pixelSize: Theme.fontSmall
                                elide: Text.ElideRight
                                width: parent.width
                                textFormat: Text.PlainText
                            }
                        }

                        Row {
                            id: marks
                            spacing: 8
                            anchors.verticalCenter: parent.verticalCenter

                            AppButton {
                                text: "Project page"
                                variant: "ghost"
                                compact: true
                                onClicked: Qt.openUrlExternally(
                                    "https://github.com/spacebub/qzdl")
                            }

                            AppButton {
                                text: "About"
                                compact: true
                                onClicked: page.about.show()
                            }
                        }
                    }
                }
            }
        }
    }
}
