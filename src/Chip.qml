import QtQuick
import QtQuick.Controls
import Focus

// A toggle that fills with the accent when picked (days, pause lengths).
Rectangle {
    id: chip
    property alias text: label.text
    property bool picked: false
    signal clicked()

    implicitWidth: Math.max(52, label.implicitWidth + 24)
    implicitHeight: 38
    radius: Math.min(19, theme.radius)
    color: picked ? theme.accent : mouse.containsMouse ? Palette.raised : "transparent"
    border.color: picked ? theme.accent : Palette.line

    Label {
        id: label
        anchors.centerIn: parent
        color: chip.picked ? theme.accentForeground : Palette.text
        font.weight: chip.picked ? Font.DemiBold : Font.Normal
    }
    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: chip.clicked()
    }
}
