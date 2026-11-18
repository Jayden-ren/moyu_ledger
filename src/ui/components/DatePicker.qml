import QtQuick
import QtQuick.Layouts
import MoyuLedger

// 日期选择弹框：月历网格（7 列），点日期即选中
Item {
    id: picker
    anchors.fill: parent
    visible: false
    z: 950

    signal picked(string value)

    property int viewYear: 2026
    property int viewMonth: 11
    property int selectedDay: 1

    readonly property int firstWeekday: {
        const d = new Date(viewYear, viewMonth - 1, 1)
        return d.getDay()
    }
    readonly property int daysInMonth: new Date(viewYear, viewMonth, 0).getDate()
    readonly property int cellCount: firstWeekday + daysInMonth

    function show(current) {
        const parts = (current || "").split("-")
        const now = new Date()
        viewYear = parts.length === 3 ? parseInt(parts[0], 10) : now.getFullYear()
        viewMonth = parts.length === 3 ? parseInt(parts[1], 10) : now.getMonth() + 1
        selectedDay = parts.length === 3 ? parseInt(parts[2], 10) : now.getDate()
        if (isNaN(viewYear) || viewYear < 1970) viewYear = now.getFullYear()
        if (isNaN(viewMonth) || viewMonth < 1 || viewMonth > 12) viewMonth = now.getMonth() + 1
        if (isNaN(selectedDay) || selectedDay < 1) selectedDay = 1
        visible = true
    }

    function close() { visible = false }

    function pad(value) { return value < 10 ? "0" + value : "" + value }

    function apply() {
        picked(viewYear + "-" + pad(viewMonth) + "-" + pad(selectedDay))
        close()
    }

    function shiftMonth(delta) {
        let year = viewYear
        let month = viewMonth + delta
        if (month < 1) { month = 12; year -= 1 }
        if (month > 12) { month = 1; year += 1 }
        viewYear = year
        viewMonth = month
        if (selectedDay > daysInMonth)
            selectedDay = daysInMonth
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.scrimSoft
        MouseArea { anchors.fill: parent; onClicked: picker.close() }
    }

    Rectangle {
        width: 340
        height: 424
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

                Rectangle {
                    width: 26
                    height: 26
                    radius: 6
                    color: prevMouse.containsMouse ? Theme.bgHover : "transparent"
                    Text { anchors.centerIn: parent; text: "‹"; font.pixelSize: 16; color: Theme.textSecondary }
                    MouseArea {
                        id: prevMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: picker.shiftMonth(-1)
                    }
                }

                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: picker.viewYear + qsTr(" 年 ") + picker.viewMonth + qsTr(" 月")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody2
                    font.weight: Theme.weightSemibold
                    color: Theme.textPrimary
                }

                Rectangle {
                    width: 26
                    height: 26
                    radius: 6
                    color: nextMouse.containsMouse ? Theme.bgHover : "transparent"
                    Text { anchors.centerIn: parent; text: "›"; font.pixelSize: 16; color: Theme.textSecondary }
                    MouseArea {
                        id: nextMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: picker.shiftMonth(1)
                    }
                }
            }

            // 星期表头
            GridLayout {
                Layout.fillWidth: true
                columns: 7
                columnSpacing: 2

                Repeater {
                    model: [qsTr("日"), qsTr("一"), qsTr("二"), qsTr("三"), qsTr("四"), qsTr("五"), qsTr("六")]
                    delegate: Text {
                        required property string modelData
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: modelData
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMicro3
                        color: Theme.textTertiary
                    }
                }
            }

            // 日期网格
            GridLayout {
                Layout.fillWidth: true
                columns: 7
                columnSpacing: 2
                rowSpacing: 2

                Repeater {
                    model: picker.firstWeekday + picker.daysInMonth

                    delegate: Rectangle {
                        required property int index
                        readonly property int dayNumber: index - picker.firstWeekday + 1
                        Layout.fillWidth: true
                        height: 32
                        radius: 6
                        color: dayNumber === picker.selectedDay ? Theme.accent
                                                                : (dayMouse.containsMouse ? Theme.bgHover : "transparent")

                        Text {
                            anchors.centerIn: parent
                            text: dayNumber > 0 ? dayNumber : ""
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSmall2
                            color: dayNumber === picker.selectedDay ? Theme.textOnAccent : Theme.textPrimary
                        }

                        MouseArea {
                            id: dayMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            enabled: dayNumber > 0
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                picker.selectedDay = dayNumber
                                picker.apply()
                            }
                        }
                    }
                }
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true

                Rectangle {
                    width: todayLabel.implicitWidth + Theme.spaceXl
                    height: 28
                    radius: 6
                    color: Theme.bgSubtle
                    Text {
                        id: todayLabel
                        anchors.centerIn: parent
                        text: qsTr("回到今天")
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMicro3
                        color: Theme.textSecondary
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            const now = new Date()
                            picker.viewYear = now.getFullYear()
                            picker.viewMonth = now.getMonth() + 1
                            picker.selectedDay = now.getDate()
                        }
                    }
                }

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