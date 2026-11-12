import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Focus
import "Format.js" as Format

// The running session: time left, the tasks, and the way out.
Item {
    id: root
    signal leaveRequested()

    readonly property var session: backend.session
    readonly property bool paused: session.status === "paused"
    readonly property var start: Format.parseIso(session.start)
    readonly property var end: Format.parseIso(session.end)
    readonly property var pausedUntil: Format.parseIso(session.pausedUntil)
    property var now: new Date()

    readonly property real total: start && end ? (end - start) / 1000 : 1
    readonly property real remaining: end ? Math.max(0, (end - now) / 1000) : 0

    Timer { interval: 1000; running: root.visible; repeat: true; triggeredOnStart: true; onTriggered: root.now = new Date() }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(620, parent.width - 64)
        spacing: 18

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: root.paused ? qsTr("Paused") : qsTr("Focusing")
            color: root.paused ? Palette.muted : theme.accent
            font.pixelSize: 13
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 2
        }
        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: root.session.name || ""
            color: Palette.text
            font.pixelSize: 30
            font.weight: Font.DemiBold
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: Format.clock(root.remaining)
            color: root.paused ? Palette.muted : Palette.text
            font.pixelSize: 72
            font.weight: Font.Light
            font.features: { "tnum": 1 }
        }
        ProgressBar {
            Layout.fillWidth: true
            from: 0
            to: 1
            value: 1 - root.remaining / root.total
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            color: Palette.muted
            text: root.paused && root.pausedUntil
                  ? qsTr("Back to focus in %1").arg(Format.clock((root.pausedUntil - root.now) / 1000))
                  : root.end ? qsTr("Until %1").arg(Format.time(root.end)) : ""
        }

        // Tasks, ticked off as the session goes.
        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 8
            visible: (root.session.tasks || []).length > 0
            implicitHeight: tasks.implicitHeight + 24
            radius: theme.radius
            color: Palette.panel
            border.color: Palette.line
            ColumnLayout {
                id: tasks
                anchors.fill: parent
                anchors.margins: 12
                spacing: 0
                Repeater {
                    model: root.session.tasks || []
                    delegate: CheckBox {
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        text: modelData.text
                        checked: modelData.done
                        font.strikeout: modelData.done
                        onToggled: backend.setTaskDone(index, checked)
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            color: Palette.muted
            font.pixelSize: 12
            text: (root.session.apps || []).length
                  ? qsTr("Allowed: %1").arg(root.session.apps.map(a => a.name).join(", "))
                  : qsTr("Only Focus is allowed.")
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 8
            spacing: 12
            Button {
                visible: root.paused
                text: qsTr("Resume now")
                highlighted: true
                Material.foreground: theme.accentForeground
                onClicked: backend.resume()
            }
            Button {
                text: qsTr("Pause or end…")
                flat: true
                onClicked: root.leaveRequested()
            }
        }
    }
}
