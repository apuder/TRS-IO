
import {Printer} from "./printer.js";
import editSvg from "./icons/edit.svg";
import deleteSvg from "./icons/delete.svg";
import downloadSvg from "./icons/download.svg";

// Hardware board type.
enum BoardType {
    TRS_IO_MODEL_1,
    TRS_IO_MODEL_3,
    TRS_IO_PP,
}

// Type of computer we're emulating in PocketTRS mode.
enum ModelType {
    MODEL_1,
    MODEL_3,
    MODEL_4,
    MODEL_4P,
}

interface Status {
    hardware_rev: number,
    vers_major: number,
    vers_minor: number,
    // These two only for TRS-IO++:
    fpga_vers_major?: number,
    fpga_vers_minor?: number,
    // See WIFI_STATUS_TO_STRING:
    wifi_status: number,
    ip: string,
    board: BoardType,
    // Dip switches:
    config?: number, // only for TRS-IO++, see CONFIGURATIONS list.
    git_commit: string,
    git_tag: string,
    git_branch: string,
    // 0 = White, 1 = Green, 2 = Amber.
    color: number,
    // Wifi:
    ssid: string,
    passwd: string,
    // SMB:
    smb_url: string,
    smb_user: string,
    smb_passwd: string,
    // Current local time:
    time: string,
    // Error connecting to SMB, or empty string if connected:
    smb_err: string,
    // Error mounting SD card, or empty string if mounted:
    posix_err: string,
    has_sd_card: boolean,
    // Error loading FreHD, or empty string if present:
    frehd_loaded: string,
    // Timezone name:
    tz: string,
    // Keyboard layout (see esp/components/FabGL/src/devdrivers/kbdlayouts.h):
    keyboard_layout: number,
    // Printer (0 = physical, 1 = emulated):
    printer_en: number,
}

interface Rom {
    filename: string,
    size: number,
    createdAt: number, // seconds since epoch
}

interface RomInfo {
    roms: Rom[],
    selected: string[],
}

interface FilesInfo {
    files: Rom[],
}

interface ErrorResponse {
    error: string,
}

// DIP switch configuration.
class Configuration {
    public constructor(public readonly name: string,
                       public readonly isPtrs: boolean,
                       public readonly model?: ModelType) {}
}

// Controlled by the DIP switches and given to us in the "config" status field.
const CONFIGURATIONS: Configuration[] = [
    new Configuration("TRS-IO (Model 1)", false),
    new Configuration("PocketTRS (Model 1, internal TRS-IO)", true, ModelType.MODEL_1),
    new Configuration("Reserved", false),
    new Configuration("PocketTRS (Model III, internal TRS-IO)", true, ModelType.MODEL_3),
    new Configuration("PocketTRS (Model 4, internal TRS-IO)", true, ModelType.MODEL_4),
    new Configuration("PocketTRS (Model 4P, internal TRS-IO)", true, ModelType.MODEL_4P),
    new Configuration("Custom 1", false),
    new Configuration("Custom 2", false),
    new Configuration("TRS-IO (Model III)", false),
    new Configuration("PocketTRS (Model 1, external TRS-IO)", true, ModelType.MODEL_1),
    new Configuration("Reserved", false),
    new Configuration("PocketTRS (Model III, external TRS-IO)", true, ModelType.MODEL_3),
    new Configuration("PocketTRS (Model 4, external TRS-IO)", true, ModelType.MODEL_4),
    new Configuration("PocketTRS (Model 4P, external TRS-IO)", true, ModelType.MODEL_4P),
    new Configuration("Custom 1", false),
    new Configuration("Custom 2", false),
];
// Missing "config" status field, indicating TRS-IO board.
const TRS_IO_M1_CONFIGURATION = new Configuration("TRS-IO (Model 1)", false);
const TRS_IO_M3_CONFIGURATION = new Configuration("TRS-IO (Model III)", false);

const WIFI_STATUS_TO_STRING = new Map<number,string>([
    [1, "Connecting"],
    [2, "Connected"],
    [3, "Not connected"],
    [4, "Not configured"],
]);

