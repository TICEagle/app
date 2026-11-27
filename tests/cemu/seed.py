import shutil

import fixtures as f
from server import CEAGLE, SCRATCH

FIXTURES = SCRATCH / "fixtures"
TREE_DEPS = 63
RAM_FILLER = 25000
HUGE_PARTS = 47
FILES64 = 63
SUBPROGRAMS = [bytes([ord("Q"), 65 + i // 26, 65 + i % 26]) for i in range(FILES64)]


def make(name, var_type, raw, archived=False, filename=None):
    FIXTURES.mkdir(parents=True, exist_ok=True)
    return f.write(FIXTURES, filename or f"{name.decode()}.{f.EXTENSIONS[var_type]}", var_type, name, raw, archived)


def fixtures():
    FIXTURES.mkdir(parents=True, exist_ok=True)
    shutil.copy(CEAGLE / "app/bin/CEAGLE.8xp", FIXTURES / "CEAGLE.8xp")
    fx = {
        "testlib": make(b"TESTLIB", f.APPVAR, f.library(5), True),
        "libuser": make(b"LIBUSER", f.PROTECTED, f.asm(300, [("TESTLIB", 5), ("USBDRVCE", 0)]), True),
        "libpal": make(b"LIBPAL", f.PROTECTED, f.asm(200, [("TESTLIB", 5)]), True),
        "hello": make(b"HELLO", f.PROGRAM, f.basic('Disp "HELLO"')),
        "multi": make(b"MULTI", f.PROGRAM, f.basic('Disp "MULTI ONE":prgmMULTISUB'), True),
        "multi2": make(b"MULTI", f.PROGRAM, f.basic('Disp "MULTI TWO":prgmMULTISUB'), True, "MULTI2.8xp"),
        "multisub": make(b"MULTISUB", f.PROGRAM, f.basic('Disp "SUB"')),
        "multinew": make(b"MULTINEW", f.PROGRAM, f.basic('Disp "NEW"')),
        "mlist": make(b"\x5dMLIST", f.LIST, f.real_list([1, 2, 3, 42]), filename="MLIST.8xl"),
        "matrix": make(b"\x5c\x09", f.MATRIX, f.matrix(2, 3), True, "MATJ.8xm"),
        "str9": make(b"\xaa\x08", f.STRING, f.string("HELLO WORLD"), True, "STR9.8xs"),
        "str9b": make(b"\xaa\x08", f.STRING, f.string("GOODBYE"), True, "STR9B.8xs"),
        "pic9": make(b"\x60\x08", f.PICTURE, f.picture(), True, "PIC9.8ci"),
        "doomed": make(b"DOOMED", f.PROGRAM, f.basic('Disp "BAD"')),
        "bigprg": make(b"BIGPRG", f.PROTECTED, f.asm(f.DATA_MAX), True),
        "bigprg2": make(b"BIGPRG", f.PROTECTED, f.asm(f.DATA_MAX, fill=0xC9), True, "BIGPRG2.8xp"),
        "bigvar": make(b"BIGVAR", f.APPVAR, f.appvar(bytes((i * 7) & 0xFF for i in range(f.DATA_MAX))), True),
        "f64": make(b"F64", f.PROGRAM, f.basic('Disp "F64"'), True),
        "tree": make(b"TREE", f.PROGRAM, f.basic('Disp "TREE"'), True),
        "cleaner": CEAGLE / "scanner/tests/fixtures/DELVAR.8xp",
        "ceagle": FIXTURES / "CEAGLE.8xp",
    }
    fx["user_hello"] = make(b"HELLO", f.PROGRAM, f.basic('Disp "MINE"'), filename="USERHELLO.8xp")
    fx["ram_fillers"] = [make(b"FILLR", f.APPVAR, f.appvar(bytes(RAM_FILLER)), False)]
    fx["huge"] = make(b"HUGE", f.PROTECTED, f.asm(100), True)
    fx["huge_parts"] = [make(b"HUGE%02d" % i, f.APPVAR, f.appvar(bytes([i]) * f.DATA_MAX), True) for i in range(HUGE_PARTS)]
    for name in SUBPROGRAMS:
        fx[name.decode()] = make(name, f.PROGRAM, f.basic(f'Disp "{name.decode()}"'), True)
    for i in range(1, TREE_DEPS + 1):
        fx[f"t{i:02d}"] = make(f"T{i:02d}".encode(), f.PROGRAM, f.basic(f'Disp "T{i:02d}"'), True)
    return fx


def base(api, fx, binary):
    alice = api.user("alice", 1)
    api.admin(binary, "promote", "alice")
    api.project(alice, "ceagle", "asm", "The CEagle app itself.")
    api.publish(alice, "ceagle", "1.0.0", [(fx["ceagle"], "main")])
    api.project(alice, "testlib", "library", "A LibLoad library for tests.")
    api.publish(alice, "testlib", "5.0.0", [(fx["testlib"], "main")])
    api.project(alice, "libuser", "asm", "Uses testlib and USB.")
    api.publish(alice, "libuser", "1.0.0", [(fx["libuser"], "main")])
    api.project(alice, "libpal", "asm", "Also uses testlib.")
    api.publish(alice, "libpal", "1.0.0", [(fx["libpal"], "main")])
    api.project(alice, "hello", "basic", "Says hello.", category="games")
    api.publish(alice, "hello", "1.0.0", [(fx["hello"], "main")])
    api.project(alice, "cleaner", "asm", "Deletes variables.")
    api.publish(alice, "cleaner", "1.0.0", [(fx["cleaner"], "main")])
    api.project(alice, "doomed", "basic", "Will be taken down.")
    api.publish(alice, "doomed", "1.0.0", [(fx["doomed"], "main")])
    bob = api.user("bob", 2)
    api.project(bob, "multi", "basic", "A program with many kinds of files.")
    api.publish(bob, "multi", "1.0.0", [(fx["multi"], "main"), (fx["multisub"], "data"), (fx["mlist"], "data"),
                                        (fx["matrix"], "data"), (fx["str9"], "data"), (fx["pic9"], "data")])
    api.project(bob, "big", "asm", "The largest program and appvar.")
    api.publish(bob, "big", "1.0.0", [(fx["bigprg"], "main"), (fx["bigvar"], "data")])
    api.project(bob, "huge", "asm", "A 3 MiB release.")
    api.publish(bob, "huge", "1.0.0", [(fx["huge"], "main")] + [(p, "data") for p in fx["huge_parts"]])
    api.project(bob, "files64", "basic", "A release with 64 files.")
    api.publish(bob, "files64", "1.0.0", [(fx["f64"], "main")] + [(fx[n.decode()], "data") for n in SUBPROGRAMS])
    users = [api.user(f"tree{i}", 10 + i) for i in range(7)]
    for i in range(1, TREE_DEPS + 1):
        owner = users[(i - 1) // 10]
        api.project(owner, f"dep{i:02d}", "basic", f"Dependency {i}.")
        api.publish(owner, f"dep{i:02d}", "1.0.0", [(fx[f"t{i:02d}"], "main")])
    api.project(users[6], "tree", "basic", "Needs 63 other projects.")
    api.publish(users[6], "tree", "1.0.0", [(fx["tree"], "main")],
                {f"dep{i:02d}": ">=1.0.0" for i in range(1, TREE_DEPS + 1)})


def updates(api, fx):
    bob = api.user("bob", 2)
    api.publish(bob, "multi", "1.1.0", [(fx["multi2"], "main"), (fx["multisub"], "data"), (fx["matrix"], "data"),
                                        (fx["str9b"], "data"), (fx["pic9"], "data"), (fx["multinew"], "data")])
    api.publish(bob, "big", "1.1.0", [(fx["bigprg2"], "main"), (fx["bigvar"], "data")])


def takedown(api, binary):
    api.admin(binary, "takedown", "doomed", "test takedown")
