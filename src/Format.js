.pragma library

// The service writes local times as ISO text without an offset; build the
// Date by hand so it's read as local time, not UTC.
function parseIso(text) {
    const m = /^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2})(?::(\d{2}))?/.exec(text || "")
    if (!m) return null
    return new Date(+m[1], +m[2] - 1, +m[3], +m[4], +m[5], +(m[6] || 0))
}

// Omarchy day numbers are Monday 1 … Sunday 7; QML's Locale counts Sunday 0.
function dayName(isoDay, format) {
    return Qt.locale().dayName(isoDay % 7, format)
}

// The week in the order the language starts it.
function weekDays() {
    const first = Qt.locale().firstDayOfWeek
    const days = []
    for (let i = 0; i < 7; ++i) {
        const qml = (first + i) % 7
        days.push(qml === 0 ? 7 : qml)
    }
    return days
}

function time(date) {
    return date.toLocaleTimeString(Qt.locale(), 1 /* Locale.ShortFormat */)
}

function when(date) {
    if (!date) return ""
    const today = new Date()
    const tomorrow = new Date(today.getFullYear(), today.getMonth(), today.getDate() + 1)
    if (date.toDateString() === today.toDateString())
        return qsTr("Today, %1").arg(time(date))
    if (date.toDateString() === tomorrow.toDateString())
        return qsTr("Tomorrow, %1").arg(time(date))
    return qsTr("%1, %2").arg(date.toLocaleDateString(Qt.locale(), "dddd d MMMM")).arg(time(date))
}

function clock(seconds) {
    seconds = Math.max(0, Math.round(seconds))
    const h = Math.floor(seconds / 3600), m = Math.floor(seconds % 3600 / 60), s = seconds % 60
    const two = (n) => (n < 10 ? "0" : "") + n
    return (h > 0 ? h + ":" + two(m) : m) + ":" + two(s)
}

function schedule(plan) {
    if (!plan) return ""
    const at = plan.time || ""
    if (plan.date)
        return qsTr("Once, %1 at %2").arg(plan.date).arg(at)
    const days = (plan.days || []).slice().sort()
    if (days.length === 0) return qsTr("No days picked")
    if (days.length === 7) return qsTr("Every day at %1").arg(at)
    if (days.join() === "1,2,3,4,5") return qsTr("Weekdays at %1").arg(at)
    return qsTr("%1 at %2").arg(days.map(d => dayName(d, 1 /* Locale.ShortFormat */)).join(", ")).arg(at)
}
