import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Focus

// Leaving a session early takes a written reason, then a choice: a short,
// timed pause, or ending it for good.
Dialog {
    id: dialog
    signal failed(string message)

    readonly property bool enough: backend.isReasonEnough(reason.text)
    readonly property bool active: backend.session.status === "active"
    property int minutes: 10

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(520, parent.width - 48)
    modal: true
    title: qsTr("Leaving the session?")
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

    onOpened: { reason.text = ""; minutes = 10; reason.forceActiveFocus() }

    function act(result) {
        if (result) failed(result)
        else close()
    }

    ColumnLayout {
        width: parent.width
        spacing: 14

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Palette.muted
            text: qsTr("Write down why you're stopping. It's kept in your history.")
        }

        TextArea {
            id: reason
            Layout.fillWidth: true
            Layout.preferredHeight: 110
            wrapMode: TextEdit.Wrap
            placeholderText: qsTr("Reason (at least three words)")
        }

        ColumnLayout {
            visible: dialog.active
            spacing: 4
            Label { text: qsTr("Pause for"); color: Palette.muted }
            RowLayout {
                spacing: 8
                Repeater {
                    model: [5, 10, 15]
                    delegate: Chip {
                        required property int modelData
                        text: qsTr("%n min", "", modelData)
                        picked: dialog.minutes === modelData
                        onClicked: dialog.minutes = modelData
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 6
            spacing: 8
            Button {
                text: qsTr("Keep focusing")
                flat: true
                onClicked: dialog.close()
            }
            Item { Layout.fillWidth: true }
            Button {
                visible: dialog.active
                enabled: dialog.enough
                text: qsTr("Pause %n min", "", dialog.minutes)
                onClicked: dialog.act(backend.pause(reason.text, dialog.minutes))
            }
            Button {
                enabled: dialog.enough
                text: qsTr("End session")
                Material.background: dialog.enough ? "#93000a" : undefined
                onClicked: dialog.act(backend.endSession(reason.text))
            }
        }
    }
}
