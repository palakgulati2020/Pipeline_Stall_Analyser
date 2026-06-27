#!/usr/bin/env python3
"""
Pipeline Stall Visualizer — Auto-serving server
Usage:  python3 serve.py [--port 8765] [--dir .]

Opens browser automatically. Watches results.json & without_forwarding.json
in the same directory. Browser polls every 1s and re-renders when files change.
"""

import http.server, json, os, sys, threading, time, webbrowser
from pathlib import Path

PORT     = 8765
WATCH    = ["results.json", "without_forwarding.json"]
SERVE_DIR = Path(__file__).parent.resolve()

# ── track mtimes ──────────────────────────────────────────────────────────────
_mtimes = {}

def get_mtime(name):
    p = SERVE_DIR / name
    return p.stat().st_mtime if p.exists() else 0

def get_status():
    status = {}
    for name in WATCH:
        p = SERVE_DIR / name
        mt = get_mtime(name)
        status[name] = {"exists": p.exists(), "mtime": mt}
    return status

# ── read JSON safely ──────────────────────────────────────────────────────────
def read_json(name):
    p = SERVE_DIR / name
    if not p.exists():
        return None
    try:
        with open(p) as f:
            return json.load(f)
    except Exception:
        return None

# ── HTTP handler ──────────────────────────────────────────────────────────────
class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **kw):
        super().__init__(*a, directory=str(SERVE_DIR), **kw)

    def log_message(self, fmt, *args):
        pass  # silent

    def do_GET(self):
        # ── /api/data  →  combined JSON payload ──────────────────────────────
        if self.path == "/api/data":
            with_data    = read_json("results.json")
            without_data = read_json("without_forwarding.json")
            status       = get_status()
            payload = json.dumps({
                "with":    with_data,
                "without": without_data,
                "status":  status,
                "ts":      time.time()
            }).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(payload)))
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(payload)
            return

        # ── /api/status  →  lightweight mtime check ──────────────────────────
        if self.path == "/api/status":
            payload = json.dumps(get_status()).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(payload)))
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(payload)
            return

        # ── / or /index.html → serve the visualizer ──────────────────────────
        if self.path in ("/", "/index.html"):
            self.path = "/visualizer.html"

        super().do_GET()

# ── main ──────────────────────────────────────────────────────────────────────
if __name__ == "__main__":
    import argparse
    p = argparse.ArgumentParser()
    p.add_argument("--port", type=int, default=PORT)
    p.add_argument("--dir",  default=str(SERVE_DIR))
    args = p.parse_args()
    SERVE_DIR = Path(args.dir).resolve()
    PORT = args.port

    server = http.server.ThreadingHTTPServer(("", PORT), Handler)
    print(f"\n  Pipeline Stall Visualizer")
    print(f"  ─────────────────────────")
    print(f"  Server  : http://localhost:{PORT}")
    print(f"  Watching: {SERVE_DIR}")
    for w in WATCH:
        p2 = SERVE_DIR / w
        state = "✓ found" if p2.exists() else "⏳ waiting"
        print(f"    {w:35s} {state}")
    print(f"\n  Run PIN tool, then refresh your browser automatically!\n")

    # Open browser after short delay
    def open_browser():
        time.sleep(0.8)
        webbrowser.open(f"http://localhost:{PORT}")
    threading.Thread(target=open_browser, daemon=True).start()

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n  Server stopped.")
