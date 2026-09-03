import QtQuick
import Zdl

// A path, typed or browsed for. Every place that asks for one uses this, so
// they all behave the same.
Field {
    id: control

    property var pick: null
    property string browseTitle: "Select a file"
    property var filters: [ "*" ]
    property bool directories: false
    property string remember: "general"

    icon: "folder"
    iconHint: control.directories ? "Browse for a directory" : "Browse"
    mono: true

    onActionTriggered: {
        if (!control.pick) {
            return
        }

        control.pick.open(control.browseTitle, control.filters, control.directories,
                          function (path) { control.text = path }, control.remember)
    }
}
