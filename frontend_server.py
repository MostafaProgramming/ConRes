from __future__ import annotations

import json
from http import HTTPStatus
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse
from datetime import datetime


ROOT = Path(__file__).resolve().parent
FRONTEND_DIR = ROOT / "frontend"
PRODUCT_FILE = ROOT / "ProductSpecification.txt"


def ensure_product_file() -> None:
    """Keep the manual frontend usable even before the first simulated write occurs."""
    if not PRODUCT_FILE.exists():
        PRODUCT_FILE.write_text("Initial Product Specification\n", encoding="utf-8")


class ConResHandler(SimpleHTTPRequestHandler):
    """Serve the static dashboard and a tiny JSON API for the shared file view."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(FRONTEND_DIR), **kwargs)

    def do_GET(self) -> None:
        """Route API reads to the shared-file handler and everything else to static assets."""
        parsed = urlparse(self.path)
        if parsed.path == "/api/shared-file":
            self.handle_get_shared_file()
            return
        super().do_GET()

    def do_POST(self) -> None:
        """Accept manual write requests from the dashboard when a writer is active."""
        parsed = urlparse(self.path)
        if parsed.path == "/api/shared-file":
            self.handle_post_shared_file()
            return
        self.send_error(HTTPStatus.NOT_FOUND, "Endpoint not found")

    def handle_get_shared_file(self) -> None:
        """Return the current shared-file contents and a little file metadata for the UI."""
        ensure_product_file()
        content = PRODUCT_FILE.read_text(encoding="utf-8")
        stats = PRODUCT_FILE.stat()
        payload = {
            "path": PRODUCT_FILE.name,
            "content": content,
            "size": stats.st_size,
            "updatedAt": datetime.fromtimestamp(stats.st_mtime).strftime("%Y-%m-%d %H:%M:%S"),
        }
        self.send_json(payload)

    def handle_post_shared_file(self) -> None:
        """Append a clearly labelled manual update so the demo leaves visible evidence."""
        ensure_product_file()
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length).decode("utf-8") if length else "{}"
        try:
            payload = json.loads(body)
        except json.JSONDecodeError:
            self.send_json({"error": "Invalid JSON body"}, status=HTTPStatus.BAD_REQUEST)
            return

        user_id = payload.get("userId")
        username = payload.get("username", "unknown_user")
        content = str(payload.get("content", "")).strip()
        if not content:
            self.send_json({"error": "Write content cannot be empty"}, status=HTTPStatus.BAD_REQUEST)
            return

        with PRODUCT_FILE.open("a", encoding="utf-8") as out:
            out.write(f"[Manual update by {username} (User #{user_id})]\n")
            out.write(content)
            out.write("\n\n")

        self.handle_get_shared_file()

    def send_json(self, payload: dict, status: HTTPStatus = HTTPStatus.OK) -> None:
        """Serialize JSON responses consistently and disable caching for live demos."""
        encoded = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(encoded)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(encoded)


def main() -> None:
    """Expose the dashboard and local API from a single development server."""
    ensure_product_file()
    server = ThreadingHTTPServer(("127.0.0.1", 8000), ConResHandler)
    print("Serving ConRes frontend and shared-file API at http://localhost:8000")
    server.serve_forever()


if __name__ == "__main__":
    main()
