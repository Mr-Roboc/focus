import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Focus
import "Format.js" as Format

// Planning: the sessions on the left, the selected one's editor on the right.
Item {
    id: root

    function focusName() { editor.focusName() }
    function selectRelative(step) {
        const plans = backend.plans
        if (plans.length === 0) return
        let index = plans.findIndex(p => p.id === win.selectedId)
        index = Math.max(0, Math.min(plans.length - 1, index + step))
        win.selectedId = plans[index].id
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 300
            Layout.fillHeight: true
            color: Palette.panel

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Focus sessions")
                        font.pixelSize: 20
                        font.weight: Font.DemiBold
                        color: Palette.text
                    }
                    RoundButton {
                        text: "+"
                        flat: true
                        font.pixelSize: 20
                        onClicked: win.newSession()
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("New session (Ctrl+N)")
                    }
                }

                ListView {
                    id: list
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 6
                    model: backend.plans

                    delegate: Rectangle {
                        id: row
                        required property var modelData
                        readonly property bool selected: modelData.id === win.selectedId
                        width: ListView.view.width
                        height: rowColumn.implicitHeight + 20
                        radius: Math.min(4, theme.radius)
                        color: selected ? Palette.raised : rowMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.03) : "transparent"

                        Rectangle {
                            visible: row.selected
                            width: 3
                            height: parent.height - 12
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            color: theme.accent
                            radius: 1
                        }

                        ColumnLayout {
                            id: rowColumn
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 14
                            anchors.rightMargin: 10
                            spacing: 2
                            Label {
                                Layout.fillWidth: true
                                text: row.modelData.name || qsTr("Untitled")
                                elide: Text.ElideRight
                                color: row.modelData.enabled === false ? Palette.muted : Palette.text
                                font.weight: Font.Medium
                            }
                            Label {
                                Layout.fillWidth: true
                                text: Format.schedule(row.modelData)
                                elide: Text.ElideRight
                                color: Palette.muted
                                font.pixelSize: 12
                            }
                            Label {
                                Layout.fillWidth: true
                                readonly property var next: { backend.plans; clock.tick; return backend.nextRun(row.modelData.id) }
                                visible: row.modelData.enabled !== false
                                text: next ? qsTr("Next: %1").arg(Format.when(next)) : qsTr("Not scheduled")
                                elide: Text.ElideRight
                                color: next ? theme.accent : Palette.muted
                                font.pixelSize: 12
                            }
                        }

                        MouseArea {
                            id: rowMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: win.selectedId = row.modelData.id
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        width: parent.width - 20
                        visible: list.count === 0
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        color: Palette.muted
                        text: qsTr("No sessions yet.\nPress Ctrl+N to plan one.")
                    }
                }

                Button {
                    Layout.fillWidth: true
                    flat: true
                    text: qsTr("History")
                    onClicked: history.open()
                }
            }
        }

        Rectangle { Layout.fillHeight: true; width: 1; color: Palette.line }

        SessionEditor {
            id: editor
            Layout.fillWidth: true
            Layout.fillHeight: true
            plan: win.selectedPlan
        }
    }

    // Keeps "Next: Today, 15:00" honest as the clock moves.
    QtObject {
        id: clock
        property int tick: 0
        property Timer timer: Timer { interval: 30000; running: root.visible; repeat: true; onTriggered: clock.tick++ }
    }
}