const BOARD_TYPE_TO_STRING = new Map<BoardType,string>([
    [BoardType.TRS_IO_MODEL_1, "TRS-IO for Model 1"],
    [BoardType.TRS_IO_MODEL_3, "TRS-IO for Model III"],
    [BoardType.TRS_IO_PP, "TRS-IO++"],
]);
const BOARD_TYPE_TO_BODY_DATASET = new Map<BoardType,string>([
    [BoardType.TRS_IO_MODEL_1, "trs-io-m1"],
    [BoardType.TRS_IO_MODEL_3, "trs-io-m3"],
    [BoardType.TRS_IO_PP, "trs-io-pp"],
]);
const MODEL_TYPE_TO_BODY_DATASET = new Map<ModelType | undefined,string>([
    [undefined, "none"],
    [ModelType.MODEL_1, "trs-80-model-1"],
    [ModelType.MODEL_3, "trs-80-model-3"],
    [ModelType.MODEL_4, "trs-80-model-4"],
    [ModelType.MODEL_4P, "trs-80-model-4p"],
]);

// Message displayed to the user (usually an error).
class UserMessage {
    public readonly messageBox: HTMLSpanElement;

    public constructor(message: string, autohide: boolean) {
        this.messageBox = document.createElement("span");
        this.messageBox.classList.add("message-box");
        this.messageBox.textContent = message;

        const container = document.querySelector(".article-container") as Element;
        container.append(this.messageBox);
        if (autohide) {
            setTimeout(() => this.hide(), 3000);
        }
    }

    // Fade out and hide this message.
    public async hide() {
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

let gMostRecentStatus: Status | undefined = undefined;
let gStatusFetchAbortController: AbortController | undefined = undefined;
const gUserMessages: UserMessage[] = [];
let gOfflineUserMessage: UserMessage | undefined = undefined;

/**
 * Switch the view to the Settings tab.
 */
function goToSettings() {
    const radio = document.getElementById("settings_article") as (HTMLInputElement | null);
    if (radio !== null) {
        radio.checked = true;
    }
}

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
        userMessage.messageBox.style.bottom = (30 + 50*i) + "px";
    }
}

// Display a brief error to the user.
function displayError(message: string, autohide = true): UserMessage {
    const userMessage = new UserMessage(message, autohide);
    gUserMessages.push(userMessage);
    updateUserMessages();
    return userMessage;
}

// Replaces the optional trailing "+" with an English message.
function cleanUpGitCommit(gitTag: string, gitCommit: string): string {
    if (gitCommit.endsWith("+")) {
        gitCommit = gitCommit.substring(0, gitCommit.length - 1) + ", with local changes";
    }
    if (gitTag !== "") {
        gitCommit = gitTag + " (" + gitCommit + ")";
    }

    return gitCommit;
}

// Generic name for the device we're connected to (not the specific configuration of it).
function getDeviceName(status: Status | undefined): string {
    return status === undefined
        ? "device"
        : status.board === BoardType.TRS_IO_PP
            ? "TRS-IO++"
            : "TRS-IO";
}

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

function updateSettingsField(id: string, value: string | undefined): void {
    const element = document.getElementById(id) as HTMLInputElement;
    if (element === null) {
        console.error("Element not found: " + id);
        return;
    }

    element.value = value ?? "";
}

function updateSettingsEnumField(name: string, value: string): void {
    const elements = document.getElementsByName(name);
    for (const element of elements) {
        const inputElement = element as HTMLInputElement;
        inputElement.checked = inputElement.value === value;
    }
}

function updateStatusIcon(id: string, enabled: boolean, message: string): void {
    const icon = document.getElementById(id) as HTMLImageElement;

    icon.classList.toggle("enabled", enabled);
    icon.classList.toggle("disabled", !enabled);
    icon.title = message;
}

function updateSettingsForm(status: Status): void {
    updateSettingsEnumField("color", status.color.toString());
    updateSettingsEnumField("printer_en", status.printer_en.toString());
    updateSettingsField("tz", status.tz);
    updateSettingsField("ssid", status.ssid);
    updateSettingsField("passwd", status.passwd);
    updateSettingsField("smb_url", status.smb_url);
    updateSettingsField("smb_user", status.smb_user);
    updateSettingsField("smb_passwd", status.smb_passwd);

    const keyboardSelect = document.getElementById("keyboard_layout") as HTMLSelectElement;
    keyboardSelect.selectedIndex = status.keyboard_layout;
}

