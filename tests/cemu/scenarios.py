import hashlib

import fixtures as f
import seed

LETTER_KEYS = dict(zip("abcdefghijklmnopqrstuvwxyz ", ["math", "apps", "prgm", "inv", "sin", "cos", "tan", "pow", "sq",
                                                       "comma", "lpar", "rpar", "div", "log", "7", "8", "9", "mul",
                                                       "ln", "4", "5", "6", "sub", "sto", "1", "2", "0"]))


def wait():
    return ["delay|700", "hashWait|idle"]


def press(*keys):
    out = []
    for key in keys:
        out += [f"key|{key}"] + wait()
    return out


def shot(name):
    return [f"shot|{name}"]


def type_text(text):
    out, letters = [], True
    for c in text:
        digit = c.isdigit() or c in "./-"
        if digit == letters:
            out.append("key|alpha")
            letters = not letters
        key = {".": "dot", "/": "div", "-": "sub"}.get(c, c) if digit else LETTER_KEYS[c]
        out += [f"key|{key}", "delay|150"]
    return out


def search(text):
    return ["key|y="] + wait() + type_text(text) + press("enter")


def open_project(text):
    return search(text) + press("enter")


def quit_app():
    return ["key|clear", "delay|300", "key|clear", "delay|300", "key|clear", "delay|2000"]


def db_has(db, slug, version, auto=None, files=None):
    p = db.get(slug)
    if not p:
        return [f"CEAGLEDB has no {slug}"]
    problems = []
    if p["version"] != version:
        problems.append(f"{slug} is {p['version']} in CEAGLEDB, not {version}")
    if auto is not None and p["auto"] != auto:
        problems.append(f"{slug} auto flag is {p['auto']}")
    for name, fixture in (files or {}).items():
        want = (f.raw_of(fixture)[0], hashlib.sha256(f.raw_of(fixture)[2]).hexdigest())
        got = p["files"].get(name)
        if not got or (got[0] in (5, 6)) != (want[0] in (5, 6)) or got[1] != want[1]:
            problems.append(f"{slug}: record of {name!r} is {got}, want {want}")
    return problems


def self_updated(c, running):
    got = c.raw("CEAGLE")
    if not got:
        return ["CEAGLE is missing"]
    record = c.db().get("ceagle", {}).get("files", {}).get(b"CEAGLE")
    problems = [] if record and record[1] == hashlib.sha256(got[2]).hexdigest() else ["CEAGLE does not match its record"]
    return problems + (["CEAGLE was not replaced"] if got[2] == f.raw_of(running)[2] else [])


