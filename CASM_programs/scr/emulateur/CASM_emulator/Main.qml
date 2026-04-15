import QtQuick

Window {
    visible: true
    width: 800
    height: 600

    Column {
        anchors.centerIn: parent
        spacing: 10

        Button {
            text: "Start Emulator"
            onClicked: emu.start()
        }

        Button {
            text: "Stop"
            onClicked: emu.stop()
        }
    }
}