function updateStatus(status: Status, initialFetch: boolean): void {
    gMostRecentStatus = status;

    const wifiStatusText = WIFI_STATUS_TO_STRING.get(status.wifi_status) ?? "Unknown";
    const configuration = status.board === BoardType.TRS_IO_PP
        ? CONFIGURATIONS[status.config ?? 0]
        : status.board === BoardType.TRS_IO_MODEL_1
            ? TRS_IO_M1_CONFIGURATION
            : TRS_IO_M3_CONFIGURATION;
    document.body.classList.toggle("ptrs-mode", configuration.isPtrs);
    document.body.dataset.boardType = BOARD_TYPE_TO_BODY_DATASET.get(status.board) ?? "unknown";
    document.body.dataset.emulatedModel = MODEL_TYPE_TO_BODY_DATASET.get(configuration.model) ?? "unknown";

    const sdCardMounted = status.has_sd_card && (status.posix_err === undefined || status.posix_err === "");
    const frehdLoaded = status.frehd_loaded === undefined || status.frehd_loaded === "";
    const smbConnected = status.smb_err === undefined || status.smb_err === "";

    const sdCardStatus = sdCardMounted ? "Mounted" : status.posix_err;
    const frehdStatus = frehdLoaded ? "Found" : status.frehd_loaded;
    const smbStatus = smbConnected ? "Connected" : status.smb_err;

    updateStatusField("vers_major", status.vers_major);
    updateStatusField("vers_minor", status.vers_minor);
    const fpgaVers = document.getElementById("fpga_vers") as HTMLElement;
    if (status.fpga_vers_major === undefined || status.fpga_vers_minor === undefined) {
        fpgaVers.style.display = "none";
    } else {
        fpgaVers.style.removeProperty("display");
        updateStatusField("fpga_vers_major", status.fpga_vers_major);
        updateStatusField("fpga_vers_minor", status.fpga_vers_minor);
    }
    updateStatusField("board", BOARD_TYPE_TO_STRING.get(status.board) ?? "Unknown");
    updateStatusField("configuration", configuration.name);
    updateStatusField("git_commit", cleanUpGitCommit(status.git_tag, status.git_commit));
    updateStatusField("git_branch", status.git_branch);
    updateStatusField("time", status.time);
    updateStatusField("time_zone", status.tz || "Not set");
    updateStatusField("wifi_ssid", status.ssid || "Not configured");
    updateStatusField("wifi_status", wifiStatusText);
    updateStatusField("ip", status.ip);
    updateStatusField("smb_err", smbStatus);
    updateStatusField("posix_err", sdCardStatus);
    updateStatusField("frehd_loaded", frehdStatus);

    updateStatusIcon("wifi_status_icon", status.wifi_status === 2, wifiStatusText);
    updateStatusIcon("smb_share_status_icon", smbConnected, smbStatus);
    updateStatusIcon("sd_card_status_icon", sdCardMounted, sdCardStatus);

    const deviceTypeSpan = document.getElementById("device_type")!;
    deviceTypeSpan.textContent = getDeviceName(status);

    const printerEmulationWarning = document.getElementById("printerEmulationWarning")!;
    printerEmulationWarning.style.display = status.printer_en === 1 ? "none" : "block";

    if (initialFetch) {
        updateSettingsForm(status);

        // Select the settings tab on start-up if Wi-Fi hasn't yet been configured.
        if (!status.ssid) {
            goToSettings();
        }
    }
}

async function fetchStatus(initialFetch: boolean) {
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
    } catch (error: any) {
        // Fetch was aborted.
        if (gOfflineUserMessage === undefined) {
            gOfflineUserMessage = displayError("Can’t reach " + getDeviceName(gMostRecentStatus), false);
        }
        return;
    }

    // Get response.
    gStatusFetchAbortController = undefined;
    if (response.status === 200) {
        const status = await response.json() as Status;
        updateStatus(status, initialFetch);
        if (gOfflineUserMessage !== undefined) {
            gOfflineUserMessage.hide();
            gOfflineUserMessage = undefined;
        }
    } else {
        console.log("Error fetching status", response);
    }
}

function scheduleFetchStatus() {
    setInterval(async () => await fetchStatus(false), 2000);
}

