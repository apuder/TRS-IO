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
export function main() {
    fetchStatus(true);
    scheduleFetchStatus();
}
