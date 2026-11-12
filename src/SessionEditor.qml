import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Focus
import "Format.js" as Format

// Edits one planned session. Every change saves right away (Ctrl+Z undoes).
Item {
    id: root
    property var plan: null
    readonly property string planId: plan ? plan.id : ""
    readonly property bool oneTime: plan ? !!plan.date : false

    function focusName() { Qt.callLater(() => { nameField.forceActiveFocus(); nameField.selectAll() }) }

    Label {
        anchors.centerIn: parent
        visible: root.plan === null
        color: Palette.muted
        text: qsTr("Pick a session, or press Ctrl+N to plan a new one.")
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
        visible: root.plan !== null
        contentWidth: availableWidth

        ColumnLayout {
            width: scroll.availableWidth
            spacing: 22

            Item { Layout.preferredHeight: 6 }

            TextField {
                id: nameField
                Layout.fillWidth: true
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                text: root.plan ? root.plan.name : ""
                placeholderText: qsTr("What is this session for?")
                font.pixelSize: 22
                onEditingFinished: if (root.plan && text !== root.plan.name) backend.setField(root.planId, "name", text)
            }

            // When.
            Section {
                title: qsTr("When")
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    RowLayout {
                        spacing: 8
                        RadioButton {
                            text: qsTr("Every week")
                            checked: !root.oneTime
                            onClicked: if (root.oneTime) backend.setField(root.planId, "date", "")
                        }
                        RadioButton {
                            text: qsTr("One time")
                            checked: root.oneTime
                            onClicked: if (!root.oneTime) backend.setField(root.planId, "date", new Date().toLocaleDateString(Qt.locale("C"), "yyyy-MM-dd"))
                        }
                    }

                    Flow {
                        Layout.fillWidth: true
                        visible: !root.oneTime
                        spacing: 8
                        Repeater {
                            model: Format.weekDays()
                            delegate: Chip {
                                required property int modelData
                                text: Format.dayName(modelData, Locale.ShortFormat)
                                picked: !!root.plan && (root.plan.days || []).indexOf(modelData) >= 0
                                onClicked: backend.toggleDay(root.planId, modelData)
                            }
                        }
                    }

                    RowLayout {
                        spacing: 16
                        TextField {
                            id: dateField
                            visible: root.oneTime
                            Layout.preferredWidth: 160
                            placeholderText: qsTr("Date")
                            text: root.plan ? root.plan.date : ""
                            inputMask: "9999-99-99"
                            validator: RegularExpressionValidator { regularExpression: /^\d{4}-(0[1-9]|1[0-2])-(0[1-9]|[12]\d|3[01])$/ }
                            onEditingFinished: if (acceptableInput) backend.setField(root.planId, "date", text)
                        }
                        TextField {
                            Layout.preferredWidth: 140
                            placeholderText: qsTr("Starts at")
                            text: root.plan ? root.plan.time : ""
                            inputMask: "99:99"
                            validator: RegularExpressionValidator { regularExpression: /^([01]\d|2[0-3]):[0-5]\d$/ }
                            onEditingFinished: if (acceptableInput) backend.setField(root.planId, "time", text)
                        }
                        Label { text: qsTr("for"); color: Palette.muted }
                        SpinBox {
                            id: minutes
                            from: 5
                            to: 720
                            stepSize: 5
                            editable: true
                            value: root.plan ? root.plan.minutes : 60
                            textFromValue: (v) => qsTr("%n min", "", v)
                            valueFromText: (t) => parseInt(t) || 60
                            onValueModified: backend.setField(root.planId, "minutes", value)
                        }
                    }

                    Label {
                        readonly property var next: { root.plan; return root.plan ? backend.nextRun(root.planId) : null }
                        color: next ? theme.accent : Palette.muted
                        text: root.plan && root.plan.enabled === false ? qsTr("Switched off: it won't start on its own.")
                            : next ? qsTr("Next: %1").arg(Format.when(next))
                            : qsTr("Not scheduled: pick a day, a time, or a date ahead.")
                    }
                }
            }

            // Tasks.
            Section {
                title: qsTr("Tasks")
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Repeater {
                        model: root.plan ? root.plan.tasks : []
                        delegate: RowLayout {
                            required property string modelData
                            required property int index
                            Layout.fillWidth: true
                            Label { text: "•"; color: theme.accent }
                            Label { Layout.fillWidth: true; text: modelData; color: Palette.text; wrapMode: Text.WordWrap }
                            ToolButton {
                                text: "×"
                                onClicked: backend.removeTask(root.planId, index)
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Remove task")
                            }
                        }
                    }
                    TextField {
                        id: taskField
                        Layout.fillWidth: true
                        placeholderText: qsTr("Add a task and press Enter")
                        onAccepted: { backend.addTask(root.planId, text); text = "" }
                    }
                }
            }

            // Environment.
            Section {
                title: qsTr("Allowed apps")
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Palette.muted
                        text: qsTr("Everything else closes while the session runs. Focus itself, file pickers, and password prompts always stay.")
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        Repeater {
                            model: root.plan ? root.plan.apps : []
                            delegate: Rectangle {
                                required property var modelData
                                required property int index
                                height: 34
                                width: chipRow.implicitWidth + 20
                                radius: Math.min(17, theme.radius)
                                color: Palette.raised
                                border.color: Palette.line
                                RowLayout {
                                    id: chipRow
                                    anchors.centerIn: parent
                                    spacing: 6
                                    Label { text: modelData.name; color: Palette.text }
                                    Label {
                                        text: "×"
                                        color: Palette.muted
                                        font.pixelSize: 16
                                        MouseArea {
                                            anchors.fill: parent
                                            anchors.margins: -6
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: backend.removeApp(root.planId, index)
                                        }
                                    }
                                }
                            }
                        }
                        Button {
                            text: qsTr("Add apps…")
                            onClicked: picker.open()
                        }
                    }
                    Label {
                        visible: root.plan && root.plan.apps.length === 0
                        color: Palette.danger
                        text: qsTr("No apps picked yet: every other window would close.")
                    }
                }
            }

            Section {
                title: qsTr("During the session")
                ColumnLayout {
                    Switch {
                        text: qsTr("Do Not Disturb")
                        checked: root.plan ? root.plan.dnd !== false : true
                        onToggled: backend.setField(root.planId, "dnd", checked)
                    }
                    Switch {
                        text: qsTr("Start on schedule")
                        checked: root.plan ? root.plan.enabled !== false : true
                        onToggled: backend.setField(root.planId, "enabled", checked)
                    }
                }
            }

            RowLayout {
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                Layout.bottomMargin: 32
                spacing: 12
                Button {
                    text: qsTr("Start now")
                    highlighted: true
                    Material.foreground: theme.accentForeground
                    enabled: backend.serviceRunning
                    onClicked: win.startSelected()
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("Delete")
                    flat: true
                    Material.foreground: Palette.danger
                    onClicked: { backend.deletePlan(root.planId); win.selectedId = backend.plans.length ? backend.plans[0].id : "" }
                }
            }
        }
    }

    AppPicker { id: picker; planId: root.planId; plan: root.plan }

    component Section: ColumnLayout {
        id: section
        property string title
        default property alias content: body.data
        Layout.fillWidth: true
        Layout.leftMargin: 32
        Layout.rightMargin: 32
        spacing: 10
        Label {
            text: section.title
            color: Palette.muted
            font.pixelSize: 12
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 1
        }
        ColumnLayout { id: body; Layout.fillWidth: true }
    }
}