// Returns whether successful
async function deleteRomFile(filename: string): Promise<boolean> {
    startProgressIndicator();
    try {
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
            const romInfo = await response.json() as RomInfo | ErrorResponse;
            if ("error" in romInfo) {
                displayError(romInfo.error);
            } else {
                updateRomInfo(romInfo);
                return true;
            }
        } else {
            displayError("Error deleting ROM");
        }
    } finally {
        stopProgressIndicator();
    }
    return false;
}

// Returns whether successful
async function renameRomFile(oldFilename: string, newFilename: string): Promise<boolean> {
    startProgressIndicator();
    try {
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
            const romInfo = await response.json() as RomInfo | ErrorResponse;
            if ("error" in romInfo) {
                displayError(romInfo.error);
            } else {
                updateRomInfo(romInfo);
                return true;
            }
        } else {
            displayError("Error renaming ROM");
        }
    } finally {
        stopProgressIndicator();
    }
    return false;
}

// Returns whether successful
async function assignRomFile(model: number, filename: string): Promise<boolean> {
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
        const romInfo = await response.json() as RomInfo | ErrorResponse;
        if ("error" in romInfo) {
            displayError(romInfo.error);
        } else {
            updateRomInfo(romInfo);
            return true;
        }
    } else {
        displayError("Error assigning ROM");
    }
    return false;
}

function updateRomInfo(romInfo: RomInfo) {
    romInfo.roms.sort((a, b) => {
        return a.filename.localeCompare(b.filename, undefined, {
            numeric: true,
        });
    });

    const tbody = document.querySelector(".rom-table tbody") as HTMLElement;
    tbody.replaceChildren();

    for (let romIndex = 0; romIndex < romInfo.roms.length; romIndex++) {
        const rom = romInfo.roms[romIndex];

        const tr = document.createElement("tr");
        let td;

        let filenameTd = document.createElement("td");
        filenameTd.classList.add("file-name");
        filenameTd.textContent = rom.filename;
        tr.append(filenameTd);

        td = document.createElement("td");
        td.classList.add("file-edit");
        const renameIcon = document.createElement("img");
        renameIcon.src = editSvg;
        const renameLink = document.createElement("a");
        renameLink.append(renameIcon);
        renameLink.href = "#";
        renameLink.addEventListener("click", async e => {
            e.preventDefault();
            startRename();
        });
        td.append(renameLink);
        tr.append(td);

        td = document.createElement("td");
        td.classList.add("file-delete");
        const deleteIcon = document.createElement("img");
        deleteIcon.src = deleteSvg;
        const deleteLink = document.createElement("a");
        deleteLink.append(deleteIcon);
        deleteLink.href = "#";
        deleteLink.addEventListener("click", async e => {
            e.preventDefault();
            const success = await deleteRomFile(rom.filename);
        });
        td.append(deleteLink);
        tr.append(td);

        td = document.createElement("td");
        td.classList.add("file-size");
        td.textContent = rom.size.toLocaleString(undefined, {
            useGrouping: true,
        });
        tr.append(td);

        td = document.createElement("td");
        td.classList.add("file-date");
        if (rom.createdAt === 0) {
            td.classList.add("file-no-date");
            td.textContent = "—"; // em dash for missing date.
        } else {
            td.textContent = new Date(rom.createdAt*1000).toLocaleString(undefined, {
                dateStyle: "short",
            } as any); // "any" needed because TS doesn't know about "dateStyle" option.
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
                filenameTd.removeEventListener("blur", blurListener);
                filenameTd.removeEventListener("keydown", keyListener);
                filenameTd.contentEditable = "false";
                window.getSelection()?.removeAllRanges();
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
                } else {
                    const success = await renameRomFile(rom.filename, newFilename);
                    if (!success) {
                        rollback();
                    }
                }
            };

            const blurListener = () => {
                commit();
            };

            const keyListener = (e: KeyboardEvent) => {
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
            td.classList.add("file-select");
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

        tbody.append(tr);
    }
}

async function fetchRomInfo() {
    startProgressIndicator();
    try {
        const response = await fetch("/roms");
        if (response.status === 200) {
            const romInfo = await response.json() as RomInfo;
            updateRomInfo(romInfo);
        } else {
            console.log("Error fetching ROM info", response);
        }
    } finally {
        stopProgressIndicator();
    }
}

