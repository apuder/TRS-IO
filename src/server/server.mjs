
import fs from "fs";
import path from "path";
import { globSync } from 'glob'
import { WebSocketServer } from 'ws';
import express from 'express';
import expressStaticGzip from "express-static-gzip";

// Physical board type.
const BOARD_TYPE_TRS_IO_MODEL_1 = 0;
const BOARD_TYPE_TRS_IO_MODEL_3 = 1;
const BOARD_TYPE_TRS_IO_PP = 2;

// Switch 4 to switch 1.
const DIP_TRS_IO_M1 = 0b0000;
const DIP_TRS_IO_M3 = 0b1000;
const DIP_PTRS_M1_INT_IO = 0b0001;
const DIP_PTRS_M1_EXT_IO = 0b1001;
const DIP_PTRS_M3_INT_IO = 0b0011;
const DIP_PTRS_M3_EXT_IO = 0b1011;
const DIP_PTRS_M4_INT_IO = 0b0100;
const DIP_PTRS_M4_EXT_IO = 0b1100;
const DIP_PTRS_M4P_INT_IO = 0b0101;
const DIP_PTRS_M4P_EXT_IO = 0b1101;

// Update these to change the hardware configuration.
const BOARD_TYPE = BOARD_TYPE_TRS_IO_PP;
const DIP_SWITCHES = DIP_PTRS_M3_INT_IO;

const STATE_DIR = "state";
const ROMS_DIR = "state/roms";
const FILES_DIR = "state/files";
const DEFAULT_SETTINGS = {
    color: 1,
    printer_en: 1,
    tz: "GMT-8",
    ssid: "fandango",
    passwd: "22PineCreekSLO",
    smb_url: "smb://unifi/TRS-IO/smb-m3",
    smb_user: "lk",
    smb_passwd: "XXX",
    rom_assignments: ["", "", "", "", ""],
    keyboard_layout: 3, // US English
};
const PRINTER_OUTPUT = `40 CLS
45 OUT 255,04
100 CLS
200 REM *** COUNT DOWN FROM 10 TO 0 ***
210 FOR C=10 TO 0 STEP -1
220 PRINT C
230 FOR Z=1 TO 300:NEXT
233 CLS
235 NEXT C
250 PRINT "BLASTOFF ! ! !":T = 400:GOSUB 910
300 REM *** SPACESHIP ***
310 CLS
320 PRINT@512,"   *   "
330 PRINT     "  *U*  "
340 PRINT     "  *S*  "
350 PRINT     "  *A*  "
360 PRINT     " ***** "
370 PRINT     "*******"
380 T=400:GOSUB 910
400 REM *** LAUNCH THE SPACESHIP ***
410 PRINT "  !!!  ":T=300:GOSUB 910
420 PRINT "  !!!  ":T=200:GOSUB 910
430 PRINT "  !!!  ":T=100:GOSUB 910
440 FOR K=1 TO 16
450   PRINT:T = 100:GOSUB 910
460 NEXT K
500 REM *** END OF THE PROGRAM ***
510 CLS
520 PRINT"ALL SYSTEMS ARE GO.EVERYTHING IS AOK ! "
530 GOTO 530
900 REM *** TIME DELAY ***
910 FOR Z=1 TO T
920 NEXT:RETURN
`;

// Make sure the filename isn't trying to reach outside the ROM directory.
function isValidRomFilename(filename) {
    return filename !== "" && !filename.startsWith(".") && filename.indexOf("/") === -1;
}

// Make sure the filename isn't trying to reach outside the ROM directory.
function isValidFilename(filename) {
    // Currently the same.
    return isValidRomFilename(filename);
}

function sendJsonError(res, message) {
    console.log("Responding with error message: " + message);
    res.setHeader('Content-Type', 'application/json');
    res.end(JSON.stringify({
        "error": message,
    }));
}

function createStateDirsIfNecessary() {
    if (!fs.existsSync(STATE_DIR)){
        fs.mkdirSync(STATE_DIR);
    }
    if (!fs.existsSync(ROMS_DIR)){
        fs.mkdirSync(ROMS_DIR);
    }
    if (!fs.existsSync(FILES_DIR)){
        fs.mkdirSync(FILES_DIR);
    }
}

function getSettingsPathname() {
   return path.join(STATE_DIR, "settings.json");
}

function readSettings() {
    const path = getSettingsPathname();
    if (fs.existsSync(path)) {
        return JSON.parse(fs.readFileSync(path, 'utf8'));
    } else {
        return DEFAULT_SETTINGS;
    }
}

function writeSettings(settings) {
    createStateDirsIfNecessary();
    const path = getSettingsPathname();
    fs.writeFileSync(path, JSON.stringify(settings));
}

