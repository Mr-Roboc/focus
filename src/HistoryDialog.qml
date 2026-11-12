import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Focus
import "Format.js" as Format

// What happened in past sessions, newest first, with the reasons given for
// leaving early.
Dialog {
    id: dialog
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(620, parent.width - 48)
    height: Math.min(640, parent.height - 48)
    modal: true
    title: qsTr("History")
    standardButtons: Dialog.Close
    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.6) }
    background: Rectangle { color: Palette.panel; radius: theme.radius; border.color: Palette.line }
    header: Label {
        text: parent ? parent.title : ""
        padding: 24
        bottomPadding: 4
        font.pixelSize: 20
        font.weight: Font.DemiBold
        color: Palette.text
        wrapMode: Text.WordWrap
    }

    property bool showBlocked: false

    function describe(entry) {
        switch (entry.event) {
        case "started": return qsTr("Started")
        case "paused": return qsTr("Paused for %n min", "", entry.minutes || 0)
        case "resumed": return qsTr("Resumed")
        case "ended": return qsTr("Ended early")
        case "completed": return qsTr("Completed")
        case "blocked": return qsTr("Closed %1").arg(entry.title || entry["class"])
        }
        return entry.event
    }

    ColumnLayout {
        anchors.fill: parent
        CheckBox {
            text: qsTr("Show closed windows")
            checked: dialog.showBlocked
            onToggled: dialog.showBlocked = checked
        }
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 10
            model: backend.history.filter(e => dialog.showBlocked || e.event !== "blocked")
            ScrollBar.vertical: ScrollBar {}
            delegate: ColumnLayout {
                required property var modelData
                width: ListView.view.width
                spacing: 2
                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: dialog.describe(modelData)
                        color: modelData.event === "ended" ? Palette.danger
                             : modelData.event === "completed" ? theme.accent : Palette.text
                        font.weight: Font.Medium
                    }
                    Label { Layout.fillWidth: true; text: modelData.name || ""; color: Palette.muted; elide: Text.ElideRight }
                    Label { text: Format.when(Format.parseIso(modelData.at)); color: Palette.muted; font.pixelSize: 12 }
                }
                Label {
                    Layout.fillWidth: true
                    visible: !!modelData.reason
                    text: "“" + (modelData.reason || "") + "”"
                    wrapMode: Text.WordWrap
                    color: Palette.text
                    font.italic: true
                }
            }
            Label {
                anchors.centerIn: parent
                visible: list.count === 0
                color: Palette.muted
                text: qsTr("Nothing yet.")
            }
        }
    }
}
