import http.client
import json
import os
import re
import subprocess
import threading
import time
import uuid
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit

CEAGLE = Path(__file__).resolve().parents[3]
API_DIR = CEAGLE / "api"
SCRATCH = Path(os.environ.get("CEAGLE_TEST_DIR", "/tmp/ceagle-app-test"))
API_ADDRESS = ("127.0.0.1", 8094)
PROXY_ADDRESS = ("192.168.84.1", 8092)
DATABASE = "postgres://ceagle:ceagle@localhost:5434/ceagle_app?sslmode=disable"
GARAGE_KEY = re.compile(r"GARAGE_DEFAULT_(ACCESS|SECRET)_KEY: (\S+)")


def r2_settings():
    keys = dict(GARAGE_KEY.findall((API_DIR / "compose.yml").read_text()))
    return {"R2_ENDPOINT": "http://localhost:3900", "R2_ACCESS_KEY_ID": keys["ACCESS"],
            "R2_SECRET_ACCESS_KEY": keys["SECRET"], "R2_REGION": "garage", "R2_BUCKET": "ceagle"}


FAULTS = {}
TURNSTILE_TOKEN = "XXXX.DUMMY.TOKEN.XXXX"


def environment():
    env = dict(os.environ, DATABASE_URL=DATABASE, BASE_URL="http://localhost:5173", ENV="development",
               ADDRESS="%s:%d" % API_ADDRESS, STORAGE_DIR=str(SCRATCH / "storage"), TRUST_PROXY="1",
               SCANNER_PATH=str(CEAGLE / "scanner/target/release/ceagle-scanner"), **r2_settings())
    return env