def all(fx, results, build):
    multi_files = {b"MULTI": fx["multi"], b"MULTISUB": fx["multisub"], b"\x5dMLIST": fx["mlist"],
                   b"\x5c\x09": fx["matrix"], b"\xaa\x08": fx["str9"], b"\x60\x08": fx["pic9"]}
    return [
        {"name": "browse",
         "steps": wait() + shot("01_list") + press("down", "down") + shot("02_moved") + press("window") +
         shot("03_basic") + press("window", "window", "window", "window") + search("multi") + shot("04_search") +
         press("enter") + shot("05_detail") + press("clear", "zoom") + shot("06_installed") + press("clear", "trace") +
         shot("07_settings") + press("clear") + quit_app(),
         "check": lambda c: [] if not c.db() else ["CEAGLEDB should not exist"]},
        {"name": "relaunch",
         "steps": wait() + quit_app() + ["action|launch"] + wait() + shot("01_started_again") + open_project("hello") +
         press("enter") + shot("02_done") + press("enter") + quit_app(),
         "save": ["HELLO"],
         "check": lambda c: c.same("HELLO", fx["hello"]) + db_has(c.db(), "hello", "1.0.0")},
        {"name": "install_dep",
         "steps": wait() + open_project("libuser") + shot("01_detail") + press("enter") + shot("02_review") +
         press("enter") + shot("02_done") + press("enter") + shot("03_detail") + press("clear", "zoom") + shot("04_installed") + quit_app(),
         "save": ["LIBUSER", "TESTLIB"],
         "check": lambda c: c.same("LIBUSER", fx["libuser"]) + c.same("TESTLIB", fx["testlib"]) +
         db_has(c.db(), "testlib", "5.0.0", True, {b"TESTLIB": fx["testlib"]}) +
         db_has(c.db(), "libuser", "1.0.0", False, {b"LIBUSER": fx["libuser"]}) +
         ([] if c.db().get("libuser", {}).get("needs") == ["testlib"] else ["libuser should need testlib"])},
        {"name": "decline_capabilities",
         "steps": wait() + open_project("cleaner") + press("enter") + shot("01_capabilities") + press("clear") +
         shot("02_back") + quit_app(),
         "save": ["DELVAR"],
         "check": lambda c: c.missing("DELVAR") + ([] if not c.db() else ["CEAGLEDB should not exist"])},
        {"name": "install_multi",
         "steps": wait() + open_project("multi") + press("enter") + shot("01_done") + press("enter") + quit_app(),
         "save": ["MULTI", "MULTISUB", "\u029fMLIST", "\u029fMLIS", "[J]", "Str9", "Pic9"],
         "check": lambda c: c.same("MULTI", fx["multi"]) + c.same("MULTISUB", fx["multisub"]) +
         c.same("MLIST", fx["mlist"]) + c.same("[J]", fx["matrix"]) + c.same("Str9", fx["str9"]) +
         c.same("Pic9", fx["pic9"]) + c.archived("Str9", True) + c.archived("[J]", True) + c.archived("Pic9", False) +
         db_has(c.db(), "multi", "1.0.0", False, multi_files)},
        {"name": "install_big",
         "steps": wait() + open_project("big") + press("enter") + shot("01_done") + press("enter") + quit_app(),
         "save": ["BIGPRG", "BIGVAR"],
         "check": lambda c: c.same("BIGPRG", fx["bigprg"]) + c.same("BIGVAR", fx["bigvar"]) +
         db_has(c.db(), "big", "1.0.0", False, {b"BIGPRG": fx["bigprg"], b"BIGVAR": fx["bigvar"]})},
        {"name": "install_doomed",
         "steps": wait() + open_project("doomed") + press("enter", "enter") + quit_app(),
         "save": ["DOOMED"],
         "check": lambda c: c.same("DOOMED", fx["doomed"]) + db_has(c.db(), "doomed", "1.0.0")},
        {"name": "bad_checksum", "faults": {"/HELLO.8xp": "corrupt"},
         "steps": wait() + open_project("hello") + press("enter") + shot("01_checksum") + press("enter") + quit_app(),
         "save": ["HELLO"],
         "check": lambda c: c.missing("HELLO") + ([] if "hello" not in c.db() else ["hello should not be recorded"])},
        {"name": "truncated", "faults": {"/BIGVAR.8xv": "truncate"},
         "steps": wait() + open_project("big") + press("enter") + shot("01_truncated") + press("enter") + quit_app(),
         "save": ["BIGPRG", "BIGVAR"],
         "check": lambda c: c.missing("BIGVAR") + c.same("BIGPRG", fx["bigprg"]) +
         ([] if c.db().get("big", {}).get("version") == "" else ["big should be recorded as incomplete"])},
        {"name": "exists_conflict", "files": [fx["user_hello"]],
         "steps": wait() + open_project("hello") + press("enter") + shot("01_exists") + press("enter") + quit_app(),
         "save": ["HELLO"],
         "check": lambda c: c.same("HELLO", fx["user_hello"]) + ([] if not c.db() else ["CEAGLEDB should not exist"])},
        {"name": "low_ram", "files": fx["ram_fillers"],
         "steps": wait() + open_project("big") + press("enter") + shot("01_low_ram") + press("enter") + quit_app(),
         "save": ["BIGPRG"],
         "check": lambda c: c.missing("BIGPRG") + ([] if not c.db() else ["CEAGLEDB should not exist"])},
        {"name": "low_archive",
         "steps": wait() + open_project("huge") + press("enter") + shot("01_low_archive") + press("enter") + quit_app(),
         "save": ["HUGE", "HUGE00"],
         "check": lambda c: c.missing("HUGE") + c.missing("HUGE00") + ([] if not c.db() else ["CEAGLEDB should not exist"])},
        {"name": "self_update",
         "steps": wait() + open_project("ceagle") + press("enter") + shot("01_confirm") + press("enter") +
         shot("02_done") + press("enter") + quit_app(),
         "save": ["CEAGLE"],
         "check": lambda c: self_updated(c, build / "bin/CEAGLE.8xp")},
        {"name": "files64",
         "steps": wait() + open_project("files64") + press("enter") + shot("01_done") + press("enter") + quit_app(),
         "save": ["F64", "QAA", "QCK"],
         "check": lambda c: c.same("F64", fx["f64"]) + c.same("QAA", fx["QAA"]) + c.same("QCK", fx["QCK"]) +
         ([] if len(c.db().get("files64", {}).get("files", {})) == 64 else ["files64 should record 64 files"])},
        {"name": "files64_uninstall", "files": "files64",
         "steps": wait() + press("zoom") + shot("01_installed") + press("del") + shot("02_confirm") + press("enter") +
         shot("03_removed") + quit_app(),
         "save": ["F64", "QAA", "QCK"],
         "check": lambda c: c.missing("F64") + c.missing("QAA") + c.missing("QCK") +
         ([] if not c.db() else ["CEAGLEDB should be empty"])},
        {"name": "tree64",
         "steps": wait() + open_project("tree") + press("enter") + shot("00_review") + press("down", "down", "enter") +
         shot("01_done") + press("enter", "clear", "zoom") +
         shot("02_installed") + quit_app(),
         "save": ["TREE", "T01", "T63"],
         "check": lambda c: c.same("TREE", fx["tree"]) + c.same("T01", fx["t01"]) + c.same("T63", fx["t63"]) +
         ([] if len(c.db()) == 64 and sum(p["auto"] for p in c.db().values()) == 63 else
          [f"CEAGLEDB should hold 64 projects, 63 of them needed by tree, has {len(c.db())}"])},
        {"name": "publish_updates_and_takedown", "stage": lambda api, fx, binary: (seed.updates(api, fx), seed.takedown(api, binary))},
        {"name": "update_multi", "files": "install_multi",
         "steps": wait() + press("zoom") + shot("01_updates") + press("enter") + shot("02_done") + press("enter") +
         shot("03_installed") + quit_app(),
         "save": ["MULTI", "MULTISUB", "\u029fMLIST", "\u029fMLIS", "[J]", "Str9", "Pic9", "MULTINEW"],
         "check": lambda c: c.same("MULTI", fx["multi2"]) + c.same("MULTISUB", fx["multisub"]) + c.missing("MLIST") +
         c.same("Str9", fx["str9b"]) + c.same("MULTINEW", fx["multinew"]) + c.same("[J]", fx["matrix"]) +
         c.archived("Str9", True) + db_has(c.db(), "multi", "1.1.0", False, {b"MULTI": fx["multi2"], b"MULTINEW": fx["multinew"]}) +
         ([] if len(c.db().get("multi", {}).get("files", {})) == 6 else ["multi should record 6 files"])},
        {"name": "update_big", "files": "install_big",
         "steps": wait() + press("zoom", "enter") + shot("01_done") + press("enter") + shot("02_installed") + quit_app(),
         "save": ["BIGPRG", "BIGVAR"],
         "check": lambda c: c.same("BIGPRG", fx["bigprg2"]) + c.same("BIGVAR", fx["bigvar"]) +
         db_has(c.db(), "big", "1.1.0", False, {b"BIGPRG": fx["bigprg2"]})},
        {"name": "uninstall_big", "files": "update_big",
         "steps": wait() + press("zoom", "del") + shot("01_confirm") + press("enter") + shot("02_removed") + quit_app(),
         "save": ["BIGPRG", "BIGVAR"],
         "check": lambda c: c.missing("BIGPRG") + c.missing("BIGVAR") + ([] if not c.db() else ["big should be gone"])},
        {"name": "takedown_warning", "files": "install_doomed",
         "steps": wait() + shot("01_warning") + press("enter") + shot("02_after") + quit_app(),
         "save": ["DOOMED"],
         "check": lambda c: c.missing("DOOMED") + ([] if "doomed" not in c.db() else ["doomed should be gone"])},
        {"name": "shared_library", "files": "install_dep",
         "steps": wait() + open_project("libpal") + press("enter") + shot("00_review") + press("enter") + shot("01_done") +
         press("enter", "clear", "zoom") +
         shot("02_installed") + press("down", "del") + shot("03_confirm") + press("enter") + shot("04_kept") + quit_app(),
         "save": ["LIBUSER", "LIBPAL", "TESTLIB"],
         "check": lambda c: c.missing("LIBUSER") + c.same("LIBPAL", fx["libpal"]) + c.same("TESTLIB", fx["testlib"]) +
         db_has(c.db(), "testlib", "5.0.0", True) + db_has(c.db(), "libpal", "1.0.0", False) +
         ([] if "libuser" not in c.db() else ["libuser should be gone"])},
        {"name": "shared_library_last", "files": "shared_library",
         "steps": wait() + press("zoom", "down", "del") + shot("01_confirm") + press("enter") + shot("02_removed") +
         quit_app(),
         "save": ["LIBPAL", "TESTLIB"],
         "check": lambda c: c.missing("LIBPAL") + c.missing("TESTLIB") + ([] if not c.db() else ["CEAGLEDB should be empty"])},
    ]
