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

    /** There is nothing to run with, and ports are set up on the other page. */
    signal settingsRequested()

    /** There is nothing to play, and games are added on the shelf. */
    signal gamesRequested()

    // "(Default)" is index 0 in both of these, which is also what 0 means in
    // the config, so a picker's index is the stored value and back again.
    readonly property var skills: [ "V. Easy", "Easy", "Medium", "Hard", "V. Hard" ]
    readonly property var monsters: [ "No monsters", "Fast", "Respawn", "Fast & respawn" ]

    // The three sides of a netgame, the three game types and the three net
    // modes, each in the order the config numbers them, so a key's place in
    // the list is the value stored for it and back again.
    readonly property var roles: [ "alone", "host", "join" ]
    readonly property var types: [ "coop", "dm", "altdm" ]
    readonly property var modes: [ "any", "p2p", "cs" ]

    /** Whether the connection settings under the multiplayer panel are shown. */
    property bool tuning: false

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
                    file: App.config.iwadFile(App.config.iwad)
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
                                    file: entry.modelData.iwadFile
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
                visible: App.config.captureOutput && !App.config.dosPort
                    && !App.config.autoClose
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

                            // Ports are set up on the Settings page, so an empty
                            // list is a signpost rather than a dropdown of nothing.
                            Column {
                                width: parent.width
                                spacing: 6
                                visible: App.config.ports.count === 0

                                SectionLabel { text: "SOURCE PORT" }

                                AppButton {
                                    width: parent.width
                                    text: "Add a source port…"
                                    glyph: "plus"
                                    hint: "Ports are set up on the Settings page"
                                    onClicked: page.settingsRequested()
                                }
                            }

                            Picker {
                                width: parent.width
                                visible: App.config.ports.count > 0
                                label: "Source port"
                                placeholder: "None selected"
                                options: App.config.ports.names

                                // Which of them are DOS programs, said here as
                                // well as in the list they are set up in.
                                badges: App.config.ports.entries.map(function (entry) {
                                    return entry.dosbox ? "DOS" : ""
                                })

                                current: App.config.ports.indexOfName(App.config.port)
                                clearable: true
                                hint: "What actually runs. Add ports on the Settings page."
                                onSelected: function (index) {
                                    App.config.port = index < 0 ? "" : App.config.ports.names[index]
                                }
                            }

                            // Games are kept on the shelf, so an empty list is a
                            // signpost rather than a dropdown of nothing. Adding
                            // one here would work and would teach nowhere to go
                            // the second time.
                            Column {
                                width: parent.width
                                spacing: 6
                                visible: App.config.iwads.count === 0

                                SectionLabel { text: "GAME" }

                                AppButton {
                                    width: parent.width
                                    text: "Add a game…"
                                    glyph: "plus"
                                    hint: "Games are added on the library's games shelf"
                                    onClicked: page.gamesRequested()
                                }
                            }

                            Picker {
                                width: parent.width
                                visible: App.config.iwads.count > 0
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

                            // A DOS port prints into DOSBox's own window, where
                            // nothing here can reach it, and closing on launch
                            // takes the log away before anything reaches it.
                            Toggle {
                                width: parent.width
                                visible: !App.config.dosPort
                                enabled: !App.config.autoClose
                                text: "Record the game's output"
                                checked: App.config.captureOutput && !App.config.autoClose
                                hint: App.config.autoClose
                                    ? "Nothing to record while ZDL closes on launch: the log goes "
                                      + "with the window. Turn that off in Settings."
                                    : "Takes what the source port prints into a log along the "
                                      + "bottom of the window. Off, nothing is piped at all."
                                onToggled: function (value) { App.config.captureOutput = value }
                            }

                            // Only worth a line when profiles have configs of their
                            // own; with the setting off there is nothing to
                            // bypass, and a DOS port takes no -config at all.
                            Toggle {
                                width: parent.width
                                visible: App.config.profileConfigs && !App.config.dosPort
                                text: "Use the port's own settings"
                                checked: App.config.sharedConfig
                                hint: "Launch on the settings the source port keeps for itself, "
                                    + "shared with everything else that uses them"
                                onToggled: function (value) { App.config.sharedConfig = value }
                            }

                            Fact {
                                visible: App.config.profileConfigs && !App.config.sharedConfig
                                    && !App.config.dosPort
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
                                text: App.config.commandLine !== "" ? App.config.commandLine
                                    : App.config.dosPort && App.config.dosbox === ""
                                      && App.config.systemDosbox === ""
                                    ? "A DOS source port, and no DOSBox to run it in. "
                                    + "Set one in Settings."
                                    : "Nothing to run yet."
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

                /*
                Multiplayer. Which side the profile is on is the one decision
                the rest of the panel follows from, so the header carries it
                and nothing is shown for the side it is not on. A DOS netgame
                is IPX and a setup program of its own, none of which is any of
                this, so the panel is not offered on a DOS port at all.
                */
                Surface {
                    visible: !App.config.dosPort
                    Layout.fillWidth: true
                    Layout.preferredHeight: net.implicitHeight + 32

                    Behavior on Layout.preferredHeight {
                        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
                    }

                    Column {
                        id: net

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 16
                        spacing: 16

                        Item {
                            id: netBar

                            width: parent.width
                            height: Theme.control

                            Row {
                                id: heading

                                anchors.left: parent.left
                                anchors.right: role.left
                                anchors.rightMargin: 14
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 10

                                Glyph {
                                    name: App.config.multiplayerOpen ? "up" : "down"
                                    weight: 1
                                    tone: fold.containsMouse ? Theme.text : Theme.faint
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
                                    visible: App.config.netRole !== 0
                                    text: App.config.netRole === 2 ? "Joining"
                                        : App.config.gameType === 1 ? "Co-op"
                                        : App.config.gameType === 2 ? "Deathmatch"
                                        : "Alt deathmatch"
                                    tone: page.netBroken ? Theme.warning : Theme.accent
                                    wash: page.netBroken ? Theme.warningSoft : Theme.accentSoft
                                    anchors.verticalCenter: parent.verticalCenter
                                }

                                // Its width comes off what the row has spent
                                // already, which is the items before it alone
                                // and so does not feed back into itself.
                                Text {
                                    width: Math.max(0, parent.width - x)
                                    text: page.netSummary()
                                    color: page.netBroken ? Theme.warning : Theme.faint
                                    font.pixelSize: Theme.fontSmall
                                    elide: Text.ElideRight
                                    textFormat: Text.PlainText
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            // Only the words fold the panel away. The control
                            // beside them is pressed for its own sake, and one
                            // area over the pair would swallow it.
                            MouseArea {
                                id: fold

                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: heading.width
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: App.config.multiplayerOpen = !App.config.multiplayerOpen
                            }

                            GlyphButton {
                                id: reset

                                glyph: "refresh"
                                size: Theme.control
                                outlined: true
                                enabled: App.config.multiplayerSet
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                hint: enabled
                                    ? "Put every multiplayer setting back to its default"
                                    : "Nothing here has been set"
                                onClicked: page.confirm.ask(
                                    "Reset the multiplayer settings?",
                                    "The side this profile is on, the game it opens and every "
                                    + "address, limit and flag under it go back to their "
                                    + "defaults. The rest of the profile is untouched.",
                                    "Reset", true,
                                    function () { App.config.clearMultiplayer() })
                            }

                            Segmented {
                                id: role

                                anchors.right: reset.left
                                anchors.rightMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                options: [ { key: "alone", label: "Off" },
                                           { key: "host", label: "Host" },
                                           { key: "join", label: "Join" } ]
                                current: page.roles[App.config.netRole]
                                onSelected: function (key) {
                                    App.config.netRole = page.roles.indexOf(key)

                                    // Picking a side is asking to set it up.
                                    if (App.config.netRole !== 0) {
                                        App.config.multiplayerOpen = true
                                    }
                                }
                            }
                        }

                        Column {
                            width: parent.width
                            visible: App.config.multiplayerOpen
                            spacing: 16

                            Rectangle {
                                width: parent.width
                                height: 1
                                color: Theme.border
                            }

                            Text {
                                width: parent.width
                                visible: App.config.netRole === 0
                                text: "This profile starts a game for one. Host opens a game "
                                    + "other machines can connect to; Join connects to one "
                                    + "somebody else is running."
                                color: Theme.faint
                                font.pixelSize: Theme.fontSmall
                                wrapMode: Text.WordWrap
                                textFormat: Text.PlainText
                            }

                            /* Hosting: the game being opened, and the room in it. */

                            Flow {
                                width: parent.width
                                visible: App.config.netRole === 1
                                spacing: 20

                                Column {
                                    spacing: 6

                                    SectionLabel { text: "GAME TYPE" }

                                    Segmented {
                                        options: [ { key: "coop", label: "Co-op" },
                                                   { key: "dm", label: "Deathmatch" },
                                                   { key: "altdm", label: "Alt deathmatch" } ]
                                        current: page.types[Math.max(0, App.config.gameType - 1)]
                                        onSelected: function (key) {
                                            App.config.gameType = page.types.indexOf(key) + 1
                                        }
                                    }
                                }

                                Stepper {
                                    width: 170
                                    label: "Players"
                                    from: 1
                                    to: 8
                                    value: App.config.players
                                    hint: "How many the game is opened for, this machine included"
                                    onStepped: function (value) { App.config.players = value }
                                }

                                Field {
                                    width: 160
                                    label: "Listen on port"
                                    placeholder: "Default"
                                    text: App.config.netPort
                                    mono: true
                                    onTextChanged: App.config.netPort = text
                                }
                            }

                            /* Joining: an address, and nothing else that matters. */

                            Column {
                                width: parent.width
                                visible: App.config.netRole === 2
                                spacing: 10

                                Row {
                                    width: parent.width
                                    spacing: 12

                                    Field {
                                        width: parent.width - 172
                                        label: "Address of the game"
                                        placeholder: "A host name or an address"
                                        text: App.config.host
                                        mono: true
                                        onTextChanged: App.config.host = text
                                    }

                                    Field {
                                        width: 160
                                        label: "Port"
                                        placeholder: "Default"
                                        text: App.config.netPort
                                        mono: true
                                        hint: "Replaces any port typed into the address"
                                        onTextChanged: App.config.netPort = text
                                    }
                                }

                                Text {
                                    width: parent.width
                                    visible: App.config.host === ""
                                    text: "Without an address there is nothing to join, and the "
                                        + "profile launches a single player game."
                                    color: Theme.warning
                                    font.pixelSize: Theme.fontSmall
                                    wrapMode: Text.WordWrap
                                    textFormat: Text.PlainText
                                }

                                Text {
                                    width: parent.width
                                    visible: App.config.host !== ""
                                    text: "How the game is played is the host's to decide, so "
                                        + "there is nothing else to set on this side."
                                    color: Theme.faint
                                    font.pixelSize: Theme.fontSmall
                                    wrapMode: Text.WordWrap
                                    textFormat: Text.PlainText
                                }
                            }

                            /* The rules, which only whoever opens the game sets. */

                            Column {
                                width: parent.width
                                visible: App.config.netRole === 1
                                spacing: 12

                                Rectangle {
                                    width: parent.width
                                    height: 1
                                    color: Theme.border
                                }

                                SectionLabel { text: "RULES OF THE GAME" }

                                Grid {
                                    width: parent.width
                                    columns: Math.max(1, Math.floor(width / 230))
                                    columnSpacing: 12
                                    rowSpacing: 12

                                    readonly property real cell:
                                        (width - (columns - 1) * 12) / columns

                                    Field {
                                        width: parent.cell
                                        label: "Frag limit"
                                        placeholder: "None"
                                        text: App.config.fragLimit
                                        mono: true
                                        onTextChanged: App.config.fragLimit = text
                                    }

                                    Field {
                                        width: parent.cell
                                        label: "Time limit"
                                        placeholder: "None"
                                        text: App.config.timeLimit
                                        mono: true
                                        hint: "In minutes"
                                        onTextChanged: App.config.timeLimit = text
                                    }

                                    Field {
                                        width: parent.cell
                                        label: "dmflags"
                                        placeholder: "None"
                                        text: App.config.dmflags
                                        mono: true
                                        hint: "The port's own flag word"
                                        onTextChanged: App.config.dmflags = text
                                    }

                                    Field {
                                        width: parent.cell
                                        label: "dmflags2"
                                        placeholder: "None"
                                        text: App.config.dmflags2
                                        mono: true
                                        hint: "The second, where a port has one"
                                        onTextChanged: App.config.dmflags2 = text
                                    }
                                }

                                PathField {
                                    width: parent.width
                                    label: "Start from a save"
                                    placeholder: "None — the game starts at its first map"
                                    text: App.config.savegame
                                    pick: page.pick
                                    filters: App.saveFilters
                                    remember: "save"
                                    browseTitle: "Select a save game"
                                    hint: "Everyone joining drops into the host's saved game"
                                    onTextChanged: App.config.savegame = text
                                }
                            }

                            // How this machine talks: the same either side,
                            // and almost never touched.

                            Column {
                                width: parent.width
                                visible: App.config.netRole !== 0
                                spacing: 12

                                Rectangle {
                                    width: parent.width
                                    height: 1
                                    color: Theme.border
                                }

                                Item {
                                    width: parent.width
                                    height: 18

                                    Row {
                                        id: tuningHead

                                        anchors.left: parent.left
                                        anchors.verticalCenter: parent.verticalCenter
                                        spacing: 8

                                        Glyph {
                                            name: page.tuning ? "up" : "down"
                                            weight: 0.85
                                            tone: more.containsMouse ? Theme.text : Theme.faint
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        SectionLabel {
                                            text: "CONNECTION"
                                            color: more.containsMouse ? Theme.text : Theme.faint
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        SectionLabel {
                                            visible: !page.tuning
                                            text: page.tuningSummary()
                                            font.letterSpacing: 0
                                            font.weight: Font.Normal
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                    }

                                    MouseArea {
                                        id: more

                                        anchors.left: parent.left
                                        anchors.top: parent.top
                                        anchors.bottom: parent.bottom
                                        width: tuningHead.width
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: page.tuning = !page.tuning
                                    }
                                }

                                Flow {
                                    width: parent.width
                                    visible: page.tuning
                                    spacing: 20

                                    Column {
                                        spacing: 6

                                        SectionLabel { text: "NET MODE" }

                                        Segmented {
                                            options: [ { key: "any", label: "The port's own" },
                                                       { key: "p2p", label: "Peer to peer" },
                                                       { key: "cs", label: "Client/server" } ]
                                            current: page.modes[App.config.netmode + 1]
                                            onSelected: function (key) {
                                                App.config.netmode = page.modes.indexOf(key) - 1
                                            }
                                        }
                                    }

                                    Stepper {
                                        width: 170
                                        label: "Duplicate tics"
                                        from: 1
                                        to: 9
                                        value: App.config.dup
                                        clearable: true
                                        placeholder: "Off"
                                        hint: "Sends each tic more than once, which trades "
                                            + "bandwidth for a connection that drops packets"
                                        onStepped: function (value) { App.config.dup = value }
                                    }

                                    Column {
                                        spacing: 6

                                        SectionLabel { text: "EXTRA TIC" }

                                        Segmented {
                                            options: [ { key: "no", label: "Off" },
                                                       { key: "yes", label: "On" } ]
                                            current: App.config.extratic === 1 ? "yes" : "no"
                                            onSelected: function (key) {
                                                App.config.extratic = key === "yes" ? 1 : 0
                                            }
                                        }
                                    }
                                }
                            }
                        }
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

    /** Set to join, but with nowhere to join, which launches a game for one. */
    readonly property bool netBroken: App.config.netRole === 2 && App.config.host === ""

    // The rest of what the panel is set to, beside the pill that names it, so
    // that it reads shut as well as open.
    function netSummary() {
        if (App.config.netRole === 0) {
            return "Off — this profile launches a single player game"
        }

        if (App.config.netRole === 1) {
            return "for " + App.config.players + " players"
                + (App.config.netPort === "" ? "" : ", on port " + App.config.netPort)
        }

        if (page.netBroken) {
            return "no address yet"
        }

        return App.config.host
            + (App.config.netPort === "" ? "" : ":" + App.config.netPort)
    }

    /** The same again for the connection settings folded under the panel. */
    function tuningSummary() {
        const said = []

        if (App.config.netmode !== -1) {
            said.push(App.config.netmode === 0 ? "peer to peer" : "client/server")
        }

        if (App.config.dup > 0) {
            said.push(App.config.dup + "× tics")
        }

        if (App.config.extratic === 1) {
            said.push("extra tic")
        }

        return said.length === 0 ? "left to the port" : said.join(" · ")
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
            page.pick.open("Save this profile as a .zdl", App.zdlFilters, true,
                           function (directory) {
                               App.config.saveZdl(directory + "/" + App.config.zdlFileName())
                           }, "zdl")
        }
    }
}
