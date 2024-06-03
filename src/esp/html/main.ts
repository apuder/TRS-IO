
interface Status {
    hardware_rev: number,
    vers_major: number,
    vers_minor: number,
    wifi_status: number,
    ip: string,
    config?: number, // only for TRS-IO++
    color: number,
    ssid: string,
    passwd: string,
    smb_url: string,
    smb_user: string,
    smb_passwd: string,
    time: string,
    smb_err: string,
    posix_err: string,
    has_sd_card: boolean,
    frehd_loaded: string,
    tz: string,
}

const WIFI_STATUS_TO_STRING = new Map<number,string>([
    [1, "Connecting"],
    [2, "Connected"],
    [3, "Not connected"],
    [4, "Not configured"],
]);

const CONFIGURATIONS = [
    "TRS-IO (Model 1)",
    "TRS-IO (Model III)",
    "PocketTRS (Model 1, internal TRS-IO)",
    "Reserved",
    "PocketTRS (Model III, internal TRS-IO)",
    "PocketTRS (Model 4, internal TRS-IO)",
    "PocketTRS (Model 4P, internal TRS-IO)",
    "Custom 1",
    "Custom 2",
    "PocketTRS (Model 1, external TRS-IO)",
    "Reserved",
    "PocketTRS (Model III, external TRS-IO)",
    "PocketTRS (Model 4, external TRS-IO)",
    "PocketTRS (Model 4P, external TRS-IO)",
    "Custom 1",
    "Custom 2"
];

let g_mostRecentStatus: Status | undefined = undefined;

function updateStatusField(id: string, value: number | string): void {
    if (typeof(value) === "number") {
        value = value.toString();
    }

    const element = document.getElementById(id);
    if (element === null) {
        console.error("Element not found: " + id);
        return;
    }

    element.textContent = value;
}

function getSettingsField(id: string): string {
    const element = document.getElementById(id) as HTMLInputElement;
    if (element === null) {
        console.error("Element not found: " + id);
        return "";
    }

    return element.value;
}

function getSettingsEnumField(name: string): string {
    const elements = document.getElementsByName(name);
    for (const element of elements) {
        const inputElement = element as HTMLInputElement;
        if (inputElement.checked) {
            return inputElement.value;
        }
    }

    return "unknown";
}

function updateSettingsField(id: string, value: string): void {
    const element = document.getElementById(id) as HTMLInputElement;
    if (element === null) {
        console.error("Element not found: " + id);
        return;
    }

    element.value = value;
}

function updateSettingsEnumField(name: string, value: string): void {
    const elements = document.getElementsByName(name);
    for (const element of elements) {
        const inputElement = element as HTMLInputElement;
        inputElement.checked = inputElement.value === value;
    }
}

function updateSettingsForm(status: Status): void {
    updateSettingsEnumField("color", status.color.toString());
    updateSettingsField("tz", status.tz);
    updateSettingsField("ssid", status.ssid);
    updateSettingsField("passwd", status.passwd);
    updateSettingsField("smb_url", status.smb_url);
    updateSettingsField("smb_user", status.smb_user);
    updateSettingsField("smb_passwd", status.smb_passwd);
}

function updateStatus(status: Status, initialFetch: boolean): void {
    g_mostRecentStatus = status;

    updateStatusField("hardware_rev", status.hardware_rev);
    updateStatusField("vers_major", status.vers_major);
    updateStatusField("vers_minor", status.vers_minor);
    updateStatusField("time", status.time);
    updateStatusField("ip", status.ip);
    updateStatusField("wifi_status", WIFI_STATUS_TO_STRING.get(status.wifi_status) ?? "Unknown");
    updateStatusField("smb_err", status.smb_err);
    updateStatusField("posix_err", status.posix_err);
    updateStatusField("frehd_loaded", status.frehd_loaded);

    if (initialFetch) {
        updateSettingsForm(status);
    }
}

async function fetchStatus(initialFetch: boolean) {
    const response = await fetch("/status");
    const status = await response.json() as Status;
    updateStatus(status, initialFetch);
}

function scheduleFetchStatus() {
    setInterval(async () => await fetchStatus(false), 2000);
}

async function saveSettings(): Promise<void> {
    const settings = {
        color: parseInt(getSettingsEnumField("color")),
        tz: getSettingsField("tz"),
        ssid: getSettingsField("ssid"),
        passwd: getSettingsField("passwd"),
        smb_url: getSettingsField("smb_url"),
        smb_user: getSettingsField("smb_user"),
        smb_passwd: getSettingsField("smb_passwd"),
    };

    const response = await fetch("/config", {
        method: "POST",
        cache: "no-cache",
        headers: {
            "Content-Type": "application/json",
        },
        body: JSON.stringify(settings),
    });
    if (response.status !== 200) {
        console.log("Failed to save settings", response);
    } else {
        const status = await response.json() as Status;
        updateStatus(status, false);
    }
}

function resizeDots(): void {
    const canvas = document.getElementById("dots_canvas") as HTMLCanvasElement;
    const parent = canvas.parentNode as HTMLElement;

    canvas.width = parent.offsetWidth;
    canvas.height = parent.offsetHeight;
}

function redrawDots(): void {
    const canvas = document.getElementById("dots_canvas") as HTMLCanvasElement;
    const ctx = canvas.getContext("2d") as CanvasRenderingContext2D;

    ctx.fillStyle = "rgb(0 0 0 / 0%)";
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    const width = canvas.width;
    const height = canvas.height;
    const emptyY = height*3/6;
    const fullY = height*5/6;

    ctx.fillStyle = "#67525440"; // var(--brown)
    for (let y = 0; y < height; y++) {
        const t = Math.min(Math.max((y - emptyY) / (fullY - emptyY), 0), 1);
        const dotCount = t*30;

        for (let c = 0; c < dotCount; c++) {
            ctx.beginPath();
            ctx.arc(Math.random()*width, y, 1.2, 0, 2*Math.PI);
            ctx.fill();
        }
    }
}

function configureButtons() {
    const timezoneDetectButton = document.getElementById("timezoneDetectButton");
    const timezoneField = document.getElementById("tz") as HTMLInputElement | null;
    if (timezoneDetectButton !== null && timezoneField !== null) {
        timezoneDetectButton.addEventListener("click", () => {
            const timezone = Intl.DateTimeFormat().resolvedOptions().timeZone;
            if (timezone !== "") {
                timezoneField.value = timezone;
            }
        });
    }

    const saveStatusButton = document.getElementById("saveSettings") as HTMLButtonElement;
    saveStatusButton.addEventListener("click", () => saveSettings());

    const revertStatusButton = document.getElementById("revertSettings") as HTMLButtonElement;
    revertStatusButton.addEventListener("click", () => {
        if (g_mostRecentStatus !== undefined) {
            updateSettingsForm(g_mostRecentStatus);
        }
    });
}

export function main() {
    configureButtons();
    fetchStatus(true);
    resizeDots();
    redrawDots();
    scheduleFetchStatus();
}
