import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Zdl
import Zdl.Components

// One profile: what runs it, what it plays, what is loaded on top, and the
// button that starts it.
Item {
    id: page

    property var pick: null
    property var confirm: null
    property var prompt: null
    property var command: null

    signal launched()

    /** The way back to the shelf. */
    signal closed()

    // "(Default)" is index 0 in both of these, which is also what 0 means in
    // the config, so a picker's index is the stored value and back again.
    readonly property var skills: [ "V. Easy", "Easy", "Medium", "Hard", "V. Hard" ]
    readonly property var monsters: [ "No monsters", "Fast", "Respawn", "Fast & respawn" ]

    // The same inset the library uses, so the two pages line up as they swap.
    readonly property int bleed: 16

    readonly property bool ready: App.config.port !== ""

    ColumnLayout {
        anchors.fill: parent
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: page.bleed
            Layout.rightMargin: page.bleed
            spacing: 14

            // The heading is the way to the other profiles: a page about one
            // thing says which, and saying which is where one changes it.
            Item {
                id: chooser

                Layout.fillWidth: true
                Layout.preferredHeight: 74

                Wash {
                    anchors.fill: parent
                    anchors.leftMargin: -8
                    anchors.rightMargin: -8
                    rounding: Theme.radius
                    hovered: header.hovered
                    selected: profiles.visible
                    tint: Theme.mutedSoft
                }

                CardArt {
                    id: thumb

                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: 116
                    height: 66
                    rounding: Theme.radiusSmall
                    bottomRounding: Theme.radiusSmall
                    lit: header.hovered
                    caption: App.config.iwad === "" ? "NO GAME" : App.config.iwad.toUpperCase()
                }

                Column {
                    anchors.left: thumb.right
                    anchors.leftMargin: 14
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4

                    Row {
                        width: parent.width
                        spacing: 10

                        Text {
                            width: Math.min(implicitWidth, parent.width - 130)
                            text: App.config.profileName
                            color: Theme.text
                            font.pixelSize: Theme.fontDisplay
                            font.weight: Theme.headingWeight
                            elide: Text.ElideRight
                            textFormat: Text.PlainText
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        RunPill {
                            status: App.runs.states[App.config.profileKey] || ""
                            reason: App.runs.states[App.config.profileKey]
                                ? App.runs.reason(App.config.profileKey) : ""
                            anchors.verticalCenter: parent.verticalCenter
                            onClicked: App.runs.show(App.config.profileKey)
                        }
                    }

                    Text {
                        width: parent.width
                        text: page.summary()
                        color: page.ready ? Theme.faint : Theme.warning
                        font.pixelSize: Theme.fontSmall
                        elide: Text.ElideRight
                        textFormat: Text.PlainText
                    }
                }

                HoverHandler {
                    id: header
                    cursorShape: Qt.PointingHandCursor
                }

                TapHandler {
                    onTapped: profiles.visible ? profiles.close() : profiles.open()
                }

                Popup {
                    id: profiles

                    y: chooser.height + 6
                    width: Math.min(Math.max(chooser.width, 280), 460)
                    padding: 5
                    modal: false

                    // Pressing the header again folds it away rather than
                    // closing and opening it in the one gesture.
                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

                    readonly property int rows: Math.min(App.config.profileCards.length, 8)

                    height: profiles.rows * 52 + 10 + adder.height

                    background: Rectangle {
                        color: Theme.raised
                        radius: Theme.radiusSmall
                        border.width: 1
                        border.color: Theme.borderStrong
                    }

                    enter: Transition {
                        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 110 }
                    }

                    contentItem: Item {
                        ListView {
                            id: others

                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.bottom: adder.top
                            clip: true
                            model: App.config.profileCards
                            currentIndex: App.config.profileIndex

                            ScrollBar.vertical: ScrollBar {
                                id: pick
                                visible: pick.size < 1
                                width: 7

                                contentItem: Rectangle {
                                    radius: 3.5
                                    color: Theme.borderStrong
                                    opacity: pick.pressed ? 0.9 : 0.45
                                }
                            }

                            delegate: Item {
                                id: entry

                                required property var modelData

                                readonly property bool current:
                                    entry.modelData.index === App.config.profileIndex

                                width: others.width - (pick.visible ? pick.width + 2 : 0)
                                height: 52

                                Wash {
                                    anchors.fill: parent
                                    anchors.margins: 1
                                    selected: entry.current
                                    hovered: over.hovered
                                }

                                CardArt {
                                    id: chip

                                    anchors.left: parent.left
                                    anchors.leftMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 60
                                    height: 36
                                    rounding: Theme.radiusSmall - 3
                                    bottomRounding: Theme.radiusSmall - 3
                                }

                                Column {
                                    anchors.left: chip.right
                                    anchors.leftMargin: 10
                                    anchors.right: parent.right
                                    anchors.rightMargin: 10
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 1

                                    Text {
                                        width: parent.width
                                        text: entry.modelData.name
                                        color: entry.current ? Theme.accent : Theme.text
                                        font.pixelSize: Theme.fontBody
                                        font.weight: entry.current ? Font.DemiBold : Font.Normal
                                        elide: Text.ElideRight
                                        textFormat: Text.PlainText
                                    }

                                    Text {
                                        width: parent.width
                                        text: entry.modelData.port === ""
                                            ? "No source port"
                                            : entry.modelData.port + " · "
                                              + (entry.modelData.iwad === ""
                                                 ? "no game" : entry.modelData.iwad)
                                        color: Theme.faint
                                        font.pixelSize: Theme.fontTiny
                                        elide: Text.ElideRight
                                        textFormat: Text.PlainText
                                    }
                                }

                                HoverHandler {
                                    id: over
                                    cursorShape: Qt.PointingHandCursor
                                }

                                TapHandler {
                                    onTapped: {
                                        App.config.profileIndex = entry.modelData.index
                                        profiles.close()
                                    }
                                }
                            }

                            Component.onCompleted:
                                others.positionViewAtIndex(others.currentIndex, ListView.Contain)
                        }

                        Item {
                            id: adder

                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 38

                            Rectangle {
                                anchors.top: parent.top
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.margins: 4
                                height: 1
                                color: Theme.border
                            }

                            Wash {
                                anchors.fill: parent
                                anchors.topMargin: 5
                                hovered: fresh.hovered
                            }

                            Row {
                                anchors.left: parent.left
                                anchors.leftMargin: 14
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.verticalCenterOffset: 2
                                spacing: 9

                                Glyph {
                                    name: "plus"
                                    weight: 1
                                    tone: fresh.hovered ? Theme.accent : Theme.faint
                                    anchors.verticalCenter: parent.verticalCenter
                                }

                                Text {
                                    text: "New profile…"
                                    color: fresh.hovered ? Theme.accent : Theme.text
                                    font.pixelSize: Theme.fontBody
                                    anchors.verticalCenter: parent.verticalCenter
                                    textFormat: Text.PlainText
                                }
                            }

                            HoverHandler {
                                id: fresh
                                cursorShape: Qt.PointingHandCursor
                            }

                            TapHandler {
                                onTapped: {
                                    profiles.close()
                                    page.prompt.ask("New profile", "Name", "New profile", "Create",
                                                    function (name) { App.config.addProfile(name) })
                                }
                            }
                        }
                    }
                }
            }

            GlyphButton {
                glyph: "terminal"
                size: Theme.control
                outlined: true
                visible: App.config.captureOutput
                enabled: App.runs.logged.indexOf(App.config.profileKey) >= 0
                hint: enabled ? "Show what this profile printed"
                              : "Nothing has been launched from this profile yet"
                Layout.alignment: Qt.AlignVCenter
                onClicked: App.runs.show(App.config.profileKey)
            }

            GlyphButton {
                glyph: "cog"
                size: Theme.control
                outlined: true
                hint: "What else can be done with this profile"
                turn: menu.visible ? 90 : 0
                Layout.alignment: Qt.AlignVCenter
                onClicked: menu.visible ? menu.close() : menu.open()

                ActionMenu {
                    id: menu

                    y: parent.height + 4
                    x: -width + parent.width

                    items: [
                        { action: "rename", label: "Rename…", glyph: "edit" },
                        { action: "duplicate", label: "Duplicate", glyph: "extract" },
                        { action: "clear", label: "Empty this profile", glyph: "refresh" },
                        { separator: true },
                        { action: "saveZdl", label: "Save as .zdl…", glyph: "up" },
                        { separator: true },
                        { action: "delete", label: "Delete this profile", glyph: "trash",
                          danger: true }
                    ]

                    onTriggered: function (action) { page.act(action) }
                }
            }

            AppButton {
                text: "Launch"
                variant: "primary"
                glyph: "play"
                enabled: page.ready
                hint: page.ready ? App.config.commandLine : "Pick a source port first"
                Layout.alignment: Qt.AlignVCenter
                onClicked: page.launch()
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

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(340, game.implicitHeight + 32)
                    spacing: 16

                    Surface {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumWidth: 260

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 12

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Text {
                                    Layout.fillWidth: true
                                    text: "Add-ons"
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
                                        : App.config.files.enabledCount + " of "
                                          + App.config.files.count + " loaded"
                                    tone: Theme.muted
                                    wash: Theme.mutedSoft
                                }

                                GlyphButton {
                                    glyph: "plus"
                                    hint: "Add files"
                                    onClicked: page.pick.openMany(
                                        "Add files", App.wadFilters,
                                        function (paths) { App.config.files.add(paths) }, "wad")
                                }

                                GlyphButton {
                                    glyph: "trash"
                                    hint: "Remove every file from this profile"
                                    enabled: App.config.files.count > 0
                                    hoverTone: Theme.danger
                                    onClicked: page.confirm.ask(
                                        "Clear the file list?",
                                        "Every file in this profile's list is removed. The files "
                                        + "themselves are left alone, and the rest of the profile "
                                        + "is untouched.",
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
                                    body: "WADs, PK3s, DEH and BEX patches, demos and configs go "
                                        + "here. They are passed to the source port in the order "
                                        + "they are listed."
                                }

                                ListView {
                                    id: files

                                    anchors.fill: parent
                                    anchors.margins: 6
                                    clip: true
                                    spacing: 1
                                    model: App.config.files

                                    move: Transition {
                                        NumberAnimation {
                                            properties: "y"
                                            duration: 160
                                            easing.type: Easing.OutQuad
                                        }
                                    }

                                    displaced: Transition {
                                        NumberAnimation {
                                            properties: "y"
                                            duration: 160
                                            easing.type: Easing.OutQuad
                                        }
                                    }

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
                                        required property bool loaded
                                        required property bool missing
                                        required property int index

                                        width: files.width - (fileBar.visible ? fileBar.width + 4 : 0)
                                        height: App.config.showPaths ? 46 : 34

                                        // The row's own contents, which is what
                                        // leaves the list while being dragged.
                                        Item {
                                            id: content

                                            anchors.horizontalCenter: parent.horizontalCenter
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: row.width
                                            height: row.height

                                            Drag.active: grab.drag.active
                                            Drag.source: row
                                            Drag.hotSpot.x: content.width / 2
                                            Drag.hotSpot.y: content.height / 2

                                            states: State {
                                                when: grab.drag.active

                                                ParentChange { target: content; parent: files }

                                                AnchorChanges {
                                                    target: content
                                                    anchors.horizontalCenter: undefined
                                                    anchors.verticalCenter: undefined
                                                }

                                                PropertyChanges { content.z: 2 }
                                            }

                                            Wash {
                                                anchors.fill: parent
                                                rounding: Theme.radiusSmall - 2
                                                hovered: hover.hovered
                                                selected: grab.drag.active
                                                tint: Theme.mutedSoft
                                            }

                                            MouseArea {
                                                id: grab

                                                width: 22
                                                height: parent.height
                                                anchors.left: parent.left
                                                hoverEnabled: true
                                                cursorShape: Qt.SizeVerCursor
                                                drag.target: content
                                                drag.axis: Drag.YAxis

                                                Glyph {
                                                    anchors.centerIn: parent
                                                    name: "grip"
                                                    weight: 1
                                                    tone: grab.containsMouse || grab.drag.active
                                                        ? Theme.muted : Theme.border
                                                }
                                            }

                                            Check {
                                                id: mark
                                                anchors.left: grab.right
                                                anchors.leftMargin: 2
                                                anchors.verticalCenter: parent.verticalCenter
                                                checked: row.loaded
                                                hint: row.loaded ? "Loaded. Click to leave it out"
                                                                 : "Not loaded. Click to load it"
                                                onToggled: function (value) {
                                                    App.config.files.setEnabled(row.index, value)
                                                }
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
                                                        width: Math.min(implicitWidth,
                                                                        parent.width - (row.missing ? 60 : 0))
                                                        text: row.name
                                                        color: row.missing ? Theme.danger
                                                             : row.loaded ? Theme.text
                                                             : Theme.faint
                                                        font.pixelSize: Theme.fontBody
                                                        font.strikeout: !row.loaded
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
                                                }
                                            }

                                            Row {
                                                id: tools

                                                anchors.right: parent.right
                                                anchors.rightMargin: 6
                                                anchors.verticalCenter: parent.verticalCenter
                                                spacing: 1
                                                opacity: hover.hovered ? 1 : 0

                                                Behavior on opacity { NumberAnimation { duration: 110 } }

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

                                        // Stays where the row is while the contents
                                        // are away, and catches whatever is dropped.
                                        DropArea {
                                            anchors.fill: parent

                                            onEntered: function (event) {
                                                App.config.files.moveTo(event.source.index, row.index)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Surface {
                        Layout.preferredWidth: 340
                        Layout.maximumWidth: 340
                        Layout.fillHeight: true

                        Column {
                            id: game

                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 16
                            spacing: 14

                            Text {
                                text: "The run"
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
                                label: "Game"
                                placeholder: "None selected"
                                options: App.config.iwads.names
                                current: App.config.iwads.indexOfName(App.config.iwad)
                                clearable: true
                                hint: "The IWAD itself. Add games from the library."
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
                                hint: "Read out of the game and everything loaded on top of it"
                                onSelected: function (index) {
                                    App.config.warp = index < 0 ? "" : App.config.maps[index]
                                }
                            }

                            // The two short ones share a row: side by side they still
                            // read, and the card then holds the whole of the run.
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

                            Rectangle {
                                width: parent.width
                                height: 1
                                color: Theme.border
                            }

                            Toggle {
                                width: parent.width
                                text: "Record the game's output"
                                checked: App.config.captureOutput
                                hint: "Takes what the source port prints into a log along the "
                                    + "bottom of the window. Off, nothing is piped at all."
                                onToggled: function (value) { App.config.captureOutput = value }
                            }

                            // Only worth a line when profiles have configs of their
                            // own; with the setting off there is nothing to bypass.
                            Toggle {
                                width: parent.width
                                visible: App.config.profileConfigs
                                text: "Use the port's own settings"
                                checked: App.config.sharedConfig
                                hint: "Launch on the settings the source port keeps for itself, "
                                    + "shared with everything else that uses them"
                                onToggled: function (value) { App.config.sharedConfig = value }
                            }

                            Fact {
                                visible: App.config.profileConfigs && !App.config.sharedConfig
                                label: "Its own port settings"
                                value: App.config.configFile
                                path: true
                                clickable: true
                                hint: "Open the folder the source port writes them in"
                                maximumWidth: parent.width
                                onActivated: App.reveal(App.directoryOf(App.config.configFile))
                            }
                        }
                    }
                }

                Surface {
                    Layout.fillWidth: true
                    Layout.preferredHeight: line.implicitHeight + 32

                    Column {
                        id: line

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 16
                        spacing: 12

                        Text {
                            text: "Command line"
                            color: Theme.text
                            font.pixelSize: Theme.fontMedium
                            font.weight: Theme.headingWeight
                            textFormat: Text.PlainText
                        }

                        Field {
                            width: parent.width
                            label: "Extra arguments"
                            placeholder: "Passed to the source port as typed"
                            text: App.config.extra
                            mono: true
                            onTextChanged: App.config.extra = text
                        }

                        // What all of it comes out as, which is the thing being
                        // edited and so is worth having in front of one.
                        Surface {
                            width: parent.width
                            height: Math.min(resolved.implicitHeight + 24, 108)
                            inset: true
                            hovered: reach.hovered

                            HoverHandler {
                                id: reach
                                cursorShape: Qt.PointingHandCursor
                            }

                            TapHandler { onTapped: page.command.show() }

                            Hint {
                                text: "See the whole of it"
                                visible: reach.hovered
                            }

                            Text {
                                id: resolved

                                anchors.left: parent.left
                                anchors.right: copy.left
                                anchors.top: parent.top
                                anchors.margins: 12
                                text: App.config.commandLine === ""
                                    ? "Nothing to run yet." : App.config.commandLine
                                color: page.ready ? Theme.muted : Theme.faint
                                font.pixelSize: Theme.fontSmall
                                font.family: Theme.mono
                                wrapMode: Text.WrapAnywhere
                                maximumLineCount: 4
                                elide: Text.ElideRight
                                textFormat: Text.PlainText
                            }

                            GlyphButton {
                                id: copy

                                glyph: "extract"
                                size: 26
                                anchors.right: parent.right
                                anchors.rightMargin: 8
                                anchors.top: parent.top
                                anchors.topMargin: 8
                                hint: "Copy it"
                                onClicked: {
                                    App.copyToClipboard(App.config.commandLine)
                                    App.notify.success("The command line is on the clipboard.")
                                }
                            }
                        }
                    }
                }

                // Multiplayer, folded away until it is wanted. Which way it is left
                // is remembered with the profile.
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
                                hint: "How many this machine hosts. Leave it unset to join "
                                    + "someone else's game."
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
            }
        }
    }

    function summary() {
        if (!page.ready) {
            return "No source port — this profile cannot be launched yet"
        }

        let said = App.config.port

        said += " · " + (App.config.iwad === "" ? "no game" : App.config.iwad)

        if (App.config.warp !== "") {
            said += " · " + App.config.warp
        }

        const count = App.config.files.count

        if (count > 0) {
            said += " · " + count + (count === 1 ? " file" : " files")
        }

        return said
    }

    function launch() {
        if (App.config.launch()) {
            page.launched()
        }
    }

    function act(action) {
        if (action === "rename") {
            prompt.ask("Rename profile", "Name", App.config.profileName, "Rename",
                       function (name) { App.config.renameProfile(name) })
        } else if (action === "duplicate") {
            App.config.duplicateProfile()
        } else if (action === "clear") {
            confirm.ask("Empty \"" + App.config.profileName + "\"?",
                        "Everything this profile launches is emptied: the port, the game, the "
                        + "files and the multiplayer settings. The profile itself stays.",
                        "Empty it", true,
                        function () { App.config.clearProfile() })
        } else if (action === "delete") {
            confirm.ask("Delete \"" + App.config.profileName + "\"?",
                        "The profile and everything in it goes. The files it loaded are left alone.",
                        "Delete", true,
                        function () {
                            App.config.removeProfile()
                            page.closed()
                        })
        } else if (action === "saveZdl") {
            pick.open("Save this profile as a .zdl", App.zdlFilters, true,
                      function (directory) {
                          App.config.saveZdl(directory + "/" + App.config.profileName + ".zdl")
                      }, "zdl")
        }
    }
}
