from http.server import HTTPServer, BaseHTTPRequestHandler

class H(BaseHTTPRequestHandler):
    def do_POST(self):
        l = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(l)
        print(f"[C2] beacon from {self.client_address}: {body.decode(errors='replace')}", flush=True)
        self.send_response(200)
        self.send_header("Content-Length", "2")
        self.end_headers()
        self.wfile.write(b"ok")

    def do_GET(self):
        print(f"[C2] GET {self.path} from {self.client_address}", flush=True)
        self.send_response(200)
        self.send_header("Content-Length", "4")
        self.end_headers()
        self.wfile.write(b"ping")

    def log_message(self, *a):
        pass

print("[C2] listener online on :9090", flush=True)
HTTPServer(("0.0.0.0", 9090), H).serve_forever()