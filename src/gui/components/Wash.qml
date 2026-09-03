import QtQuick
import Zdl

/*
What is painted behind a row, a tab or an entry: a tint for the one that is
picked and a lighter wash for the one under the pointer.

They are two rectangles fading on their own opacity rather than one that
changes colour, because the two washes have very different alpha and a
colour animation between them runs the alpha up while the colour is still
near the one it started from. Half way across a click that lands on a row
is a half-opaque near-white over the surface, which is lighter than either
end and reads as a flash before the tint settles.
*/
Item {
    id: wash

    property bool selected: false
    property bool hovered: false
    property color tint: Theme.accentSoft
    property color pointer: Theme.hover
    property int rounding: Theme.radiusSmall

    Rectangle {
        anchors.fill: parent
        radius: wash.rounding
        color: wash.tint
        opacity: wash.selected ? 1 : 0

        Behavior on opacity { NumberAnimation { duration: 110 } }
    }

    Rectangle {
        anchors.fill: parent
        radius: wash.rounding
        color: wash.pointer
        opacity: wash.hovered && !wash.selected ? 1 : 0

        Behavior on opacity { NumberAnimation { duration: 110 } }
    }
}
