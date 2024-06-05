# Simulates the TRS-IO++, for easier development.

from http.server import HTTPServer, BaseHTTPRequestHandler
import os, json, time, glob

EXT_TO_MINE_TYPE = {
        ".html": "text/html",
        ".css": "text/css",
        ".ttf": "font/ttf",
        ".js": "text/javascript",
}

STATE_DIR = "state"
ROMS_DIR = "state/roms"
DEFAULT_SETTINGS = {
    "color": 1,
    "tz": "GMT-8",
    "ssid": "fandango",
    "passwd": "22PineCreekSLO",
    "smb_url": "smb://unifi/TRS-IO/smb-m3",
    "smb_user": "lk",
    "smb_passwd": "XXX"
}

def createStateDirsIfNecessary():
    global STATE_DIR
    global ROMS_DIR

    os.makedirs(STATE_DIR, exist_ok=True)
    os.makedirs(ROMS_DIR, exist_ok=True)

def getSettingsPathname():
    return os.path.join(STATE_DIR, "settings.json")

def readSettings():
    path = getSettingsPathname()
    if os.path.exists(path):
        settings = json.load(open(path))
    else:
        settings = DEFAULT_SETTINGS

    return settings

def writeSettings(settings):
    createStateDirsIfNecessary()
    path = getSettingsPathname()
    f = open(path, "w")
    json.dump(settings, f, indent="    ")
    f.close()

def makeStatus():
    settings = readSettings()

    t = time.localtime()
    status = {
        "hardware_rev": 1,
        "vers_major": 2,
        "vers_minor": 0,
        "wifi_status": 2,
        "ip": "192.168.1.188",
        "config": 0, # only for TRS-IO++
        "time": "%d:%02d" % (t.tm_hour, t.tm_min),
        "smb_err": "Session setup failed with (0xc000006d) STATUS_LOGON_FAILURE",
        "posix_err": "Failed to initialize the SD card",
        "has_sd_card": True,
        "frehd_loaded": "FREHD.ROM not found",

        "color": settings["color"],
        "tz": settings["tz"],
        "ssid": settings["ssid"],
        "passwd": settings["passwd"],
        "smb_url": settings["smb_url"],
        "smb_user": settings["smb_user"],
        "smb_passwd": settings["smb_passwd"]
    }

    return status

def makeRomInfo():
    createStateDirsIfNecessary()
    romPathnames = glob.glob(os.path.join(ROMS_DIR, "*"))
    data = {
        "roms": [],
        "selected": [],
    }
    for romPathname in romPathnames:
        data["roms"].append({
            "filename": os.path.basename(romPathname),
            "size": os.path.getsize(romPathname),
            "createdAt": int(os.path.getmtime(romPathname)),
        })
    data["selected"] = [
        data["roms"][4]["filename"],
        data["roms"][0]["filename"],
        data["roms"][6]["filename"],
        data["roms"][0]["filename"],
        data["roms"][0]["filename"],
    ]
    return data

# Make sure the filename isn't trying to reach outside the ROM directory.
def isValidRomFilename(filename):
    return filename != "" and not filename.startswith(".") and "/" not in filename

class TrsIoRequestHandler(BaseHTTPRequestHandler):
    # Handler for GET requests.
    def do_GET(self):
        # Procedural paths.
        if self.path == "/get-roms":
            return self.handle_get_roms()
        if self.path == "/status":
            return self.handle_status()

        # Redirects.
        if self.path in ["/printer", "/roms"]:
            self.send_response(307)
            self.send_header("Location", self.path + "/")
            self.end_headers()
            return

        # Static paths.
        if self.path.startswith("/printer/"):
            return self.serve_static_files(self.path[9:], "../esp/html/printer")
        if self.path.startswith("/roms/"):
            return self.serve_static_files(self.path[6:], "../esp/elFinder/roms")
        return self.serve_static_files(self.path[1:], "../esp/html")

    # Handler for POST requests.
    def do_POST(self):
        if self.path == "/get-roms":
            return self.handle_roms_command()
        elif self.path == "/config":
            return self.handle_config()
        self.send_error(404)

    # Serve static files from a local directory.
    def serve_static_files(self, path, directory):
        while path.startswith("/"):
            path = path[1:]
        if path == "":
            path = "index.html"
        path = os.path.join(directory, path)
        # print(path)

        try:
            contents = open(path, "rb").read()
        except Exception as e:
            # print(e)
            self.send_error(404)
            return

        file_extension = os.path.splitext(path)[1].lower()
        mime_type = EXT_TO_MINE_TYPE.get(file_extension, "application/octet-stream")

        self.send_response(200)
        self.send_header("Content-type", mime_type)
        self.send_header("Content-Length", str(len(contents)))
        self.end_headers()
        self.wfile.write(contents)

    def handle_get_roms(self):
        self.send_json(makeRomInfo())

    def handle_config(self):
        request = self.read_json();
        writeSettings(request)
        self.send_json(makeStatus())

    def handle_roms_command(self):
        request = self.read_json();
        command = request["command"]
        if command == "deleteRom":
            filename = request["filename"]
            if not isValidRomFilename(filename):
                return self.send_json_error(f"Invalid filename \"{filename}\"")
            pathname = os.path.join(ROMS_DIR, filename)
            if not os.path.exists(pathname):
                return self.send_json_error(f"ROM \"{filename}\" does not exist")
            os.remove(pathname)
        elif command == "renameRom":
            oldFilename = request["oldFilename"]
            newFilename = request["newFilename"]
            if not isValidRomFilename(oldFilename):
                return self.send_json_error(f"Invalid filename \"{oldFilename}\"")
            if not isValidRomFilename(newFilename):
                return self.send_json_error(f"Invalid filename \"{newFilename}\"")
            oldPathname = os.path.join(ROMS_DIR, oldFilename)
            newPathname = os.path.join(ROMS_DIR, newFilename)
            if not os.path.exists(oldPathname):
                return self.send_json_error(f"ROM \"{oldFilename}\" does not exist")
            if os.path.exists(newPathname):
                return self.send_json_error(f"ROM \"{newFilename}\" already exists")
            try:
                os.rename(oldPathname, newPathname)
            except OSError as e:
                return self.send_json_error(f"Error renaming \"{oldFilename}\" to \"{newFilename}\": {e}")
        else:
            return self.send_error(400)
        self.send_json(makeRomInfo())

    def handle_status(self):
        self.send_json(makeStatus())

    def read_json(self):
        length = int(self.headers["content-length"])
        return json.loads(self.rfile.read(length))

    def send_json(self, data):
        self.send_response(200)
        self.send_header("Content-type", "application/json")
        self.end_headers()
        self.wfile.write(bytes(json.dumps(data), "utf-8"))

    def send_json_error(self, message):
        print("Responding with error message: %s" % message)
        self.send_json({
            "error": message,
        })

def main():
    hostname = "localhost"
    port = 8134
    httpd = HTTPServer((hostname, port), TrsIoRequestHandler)
    print(f"Serving at http://{hostname}:{port}/")

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    httpd.server_close()
    print("\n")

main()

