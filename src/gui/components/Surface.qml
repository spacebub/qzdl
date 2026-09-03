import QtQuick
import Zdl

// The one card in the interface: a flat panel with a hairline border.
Rectangle {
    property bool inset: false
    property bool hoverable: false
    property bool hovered: false

    color: inset ? Theme.sunken : Theme.surface
    radius: Theme.radius
    border.width: 1
    border.color: hovered ? Theme.borderStrong : Theme.border

    Behavior on border.color { ColorAnimation { duration: 120 } }
    Behavior on color { ColorAnimation { duration: 120 } }
}
