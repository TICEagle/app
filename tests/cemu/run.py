#!/usr/bin/env python3
import json
import os
import shutil
import struct
import subprocess
import sys
from pathlib import Path

from PIL import Image

import fixtures as f
import scenarios
import seed
from server import API_DIR, CEAGLE, FAULTS, SCRATCH, Api, start_proxy

ROOT = Path(__file__).resolve().parent
APP = ROOT.parents[1]
NET = CEAGLE.parent / "networking"
AUTOTESTER = NET / "CEmu-lwIP-CE/tests/autotester/autotester"
ROM = os.environ.get("AUTOTESTER_ROM", str(CEAGLE.parent / "ROM/rom_5.4.0034.rom"))
LWIP = NET / "lwip-ce/tests/common"
BUILD = ROOT / "build"
RESULTS = ROOT / "results"
ETH = "usb|eth,cemu0,dhcp=192.168.84.2/24,gw=192.168.84.1,dns=1.1.1.1"
SERVER = "http://192.168.84.1:8092"
IDLE_MARK = 0x07
LWIP_DONE_CRC = "F7C03D8D"
WAIT_MS = 300000


def crc32c(data):
    crc = 0xFFFFFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0x82F63B78 if crc & 1 else 0)
    return crc ^ 0xFFFFFFFF


def build_app():
    subprocess.run(["make", "-C", str(APP)], check=True, capture_output=True)
    r = subprocess.run(["make", "-C", str(APP), "CEAGLE_TEST=1", "OBJDIR=tests/cemu/build/obj", "BINDIR=tests/cemu/build/bin"],
                       capture_output=True, text=True)
    if r.returncode:
        sys.stderr.write(r.stdout[-3000:] + r.stderr[-3000:])
        raise SystemExit("build failed")
    settings = b"\xce" + SERVER.encode().ljust(64, b"\0") + b"\0"
    f.write(BUILD, "CEAGLECF.8xv", f.APPVAR, b"CEAGLECF", f.sized(settings), True)


def build_api():
    binary = SCRATCH / "ceagle-api"
    subprocess.run(["go", "build", "-o", str(binary), "./cmd/api"], cwd=API_DIR, check=True)
    return binary


def reset_database():
    compose = ["docker", "compose", "exec", "-T", "postgres"]
    subprocess.run(compose + ["dropdb", "-U", "ceagle", "--if-exists", "ceagle_app"], cwd=API_DIR, check=True)
    subprocess.run(compose + ["createdb", "-U", "ceagle", "ceagle_app"], cwd=API_DIR, check=True)
    shutil.rmtree(SCRATCH / "storage", ignore_errors=True)
    (SCRATCH / "api.log").unlink(missing_ok=True)


def config(scenario):
    files = [str(BUILD / "bin/CEAGLE.8xp"), str(LWIP / "clibs.8xg"), str(BUILD / "CEAGLECF.8xv")]
    files += [str(LWIP / n) for n in ["lwIPINST.8xp", "lwip.8xv", "lwIPCERT.8xv"]]
    files += [str(p) for p in sorted(LWIP.glob("LWIP.[0-9].8xv"))]
    extra = scenario.get("files", [])
    if isinstance(extra, str):
        extra = [p for p in sorted((RESULTS / extra).glob("*.8x*")) if p.stem != "CEATEST"]
    files += [str(p) for p in extra]
    hashes = {"lwip": {"description": "lwIP installed", "start": "vram_start", "size": "vram_16_size",
                       "expected_CRCs": [LWIP_DONE_CRC], "timeout": 150000},
              "idle": {"description": "CEagle waits for a key", "start": "0xD40000", "size": 1,
                       "expected_CRCs": ["%08X" % crc32c(bytes([IDLE_MARK]))], "timeout": WAIT_MS}}
    transfer_ms = 45000 + sum(Path(p).stat().st_size for p in files) // 8
    install = ["action|launch:INSTALL", "hashWait|lwip", "key|left", "key|enter", "delay|1000", "key|clear", "delay|500",
               "key|clear", "delay|500"]
    seq = [f"delay|{transfer_ms}", "key|clear", "delay|1000"] + install + install + [ETH, "delay|2000", "action|launch"]
    for line in scenario["steps"]:
        if line.startswith("shot|"):
            key = f"s{len(hashes)}"
            hashes[key] = {"description": line[5:], "start": "vram_start", "size": "vram_8_size",
                           "expected_CRCs": ["0"], "timeout": 0}
            seq.append(f"hash|{key}")
        else:
            seq.append(line)
    seq += ["delay|3000"] + [f"saveVar|{v}" for v in scenario.get("save", []) + ["CEAGLEDB", "CEATEST"]]
    return {"description": scenario["name"], "transfer_files": files, "target": {"name": "CEAGLE", "isASM": False},
            "sequence": seq, "hashes": hashes}


