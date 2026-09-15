import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Controls.Material 2.2

// Sonnenschein host-library view: a portrait cover grid of the paired host's
// Steam games. Bound to the `libraryModel` context property (HostLibraryModel).
Item {
    id: root
    // Sized by the StackView that hosts it — no anchors here (they conflict
    // with StackView-managed geometry).

    Column {
        anchors.fill: parent

        // Header
        Item {
            width: parent.width
            height: 56

            Label {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 20
                font.pointSize: 16
                font.bold: true
                color: "white"
                text: libraryModel.busy
                      ? qsTr("Host-Bibliothek wird geladen…")
                      : (libraryModel.error.length > 0
                         ? qsTr("Fehler: ") + libraryModel.error
                         : qsTr("Host-Bibliothek — %1 Spiele").arg(grid.count))
            }

            BusyIndicator {
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: 20
                running: libraryModel.busy
                visible: libraryModel.busy
            }
        }

        GridView {
            id: grid
            width: parent.width
            height: parent.height - 56
            leftMargin: 20
            topMargin: 10
            cellWidth: 230
            cellHeight: 320
            clip: true
            model: libraryModel

            delegate: Item {
                width: grid.cellWidth
                height: grid.cellHeight

                Rectangle {
                    id: card
                    width: 200
                    height: 300
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: "#1b1b1b"
                    radius: 6
                    border.color: model.installed ? "#3aa655" : "#444"
                    border.width: 1

                    // Cover art (portrait 600x900). Falls back to the game name
                    // centered on the card when the host has no local cover.
                    Image {
                        anchors.fill: parent
                        anchors.margins: 1
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        cache: true
                        source: model.cover ? model.cover : ""
                        visible: model.cover != ""
                    }

                    Label {
                        anchors.centerIn: parent
                        width: parent.width - 24
                        visible: !model.cover || model.cover == ""
                        text: model.name
                        color: "white"
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                        font.pointSize: 12
                    }

                    // Name caption strip at the bottom.
                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: 34
                        color: "#cc000000"
                        visible: model.cover && model.cover != ""
                        Label {
                            anchors.fill: parent
                            anchors.margins: 4
                            text: model.name
                            color: "white"
                            elide: Text.ElideRight
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            font.pointSize: 9
                        }
                    }
                }
            }
        }
    }
}
