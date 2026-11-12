import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Focus

// Picks the apps a session allows, from the launcher's entries or from the
// windows open right now.
Dialog {
    id: dialog
    property string planId
    property var plan

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(560, parent.width - 48)
    height: Math.min(620, parent.height - 48)
    modal: true
    title: qsTr("Allow apps")
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

    function isAdded(id) {
        if (!plan) return false
        return plan.apps.some(a => a.id === id)
    }
    function matches(text) {
        const q = search.text.trim().toLowerCase()
        return q === "" || text.toLowerCase().indexOf(q) >= 0
    }

    onOpened: {
        backend.refreshApps()
        backend.refreshWindows()
        search.text = ""
        search.forceActiveFocus()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        TabBar {
            id: tabs
            Layout.fillWidth: true
            TabButton { text: qsTr("Installed apps") }
            TabButton { text: qsTr("Open windows") }
        }

        TextField {
            id: search
            Layout.fillWidth: true
            placeholderText: qsTr("Search")
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: tabs.currentIndex === 0
                   ? backend.apps.filter(a => dialog.matches(a.name + " " + a.id))
                   : backend.windows.filter(w => dialog.matches(w["class"] + " " + w.title))
            ScrollBar.vertical: ScrollBar {}

            delegate: ItemDelegate {
                required property var modelData
                readonly property string itemId: tabs.currentIndex === 0 ? modelData.id : "class:" + modelData["class"]
                readonly property bool added: dialog.isAdded(itemId)
                width: ListView.view.width
                contentItem: RowLayout {
                    spacing: 12
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Label {
                            Layout.fillWidth: true
                            text: tabs.currentIndex === 0 ? modelData.name : modelData.title || modelData["class"]
                            elide: Text.ElideRight
                            color: Palette.text
                        }
                        Label {
                            Layout.fillWidth: true
                            text: tabs.currentIndex === 0 ? modelData.classes.join(", ") : modelData["class"]
                            elide: Text.ElideRight
                            color: Palette.muted
                            font.pixelSize: 11
                        }
                    }
                    Label {
                        text: added ? qsTr("Allowed") : qsTr("Allow")
                        color: added ? theme.accent : Palette.muted
                    }
                }
                onClicked: {
                    if (added) {
                        const index = dialog.plan.apps.findIndex(a => a.id === itemId)
                        backend.removeApp(dialog.planId, index)
                    } else if (tabs.currentIndex === 0) {
                        backend.addApp(dialog.planId, modelData)
                    } else {
                        backend.addWindowClass(dialog.planId, modelData["class"], modelData.title)
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: list.count === 0
                color: Palette.muted
                text: qsTr("Nothing matches.")
            }
        }
    }
}
