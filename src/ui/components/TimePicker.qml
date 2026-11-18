import QtQuick
import QtQuick.Layouts
import MoyuLedger

// 时间选择弹框：点时间字段后弹出，选择「时 + 分」
Item {
    id: picker
    anchors.fill: parent
    visible: false
    z: 950

    signal picked(string value)

    property int hour: 12
    property int minute: 30

    function show(current) {
        const parts = (current || "").split(":")
        hour = parts.length === 2 ? parseInt(parts[0], 10) : 12
        minute = parts.length === 2 ? parseInt(parts[1], 10) : 30
        if (isNaN(hour) || hour < 0 || hour > 23)
            hour = 12
        if (isNaN(minute) || minute < 0 || minute > 59)
            minute = 30
        visible = true
    }

    function close() { visible = false }

    function apply() {
        const hh = (hour < 10 ? "0" : "") + hour
        const mm = (minute < 10 ? "0" : "") + minute
        picked(hh + ":" + mm)
        close()
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.scrimSoft
        MouseArea { anchors.fill: parent; onClicked: picker.close() }
    }

    Rectangle {
        width: 326
        height: 452
        anchors.centerIn: parent
        radius: Theme.radiusPanel
        color: Theme.bgSurface
        border.width: 1
        border.color: Theme.borderBase

        MouseArea { anchors.fill: parent }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.spaceXl
            spacing: Theme.spaceXs

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: qsTr("选择时间")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontH1
                    font.weight: Theme.weightSemibold
                    color: Theme.textPrimary
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: (picker.hour < 10 ? "0" : "") + picker.hour + ":"
                          + (picker.minute < 10 ? "0" : "") + picker.minute
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontH1
                    font.weight: Theme.weightSemibold
                    font.features: ({ "tnum": 1 })
                    color: Theme.accent
                }
            }

            Text {
                text: qsTr("时")
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontMicro3
                color: Theme.textTertiary
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 6
                columnSpacing: 6
                rowSpacing: 6

                Repeater {
                    model: 24

                    delegate: Rectangle {
                        required property int index
                        Layout.fillWidth: true
                        height: 28
                        radius: 6
                        color: picker.hour === index ? Theme.accent : Theme.bgSurface
                        border.width: 1
                        border.color: picker.hour === index ? Theme.accent : Theme.borderBase

                        Text {
                            anchors.centerIn: parent
                            text: (index < 10 ? "0" : "") + index
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro3
                            font.features: ({ "tnum": 1 })
                            color: picker.hour === index ? Theme.textOnAccent : Theme.textPrimary
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: picker.hour = index
                        }
                    }
                }
            }

            Text {
                Layout.topMargin: Theme.spaceXs
                text: qsTr("分")
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontMicro3
                color: Theme.textTertiary
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 6
                columnSpacing: 6
                rowSpacing: 6

                Repeater {
                    model: 12

                    delegate: Rectangle {
                        required property int index
                        Layout.fillWidth: true
                        height: 28
                        radius: 6
                        color: picker.minute === index * 5 ? Theme.accent : Theme.bgSurface
                        border.width: 1
                        border.color: picker.minute === index * 5 ? Theme.accent : Theme.borderBase

                        Text {
                            anchors.centerIn: parent
                            text: (index * 5 < 10 ? "0" : "") + (index * 5)
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro3
                            font.features: ({ "tnum": 1 })
                            color: picker.minute === index * 5 ? Theme.textOnAccent : Theme.textPrimary
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: picker.minute = index * 5
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Theme.spaceS

                Repeater {
                    model: [{ label: qsTr("−1 分"), delta: -1 }, { label: qsTr("+1 分"), delta: 1 },
                            { label: qsTr("+5 分"), delta: 5 }, { label: qsTr("+10 分"), delta: 10 }]

                    delegate: Rectangle {
                        required property var modelData
                        Layout.fillWidth: true
                        height: 26
                        radius: 6
                        color: Theme.bgSubtle

                        Text {
                            anchors.centerIn: parent
                            text: modelData.label
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro3
                            color: Theme.textSecondary
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                let value = picker.minute + modelData.delta
                                if (value < 0)
                                    value += 60
                                picker.minute = value % 60
                            }
                        }
                    }
                }
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Row {
                    spacing: Theme.spaceMd
                    FlatButton { text: qsTr("取消"); kind: "ghost"; onClicked: picker.close() }
                    FlatButton { text: qsTr("确定"); kind: "primary"; onClicked: picker.apply() }
                }
            }
        }
    }

    Shortcut { sequence: "Escape"; enabled: picker.visible; onActivated: picker.close() }
}