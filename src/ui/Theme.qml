pragma Singleton
import QtQuick

// 设计 token 唯一来源：方案设计/08-推荐方案原型/Theme.tokens.json
// 用法：Theme.bgCanvas / Theme.fontBody / Theme.radiusCard / Theme.navW ...
// 注意：QML 颜色是 #AARRGGBB，CSS 的 #RRGGBBAA 必须转换（本文件已转换）。
QtObject {
    // ===== 主题开关（暗色值来自 tokens 的 dark 段）=====
    property bool dark: false

    // ===== 背景 =====
    readonly property color bgCanvas:    dark ? "#0E1116" : "#F5F6F8"
    readonly property color bgSurface:   dark ? "#161A20" : "#FFFFFF"
    readonly property color bgNav:       dark ? "#12161C" : "#FBFCFD"
    readonly property color bgSubtle:    dark ? "#1A1F27" : "#FAFBFC"
    readonly property color bgTint:      dark ? "#1F2530" : "#F2F5FA"
    readonly property color bgHover:     dark ? "#1C222B" : "#F4F6F9"
    readonly property color bgSelection: dark ? "#232A36" : "#F1F6FF"

    // ===== 描边 =====
    readonly property color borderBase:     dark ? "#262C36" : "#E3E6EB"
    readonly property color borderHairline: dark ? "#1E242C" : "#F4F5F8"
    readonly property color borderStrong:   dark ? "#2E3542" : "#DDE1E7"
    readonly property color borderDivider:  dark ? "#232933" : "#EDEFF3"

    // ===== 文字 =====
    readonly property color textPrimary:   dark ? "#E7EAF0" : "#1F2430"
    readonly property color textSecondary: dark ? "#9AA4B2" : "#7A8290"
    readonly property color textTertiary:  dark ? "#6F7987" : "#9AA1AC"
    readonly property color textDisabled:  dark ? "#545D6A" : "#B4BAC4"
    readonly property color textInverse:   dark ? "#0E1116" : "#FFFFFF"
    // 彩色底（分类徽标 / 主按钮 / 危险按钮）上的文字，两种主题都应是白色
    readonly property color textOnAccent:  "#FFFFFF"
    // 主按钮文字：亮色主题是蓝底白字，暗色主题是金底深字
    readonly property color textOnPrimary: dark ? "#0E1116" : "#FFFFFF"

    // ===== 强调色 =====
    readonly property color accent:           dark ? "#E4B363" : "#2563EB"
    readonly property color accentStrong:     dark ? "#D19C43" : "#1D4ED8"
    readonly property color accentSoft:       dark ? "#38E4B363" : "#EAF1FF"
    readonly property color accentSoftStrong: dark ? "#56E4B363" : "#DBE7FF"
    readonly property color focusRing:        dark ? "#38E4B363" : "#212563EB"

    // ===== 语义色 =====
    readonly property color income:    dark ? "#4ADE80" : "#15803D"
    readonly property color expense:   dark ? "#E7EAF0" : "#24292F"
    readonly property color negative:  dark ? "#F87171" : "#DC2626"
    readonly property color warnBg:    dark ? "#2A2116" : "#FFFBEB"
    readonly property color warnBorder: dark ? "#3D3020" : "#FBE5B4"
    readonly property color warnFg:    dark ? "#E4B363" : "#B45309"
    readonly property color warnSolid: dark ? "#E4B363" : "#F59E0B"
    readonly property color dangerBg:  dark ? "#2A1818" : "#FFEDE8"
    readonly property color dangerFg:  dark ? "#F87171" : "#DC2626"
    readonly property color ok:        dark ? "#4ADE80" : "#15803D"
    readonly property color closeHover: "#E81123"   // 窗口关闭按钮 hover（四套设计稿一致）

    // ===== 图表 =====
    readonly property color chartLine:       dark ? "#E4B363" : "#2563EB"
    readonly property color chartFillTop:    dark ? "#38E4B363" : "#382563EB"
    readonly property color chartBudgetLine: dark ? "#E4B363" : "#F59E0B"
    readonly property color chartGrid:       dark ? "#262C36" : "#F1F3F6"
    readonly property color chartTrack:      dark ? "#262C36" : "#EEF0F4"

    // ===== 遮罩 =====
    readonly property color scrim:     "#8C0F1218"
    readonly property color scrimSoft: "#6B0F1218"

    // ===== 分类色（不随主题变化）=====
    readonly property color catHousing:       "#6366F1"
    readonly property color catShopping:      "#F59E0B"
    readonly property color catFood:          "#EF4444"
    readonly property color catGift:          "#EC4899"
    readonly property color catEducation:     "#8B5CF6"
    readonly property color catEntertainment: "#06B6D4"
    readonly property color catTelecom:       "#64748B"
    readonly property color catDaily:         "#22C55E"
    readonly property color catMedical:       "#14B8A6"
    readonly property color catTransport:     "#3B82F6"
    readonly property color catIncome:        "#16A34A"

    // ===== 字体 =====
    // QML 的 font.family 只接受单一族名（不接受 CSS 列表），中文由 Qt 按字形自动回退到雅黑
    readonly property string fontFamily: "Segoe UI"
    readonly property string fontFamilyLatin: "Segoe UI"
    readonly property string fontFamilyMono: "Consolas"

    readonly property real fontMicro:    10
    readonly property real fontMicro2:   10.5
    readonly property real fontMicro3:   11
    readonly property real fontSmall:    12
    readonly property real fontSmall2:   12.5
    readonly property real fontBody:     13
    readonly property real fontBody2:    14
    readonly property real fontTitle:    16
    readonly property real fontH1:       17
    readonly property real fontH2:       20
    readonly property real fontKpi:      21
    readonly property real fontAmount:   28
    readonly property real fontAmountLg: 44
    readonly property real fontHero:     42

    readonly property int weightRegular:  400
    readonly property int weightMedium:   500
    readonly property int weightSemibold: 600
    readonly property int weightBold:     700

    readonly property real lineHeightBody:    1.5
    readonly property real lineHeightTitle:   1.25
    readonly property real lineHeightCompact: 1.2

    // ===== 圆角 =====
    readonly property int radiusChip:    4
    readonly property int radiusTag:     5
    readonly property int radiusBtn:     6
    readonly property int radiusCard:    8
    readonly property int radiusPanel:   12
    readonly property int radiusPalette: 16
    readonly property int radiusCircle:  999

    // ===== 间距 =====
    readonly property int spaceXs:   4
    readonly property int spaceS:    6
    readonly property int spaceSm:   8
    readonly property int spaceMd:   10
    readonly property int spaceLg:   12
    readonly property int spaceXl:   14
    readonly property int spaceXxl:  16
    readonly property int spaceXxxl: 24
    readonly property int pagePadding: 16
    readonly property int cardPadding: 14
    readonly property int cardGap:     14
    readonly property int sectionGap:  20

    // ===== 阴影（配合 QtQuick.Effects 的 MultiEffect 使用）=====
    readonly property color shadowCardColor:   "#0A101828"
    readonly property real  shadowCardBlur:    2
    readonly property real  shadowCardOffsetY: 1
    readonly property color shadowPanelColor:  "#73090E19"
    readonly property real  shadowPanelBlur:   28
    readonly property real  shadowPanelOffsetY: 8

    // ===== 窗口骨架尺寸 =====
    readonly property int windowW:       1440
    readonly property int windowH:       900
    readonly property int windowMinW:    1280
    readonly property int windowMinH:    800
    readonly property int titleBarH:     40
    readonly property int windowBtnW:    46
    readonly property int windowBtnH:    40
    readonly property int toolBarH:      56
    readonly property int controlH:      32
    readonly property int primaryBtnH:   34
    readonly property int searchW:       250
    readonly property int monthSwitchW:  168

    // ===== 三栏 =====
    readonly property int navW:          248
    readonly property int detailW:       352
    readonly property int navItemH:      36
    readonly property int accountRowH:   38
    readonly property int categoryRowH:  32
    readonly property int contentPadX:   16
    readonly property int contentPadY:   12

    // ===== KPI / 流水表 =====
    readonly property int kpiStripH:     68
    readonly property int kpiCellMinW:   150
    readonly property int colHeaderH:    33
    readonly property int groupHeaderH:  28
    readonly property int rowH:          45
    readonly property int statusBarH:    34
    readonly property int colCategoryW:  130
    readonly property int colTimeW:      150
    readonly property int colAccountW:   104
    readonly property int colAmountW:    104
    readonly property int selectedBarW:  2
    readonly property int detailKvRowH:  29

    // ===== 浮层 =====
    readonly property int panelW:         860
    readonly property int panelH:         620
    readonly property int panelHeaderH:   60
    readonly property int panelFooterH:   64
    readonly property int paletteW:       640
    readonly property int paletteTop:     110
    readonly property int paletteInputH:  56
    readonly property int paletteRowH:    42
    readonly property int paletteFooterH: 36

    // ===== 图表 / 点击目标 =====
    readonly property int ringDiameter:  150
    readonly property int ringStroke:    13
    readonly property int stackedBarH:   20
    readonly property int areaChartH:    266
    readonly property int insightRowH:   52
    readonly property int minClickSize:  28
    readonly property int circleBtn:     44
    readonly property int keypadKey:     46
    readonly property int badge:         24
    readonly property int badgeLg:       32
    readonly property int iconSize:      24
    readonly property real iconStroke:   1.6

    // ===== 动效（毫秒）=====
    readonly property int motionFast:   120
    readonly property int motionBase:   180
    readonly property int motionSlow:   240
    readonly property int listScrollDuration: 140
}