// Returns whether successful
async function downloadFile(filename: string): Promise<boolean> {
    let downloadFilename = filename.replace("/", ".");
    const i = downloadFilename.indexOf(":");
    if (i > 0) {
        downloadFilename = downloadFilename.substring(0, i);
    }

    const a = document.createElement("a");
    a.href = "/files/" + filename;
    a.download = downloadFilename;
    a.click();
    return true;
}

// Returns whether successful
async function deleteFile(filename: string): Promise<boolean> {
    startProgressIndicator();
    try {
        const response = await fetch("/files/", {
            method: "POST",
            cache: "no-cache",
            headers: {
                "Content-Type": "application/json",
            },
            body: JSON.stringify({
                command: "delete",
                filename,
            }),
        });
        if (response.status === 200) {
            const filesInfo = await response.json() as FilesInfo | ErrorResponse;
            if ("error" in filesInfo) {
                displayError(filesInfo.error);
            } else {
                updateFilesInfo(filesInfo);
                return true;
            }
        } else {
            displayError("Error deleting file");
        }
    } finally {
        stopProgressIndicator();
    }
    return false;
}

// Returns whether successful
async function renameFile(oldFilename: string, newFilename: string): Promise<boolean> {
    startProgressIndicator();
    try {
        const response = await fetch("/files/", {
            method: "POST",
            cache: "no-cache",
            headers: {
                "Content-Type": "application/json",
            },
            body: JSON.stringify({
                command: "rename",
                oldFilename,
                newFilename,
            }),
        });
        if (response.status === 200) {
            const filesInfo = await response.json() as FilesInfo | ErrorResponse;
            if ("error" in filesInfo) {
                displayError(filesInfo.error);
            } else {
                updateFilesInfo(filesInfo);
                return true;
            }
        } else {
            displayError("Error renaming files");
        }
    } finally {
        stopProgressIndicator();
    }
    return false;
}

