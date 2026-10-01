"""End-to-end test of `clpp --lsp`, speaking JSON-RPC exactly like VS Code does.

Usage: python test_lsp.py <path-to-clpp>
Every check prints PASS/FAIL; the exit code is the number of failures.
"""
import json, os, sys, tempfile, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lsp_client import Client, labels  # noqa: E402

EXE = sys.argv[1]
failures = 0


def check(name, condition, detail=""):
    global failures
    print(("PASS " if condition else "FAIL ") + name + ("" if condition else f"  -> {detail}"))
    if not condition:
        failures += 1


def complete(c, uri, line, ch):
    return labels(c.pos(uri, "textDocument/completion", line, ch)) or []


with tempfile.TemporaryDirectory() as ws:
    ws = ws.replace("\\", "/")
    with open(os.path.join(ws, "shapes.clp"), "w", newline="\n") as f:
        f.write(
            "<< módulo: o próprio arquivo é a interface, sem header\n"
            "const MAX_HP = 100;\n"
            "struct Animal {\n  int legs;\n  func speak() { return self.legs * 10; }\n}\n"
            "struct Dog : Animal {\n  int bones;\n  func fetch(int times) { return self.bones * times; }\n}\n"
            "enum Color { Red, Green, Blue }\n"
            "func area(int w, int h) -> int { return w * h; }\n"
            "func makeDog() -> Dog { return Dog(4, 2); }\n")
    with open(os.path.join(ws, "other.clp"), "w") as f:
        f.write("func helper() { return 1; }\n")

    c = Client(EXE, ws)
    check("initialize returns valid capabilities", bool(c.caps) and c.caps.get("completionProvider") is not None, c.caps)
    check("utf-16 position encoding", c.caps and c.caps.get("positionEncoding") == "utf-16")

    uri = c.open(ws + "/main.clp", "")

    def edit(text, version=[1]):
        version[0] += 1
        c.change(uri, text, version[0])
        time.sleep(0.05)

    # 1. stdlib link: module names after `link @clpp.`
    edit("link @clpp.\n")
    got = complete(c, uri, 0, 11)
    check("link @clpp. lists stdlib modules", {"axiom", "fs", "math", "text"} <= set(got), got)

    # 2. members of a linked stdlib module inside auto-closed parentheses
    edit("link @clpp.fs as Fs;\npost(Fs.)\n")
    got = complete(c, uri, 1, 8)
    check("Fs. inside post(...) lists module functions", {"size", "read", "list"} <= set(got), got)

    # 3. file link completion
    edit('link "./\n')
    got = complete(c, uri, 0, 8)
    check("link \"./ lists .clp files", "./shapes.clp" in got and "./other.clp" in got, got)

    # 4. members of a file module: functions, types, constants
    edit('link "./shapes.clp" as Shapes;\nShapes.\n')
    got = complete(c, uri, 1, 7)
    check("Shapes. lists functions, types and constants",
          {"area", "makeDog", "Dog", "Animal", "Color", "MAX_HP"} <= set(got), got)

    # 5. inheritance across modules
    src = 'link "./shapes.clp" as Shapes;\nDog d = Shapes.Dog(4, 2);\npost(d.)\n'
    edit(src)
    got = complete(c, uri, 2, 7)
    check("d. shows own and inherited members", {"bones", "legs", "speak", "fetch"} <= set(got), got)

    # 6. member of a function's return value
    edit('link "./shapes.clp" as Shapes;\npost(Shapes.makeDog().)\n')
    got = complete(c, uri, 1, 22)
    check("makeDog(). uses the return type", {"bones", "legs", "speak"} <= set(got), got)

    # 7. enum variants through the module
    edit('link "./shapes.clp" as Shapes;\npost(Shapes.Color.)\n')
    got = complete(c, uri, 1, 18)
    check("Shapes.Color. lists variants", {"Red", "Green", "Blue"} <= set(got), got)

    # 8. prefix completion of locals, functions and keywords
    edit("let counter = 1;\nfunc compute(value) { return value; }\nco\n")
    got = complete(c, uri, 2, 2)
    check("prefix co completes counter and compute", "counter" in got and "compute" in got, got)

    # 9. snippets at the start of a statement
    r = c.pos(uri, "textDocument/completion", 2, 2)
    items = r["result"]["items"]
    edit("\nfo")
    r = c.pos(uri, "textDocument/completion", 1, 2)
    snippet = [i for i in r["result"]["items"] if i.get("insertTextFormat") == 2 and i["label"] == "for"]
    check("for snippet offered", bool(snippet), [i["label"] for i in r["result"]["items"]][:20])

    # 10. diagnostics: one per problem, with correct ranges
    edit("let x = 1;\nx = 2;\npost(y);\n")
    time.sleep(0.2)
    diags = c.diags.get(uri, [])
    messages = [d["message"] for d in diags]
    check("diagnostics reported", "cannot assign to immutable binding" in messages and "undefined name" in messages, messages)
    check("no duplicate diagnostic at the same position",
          len({(d["range"]["start"]["line"], d["range"]["start"]["character"]) for d in diags}) == len(diags), diags)

    # 11. quick fix let -> let mut
    immutable = [d for d in diags if d["message"] == "cannot assign to immutable binding"]
    r = c.req("textDocument/codeAction", {"textDocument": {"uri": uri}, "range": immutable[0]["range"] if immutable else {},
                                          "context": {"diagnostics": immutable}})
    actions = r["result"] if r else []
    check("quick fix makes the binding mutable", any("mut" in a["title"] for a in actions), actions)

    # 12. hover with signature, cross-file definition
    edit('link "./shapes.clp" as Shapes;\npost(Shapes.area(2, 3));\n')
    r = c.pos(uri, "textDocument/hover", 1, 14)
    value = r["result"]["contents"]["value"] if r and r.get("result") else ""
    check("hover shows the signature", "func area(int w, int h) -> int" in value, value)
    r = c.pos(uri, "textDocument/definition", 1, 14)
    res = r.get("result") if r else None
    check("definition jumps into shapes.clp", bool(res) and res["uri"].endswith("/shapes.clp") and res["range"]["start"]["line"] == 11, res)

    # 13. signature help
    r = c.pos(uri, "textDocument/signatureHelp", 1, 17)
    res = r.get("result") if r else None
    check("signature help names parameters", bool(res) and "int w" in res["signatures"][0]["label"] and res["activeParameter"] == 0, res)

    # 14. UTF-8 accents before the cursor (UTF-16 columns)
    edit('let ação = "órfão";\nlet número = 1;\npost(nú)\n')
    got = complete(c, uri, 2, 7)
    check("completion after accented identifiers", "número" in got, got)

    # 15. CRLF document
    edit("<< comentário\r\nlet total = 3;\r\npost(to)\r\n")
    got = complete(c, uri, 2, 7)
    check("CRLF: comment does not swallow the file", "total" in got, got)

    # 16. rename a local
    edit("let mut score = 1;\nscore = score + 1;\npost(score);\n")
    r = c.req("textDocument/rename", {"textDocument": {"uri": uri}, "position": {"line": 2, "character": 6}, "newName": "pontos"})
    changes = (r.get("result") or {}).get("changes", {}).get(uri, []) if r else []
    check("rename edits every use", len(changes) == 4, changes)

    # 17. document symbols (outline)
    edit("struct P { int x; }\nfunc f() { return 1; }\nlet a = 1;\n")
    r = c.req("textDocument/documentSymbol", {"textDocument": {"uri": uri}})
    names = [s["name"] for s in (r.get("result") or [])]
    check("outline lists struct, function, binding", {"P", "f", "a"} <= set(names), names)

    # 18. unknown request gets MethodNotFound (the client never hangs)
    r = c.req("textDocument/unknownThing", {"textDocument": {"uri": uri}})
    check("unknown request answered with an error", bool(r) and r.get("error", {}).get("code") == -32601, r)

    # 19. unsaved edits of a linked file are used
    other = c.open(ws + "/other.clp", "func helper() { return 1; }\nfunc brandNew() { return 2; }\n")
    edit('link "./other.clp" as Other;\nOther.\n')
    got = complete(c, uri, 1, 6)
    check("linked file uses the unsaved buffer", "brandNew" in got, got)

    # 20. latency
    t0 = time.time()
    for _ in range(50):
        c.pos(uri, "textDocument/completion", 1, 6)
    ms = (time.time() - t0) * 1000 / 50
    check(f"completion latency {ms:.1f} ms < 50 ms", ms < 50, ms)

    c.close()

print(f"{failures} failure(s)")
sys.exit(failures)
