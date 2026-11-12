import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Focus

// Every shortcut, shown with ?.
Rectangle {
    id: overlay
    signal dismissed()
    anchors.fill: parent
    color: Qt.rgba(0, 0, 0, 0.72)

    MouseArea { anchors.fill: parent; onClicked: overlay.dismissed() }

    readonly property var shortcuts: [
        ["Space", qsTr("Start the selected session, or resume a pause")],
        ["Esc", qsTr("Pause or end the running session")],
        ["Ctrl+N", qsTr("New session")],
        ["Ctrl+↑ / Ctrl+↓", qsTr("Previous or next session")],
        ["Delete", qsTr("Delete the selected session")],
        ["Ctrl+Z", qsTr("Undo")],
        ["Ctrl+Shift+Z", qsTr("Redo")],
        ["H", qsTr("History")],
        ["?", qsTr("Show or hide shortcuts")],
        ["Q", qsTr("Quit (a running session keeps going)")],
    ]

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(520, parent.width - 48)
        height: grid.implicitHeight + 48
        radius: theme.radius
        color: Palette.panel
        border.color: Palette.line
        GridLayout {
            id: grid
            anchors.fill: parent
            anchors.margins: 24
            columns: 2
            columnSpacing: 24
            rowSpacing: 10
            Repeater {
                model: overlay.shortcuts.length * 2
                delegate: Label {
                    required property int index
                    readonly property var row: overlay.shortcuts[Math.floor(index / 2)]
                    text: index % 2 === 0 ? row[0] : row[1]
                    color: index % 2 === 0 ? theme.accent : Palette.text
                    font.family: index % 2 === 0 ? "monospace" : Qt.application.font.family
                    Layout.fillWidth: index % 2 === 1
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