function updateFilesInfo(filesInfo: FilesInfo | ErrorResponse) {
    const filesListNode = document.querySelector(".files .files-list") as HTMLElement;
    const filesErrorNode = document.querySelector(".files .files-error") as HTMLElement;
    const tbody = document.querySelector(".files-table tbody") as HTMLElement;
    tbody.replaceChildren();

    const isError = "error" in filesInfo;
    filesListNode.hidden = isError;
    filesErrorNode.hidden = !isError;
    if (isError) {
        filesErrorNode.textContent = filesInfo.error;
        return;
    }

    filesInfo.files.sort((a, b) => {
        return a.filename.localeCompare(b.filename, undefined, {
            numeric: true,
        });
    });

    for (let romIndex = 0; romIndex < filesInfo.files.length; romIndex++) {
        const file = filesInfo.files[romIndex];

        const tr = document.createElement("tr");
        let td;

        let filenameTd = document.createElement("td");
        filenameTd.classList.add("file-name");
        filenameTd.textContent = file.filename;
        tr.append(filenameTd);

        td = document.createElement("td");
        td.classList.add("file-edit");
        const renameIcon = document.createElement("img");
        renameIcon.src = editSvg;
        const renameLink = document.createElement("a");
        renameLink.append(renameIcon);
        renameLink.href = "#";
        renameLink.addEventListener("click", async e => {
            e.preventDefault();
            startRename();
        });
        td.append(renameLink);
        tr.append(td);

        td = document.createElement("td");
        td.classList.add("file-download");
        const downloadIcon = document.createElement("img");
        downloadIcon.src = downloadSvg;
        const downloadLink = document.createElement("a");
        downloadLink.append(downloadIcon);
        downloadLink.href = "#";
        downloadLink.addEventListener("click", async e => {
            e.preventDefault();
            const success = await downloadFile(file.filename);
        });
        td.append(downloadLink);
        tr.append(td);

        td = document.createElement("td");
        td.classList.add("file-delete");
        const deleteIcon = document.createElement("img");
        deleteIcon.src = deleteSvg;
        const deleteLink = document.createElement("a");
        deleteLink.append(deleteIcon);
        deleteLink.href = "#";
        deleteLink.addEventListener("click", async e => {
            e.preventDefault();
            const success = await deleteFile(file.filename);
        });
        td.append(deleteLink);
        tr.append(td);

        td = document.createElement("td");
        td.classList.add("file-size");
        td.textContent = file.size.toLocaleString(undefined, {
            useGrouping: true,
        });
        tr.append(td);

        td = document.createElement("td");
        td.classList.add("file-date");
        if (file.createdAt === 0) {
            td.classList.add("file-no-date");
            td.textContent = "—"; // em dash for missing date.
        } else {
            td.textContent = new Date(file.createdAt*1000).toLocaleString(undefined, {
                dateStyle: "short",
            } as any); // "any" needed because TS doesn't know about "dateStyle" option.
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
                const dot = filename.lastIndexOf("."); // Unix/DOS filename
                const slash = filename.lastIndexOf("/"); // TRSDOS filename
                const extension = dot > 0 ? dot : slash;
                const range = document.createRange();
                range.setStart(textNode, 0);
                range.setEnd(textNode, extension === -1 ? textNode.textContent.length : extension);

                const s = window.getSelection();
                if (s !== null) {
                    s.removeAllRanges();
                    s.addRange(range);
                }
            }

            const finish = () => {
                filenameTd.removeEventListener("blur", blurListener);
                filenameTd.removeEventListener("keydown", keyListener);
                filenameTd.contentEditable = "false";
                window.getSelection()?.removeAllRanges();
                tbody.classList.remove("renaming");
            };

            const rollback = () => {
                // Abort and return to old name.
                finish();
                filenameTd.textContent = file.filename;
            };

            const commit = async () => {
                if (!areRenaming()) {
                    return;
                }
                finish();
                const newFilename = filenameTd.textContent;
                if (newFilename === null || newFilename === "" || newFilename === file.filename) {
                    rollback();
                } else {
                    const success = await renameFile(file.filename, newFilename);
                    if (!success) {
                        rollback();
                    }
                }
            };

            const blurListener = () => {
                commit();
            };

            const keyListener = (e: KeyboardEvent) => {
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

        tbody.append(tr);
    }
}

async function fetchFilesInfo() {
    startProgressIndicator();
    try {
        const response = await fetch("/files/");
        if (response.status === 200) {
            const filesInfo = await response.json() as FilesInfo | ErrorResponse;
            updateFilesInfo(filesInfo);
        } else {
            console.log("Error fetching files info", response);
        }
    } finally {
        stopProgressIndicator();
    }
}

async function sleep(ms: number): Promise<void> {
    return new Promise<void>(accept => {
        setTimeout(() => accept(), ms);
    });
}

let gProgressIndicatorCount = 0;
let gProgressIndicatorNode: HTMLSpanElement | undefined = undefined;
let gProgressIndicatorTimer: number | undefined = undefined;

function cancelProgressIndicatorTimer() {
    if (gProgressIndicatorTimer !== undefined) {
        clearTimeout(gProgressIndicatorTimer);
        gProgressIndicatorTimer = undefined;
    }
}

function updateProgressIndicator(blinkOn: boolean) {
    if (gProgressIndicatorNode !== undefined) {
        gProgressIndicatorNode.textContent = "*" + (blinkOn ? "*" : "\u00A0"); // nbsp
    }

    cancelProgressIndicatorTimer();
    gProgressIndicatorTimer = setTimeout(() => {
        gProgressIndicatorTimer = undefined;
        updateProgressIndicator(!blinkOn);
    }, 200);
}

function startProgressIndicator() {
    gProgressIndicatorCount += 1;
    if (gProgressIndicatorCount === 1) {
        if (gProgressIndicatorNode !== undefined) {
            throw new Error("progress indicator is still active");
        }
        gProgressIndicatorNode = document.createElement("span");
        gProgressIndicatorNode.classList.add("stars");

        const container = document.querySelector(".article-container") as Element;
        container.append(gProgressIndicatorNode);

        updateProgressIndicator(true);
    }
}

function stopProgressIndicator() {
    gProgressIndicatorCount -= 1;
    if (gProgressIndicatorCount < 0) {
        throw new Error("progress indicator count went negative");
    }
    if (gProgressIndicatorCount === 0) {
        if (gProgressIndicatorNode === undefined) {
            throw new Error("progress indicator is not active");
        }
        gProgressIndicatorNode.remove();
        gProgressIndicatorNode = undefined;

        cancelProgressIndicatorTimer();
    }
}

async function saveSettings(): Promise<void> {
    const keyboardSelect = document.getElementById("keyboard_layout") as HTMLSelectElement;
    const settings = {
        color: parseInt(getSettingsEnumField("color")),
        printer_en: parseInt(getSettingsEnumField("printer_en")),
        tz: getSettingsField("tz"),
        ssid: getSettingsField("ssid"),
        passwd: getSettingsField("passwd"),
        smb_url: getSettingsField("smb_url"),
        smb_user: getSettingsField("smb_user"),
        smb_passwd: getSettingsField("smb_passwd"),
        keyboard_layout: keyboardSelect.selectedIndex,
    };

    startProgressIndicator();
    setTimeout(() => stopProgressIndicator(), 1000);
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
        displayError("Cannot save settings");
    } else {
        const status = await response.json() as Status;
        updateStatus(status, false);
    }
}

