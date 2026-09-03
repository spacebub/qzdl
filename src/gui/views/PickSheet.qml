import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Qt.labs.folderlistmodel
import Zdl
import Zdl.Components

/*
Choosing a path happens in the window like everything else. The row along the
top is where you are: click a piece of it to go there, or click the path
itself to type one.
*/
Item {
    id: sheet

    property string title: "Select a file"
    property var filters: [ "*" ]
    property bool directories: false
    property var chosen: null
    property bool editing: false

    // Which of the remembered directories this pick starts in and writes back
    // to, so asking for a WAD twice starts where the last WAD came from.
    property string remember: "general"

    /*
    Whether more than one file can come back at once. Adding a dozen WADs one
    sheet at a time is a dozen trips through the same directory, so in this
    mode a click marks a file instead of choosing it and the whole lot is
    handed back together.
    */
    property bool multiple: false
    property var marked: []

    readonly property string path: folder.folder.toString().replace("file://", "")
    readonly property var segments: sheet.path.split("/").filter(function (part) { return part !== "" })

    visible: false

    function openMany(title, filters, onChosen, remember) {
        sheet.multiple = true
        sheet.open(title, filters, false, onChosen, remember)
    }

    function open(title, filters, directories, onChosen, remember) {
        sheet.title = title
        sheet.filters = filters === undefined ? [ "*" ] : filters
        sheet.directories = directories === true
        sheet.chosen = onChosen
        sheet.editing = false
        sheet.remember = remember === undefined ? "general" : remember
        sheet.marked = []

        sheet.go(App.startDirectory(sheet.remember))
        sheet.visible = true
    }

    function dismiss() {
        sheet.visible = false
        sheet.chosen = null
        sheet.editing = false
        sheet.multiple = false
        sheet.marked = []
    }

    // Marking is by path rather than by row, so it survives walking out of a
    // directory and back into it.
    function mark(path) {
        const at = sheet.marked.indexOf(path)
        const next = sheet.marked.slice()

        if (at === -1) {
            next.push(path)
        } else {
            next.splice(at, 1)
        }

        sheet.marked = next
    }

    function chooseMarked() {
        const callback = sheet.chosen
        const paths = sheet.marked

        App.rememberDirectory(sheet.remember, sheet.path)
        sheet.dismiss()

        if (callback && paths.length > 0) {
            callback(paths)
        }
    }

    function go(path) {
        folder.folder = "file://" + path
    }

    function upTo(index) {
        sheet.go("/" + sheet.segments.slice(0, index + 1).join("/"))
    }

    function choose(path) {
        const callback = sheet.chosen

        App.rememberDirectory(sheet.remember, sheet.path)
        sheet.dismiss()

        if (callback) {
            callback(path)
        }
    }

    // A typed path walks into a directory, or picks whatever it points at.
    function accept(input) {
        const target = input.trim()

        if (target === "") {
            sheet.editing = false

            return
        }

        if (App.isDirectory(target)) {
            sheet.go(target)
            sheet.editing = false

            return
        }

        if (!sheet.directories && App.isFile(target)) {
            sheet.choose(target)

            return
        }

        App.notify.warning(target + " is not there.")
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.scrim

        /*
        A sheet covers the window, so nothing underneath it should react to
        anything. Without hoverEnabled the hover still goes through and the
        page below lights up under a pointer that is over a sheet, and a
        button this does not claim is a click the page below still gets.
        */
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.AllButtons
            onClicked: sheet.dismiss()
        }
    }

    Surface {
        anchors.centerIn: parent
        width: Math.min(700, parent.width - 48)
        height: Math.min(540, parent.height - 48)

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Text {
                    Layout.fillWidth: true
                    text: sheet.title
                    color: Theme.text
                    font.pixelSize: Theme.fontLarge
                    font.weight: Theme.headingWeight
                    textFormat: Text.PlainText
                }

                GlyphButton {
                    glyph: "cross"
                    size: 26
                    onClicked: sheet.dismiss()
                }
            }

            // Where we are: walkable, or typeable in the same place.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.control
                radius: Theme.radiusSmall
                color: Theme.field
                border.width: 1
                border.color: sheet.editing ? Theme.accent : Theme.borderStrong

                Behavior on border.color { ColorAnimation { duration: 120 } }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 4
                    anchors.rightMargin: 4
                    spacing: 4

                    GlyphButton {
                        glyph: "up"
                        size: 30
                        hint: "Go up one directory"
                        visible: !sheet.editing
                        onClicked: folder.folder = folder.parentFolder
                    }

                    Flickable {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: !sheet.editing
                        contentWidth: crumbs.implicitWidth
                        clip: true
                        flickableDirection: Flickable.HorizontalFlick
                        boundsBehavior: Flickable.StopAtBounds

                        onContentWidthChanged: contentX = Math.max(0, contentWidth - width)

                        Row {
                            id: crumbs
                            height: parent.height
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 1

                            Crumb {
                                label: "/"
                                onActivated: sheet.go("/")
                            }

                            Repeater {
                                model: sheet.segments

                                delegate: Row {
                                    id: crumb

                                    required property string modelData
                                    required property int index

                                    height: crumbs.height
                                    spacing: 1

                                    Text {
                                        text: "/"
                                        color: Theme.faint
                                        font.pixelSize: Theme.fontSmall
                                        anchors.verticalCenter: parent.verticalCenter
                                        textFormat: Text.PlainText
                                    }

                                    Crumb {
                                        label: crumb.modelData
                                        last: crumb.index === sheet.segments.length - 1
                                        onActivated: sheet.upTo(crumb.index)
                                    }
                                }
                            }
                        }
                    }

                    TextField {
                        id: typed
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: sheet.editing
                        verticalAlignment: TextInput.AlignVCenter
                        leftPadding: 8
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                        font.family: Theme.mono
                        selectionColor: Theme.accent
                        selectedTextColor: Theme.accentText
                        background: null
                        onAccepted: sheet.accept(text)
                        Keys.onEscapePressed: sheet.editing = false
                    }

                    GlyphButton {
                        glyph: sheet.editing ? "cross" : "edit"
                        size: 30
                        hint: sheet.editing ? "Back to browsing" : "Type a path"
                        onClicked: {
                            sheet.editing = !sheet.editing

                            if (sheet.editing) {
                                typed.text = sheet.path
                                typed.forceActiveFocus()
                                typed.selectAll()
                            }
                        }
                    }
                }
            }

            Surface {
                Layout.fillWidth: true
                Layout.fillHeight: true
                inset: true

                ListView {
                    id: entries
                    anchors.fill: parent
                    anchors.margins: 6
                    clip: true
                    spacing: 1

                    model: FolderListModel {
                        id: folder
                        showDirsFirst: true
                        showDotAndDotDot: false
                        showHidden: true
                        showFiles: !sheet.directories
                        nameFilters: sheet.filters
                    }

                    ScrollBar.vertical: ScrollBar {
                        id: entryBar
                        visible: entryBar.size < 1
                        width: 8

                        contentItem: Rectangle {
                            radius: 4
                            color: Theme.borderStrong
                            opacity: entryBar.pressed ? 0.9 : 0.45
                        }
                    }

                    delegate: Item {
                        id: entry

                        required property string fileName
                        required property string filePath
                        required property bool fileIsDir

                        width: entries.width
                        height: 34

                        readonly property bool marked: sheet.multiple
                            && sheet.marked.indexOf(entry.filePath) !== -1

                        Wash {
                            anchors.fill: parent
                            anchors.rightMargin: 6
                            rounding: Theme.radiusSmall - 2
                            selected: entry.marked
                            hovered: hover.hovered
                        }

                        Row {
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                            anchors.right: parent.right
                            anchors.rightMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 9

                            FileGlyph {
                                folder: entry.fileIsDir
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Text {
                                text: entry.fileName
                                color: entry.marked ? Theme.accent
                                     : entry.fileName.startsWith(".") ? Theme.muted
                                     : Theme.text
                                font.pixelSize: Theme.fontBody
                                font.family: Theme.mono
                                font.weight: entry.marked ? Font.DemiBold : Font.Normal
                                anchors.verticalCenter: parent.verticalCenter
                                textFormat: Text.PlainText
                            }
                        }

                        HoverHandler { id: hover }

                        Glyph {
                            visible: entry.marked
                            name: "check"
                            tone: Theme.accent
                            anchors.right: parent.right
                            anchors.rightMargin: 16
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        TapHandler {
                            onTapped: {
                                if (entry.fileIsDir) {
                                    sheet.go(entry.filePath)
                                } else if (sheet.multiple) {
                                    sheet.mark(entry.filePath)
                                } else {
                                    sheet.choose(entry.filePath)
                                }
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Text {
                    Layout.fillWidth: true
                    visible: folder.count === 0
                    text: sheet.directories
                        ? "No directories here"
                        : "Nothing here matches " + sheet.filters.join(", ")
                    color: Theme.faint
                    font.pixelSize: Theme.fontSmall
                    textFormat: Text.PlainText
                }

                Item { Layout.fillWidth: !(folder.count === 0) }

                AppButton {
                    text: "Use this directory"
                    variant: "primary"
                    compact: true
                    visible: sheet.directories
                    onClicked: sheet.choose(sheet.path)
                }

                AppButton {
                    text: sheet.marked.length === 1 ? "Add 1 file"
                        : "Add " + sheet.marked.length + " files"
                    variant: "primary"
                    compact: true
                    visible: sheet.multiple
                    enabled: sheet.marked.length > 0
                    onClicked: sheet.chooseMarked()
                }
            }
        }
    }

    Keys.onEscapePressed: sheet.dismiss()
}
