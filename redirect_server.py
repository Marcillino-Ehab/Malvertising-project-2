#!/usr/bin/env python3
"""Real HTTP 302 redirect server for malvertising lab hops (LAB ONLY).
Usage: python redirect_server.py <hop1|hop2|hop3>
Runs entirely on localhost — no configuration needed.
"""
import sys
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs

BASE = "http://localhost"

HOP_CONFIG = {
    "hop1": {  # TDS gate — referer filtering
        "port": 8081,
        "routes": {
            "/gate": ("gate", f"{BASE}:8082/track"),
        },
    },
    "hop2": {  # tracking — log victim + 302
        "port": 8082,
        "routes": {
            "/track": ("redirect", f"{BASE}:8083/scan"),
        },
    },
    "hop3": {  # final hop — serve scareware scan page
        "port": 8083,
        "routes": {
            "/scan": ("serve", "/srv/scan.html"),
        },
    },
}

DECOY_BODY = (b"<html><body><h1>Recipe of the day: Apple Pie</h1>"
              b"<p>Classic apple pie with cinnamon and a flaky crust.</p>"
              b"</body></html>")

class RedirectHandler(BaseHTTPRequestHandler):
    hop_name = None
    cfg = None

    def do_GET(self):
        parsed = urlparse(self.path)
        path = parsed.path.rstrip("/") or "/"
        qs = parse_qs(parsed.query)

        # decoy page (served to "analysts" blocked by the gate)
        if path == "/decoy":
            self.send_response(200)
            self.send_header("Content-Type", "text/html")
            self.send_header("Content-Length", str(len(DECOY_BODY)))
            self.end_headers()
            self.wfile.write(DECOY_BODY)
            print(f"[{self.hop_name}] serving DECOY (apple pie) → 200", flush=True)
            return

        route = self.cfg["routes"].get(path)
        if not route:
            self.send_error(404)
            return

        rtype, target = route

        if rtype == "gate":
            referer = self.headers.get("Referer", "")
            if "localhost" in referer or "publisher" in referer or "ads" in referer:
                print(f"[{self.hop_name}] gate PASSED (referer={referer}) → 302 {target}", flush=True)
                self.send_response(302)
                self.send_header("Location", target)
                self.end_headers()
            else:
                print(f"[{self.hop_name}] gate BLOCKED (no/suspicious referer) → 302 /decoy", flush=True)
                self.send_response(302)
                self.send_header("Location", "/decoy")
                self.end_headers()

        elif rtype == "redirect":
            cid = qs.get("c", ["?"])[0]
            camp = qs.get("camp", ["-"])[0]
            print(f"[{self.hop_name}] victim tracked cid={cid} camp={camp} "
                  f"from {self.client_address} → 302 {target}", flush=True)
            self.send_response(302)
            self.send_header("Location", target)
            self.end_headers()

        elif rtype == "serve":
            try:
                with open(target, "rb") as f:
                    body = f.read()
                print(f"[{self.hop_name}] serving {target} → 200", flush=True)
                self.send_response(200)
                self.send_header("Content-Type", "text/html")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
            except FileNotFoundError:
                self.send_error(404)

    def log_message(self, *a):
        pass

if __name__ == "__main__":
    hop = sys.argv[1]
    cfg = HOP_CONFIG[hop]
    RedirectHandler.hop_name = hop
    RedirectHandler.cfg = cfg
    print(f"[{hop}] HTTP redirect server on :{cfg['port']}", flush=True)
    HTTPServer(("0.0.0.0", cfg["port"]), RedirectHandler).serve_forever()