function drawDots(canvas: HTMLCanvasElement): void {
    const ctx = canvas.getContext("2d") as CanvasRenderingContext2D;

    const width = canvas.width;
    const height = canvas.height;
    const emptyY = height*3/6;
    const fullY = height*5/6;

    ctx.fillStyle = "rgb(0 0 0 / 0%)";
    ctx.fillRect(0, 0, width, height);

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
            const parent = document.querySelector(".article-container") as HTMLElement;
            parent.style.background = `url(${url}) bottom left repeat-x, linear-gradient(var(--yellow-top), var(--yellow-bottom))`;
        }
    }, "image/png");
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
        if (gMostRecentStatus !== undefined) {
            updateSettingsForm(gMostRecentStatus);
        }
    });

    const refreshRomsButton = document.getElementById("refreshRomsButton") as HTMLButtonElement;
    refreshRomsButton.addEventListener("click", () => fetchRomInfo());

    const refreshFilesButton = document.getElementById("refreshFilesButton") as HTMLButtonElement;
    refreshFilesButton.addEventListener("click", () => fetchFilesInfo());
}

async function handleRomUpload(file: File) {
    const contents = new Uint8Array(await file.arrayBuffer());
    const contentsString = Array.from(contents, byte => String.fromCodePoint(byte)).join("");

    startProgressIndicator();
    try {
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
        } else {
            const romInfo = await response.json() as RomInfo | ErrorResponse;
            if ("error" in romInfo) {
                displayError(romInfo.error);
            } else {
                updateRomInfo(romInfo);
                return true;
            }
        }
    } finally {
        stopProgressIndicator();
    }
}

async function handleRomDrop(e: DragEvent) {
    // Prevent default behavior (prevent file from being opened)
    e.preventDefault();

    if (e.dataTransfer) {
        const files: File[] = [];
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
        } else {
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
    const uploadRomInput = document.getElementById("uploadRomInput") as HTMLInputElement;
    uploadRomInput.addEventListener("change", async () => {
        if (uploadRomInput.files !== null) {
            for (const file of uploadRomInput.files) {
                await handleRomUpload(file);
            }
            uploadRomInput.value = "";
        }
    });

    const romsSection = document.querySelector(".roms") as HTMLElement;
    romsSection.addEventListener("drop", async e => {
        romsSection.classList.remove("hover");
        await handleRomDrop(e);
    });
    romsSection.addEventListener("dragover", e => {
        romsSection.classList.add("hover");

        // Prevent default behavior (prevent file from being opened).
        e.preventDefault();
    });
    romsSection.addEventListener("dragleave",  () => romsSection.classList.remove("hover"));
}

async function handleFilesUpload(file: File) {
    const contents = new Uint8Array(await file.arrayBuffer());
    const contentsString = Array.from(contents, byte => String.fromCodePoint(byte)).join("");

    startProgressIndicator();
    try {
        const response = await fetch("/files/", {
            method: "POST",
            cache: "no-cache",
            headers: {
                "Content-Type": "application/json",
            },
            body: JSON.stringify({
                command: "upload",
                filename: file.name,
                contents: window.btoa(contentsString),
            }),
        });
        if (response.status !== 200) {
            displayError("Error uploading file");
        } else {
            const filesInfo = await response.json() as FilesInfo | ErrorResponse;
            if ("error" in filesInfo) {
                displayError(filesInfo.error);
            } else {
                updateFilesInfo(filesInfo);
                return true;
            }
        }
    } finally {
        stopProgressIndicator();
    }
}

async function handleFilesDrop(e: DragEvent) {
    // Prevent default behavior (prevent file from being opened)
    e.preventDefault();

    if (e.dataTransfer) {
        const files: File[] = [];
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
        } else {
            // Use DataTransfer interface to access the files.
            for (const file of e.dataTransfer.files) {
                files.push(file);
            }
        }
        // Do this in a separate pass because the lists above will get blanked out
        // while these await calls block.
        for (const file of files) {
            await handleFilesUpload(file);
        }
    }
}

