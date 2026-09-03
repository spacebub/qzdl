import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Zdl
import Zdl.Components

/*
The page ZDL is for: what to play, what to play it with, and what to load on
top of it. Everything on it belongs to the profile named at the top, so
switching profiles swaps the whole page at once.
*/
Item {
    id: page

    property var pick: null
    property var confirm: null
    property var prompt: null
    property var command: null
    property var about: null

    signal launched()

    // "(Default)" is index 0 in both of these, which is also what 0 means in
    // the config, so a picker's index is the stored value and back again.
    readonly property var skills: [ "V. Easy", "Easy", "Medium", "Hard", "V. Hard" ]
    readonly property var monsters: [ "No monsters", "Fast", "Respawn", "Fast & respawn" ]

    ColumnLayout {
        anchors.fill: parent
        spacing: 14

        // Which profile is being edited, and what can be done to it.
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Picker {
                id: profiles

                Layout.fillWidth: true
                Layout.maximumWidth: 420
                options: App.config.profileNames
                current: App.config.profileIndex
                label: "Profile"
                hint: "Which launch configuration is being edited and launched"
                onSelected: function (index) { App.config.profileIndex = index }
            }

            GlyphButton {
                glyph: "cog"
                outlined: true
                size: Theme.control
                hint: "Manage profiles"
                turn: profileMenu.visible ? 90 : 0
                Layout.alignment: Qt.AlignBottom
                onClicked: profileMenu.visible ? profileMenu.close() : profileMenu.open()

                ActionMenu {
                    id: profileMenu

                    y: parent.height + 4
                    x: -width + parent.width

                    items: [
                        { label: "New profile", glyph: "plus" },
                        { label: "Duplicate this profile", glyph: "extract" },
                        { label: "Rename this profile…", glyph: "edit" },
                        { separator: true },
                        { label: "Delete this profile", glyph: "trash", danger: true }
                    ]

                    onTriggered: function (index) { page.profileAction(index) }
                }
            }

            Item { Layout.fillWidth: true }

            Fact {
                label: "Launches"
                value: App.config.port === "" ? "No source port" : App.config.port
                hint: App.config.commandLine
                clickable: true
                maximumWidth: 260
                Layout.alignment: Qt.AlignBottom
                onActivated: page.command.show()
            }
        }

        // The two halves of a launch: what is loaded, and how it is played.
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 14

            Surface {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 280

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            Layout.fillWidth: true
                            text: "External files"
                            color: Theme.text
                            font.pixelSize: Theme.fontMedium
                            font.weight: Theme.headingWeight
                            textFormat: Text.PlainText
                        }

                        Pill {
                            visible: App.config.files.count > 0
                            dot: false
                            text: App.config.files.enabledCount === App.config.files.count
                                ? App.config.files.count + " loaded"
                                : App.config.files.enabledCount + " of " + App.config.files.count + " loaded"
                            tone: Theme.muted
                            wash: Theme.mutedSoft
                        }

                        GlyphButton {
                            glyph: "plus"
                            hint: "Add files"
                            onClicked: page.pick.openMany("Add external files", App.wadFilters,
                                                          function (paths) { App.config.files.add(paths) },
                                                          "wad")
                        }

                        GlyphButton {
                            glyph: "trash"
                            hint: "Remove every file from this profile"
                            enabled: App.config.files.count > 0
                            hoverTone: Theme.danger
                            onClicked: page.confirm.ask(
                                "Clear the external file list?",
                                "Every file in this profile's list is removed. The files themselves are "
                                + "left alone, and the rest of the profile is untouched.",
                                "Clear", true,
                                function () { App.config.files.clear() })
                        }
                    }

                    Surface {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        inset: true

                        EmptyState {
                            anchors.centerIn: parent
                            width: parent.width - 48
                            visible: App.config.files.count === 0
                            title: "Nothing loaded"
                            body: "WADs, PK3s, DEH and BEX patches, demos and configs go here. They are "
                                + "passed to the source port in the order they are listed."
                        }

                        ListView {
                            id: files

                            anchors.fill: parent
                            anchors.margins: 6
                            clip: true
                            spacing: 1
                            model: App.config.files

                            ScrollBar.vertical: ScrollBar {
                                id: fileBar
                                visible: fileBar.size < 1
                                width: 8

                                contentItem: Rectangle {
                                    radius: 4
                                    color: Theme.borderStrong
                                    opacity: fileBar.pressed ? 0.9 : 0.45
                                }
                            }

                            delegate: Item {
                                id: row

                                required property string file
                                required property string name
                                required property string directory
                                required property bool enabled
                                required property bool missing
                                required property int index

                                width: files.width - (fileBar.visible ? fileBar.width + 4 : 0)
                                height: App.config.showPaths ? 46 : 34

                                Wash {
                                    anchors.fill: parent
                                    rounding: Theme.radiusSmall - 2
                                    hovered: hover.hovered
                                }

                                Check {
                                    id: mark
                                    anchors.left: parent.left
                                    anchors.leftMargin: 10
                                    anchors.verticalCenter: parent.verticalCenter
                                    checked: row.enabled
                                    hint: row.enabled ? "Loaded. Click to leave it out"
                                                      : "Not loaded. Click to load it"
                                    onToggled: function (value) { App.config.files.setEnabled(row.index, value) }
                                }

                                Column {
                                    anchors.left: mark.right
                                    anchors.leftMargin: 10
                                    anchors.right: tools.left
                                    anchors.rightMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 1

                                    Row {
                                        width: parent.width
                                        spacing: 7

                                        Text {
                                            width: Math.min(implicitWidth, parent.width - (row.missing ? 60 : 0))
                                            text: row.name
                                            color: row.missing ? Theme.danger
                                                 : row.enabled ? Theme.text
                                                 : Theme.faint
                                            font.pixelSize: Theme.fontBody
                                            font.strikeout: !row.enabled
                                            elide: Text.ElideRight
                                            textFormat: Text.PlainText
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        Text {
                                            visible: row.missing
                                            text: "missing"
                                            color: Theme.danger
                                            font.pixelSize: Theme.fontTiny
                                            font.weight: Font.DemiBold
                                            anchors.verticalCenter: parent.verticalCenter
                                            textFormat: Text.PlainText
                                        }
                                    }

                                    PathLabel {
                                        visible: App.config.showPaths
                                        width: parent.width
                                        room: parent.width
                                        path: row.directory
                                        clickable: true
                                        opens: row.directory
                                    }
                                }

                                // Only on the row under the pointer: five sets of
                                // arrows down the side of a list is a wall of them.
                                Row {
                                    id: tools

                                    anchors.right: parent.right
                                    anchors.rightMargin: 6
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 1
                                    opacity: hover.hovered ? 1 : 0

                                    Behavior on opacity { NumberAnimation { duration: 110 } }

                                    GlyphButton {
                                        glyph: "up"
                                        size: 26
                                        enabled: row.index > 0
                                        hint: "Load this one earlier"
                                        onClicked: App.config.files.move(row.index, -1)
                                    }

                                    GlyphButton {
                                        glyph: "down"
                                        size: 26
                                        enabled: row.index < App.config.files.count - 1
                                        hint: "Load this one later"
                                        onClicked: App.config.files.move(row.index, 1)
                                    }

                                    GlyphButton {
                                        glyph: "cross"
                                        size: 26
                                        hoverTone: Theme.danger
                                        hint: "Remove from the list"
                                        onClicked: App.config.files.remove(row.index)
                                    }
                                }

                                HoverHandler { id: hover }
                            }
                        }
                    }
                }
            }

            // How it is played: the port, the game, and the four dials.
            Surface {
                Layout.preferredWidth: 340
                Layout.maximumWidth: 340
                Layout.fillHeight: true

                Flickable {
                    anchors.fill: parent
                    anchors.margins: 16
                    contentHeight: game.implicitHeight
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    Column {
                        id: game
                        width: parent.width
                        spacing: 14

                        Text {
                            text: "Game"
                            color: Theme.text
                            font.pixelSize: Theme.fontMedium
                            font.weight: Theme.headingWeight
                            textFormat: Text.PlainText
                        }

                        Picker {
                            width: parent.width
                            label: "Source port"
                            placeholder: "None selected"
                            options: App.config.ports.names
                            current: App.config.ports.indexOfName(App.config.port)
                            clearable: true
                            hint: "What actually runs. Add ports on the Settings page."
                            onSelected: function (index) {
                                App.config.port = index < 0 ? "" : App.config.ports.names[index]
                            }
                        }

                        Picker {
                            width: parent.width
                            label: "IWAD"
                            placeholder: "None selected"
                            options: App.config.iwads.names
                            current: App.config.iwads.indexOfName(App.config.iwad)
                            clearable: true
                            hint: "The game itself. Add IWADs on the Settings page."
                            onSelected: function (index) {
                                App.config.iwad = index < 0 ? "" : App.config.iwads.names[index]
                            }
                        }

                        Picker {
                            width: parent.width
                            label: "Map"
                            options: App.config.maps
                            current: App.config.maps.indexOf(App.config.warp)
                            clearable: true
                            hint: "Read out of the IWAD and everything loaded on top of it"
                            onSelected: function (index) {
                                App.config.warp = index < 0 ? "" : App.config.maps[index]
                            }
                        }

                        // The two short ones share a row: side by side they still
                        // read, and the card then holds the whole of the game
                        // without anything having to be scrolled to.
                        Row {
                            width: parent.width
                            spacing: 12

                            Picker {
                                width: (parent.width - parent.spacing) / 2
                                label: "Skill"
                                options: page.skills
                                current: App.config.skill - 1
                                clearable: true
                                onSelected: function (index) { App.config.skill = index + 1 }
                            }

                            Picker {
                                width: (parent.width - parent.spacing) / 2
                                label: "Monsters"
                                options: page.monsters
                                current: App.config.monsters - 1
                                clearable: true
                                onSelected: function (index) { App.config.monsters = index + 1 }
                            }
                        }
                    }
                }
            }
        }

        // Multiplayer, folded away until it is wanted. Which way it is left is
        // remembered with the profile, since a profile is usually one or the other.
        Surface {
            Layout.fillWidth: true
            Layout.preferredHeight: multiplayer.implicitHeight + 32

            Behavior on Layout.preferredHeight {
                NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
            }

            Column {
                id: multiplayer

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 16
                spacing: 14

                Row {
                    width: parent.width
                    spacing: 10

                    Glyph {
                        name: App.config.multiplayerOpen ? "up" : "down"
                        weight: 1
                        tone: Theme.faint
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: "Multiplayer"
                        color: Theme.text
                        font.pixelSize: Theme.fontMedium
                        font.weight: Theme.headingWeight
                        anchors.verticalCenter: parent.verticalCenter
                        textFormat: Text.PlainText
                    }

                    Pill {
                        visible: App.config.gameType !== 0
                        text: App.config.gameType === 1 ? "Co-op"
                            : App.config.gameType === 2 ? "Deathmatch"
                            : "Alt deathmatch"
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        visible: App.config.gameType === 0
                        text: "Off — this profile launches a single player game"
                        color: Theme.faint
                        font.pixelSize: Theme.fontSmall
                        anchors.verticalCenter: parent.verticalCenter
                        textFormat: Text.PlainText
                    }
                }

                Grid {
                    width: parent.width
                    visible: App.config.multiplayerOpen
                    columns: Math.max(1, Math.floor(width / 220))
                    columnSpacing: 12
                    rowSpacing: 12

                    readonly property real cell: (width - (columns - 1) * 12) / columns

                    Picker {
                        width: parent.cell
                        label: "Game type"
                        placeholder: "Singleplayer"
                        options: [ "Co-op", "Deathmatch", "Alt deathmatch" ]
                        current: App.config.gameType - 1
                        clearable: true
                        onSelected: function (index) { App.config.gameType = index + 1 }
                    }

                    Picker {
                        width: parent.cell
                        label: "Players"
                        placeholder: "Joining"
                        options: [ "1", "2", "3", "4", "5", "6", "7", "8" ]
                        current: App.config.players - 1
                        clearable: true
                        enabled: App.config.gameType !== 0
                        hint: "How many this machine hosts. Leave it unset to join someone else's game."
                        onSelected: function (index) { App.config.players = index + 1 }
                    }

                    Field {
                        width: parent.cell
                        label: "Host address"
                        placeholder: "host or host:port"
                        text: App.config.host
                        enabled: App.config.gameType !== 0 && App.config.players === 0
                        mono: true
                        onTextChanged: App.config.host = text
                    }

                    Field {
                        width: parent.cell
                        label: "Port"
                        placeholder: "Default"
                        text: App.config.netPort
                        enabled: App.config.gameType !== 0
                        mono: true
                        onTextChanged: App.config.netPort = text
                    }

                    Field {
                        width: parent.cell
                        label: "Frag limit"
                        placeholder: "None"
                        text: App.config.fragLimit
                        enabled: App.config.gameType !== 0
                        mono: true
                        onTextChanged: App.config.fragLimit = text
                    }

                    Field {
                        width: parent.cell
                        label: "Time limit"
                        placeholder: "None"
                        text: App.config.timeLimit
                        enabled: App.config.gameType !== 0
                        mono: true
                        onTextChanged: App.config.timeLimit = text
                    }

                    Field {
                        width: parent.cell
                        label: "dmflags"
                        placeholder: "None"
                        text: App.config.dmflags
                        enabled: App.config.gameType !== 0
                        mono: true
                        onTextChanged: App.config.dmflags = text
                    }

                    Field {
                        width: parent.cell
                        label: "dmflags2"
                        placeholder: "None"
                        text: App.config.dmflags2
                        enabled: App.config.gameType !== 0
                        mono: true
                        onTextChanged: App.config.dmflags2 = text
                    }

                    Picker {
                        width: parent.cell
                        label: "Net mode"
                        options: [ "0 (classic peer to peer)", "1 (client/server)" ]
                        current: App.config.netmode
                        clearable: true
                        enabled: App.config.gameType !== 0
                        onSelected: function (index) { App.config.netmode = index }
                    }

                    Picker {
                        width: parent.cell
                        label: "Duplicate tics"
                        options: [ "1", "2", "3", "4", "5", "6", "7", "8", "9" ]
                        current: App.config.dup - 1
                        clearable: true
                        enabled: App.config.gameType !== 0
                        hint: "Sends each tic more than once, for a lossy connection"
                        onSelected: function (index) { App.config.dup = index + 1 }
                    }

                    Picker {
                        width: parent.cell
                        label: "Extra tic"
                        placeholder: "Off"
                        options: [ "On" ]
                        current: App.config.extratic - 1
                        clearable: true
                        enabled: App.config.gameType !== 0
                        onSelected: function (index) { App.config.extratic = index + 1 }
                    }

                    PathField {
                        width: parent.cell
                        label: "Load save game"
                        placeholder: "None"
                        text: App.config.savegame
                        enabled: App.config.gameType !== 0
                        pick: page.pick
                        filters: App.saveFilters
                        remember: "save"
                        browseTitle: "Select a save game"
                        onTextChanged: App.config.savegame = text
                    }
                }
            }

            // The whole header is the target, not just the arrow beside it.
            MouseArea {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 48
                cursorShape: Qt.PointingHandCursor
                onClicked: App.config.multiplayerOpen = !App.config.multiplayerOpen
            }
        }

        Field {
            Layout.fillWidth: true
            label: "Extra command line arguments"
            placeholder: "Passed to the source port as typed"
            text: App.config.extra
            mono: true
            onTextChanged: App.config.extra = text
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            AppButton {
                text: "Actions"
                glyph: "cog"
                compact: true
                onClicked: actions.visible ? actions.close() : actions.open()

                ActionMenu {
                    id: actions

                    y: -height - 4

                    items: [
                        { label: "Show command line", glyph: "extract" },
                        { label: "Clear this profile", glyph: "refresh" },
                        { separator: true },
                        { label: "Load .zdl…", glyph: "download" },
                        { label: "Save .zdl…", glyph: "up" },
                        { separator: true },
                        { label: "Open a config file…", glyph: "folder" },
                        { label: "Save the config as…", glyph: "up" },
                        { label: "Use this as the user config", glyph: "check",
                          enabled: !App.config.userConfig },
                        { separator: true },
                        { separator: true },
                        { label: "About ZDL", glyph: "system" },
                        { label: "Clear everything", glyph: "trash", danger: true }
                    ]

                    onTriggered: function (index) { page.action(index) }
                }
            }

            Item { Layout.fillWidth: true }

            AppButton {
                text: "Command line"
                onClicked: page.command.show()
            }

            AppButton {
                text: "Launch"
                variant: "primary"
                glyph: "check"
                enabled: App.config.port !== ""
                hint: App.config.port === "" ? "Pick a source port first" : App.config.commandLine
                onClicked: page.launch()
            }
        }
    }

    function launch() {
        if (App.config.launch()) {
            page.launched()
        }
    }

    function profileAction(index) {
        if (index === 0) {
            prompt.ask("New profile", "Name", "New profile", "Create",
                       function (name) { App.config.addProfile(name) })
        } else if (index === 1) {
            App.config.duplicateProfile()
        } else if (index === 2) {
            prompt.ask("Rename profile", "Name", App.config.profileName, "Rename",
                       function (name) { App.config.renameProfile(name) })
        } else if (index === 4) {
            confirm.ask("Delete \"" + App.config.profileName + "\"?",
                        "The profile and everything in it goes. The files it loaded are left alone.",
                        "Delete", true,
                        function () { App.config.removeProfile() })
        }
    }

    function action(index) {
        if (index === 0) {
            command.show()
        } else if (index === 1) {
            confirm.ask("Clear \"" + App.config.profileName + "\"?",
                        "Everything this profile launches is emptied: the port, the game, the files "
                        + "and the multiplayer settings. The profile itself stays.",
                        "Clear", true,
                        function () { App.config.clearProfile() })
        } else if (index === 3) {
            pick.open("Load a .zdl launch config", App.zdlFilters, false,
                      function (path) { App.config.loadZdl(path) }, "zdl")
        } else if (index === 4) {
            pick.open("Save this profile as a .zdl", App.zdlFilters, true,
                      function (directory) {
                          App.config.saveZdl(directory + "/" + App.config.profileName + ".zdl")
                      }, "zdl")
        } else if (index === 6) {
            pick.open("Open a config file", App.configFilters, false,
                      function (path) { App.config.load(path) }, "config")
        } else if (index === 7) {
            pick.open("Save the config into", [ "*" ], true,
                      function (directory) { App.config.saveAs(directory + "/zdl.json") }, "config")
        } else if (index === 8) {
            confirm.ask("Use this as the user config?",
                        "This config replaces the one ZDL opens by default, at "
                        + App.prettyPath(App.config.path) + ".",
                        "Replace it", false,
                        function () { App.config.adoptAsUserConfig() })
        } else if (index === 10) {
            about.show()
        } else if (index === 11) {
            confirm.ask("Clear everything?",
                        "Every profile, every IWAD and every source port is removed. Nothing on disk "
                        + "is touched, but this config is emptied and cannot be got back.",
                        "Clear everything", true,
                        function () { App.config.clearEverything() })
        }
    }
}
