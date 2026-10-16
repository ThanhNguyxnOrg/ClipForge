import QtQuick

QtObject {
    readonly property color navy: "#0B0E1A"
    readonly property color panel: "#141826"
    readonly property color orange: "#FF5500"
    readonly property color orangeEnd: "#FF8800"
    readonly property color cyan: "#00E5FF"
    readonly property color bone: "#F3F5F8"
    readonly property color slate: "#8A93A6"
    readonly property color danger: "#EF4444"
    readonly property color track: "#1E2433"
    readonly property color border: "#2A3145"
    readonly property int radiusCard: 12
    readonly property int radiusControl: 8
    readonly property string monoFont: "Cascadia Mono"

    function statusColor(status: string): color {
        if (status === "done") {
            return cyan;
        }
        if (status === "active") {
            return orange;
        }
        if (status === "failed") {
            return danger;
        }
        return slate;
    }
}