function configureFilesUpload() {
    const uploadFileInput = document.getElementById("uploadFilesInput") as HTMLInputElement;
    uploadFileInput.addEventListener("change", async () => {
        if (uploadFileInput.files !== null) {
            for (const file of uploadFileInput.files) {
                await handleFilesUpload(file);
            }
            uploadFileInput.value = "";
        }
    });

    const filesSection = document.querySelector(".files") as HTMLElement;
    filesSection.addEventListener("drop", async e => {
        filesSection.classList.remove("hover");
        await handleFilesDrop(e);
    });
    filesSection.addEventListener("dragover", e => {
        filesSection.classList.add("hover");

        // Prevent default behavior (prevent file from being opened).
        e.preventDefault();
    });
    filesSection.addEventListener("dragleave",  () => filesSection.classList.remove("hover"));
}

function configurePrinter() {
    const printerNode = document.querySelector(".printer-output") as HTMLDivElement;
    const printer = new Printer(printerNode);

    const savePrinterButton = document.getElementById("savePrinterButton") as HTMLButtonElement;
    savePrinterButton.addEventListener("click", () => printer.savePrintout());

    const clearPrinterButton = document.getElementById("clearPrinterButton") as HTMLButtonElement;
    clearPrinterButton.addEventListener("click", () => printer.clearPrintout());

    const attemptConnection = () => {
        const webSocket = new WebSocket("ws://" + window.location.host + "/printer");
        webSocket.addEventListener("message", async e => {
            if (e.data instanceof Blob) {
                const bytes = new Uint8Array(await e.data.arrayBuffer());
                for (let i = 0; i < bytes.length; i++) {
                    printer.printChar(bytes[i]);
                }
            }
        });
        webSocket.addEventListener("close", e => {
            setTimeout(attemptConnection, 1000);
        });
    };

    attemptConnection();
}

function configureDashboardImages() {
    function gotErrorLoadingImage(img: HTMLImageElement) {
        // Just remove the image. We could have a local fallback, but it's not worth it, this will
        // really only happen in the brief time before Wi-Fi is set up, and the user won't be
        // on the dashboard page anyway.
        img.remove();
    }

    // We source large images from the network to save on SPIFFS space, but these will fail to load if
    // Wi-Fi hasn't yet been configured. Hide the broken-image icon.
    const images = document.querySelectorAll(".dashboard-image > img") as NodeListOf<HTMLImageElement>;
    for (const image of images) {
        if (image.complete) {
            // Already loaded, see if it worked.
            if (image.naturalWidth === 0 && image.naturalHeight === 0) {
                gotErrorLoadingImage(image);
            }
        } else {
            // Not yet loaded, set a listener.
            image.addEventListener("error",
                    e => gotErrorLoadingImage(e.target as HTMLImageElement));
        }
    }
}

function configurePrinterEmulationWarning() {
    const settingsLink = document.querySelector("#printerEmulationWarning a")!;
    settingsLink.addEventListener("click", e => {
        goToSettings();
        e.preventDefault();
    });
}

export function main() {
    configureButtons();
    configureRomUpload();
    configureFilesUpload();
    fetchStatus(true);
    fetchRomInfo();
    fetchFilesInfo();
    configureDots();
    configurePrinter();
    scheduleFetchStatus();
    configureDashboardImages();
    configurePrinterEmulationWarning();
}