function makeStatus() {
    const settings = readSettings();

    const now = new Date();
    // const testMode = now.getTime() % 6000 < 3000;
    const status = {
        hardware_rev: 1,
        vers_major: 2,
        vers_minor: 0,
        fpga_vers_major: 0,
        fpga_vers_minor: 9,
        wifi_status: 2,
        ip: "192.168.1.188",
        board: BOARD_TYPE,
        git_commit: "e1e27ab+",
        git_tag: "",
        git_branch: "master",
        time: now.getHours().toString() + ":"  + now.getMinutes().toString().padStart(2, "0"),
        smb_err: "Session setup failed with (0xc000006d) STATUS_LOGON_FAILURE",
        //"posix_err": "Failed to initialize the SD card",
        has_sd_card: true,
        //"frehd_loaded": "FREHD.ROM not found",

        keyboard_layout: settings.keyboard_layout,

        color: settings.color,
        printer_en: settings.printer_en ?? 1,
        tz: settings.tz,
        ssid: settings.ssid,
        passwd: settings.passwd,
        smb_url: settings.smb_url,
        smb_user: settings.smb_user,
        smb_passwd: settings.smb_passwd,
    };

    // Only present for TRS-IO++.
    if (BOARD_TYPE === BOARD_TYPE_TRS_IO_PP) {
        status.config = DIP_SWITCHES;
    }

    return status;
}

function makeGetRoms() {
    createStateDirsIfNecessary();
    const romPathnames = globSync(path.join(ROMS_DIR, "*"));
    const settings = readSettings();
    return {
        "roms": romPathnames.map(pathname => {
            const stats = fs.statSync(pathname);
            return {
                "filename": path.basename(pathname),
                "size": stats.size,
                "createdAt": stats.mtime.getTime()/1000,
            };
        }),
        "selected": settings.rom_assignments ?? DEFAULT_SETTINGS.rom_assignments,
    };
}

function makeGetFiles() {
    createStateDirsIfNecessary();
    const filesPathnames = globSync(path.join(FILES_DIR, "*"));
    const settings = readSettings();
    if (false) {
        // For testing.
        return {
            "error": "XFER/CMD is not running",
            "files": [],
        };
    }
    return {
        "files": filesPathnames.map(pathname => {
            const stats = fs.statSync(pathname);
            return {
                "filename": path.basename(pathname).replace(".", "/") + ":0",
                "size": stats.size,
                "createdAt": stats.mtime.getTime()/1000,
            };
        }),
    };
}

const PORT = 8080;
const app = express();

app.use('/', expressStaticGzip('../esp/html/built'));

// Automatically decode JSON on POST.
app.use(express.json());

