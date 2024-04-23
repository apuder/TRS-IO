const WIFI_STATUS_TO_STRING = new Map();
WIFI_STATUS_TO_STRING.set(1, "Connecting");
WIFI_STATUS_TO_STRING.set(2, "Connected");
WIFI_STATUS_TO_STRING.set(3, "Not connected");
WIFI_STATUS_TO_STRING.set(4, "Not configured");
function updateStatusField(id, value) {
    if (typeof (value) === "number") {
        value = value.toString();
    }
    const element = document.getElementById(id);
    if (element === null) {
        console.error("Element not found: " + id);
        return;
    }
    element.textContent = value;
}
function updateSettingsField(id, value) {
    const element = document.getElementById(id);
    if (element === null) {
        console.error("Element not found: " + id);
        return;
    }
    element.value = value;
}
function updateStatus(status, initialFetch) {
    var _a;
    updateStatusField("hardware_rev", status.hardware_rev);
    updateStatusField("vers_major", status.vers_major);
    updateStatusField("vers_minor", status.vers_minor);
    updateStatusField("time", status.time);
    updateStatusField("ip", status.ip);
    updateStatusField("wifi_status", (_a = WIFI_STATUS_TO_STRING.get(status.wifi_status)) !== null && _a !== void 0 ? _a : "Unknown");
    updateStatusField("smb_err", status.smb_err);
    updateStatusField("posix_err", status.posix_err);
    updateStatusField("frehd_loaded", status.frehd_loaded);
    if (initialFetch) {
        updateSettingsField("ssid", status.ssid);
        updateSettingsField("passwd", status.passwd);
        updateSettingsField("smb_url", status.smb_url);
        updateSettingsField("smb_user", status.smb_user);
        updateSettingsField("smb_passwd", status.smb_passwd);
    }
}
async function fetchStatus(initialFetch) {
    const response = await fetch("/status");
    const status = await response.json();
    updateStatus(status, initialFetch);
}
function scheduleFetchStatus() {
    setInterval(async () => await fetchStatus(false), 2000);
}
function resizeDots() {
    const canvas = document.getElementById("dots_canvas");
    const parent = canvas.parentNode;
    canvas.width = parent.offsetWidth;
    canvas.height = parent.offsetHeight;
}
function redrawDots() {
    const canvas = document.getElementById("dots_canvas");
    const ctx = canvas.getContext("2d");
    ctx.fillStyle = "rgb(0 0 0 / 0%)";
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    const width = canvas.width;
    const height = canvas.height;
    const emptyY = height * 3 / 6;
    const fullY = height * 5 / 6;
    ctx.fillStyle = "#67525440"; // var(--brown)
    for (let y = 0; y < height; y++) {
        const t = Math.min(Math.max((y - emptyY) / (fullY - emptyY), 0), 1);
        const dotCount = t * 30;
        for (let c = 0; c < dotCount; c++) {
            ctx.beginPath();
            ctx.arc(Math.random() * width, y, 1.2, 0, 2 * Math.PI);
            ctx.fill();
        }
    }
}
export function main() {
    fetchStatus(true);
    resizeDots();
    redrawDots();
    scheduleFetchStatus();
}
