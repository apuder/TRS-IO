
import fs from "fs";
import path from "path";
import { globSync } from 'glob'
import { createServer } from 'http';
import { WebSocketServer } from 'ws';
import express from 'express';

const STATE_DIR = "state";
const ROMS_DIR = "state/roms";
const DEFAULT_SETTINGS = {
    "color": 1,
    "tz": "GMT-8",
    "ssid": "fandango",
    "passwd": "22PineCreekSLO",
    "smb_url": "smb://unifi/TRS-IO/smb-m3",
    "smb_user": "lk",
    "smb_passwd": "XXX",
    "rom_assignments": ["", "", "", "", ""],
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
    const status = {
        "hardware_rev": 1,
        "vers_major": 2,
        "vers_minor": 0,
        "wifi_status": 2,
        "ip": "192.168.1.188",
        "config": 0, // only for TRS-IO++
        "time": now.getHours().toString() + ":"  + now.getMinutes().toString().padStart(2, "0"),
        "smb_err": "Session setup failed with (0xc000006d) STATUS_LOGON_FAILURE",
        //"posix_err": "Failed to initialize the SD card",
        "has_sd_card": true,
        //"frehd_loaded": "FREHD.ROM not found",

        "color": settings["color"],
        "tz": settings["tz"],
        "ssid": settings["ssid"],
        "passwd": settings["passwd"],
        "smb_url": settings["smb_url"],
        "smb_user": settings["smb_user"],
        "smb_passwd": settings["smb_passwd"]
    };

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

const PORT = 8080;
const app = express();

/*
const wsServer = new ws.Server({ noServer: true });

httpServer.on('upgrade', (req, socket, head) => {
    wsServer.handleUpgrade(req, socket, head, (ws) => {
        wsServer.emit('connection', ws, req);
    });
});

const wss1 = new WebSocketServer({ noServer: true });
const wss2 = new WebSocketServer({ noServer: true });

wss1.on('connection', function connection(ws) {
  ws.on('error', console.error);

  // ...
});

wss2.on('connection', function connection(ws) {
  ws.on('error', console.error);

  // ...
});

server.on('upgrade', function upgrade(request, socket, head) {
  const { pathname } = new URL(request.url, 'wss://base.url');

  if (pathname === '/foo') {
    wss1.handleUpgrade(request, socket, head, function done(ws) {
      wss1.emit('connection', ws, request);
    });
  } else if (pathname === '/bar') {
    wss2.handleUpgrade(request, socket, head, function done(ws) {
      wss2.emit('connection', ws, request);
    });
  } else {
    socket.destroy();
  }
});
*/

app.use('/', express.static('../esp/html'));

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

const httpServer = app.listen(PORT, () => {
  console.log(`Example app listening on http://localhost:${PORT}/`);
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

    sendByte();
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
