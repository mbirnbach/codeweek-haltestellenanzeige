#!/usr/bin/env python3
"""Runs the setup portal page on this computer, for testing the page itself.

Serves the exact HTML from src/config/PortalPage.h and imitates the ESP32's
/config/* routes. Stop search is forwarded to the live API with the token
from include/secrets.h (like the ESP does); without a token, the captured
fixture is used. Wi-Fi is simulated: password "falsch123" fails, anything
else connects.

    python3 tools/portal_mock.py [port]     then open http://localhost:8765
"""

import json
import pathlib
import re
import sys
import time
import urllib.parse
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = pathlib.Path(__file__).resolve().parent.parent
PAGE = re.search(r'R"HTML\((.*)\)HTML"', (ROOT / "src/config/PortalPage.h").read_text(), re.S).group(1)


def read_secrets():
    path = ROOT / "include/secrets.h"
    if not path.exists():
        return None, None
    text = path.read_text()
    base = re.search(r'#define API_BASE_URL "(.*)"', text)
    token = re.search(r'#define API_TOKEN "(.*)"', text)
    if not base or not token or token.group(1) == "replace-me":
        return None, None
    return base.group(1), token.group(1)


API_BASE, API_TOKEN = read_secrets()
STATE = {"ssid": "", "password": "", "wifi": "idle", "since": 0.0, "saved": None, "scan_started": 0.0}


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        sys.stderr.write("[mock] " + (fmt % args) + "\n")

    def send_json(self, status, obj):
        body = json.dumps(obj, ensure_ascii=False).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def wifi_status(self):
        if STATE["wifi"] == "connecting" and time.time() - STATE["since"] > 2:
            STATE["wifi"] = "failed" if STATE["password"] == "falsch123" else "connected"
        return STATE["wifi"]

    def do_GET(self):
        url = urllib.parse.urlparse(self.path)
        q = urllib.parse.parse_qs(url.query)
        if url.path == "/":
            body = PAGE.encode()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers()
            self.wfile.write(body)
        elif url.path == "/config/state":
            self.send_json(200, {"ap": "Abfahrt-Setup-MOCK", "version": "mock", "ssid": STATE["ssid"],
                                 "hasPassword": bool(STATE["password"]), "stopName": "",
                                 "wifi": self.wifi_status()})
        elif url.path == "/config/wifi/scan":
            if not STATE["scan_started"]:
                STATE["scan_started"] = time.time()
            if time.time() - STATE["scan_started"] < 2:
                self.send_json(200, {"scanning": True, "networks": []})
            else:
                self.send_json(200, {"scanning": False, "networks": [
                    {"ssid": "Schule-Gast", "quality": 90, "secure": True},
                    {"ssid": "FabLab", "quality": 64, "secure": True},
                    {"ssid": "Café Offen", "quality": 30, "secure": False}]})
        elif url.path == "/config/wifi/status":
            status = self.wifi_status()
            obj = {"status": status, "ssid": STATE["ssid"]}
            if status == "failed":
                obj["message"] = "Verbindung fehlgeschlagen. Stimmen WLAN-Name und Passwort?"
            self.send_json(200, obj)
        elif url.path == "/config/stops":
            if self.wifi_status() != "connected":
                self.send_json(409, {"error": "Erst mit dem WLAN verbinden (Schritt 1)."})
                return
            query = (q.get("q") or [""])[0].strip()
            if not 2 <= len(query) <= 64:
                self.send_json(400, {"error": "Bitte 2 bis 64 Zeichen eingeben."})
                return
            if API_TOKEN:
                req = urllib.request.Request(API_BASE + "/v1/stops?query=" + urllib.parse.quote(query),
                                             headers={"Authorization": "Bearer " + API_TOKEN})
                with urllib.request.urlopen(req, timeout=15) as r:
                    stops = json.load(r)
            else:
                stops = json.loads((ROOT / "test/fixtures/stops_koenigsplatz.json").read_text())
            # Like the ESP: only id + name reach the browser.
            self.send_json(200, [{"id": s["id"], "name": s["name"]} for s in stops[:15]])
        else:
            self.send_response(302)
            self.send_header("Location", "/")
            self.end_headers()

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        form = {k: v[0] for k, v in urllib.parse.parse_qs(self.rfile.read(length).decode(),
                                                          keep_blank_values=True).items()}
        if self.path == "/config/wifi":
            pw = form.get("password", "")
            if pw and not 8 <= len(pw) <= 63:
                self.send_json(400, {"error": "Das WLAN-Passwort muss 8 bis 63 Zeichen lang sein."})
                return
            STATE.update(ssid=form.get("ssid", ""), password=pw, wifi="connecting", since=time.time())
            self.send_json(200, {"status": "connecting"})
        elif self.path == "/config/save":
            if self.wifi_status() != "connected":
                self.send_json(409, {"error": "Erst mit dem WLAN verbinden (Schritt 1)."})
                return
            STATE["saved"] = {"ssid": STATE["ssid"], "stop_id": form.get("stop_id"),
                              "stop_name": form.get("stop_name")}
            print("[mock] SAVED", json.dumps(STATE["saved"], ensure_ascii=False), flush=True)
            self.send_json(200, {"status": "saved"})
        else:
            self.send_json(404, {"error": "not found"})


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8765
    print(f"[mock] http://localhost:{port}  (stop search: {'live API' if API_TOKEN else 'fixture'})", flush=True)
    ThreadingHTTPServer(("127.0.0.1", port), Handler).serve_forever()
