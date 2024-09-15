const WIFI_STATUS_TO_STRING = new Map([
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
// Message displayed to the user (usually an error).
class UserMessage {
    constructor(message, autohide) {
        this.messageBox = document.createElement("span");
        this.messageBox.classList.add("message-box");
        this.messageBox.textContent = message;
        const container = document.querySelector(".article-container");
        container.append(this.messageBox);
        if (autohide) {
            setTimeout(() => this.hide(), 3000);
        }
    }
    // Fade out and hide this message.
    async hide() {
        const animation = this.messageBox.animate([
            { opacity: "0", },
        ], {
            duration: 300,
            fill: "forwards",
        });
        await animation.finished;
        this.messageBox.remove();
        updateUserMessages();
    }
}
let gMostRecentStatus = undefined;
let gStatusFetchAbortController = undefined;
const gUserMessages = [];
let gOfflineUserMessage = undefined;
function updateUserMessages() {
    // Remove dead messages.
    for (let i = gUserMessages.length - 1; i >= 0; i--) {
        if (gUserMessages[i].messageBox.parentNode === null) {
            gUserMessages.splice(i, 1);
        }
    }
    // Reposition them.
    for (let i = 0; i < gUserMessages.length; i++) {
        const userMessage = gUserMessages[i];
        userMessage.messageBox.style.bottom = (30 + 50 * i) + "px";
    }
}
// Display a brief error to the user.
function displayError(message, autohide = true) {
    const userMessage = new UserMessage(message, autohide);
    gUserMessages.push(userMessage);
    updateUserMessages();
    return userMessage;
}
function getDeviceName(status) {
    return status === undefined
        ? "device"
        : status.config === undefined
            ? "TRS-IO"
            : "TRS-IO++";
}
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
function getSettingsField(id) {
    const element = document.getElementById(id);
    if (element === null) {
        console.error("Element not found: " + id);
        return "";
    }
    return element.value;
}
function getSettingsEnumField(name) {
    const elements = document.getElementsByName(name);
    for (const element of elements) {
        const inputElement = element;
        if (inputElement.checked) {
            return inputElement.value;
        }
    }
    return "unknown";
}
function updateSettingsField(id, value) {
    const element = document.getElementById(id);
    if (element === null) {
        console.error("Element not found: " + id);
        return;
    }
    element.value = value !== null && value !== void 0 ? value : "";
}
function updateSettingsEnumField(name, value) {
    const elements = document.getElementsByName(name);
    for (const element of elements) {
        const inputElement = element;
        inputElement.checked = inputElement.value === value;
    }
}
function updateStatusIcon(id, enabled, message) {
    const icon = document.getElementById(id);
    icon.classList.toggle("enabled", enabled);
    icon.classList.toggle("disabled", !enabled);
    icon.title = message;
}
function updateSettingsForm(status) {
    updateSettingsEnumField("color", status.color.toString());
    updateSettingsField("tz", status.tz);
    updateSettingsField("ssid", status.ssid);
    updateSettingsField("passwd", status.passwd);
    updateSettingsField("smb_url", status.smb_url);
    updateSettingsField("smb_user", status.smb_user);
    updateSettingsField("smb_passwd", status.smb_passwd);
}
function updateStatus(status, initialFetch) {
    var _a;
    gMostRecentStatus = status;
    const wifiStatusText = (_a = WIFI_STATUS_TO_STRING.get(status.wifi_status)) !== null && _a !== void 0 ? _a : "Unknown";
    updateStatusField("hardware_rev", status.hardware_rev);
    updateStatusField("vers_major", status.vers_major);
    updateStatusField("vers_minor", status.vers_minor);
    updateStatusField("time", status.time);
    updateStatusField("ip", status.ip);
    updateStatusField("wifi_status", wifiStatusText);
    updateStatusField("smb_err", status.smb_err == "" ? "Connected" : status.smb_err);
    updateStatusField("posix_err", status.posix_err);
    updateStatusField("frehd_loaded", status.frehd_loaded == "" ? "Present" : status.frehd_loaded);
    updateStatusIcon("wifi_status_icon", status.wifi_status === 2, wifiStatusText);
    updateStatusIcon("smb_share_status_icon", status.smb_err === "", status.smb_err);
    updateStatusIcon("sd_card_status_icon", status.posix_err === "", status.posix_err);
    const deviceTypeSpan = document.getElementById("device_type");
    deviceTypeSpan.textContent = getDeviceName(status);
    if (initialFetch) {
        updateSettingsForm(status);
    }
}
async function fetchStatus(initialFetch) {
    // Abort any existing fetch.
    if (gStatusFetchAbortController !== undefined) {
        gStatusFetchAbortController.abort();
        gStatusFetchAbortController = undefined;
    }
    // Fetch status.
    let response;
    try {
        gStatusFetchAbortController = new AbortController();
        response = await fetch("/status", {
            cache: "no-store",
            signal: gStatusFetchAbortController.signal,
        });
    }
    catch (error) {
        // Fetch was aborted.
        if (gOfflineUserMessage === undefined) {
            gOfflineUserMessage = displayError("Can’t reach " + getDeviceName(gMostRecentStatus), false);
        }
        return;
    }
    // Get response.
    gStatusFetchAbortController = undefined;
    if (response.status === 200) {
        const status = await response.json();
        updateStatus(status, initialFetch);
        if (gOfflineUserMessage !== undefined) {
            gOfflineUserMessage.hide();
            gOfflineUserMessage = undefined;
        }
    }
    else {
        console.log("Error fetching status", response);
    }
}
function scheduleFetchStatus() {
    setInterval(async () => await fetchStatus(false), 2000);
}
// Returns whether successful
async function deleteRomFile(filename) {
    const response = await fetch("/roms", {
        method: "POST",
        cache: "no-cache",
        headers: {
            "Content-Type": "application/json",
        },
        body: JSON.stringify({
            command: "deleteRom",
            filename,
        }),
    });
    if (response.status === 200) {
        const romInfo = await response.json();
        if ("error" in romInfo) {
            displayError(romInfo.error);
        }
        else {
            updateRomInfo(romInfo);
            return true;
        }
    }
    else {
        displayError("Error deleting ROM");
    }
    return false;
}
// Returns whether successful
async function renameRomFile(oldFilename, newFilename) {
    const response = await fetch("/roms", {
        method: "POST",
        cache: "no-cache",
        headers: {
            "Content-Type": "application/json",
        },
        body: JSON.stringify({
            command: "renameRom",
            oldFilename,
            newFilename,
        }),
    });
    if (response.status === 200) {
        const romInfo = await response.json();
        if ("error" in romInfo) {
            displayError(romInfo.error);
        }
        else {
            updateRomInfo(romInfo);
            return true;
        }
    }
    else {
        displayError("Error renaming ROM");
    }
    return false;
}
// Returns whether successful
async function assignRomFile(model, filename) {
    const response = await fetch("/roms", {
        method: "POST",
        cache: "no-cache",
        headers: {
            "Content-Type": "application/json",
        },
        body: JSON.stringify({
            command: "assignRom",
            model,
            filename,
        }),
    });
    if (response.status === 200) {
        const romInfo = await response.json();
        if ("error" in romInfo) {
            displayError(romInfo.error);
        }
        else {
            updateRomInfo(romInfo);
            return true;
        }
    }
    else {
        displayError("Error assigning ROM");
    }
    return false;
}
function updateRomInfo(romInfo) {
    romInfo.roms.sort((a, b) => {
        return a.filename.localeCompare(b.filename, undefined, {
            numeric: true,
        });
    });
    const tbody = document.querySelector(".rom-table tbody");
    tbody.replaceChildren();
    for (let romIndex = 0; romIndex < romInfo.roms.length; romIndex++) {
        const rom = romInfo.roms[romIndex];
        const tr = document.createElement("tr");
        let td;
        let filenameTd = document.createElement("td");
        filenameTd.textContent = rom.filename;
        tr.append(filenameTd);
        td = document.createElement("td");
        const renameIcon = document.createElement("img");
        renameIcon.src = "/icons/edit.svg";
        const renameLink = document.createElement("a");
        renameLink.append(renameIcon);
        renameLink.href = "#";
        renameLink.addEventListener("click", async (e) => {
            e.preventDefault();
            startRename();
        });
        td.append(renameLink);
        tr.append(td);
        td = document.createElement("td");
        td.textContent = rom.size.toLocaleString(undefined, {
            useGrouping: true,
        });
        tr.append(td);
        td = document.createElement("td");
        if (rom.createdAt === 0) {
            td.textContent = "—"; // em dash for missing date.
        }
        else {
            td.textContent = new Date(rom.createdAt * 1000).toLocaleString(undefined, {
                dateStyle: "short",
            }); // "any" needed because TS doesn't know about "dateStyle" option.
        }
        tr.append(td);
        const startRename = () => {
            const areRenaming = () => tbody.classList.contains("renaming");
            if (areRenaming()) {
                return;
            }
            tbody.classList.add("renaming");
            filenameTd.contentEditable = "true";
            filenameTd.focus();
            // Select entire filename.
            const textNode = filenameTd.childNodes[0];
            if (textNode.textContent !== null) {
                const filename = textNode.textContent;
                const dot = filename.lastIndexOf(".");
                const range = document.createRange();
                range.setStart(textNode, 0);
                range.setEnd(textNode, dot === -1 ? textNode.textContent.length : dot);
                const s = window.getSelection();
                if (s !== null) {
                    s.removeAllRanges();
                    s.addRange(range);
                }
            }
            const finish = () => {
                var _a;
                filenameTd.removeEventListener("blur", blurListener);
                filenameTd.removeEventListener("keydown", keyListener);
                filenameTd.contentEditable = "false";
                (_a = window.getSelection()) === null || _a === void 0 ? void 0 : _a.removeAllRanges();
                tbody.classList.remove("renaming");
            };
            const rollback = () => {
                // Abort and return to old name.
                finish();
                filenameTd.textContent = rom.filename;
            };
            const commit = async () => {
                if (!areRenaming()) {
                    return;
                }
                finish();
                const newFilename = filenameTd.textContent;
                if (newFilename === null || newFilename === "" || newFilename === rom.filename) {
                    rollback();
                }
                else {
                    const success = await renameRomFile(rom.filename, newFilename);
                    if (!success) {
                        rollback();
                    }
                }
            };
            const blurListener = () => {
                commit();
            };
            const keyListener = (e) => {
                switch (e.key) {
                    case "Enter":
                        e.preventDefault();
                        commit();
                        break;
                    case "Escape":
                        e.preventDefault();
                        rollback();
                        break;
                }
            };
            filenameTd.addEventListener("blur", blurListener);
            filenameTd.addEventListener("keydown", keyListener);
        };
        filenameTd.addEventListener("click", () => startRename());
        for (let model of [0, 2, 3, 4]) {
            td = document.createElement("td");
            const input = document.createElement("input");
            input.type = "radio";
            input.name = "modelRom" + model;
            if (romInfo.selected[model] === rom.filename) {
                input.checked = true;
            }
            input.addEventListener("change", async () => {
                const success = await assignRomFile(model, rom.filename);
            });
            td.append(input);
            tr.append(td);
        }
        td = document.createElement("td");
        const deleteIcon = document.createElement("img");
        deleteIcon.src = "/icons/delete.svg";
        const deleteLink = document.createElement("a");
        deleteLink.append(deleteIcon);
        deleteLink.href = "#";
        deleteLink.addEventListener("click", async (e) => {
            e.preventDefault();
            const success = await deleteRomFile(rom.filename);
        });
        td.append(deleteLink);
        tr.append(td);
        tbody.append(tr);
    }
}
async function fetchRomInfo() {
    const response = await fetch("/roms");
    if (response.status === 200) {
        const romInfo = await response.json();
        updateRomInfo(romInfo);
    }
    else {
        console.log("Error fetching ROM info", response);
    }
}
async function sleep(ms) {
    return new Promise(accept => {
        setTimeout(() => accept(), ms);
    });
}
async function startSaveIndicator() {
    const stars = document.createElement("span");
    stars.classList.add("stars");
    const container = document.querySelector(".article-container");
    container.append(stars);
    for (let i = 0; i < 5; i++) {
        stars.textContent = "*" + (i % 2 === 0 ? "*" : "\u00A0"); // nbsp
        await sleep(200);
    }
    stars.remove();
}
async function saveSettings() {
    const settings = {
        color: parseInt(getSettingsEnumField("color")),
        tz: getSettingsField("tz"),
        ssid: getSettingsField("ssid"),
        passwd: getSettingsField("passwd"),
        smb_url: getSettingsField("smb_url"),
        smb_user: getSettingsField("smb_user"),
        smb_passwd: getSettingsField("smb_passwd"),
    };
    const responsePromise = fetch("/config", {
        method: "POST",
        cache: "no-cache",
        headers: {
            "Content-Type": "application/json",
        },
        body: JSON.stringify(settings),
    });
    const saveIndicatorPromise = startSaveIndicator();
    const [response, _] = await Promise.all([responsePromise, saveIndicatorPromise]);
    if (response.status !== 200) {
        console.log("Failed to save settings", response);
        displayError("Cannot save settings");
    }
    else {
        const status = await response.json();
        updateStatus(status, false);
    }
}
function drawDots(canvas) {
    const ctx = canvas.getContext("2d");
    const width = canvas.width;
    const height = canvas.height;
    const emptyY = height * 3 / 6;
    const fullY = height * 5 / 6;
    ctx.fillStyle = "rgb(0 0 0 / 0%)";
    ctx.fillRect(0, 0, width, height);
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
function configureDots() {
    // Draw dots once into an off-screen canvas.
    const sourceCanvas = document.createElement("canvas");
    sourceCanvas.width = 1024;
    sourceCanvas.height = 600;
    drawDots(sourceCanvas);
    // Use it as a background image.
    sourceCanvas.toBlob(blob => {
        if (blob !== null) {
            const url = URL.createObjectURL(blob);
            const parent = document.querySelector(".article-container");
            parent.style.background = `url(${url}) bottom left repeat-x, linear-gradient(var(--yellow-top), var(--yellow-bottom))`;
        }
    }, "image/png");
}
function configureButtons() {
    const timezoneDetectButton = document.getElementById("timezoneDetectButton");
    const timezoneField = document.getElementById("tz");
    if (timezoneDetectButton !== null && timezoneField !== null) {
        timezoneDetectButton.addEventListener("click", () => {
            const timezone = Intl.DateTimeFormat().resolvedOptions().timeZone;
            if (timezone !== "") {
                timezoneField.value = timezone;
            }
        });
    }
    const saveStatusButton = document.getElementById("saveSettings");
    saveStatusButton.addEventListener("click", () => saveSettings());
    const revertStatusButton = document.getElementById("revertSettings");
    revertStatusButton.addEventListener("click", () => {
        if (gMostRecentStatus !== undefined) {
            updateSettingsForm(gMostRecentStatus);
        }
    });
}
async function handleRomUpload(file) {
    const contents = new Uint8Array(await file.arrayBuffer());
    const contentsString = Array.from(contents, byte => String.fromCodePoint(byte)).join("");
    const response = await fetch("/roms", {
        method: "POST",
        cache: "no-cache",
        headers: {
            "Content-Type": "application/json",
        },
        body: JSON.stringify({
            command: "uploadRom",
            filename: file.name,
            contents: window.btoa(contentsString),
        }),
    });
    if (response.status !== 200) {
        displayError("Error uploading ROM");
    }
    else {
        const romInfo = await response.json();
        if ("error" in romInfo) {
            displayError(romInfo.error);
        }
        else {
            updateRomInfo(romInfo);
            return true;
        }
    }
}
async function handleRomDrop(e) {
    // Prevent default behavior (prevent file from being opened)
    e.preventDefault();
    if (e.dataTransfer) {
        const files = [];
        if (e.dataTransfer.items) {
            // Use DataTransferItemList interface to access the files.
            for (const item of e.dataTransfer.items) {
                // If dropped items aren't files, reject them.
                if (item.kind === "file") {
                    const file = item.getAsFile();
                    if (file !== null) {
                        files.push(file);
                    }
                }
            }
        }
        else {
            // Use DataTransfer interface to access the files.
            for (const file of e.dataTransfer.files) {
                files.push(file);
            }
        }
        // Do this in a separate pass because the lists above will get blanked out
        // while these await calls block.
        for (const file of files) {
            await handleRomUpload(file);
        }
    }
}
function configureRomUpload() {
    const uploadRomInput = document.getElementById("uploadRomInput");
    uploadRomInput.addEventListener("change", async () => {
        if (uploadRomInput.files !== null) {
            for (const file of uploadRomInput.files) {
                await handleRomUpload(file);
            }
            uploadRomInput.value = "";
        }
    });
    const romsSection = document.querySelector(".roms");
    romsSection.addEventListener("drop", async (e) => {
        romsSection.classList.remove("hover");
        await handleRomDrop(e);
    });
    romsSection.addEventListener("dragover", e => {
        romsSection.classList.add("hover");
        // Prevent default behavior (prevent file from being opened).
        e.preventDefault();
    });
    romsSection.addEventListener("dragleave", () => romsSection.classList.remove("hover"));
}
export function main() {
    configureButtons();
    configureRomUpload();
    fetchStatus(true);
    fetchRomInfo();
    configureDots();
    scheduleFetchStatus();
}