def run(scenario):
    out = RESULTS / scenario["name"]
    shutil.rmtree(out, ignore_errors=True)
    out.mkdir(parents=True)
    (out / "autotest.json").write_text(json.dumps(config(scenario), indent=2))
    FAULTS.clear()
    FAULTS.update(scenario.get("faults", {}))
    env = dict(os.environ, AUTOTESTER_ROM=ROM)
    r = subprocess.run([str(AUTOTESTER), "-d", "-s", str(out / "autotest.json")], cwd=out, env=env, capture_output=True,
                       text=True, stdin=subprocess.DEVNULL)
    (out / "autotester.log").write_text(r.stdout + r.stderr)
    for raw in out.glob("*.rgba"):
        Image.frombytes("RGB", (320, 240), raw.read_bytes(), "raw", "BGRX").save(raw.with_suffix(".png"))
        raw.unlink()
    for png in out.glob("CEagle waits*.png"):
        png.unlink()
    problems = []
    failures = [l.strip() for l in (r.stdout + r.stderr).splitlines() if "Test failed" in l]
    problems += [l for l in failures if "Hash #idle" in l]
    lwip = [l for l in failures if "Hash #lwip" in l]
    if len(lwip) == 2:
        problems += lwip
    problems += scenario["check"](Checker(out))
    return problems


class Checker:
    def __init__(self, out):
        self.out = out

    def saved(self, name):
        stem = name.replace("\u029f", "")
        for path in self.out.glob("*.8*"):
            if path.stem.replace("\u029f", "") in (stem, stem[:-1]):
                return path
        return None

    def raw(self, name):
        path = self.saved(name)
        return f.raw_of(path) if path else None

    def same(self, name, fixture):
        got = self.raw(name)
        want = f.raw_of(fixture)
        if not got:
            return [f"{name} is missing"]
        return [] if got[0] == want[0] and got[2] == want[2] else [f"{name} differs from {Path(fixture).name}"]

    def archived(self, name, want):
        got = self.raw(name)
        return [] if got and got[3] == want else [f"{name} should be {'archived' if want else 'in RAM'}"]

    def missing(self, name):
        return [] if not self.saved(name) else [f"{name} should not exist"]

    def db(self):
        got = self.raw("CEAGLEDB")
        if not got:
            return {}
        data = got[2][2:]
        if data[:5] != b"CEDB\x01":
            raise AssertionError("CEAGLEDB has a bad header")
        projects, files = data[5], struct.unpack_from("<H", data, 6)[0]
        out, at = {}, 8
        order = []
        for _ in range(projects):
            slug = data[at:at + 33].split(b"\0")[0].decode()
            version = data[at + 33:at + 58].split(b"\0")[0].decode()
            flags, needs = data[at + 58], int.from_bytes(data[at + 59:at + 68], "little")
            out[slug] = {"version": version, "auto": bool(flags & 1), "needs": needs, "files": {}}
            order.append(slug)
            at += 68
        for _ in range(files):
            project, var_type = data[at], data[at + 1]
            name = data[at + 2:at + 10].rstrip(b"\0")
            out[order[project]]["files"][name] = (var_type, data[at + 10:at + 42].hex())
            at += 42
        for slug in out:
            out[slug]["needs"] = [order[i] for i in range(len(order)) if out[slug]["needs"] >> i & 1]
        return out


def main():
    names = sys.argv[1:]
    build_app()
    binary = build_api()
    if not os.environ.get("KEEP_DB"):
        reset_database()
    api = Api(binary)
    proxy = start_proxy()
    fx = seed.fixtures()
    failed = []
    try:
        seed.base(api, fx, binary)
        for scenario in scenarios.all(fx, RESULTS, BUILD):
            if names and scenario["name"] not in names:
                continue
            if "stage" in scenario:
                scenario["stage"](api, fx, binary)
                continue
            problems = run(scenario)
            for p in problems:
                print(f"FAIL {scenario['name']}: {p}")
            print(f"{'FAIL' if problems else 'PASS'} {scenario['name']}", flush=True)
            if problems:
                failed.append(scenario["name"])
    finally:
        proxy.shutdown()
        api.stop()
    print(f"{len(failed)} failed: {' '.join(failed)}" if failed else "all scenarios passed")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