app.get('/status', (req, res) => {
    res.setHeader('Content-Type', 'application/json');
    res.end(JSON.stringify(makeStatus()));
});
app.get('/roms', (req, res) => {
    res.setHeader('Content-Type', 'application/json');
    res.end(JSON.stringify(makeGetRoms()));
});
app.get('/files', (req, res) => {
    res.setHeader('Content-Type', 'application/json');
    res.end(JSON.stringify(makeGetFiles()));
});
app.post("/config", (req, res) => {
    const newSettings = { ...readSettings(), ...req.body };
    writeSettings(newSettings);
    res.setHeader('Content-Type', 'application/json');
    res.end(JSON.stringify(makeStatus()));
});
app.post("/roms", (req, res) => {
    const data = req.body;
    const command = data.command;

    switch (command) {
        case "deleteRom": {
            const filename = data.filename;
            if (!isValidRomFilename(filename)) {
                return sendJsonError(res, `Invalid filename "${filename}"`);
            }
            const pathname = path.join(ROMS_DIR, filename);
            if (!fs.existsSync(pathname)) {
                return sendJsonError(res, `ROM "${filename}" does not exist`);
            }
            fs.unlinkSync(pathname);
            break;
        }

        case "renameRom": {
            const oldFilename = data.oldFilename;
            const newFilename = data.newFilename;
            if (!isValidRomFilename(oldFilename)) {
                return sendJsonError(res, `Invalid filename "${oldFilename}"`);
            }
            if (!isValidRomFilename(newFilename)) {
                return sendJsonError(res, `Invalid filename "${newFilename}"`);
            }
            const oldPathname = path.join(ROMS_DIR, oldFilename);
            const newPathname = path.join(ROMS_DIR, newFilename);
            if (!fs.existsSync(oldPathname)) {
                return sendJsonError(res, `ROM "${oldFilename}" does not exist`);
            }
            if (fs.existsSync(newPathname)) {
                return sendJsonError(res, `ROM "${newFilename}" already exists`);
            }
            try {
                fs.renameSync(oldPathname, newPathname);
            } catch (error) {
                return sendJsonError(res, `Error renaming "${oldFilename}" to "${newFilename}": ${e.message}`);
            }
            // Update settings.
            const settings = readSettings();
            const romAssignments = settings.rom_assignments ?? DEFAULT_SETTINGS.rom_assignments;
            for (let i = 0; i < romAssignments.length; i++) {
                if (romAssignments[i] === oldFilename) {
                    romAssignments[i] = newFilename;
                }
            }
            settings.rom_assignments = romAssignments;
            writeSettings(settings);
            break;
        }

        case "uploadRom": {
            const filename = data.filename;
            const contents = Buffer.from(data.contents, 'base64');
            if (!isValidRomFilename(filename)) {
                return sendJsonError(res, `Invalid filename "${filename}"`);
            }
            const pathname = path.join(ROMS_DIR, filename);
            fs.writeFileSync(pathname, contents);
            break;
        }

        case "assignRom": {
            const model = data.model;
            const filename = data.filename;
            if (model < 0 || model > 4) {
                return sendJsonError(res, `Invalid model "${model}"`);
            }
            if (!isValidRomFilename(filename)) {
                return sendJsonError(res, `Invalid filename "${filename}"`);
            }
            const settings = readSettings();
            const romAssignments = settings.rom_assignments ?? DEFAULT_SETTINGS.rom_assignments;
            romAssignments[model] = filename;
            settings.rom_assignments = romAssignments;
            writeSettings(settings);
            break;
        }
    }

    res.setHeader('Content-Type', 'application/json');
    res.end(JSON.stringify(makeGetRoms()));
});
app.post("/files", (req, res) => {
    const data = req.body;
    const command = data.command;

    switch (command) {
        case "delete": {
            const filename = data.filename;
            if (!isValidFilename(filename)) {
                return sendJsonError(res, `Invalid filename "${filename}"`);
            }
            const pathname = path.join(FILES_DIR, filename);
            if (!fs.existsSync(pathname)) {
                return sendJsonError(res, `File "${filename}" does not exist`);
            }
            fs.unlinkSync(pathname);
            break;
        }

        case "rename": {
            const oldFilename = data.oldFilename;
            const newFilename = data.newFilename;
            if (!isValidFilename(oldFilename)) {
                return sendJsonError(res, `Invalid filename "${oldFilename}"`);
            }
            if (!isValidFilename(newFilename)) {
                return sendJsonError(res, `Invalid filename "${newFilename}"`);
            }
            const oldPathname = path.join(FILES_DIR, oldFilename);
            const newPathname = path.join(FILES_DIR, newFilename);
            if (!fs.existsSync(oldPathname)) {
                return sendJsonError(res, `File "${oldFilename}" does not exist`);
            }
            if (fs.existsSync(newPathname)) {
                return sendJsonError(res, `File "${newFilename}" already exists`);
            }
            try {
                fs.renameSync(oldPathname, newPathname);
            } catch (error) {
                return sendJsonError(res, `Error renaming "${oldFilename}" to "${newFilename}": ${e.message}`);
            }
            break;
        }

        case "upload": {
            const filename = data.filename;
            const contents = Buffer.from(data.contents, 'base64');
            if (!isValidFilename(filename)) {
                return sendJsonError(res, `Invalid filename "${filename}"`);
            }
            const pathname = path.join(FILES_DIR, filename);
            fs.writeFileSync(pathname, contents);
            break;
        }
    }

    res.setHeader('Content-Type', 'application/json');
    res.end(JSON.stringify(makeGetFiles()));
});

const httpServer = app.listen(PORT, () => {
  console.log(`Fake TRS-IO++ server listening on http://localhost:${PORT}/`);
});

const printerWs = new WebSocketServer({ noServer: true });
printerWs.on('connection', ws => {
    console.log("ws connection")
    ws.on('error', console.error);
    let i = 0;
    const sendByte = () => {
        const byte = PRINTER_OUTPUT.codePointAt(i);
        const byteArray = new Uint8Array([byte === 10 ? 13 : byte]);
        ws.send(byteArray);
        i += 1;
        if (i === PRINTER_OUTPUT.length) {
            i = 0;
        }
        setTimeout(sendByte, 20);
    };

    // sendByte();
});

httpServer.on('upgrade', function upgrade(request, socket, head) {
    console.log("upgrade", request.url);
    const { pathname: path } = new URL(request.url, "ws://host");
    console.log("pathname", path);

    if (path === '/printer') {
        printerWs.handleUpgrade(request, socket, head, ws => {
            printerWs.emit('connection', ws, request);
        });
    } else {
        console.log("Unknown WebSocket upgrade for path " + path);
        socket.destroy();
    }
});
