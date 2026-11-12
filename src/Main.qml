import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Focus

ApplicationWindow {
    id: win
    width: 1040
    height: 720
    minimumWidth: 760
    minimumHeight: 520
    visible: true
    title: "Focus"
    color: Palette.window

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius
    Material.background: Palette.window

    property string selectedId: ""
    property bool helpVisible: false
    readonly property bool inSession: (backend.session.status || "idle") !== "idle"
    // Single-key shortcuts stay quiet while a text field has the keyboard.
    readonly property bool typing: activeFocusItem !== null && activeFocusItem.cursorPosition !== undefined
    readonly property var selectedPlan: {
        for (const plan of backend.plans)
            if (plan.id === selectedId) return plan
        return null
    }

    function newSession() {
        selectedId = backend.newPlan(qsTr("New session"))
        planner.focusName()
    }
    function startSelected() {
        if (selectedPlan) startDialog.open()
    }

    Component.onCompleted: if (backend.plans.length > 0) selectedId = backend.plans[0].id

    Shortcut { sequence: "?"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Q"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequence: "Space"; enabled: !win.typing; context: Qt.ApplicationShortcut
        onActivated: {
            if (!win.inSession) win.startSelected()
            else if (backend.session.status === "paused") error.show(backend.resume())
        }
    }
    Shortcut { sequence: "Escape"; enabled: win.inSession && !exitDialog.visible; context: Qt.ApplicationShortcut; onActivated: exitDialog.open() }
    Shortcut { sequence: "Ctrl+N"; enabled: !win.inSession; context: Qt.ApplicationShortcut; onActivated: win.newSession() }
    Shortcut { sequence: "H"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: history.visible ? history.close() : history.open() }
    Shortcut { sequence: "Delete"; enabled: !win.typing && !win.inSession && win.selectedPlan !== null; context: Qt.ApplicationShortcut
        onActivated: { backend.deletePlan(win.selectedId); win.selectedId = backend.plans.length ? backend.plans[0].id : "" }
    }
    Shortcut { sequences: [StandardKey.Undo]; enabled: !win.typing && !win.inSession; context: Qt.ApplicationShortcut; onActivated: backend.undo() }
    Shortcut { sequences: [StandardKey.Redo, "Ctrl+Shift+Z"]; enabled: !win.typing && !win.inSession; context: Qt.ApplicationShortcut; onActivated: backend.redo() }
    Shortcut { sequence: "Ctrl+Up"; enabled: !win.inSession; context: Qt.ApplicationShortcut; onActivated: planner.selectRelative(-1) }
    Shortcut { sequence: "Ctrl+Down"; enabled: !win.inSession; context: Qt.ApplicationShortcut; onActivated: planner.selectRelative(1) }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // The service is what starts and enforces sessions, so say plainly
            // when it's off instead of letting a schedule silently not happen.
            Rectangle {
                Layout.fillWidth: true
                visible: !backend.serviceRunning
                implicitHeight: banner.implicitHeight + 20
                color: "#3a1d1d"
                RowLayout {
                    id: banner
                    anchors.fill: parent
                    anchors.margins: 10
                    anchors.leftMargin: 20
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: "#ffb4ab"
                        text: qsTr("The focus service isn't running, so sessions won't start on schedule or close other apps.")
                    }
                    Button {
                        text: qsTr("Turn it on")
                        flat: true
                        onClicked: error.show(backend.enableService())
                    }
                }
            }

            PlannerView {
                id: planner
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: !win.inSession
            }

            ActiveSessionView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: win.inSession
                onLeaveRequested: exitDialog.open()
            }
        }

        // Errors from the service, shown at the bottom for a few seconds.
        Rectangle {
            id: error
            function show(message) {
                if (!message) return
                errorText.text = message
                visible = true
                errorTimer.restart()
            }
            visible: false
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 24
            width: Math.min(parent.width - 48, errorText.implicitWidth + 40)
            height: errorText.implicitHeight + 20
            radius: Math.min(4, theme.radius)
            color: "#3a1d1d"
            border.color: "#ffb4ab"
            Label { id: errorText; anchors.centerIn: parent; width: parent.width - 40; wrapMode: Text.WordWrap; color: "#ffb4ab" }
            Timer { id: errorTimer; interval: 5000; onTriggered: error.visible = false }
        }
    }

    Dialog {
        id: startDialog
        anchors.centerIn: parent
        modal: true
        width: Math.min(460, win.width - 48)
        title: win.selectedPlan ? qsTr("Start “%1” now?").arg(win.selectedPlan.name) : ""
        standardButtons: Dialog.Cancel | Dialog.Ok
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
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: qsTr("Windows of apps that aren't part of this session will close. Save your work in them first.")
        }
        onOpened: standardButton(Dialog.Ok).text = qsTr("Start")
        onAccepted: error.show(backend.startPlan(win.selectedId))
    }

    ExitDialog { id: exitDialog; onFailed: (message) => error.show(message) }
    HistoryDialog { id: history }
    HelpOverlay { visible: win.helpVisible; onDismissed: win.helpVisible = false }
}