class Api:
    def __init__(self, binary):
        SCRATCH.mkdir(parents=True, exist_ok=True)
        self.log_path = SCRATCH / "api.log"
        self.log = open(self.log_path, "a")
        self.process = subprocess.Popen([str(binary)], env=environment(), stdout=self.log, stderr=subprocess.STDOUT,
                                        stdin=subprocess.DEVNULL)
        for _ in range(100):
            try:
                self.request("GET", "/api/v1/projects")
                return
            except OSError:
                time.sleep(0.2)
        raise SystemExit(f"the API did not start, see {self.log_path}")

    def stop(self):
        self.process.terminate()
        self.process.wait()
        self.log.close()

    def admin(self, binary, *args):
        subprocess.run([str(binary), "admin", *args], env=environment(), check=True, capture_output=True)

    def request(self, method, path, body=None, headers=None, cookie=None, address=None):
        connection = http.client.HTTPConnection(*API_ADDRESS, timeout=60)
        headers = dict(headers or {})
        headers["X-Forwarded-For"] = address or "10.0.0.1"
        headers["X-Turnstile-Token"] = TURNSTILE_TOKEN
        if cookie:
            headers["Cookie"] = cookie
        if isinstance(body, dict):
            body = json.dumps(body).encode()
            headers["Content-Type"] = "application/json"
        connection.request(method, path, body=body, headers=headers)
        response = connection.getresponse()
        data = response.read()
        cookies = [v.split(";")[0] for k, v in response.getheaders() if k.lower() == "set-cookie"]
        connection.close()
        return response.status, data, cookies

    def json(self, method, path, body=None, cookie=None, address=None, ok=(200, 201, 202, 204)):
        status, data, cookies = self.request(method, path, body, cookie=cookie, address=address)
        if status not in ok:
            raise RuntimeError(f"{method} {path}: {status} {data[:300]!r}")
        return (json.loads(data) if data else None), cookies

    def user(self, name, number):
        address = f"10.1.{number}.1"
        email = f"{name}@example.test"
        login = {"login": name, "password": "correct horse battery"}
        status, _, _ = self.request("POST", "/api/v1/auth/register",
                                    {"email": email, "username": name, "password": login["password"]}, address=address)
        if status not in (201, 204, 200, 409):
            raise RuntimeError(f"register {name}: {status}")
        _, cookies = self.json("POST", "/api/v1/auth/login", login, address=address)
        cookie = "; ".join(cookies)
        me, _ = self.json("GET", "/api/v1/me", cookie=cookie)
        if not me.get("email_verified_at"):
            token = None
            for _ in range(600):
                found = re.findall(re.escape(email) + r".*?verify-email\?token=([A-Za-z0-9]+)", self.log_path.read_text())
                if found:
                    token = found[-1]
                    break
                time.sleep(0.2)
            if not token:
                raise RuntimeError(f"no verification email for {email}")
            self.json("POST", "/api/v1/auth/verify-email", {"token": token}, cookie=cookie)
        return cookie

    def project(self, cookie, slug, kind, summary, parent=""):
        status, data, _ = self.request("GET", f"/api/v1/projects/{slug}")
        if status != 404:
            return
        body = {"slug": slug, "name": slug.replace("-", " ").title(), "kind": kind, "category": "other", "summary": summary,
                "description": "", "license": "MIT", "source_url": "", "parent": parent}
        self.json("POST", "/api/v1/projects", body, cookie=cookie, ok=(201, 409))

    def publish(self, cookie, slug, version, files, dependencies=None):
        status, data, _ = self.request("GET", f"/api/v1/projects/{slug}/releases/{version}", cookie=cookie)
        if status in (404, 410) and self.request("GET", f"/api/v1/projects/{slug}")[0] == 404:
            return None
        if status != 200:
            boundary = uuid.uuid4().hex
            manifest = {"version": version, "notes": "", "keep": [],
                        "files": [{"filename": p.name, "role": role, "description": ""} for p, role in files],
                        "dependencies": dependencies or {}}
            parts = [f"--{boundary}\r\nContent-Disposition: form-data; name=\"release\"\r\n\r\n".encode() +
                     json.dumps(manifest).encode() + b"\r\n"]
            for path, _ in files:
                parts.append(f"--{boundary}\r\nContent-Disposition: form-data; name=\"file\"; filename=\"{path.name}\"\r\n"
                             "Content-Type: application/octet-stream\r\n\r\n".encode() + path.read_bytes() + b"\r\n")
            body = b"".join(parts) + f"--{boundary}--\r\n".encode()
            status, data, _ = self.request("POST", f"/api/v1/projects/{slug}/releases", body,
                                           {"Content-Type": f"multipart/form-data; boundary={boundary}"}, cookie)
            if status != 202:
                raise RuntimeError(f"publish {slug} {version}: {status} {data[:400]!r}")
        for _ in range(300):
            release, _ = self.json("GET", f"/api/v1/projects/{slug}/releases/{version}", cookie=cookie)
            if release["status"] == "published":
                return release
            if release["status"] in ("held", "rejected"):
                raise RuntimeError(f"{slug} {version} is {release['status']}: {release.get('reason')}")
            time.sleep(0.2)
        raise RuntimeError(f"{slug} {version} stayed processing")


class Proxy(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"

    def log_message(self, *args):
        pass

    def do_GET(self):
        upstream = http.client.HTTPConnection(*API_ADDRESS, timeout=60)
        upstream.request("GET", self.path, headers={"X-Forwarded-For": self.client_address[0]})
        response = upstream.getresponse()
        data = response.read()
        upstream.close()
        fault = next((f for p, f in FAULTS.items() if urlsplit(self.path).path.endswith(p)), None)
        if fault == "corrupt" and response.status == 200:
            data = data[:-1] + bytes([data[-1] ^ 0xFF])
        self.send_response(response.status)
        for key in ("Content-Type",):
            if response.getheader(key):
                self.send_header(key, response.getheader(key))
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        if fault == "truncate" and response.status == 200:
            self.wfile.write(data[:len(data) // 2])
            self.wfile.flush()
            self.close_connection = True
            return
        self.wfile.write(data)


def start_proxy():
    server = ThreadingHTTPServer(PROXY_ADDRESS, Proxy)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server
