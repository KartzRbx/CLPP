"""End-to-end test of `clpp --lsp`, speaking JSON-RPC exactly like VS Code does.

Usage: python test_lsp.py <path-to-clpp>
Every check prints PASS/FAIL; the exit code is the number of failures.
"""
import json, os, sys, tempfile, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lsp_client import Client, labels  # noqa: E402

EXE = sys.argv[1]
TIMING_BUDGET_MS = float(os.environ.get('CLPP_LSP_TIMING_BUDGET_MS', '50'))
if not 0 < TIMING_BUDGET_MS <= 500:
    raise ValueError('LSP timing budget must be in (0, 500] ms')
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
            "export const auto MAX_HP = 100;\n"
            "export def Animal {\n  number legs;\n  number speak() { return @this.legs * 10; }\n}\n"
            "export def Dog : Animal {\n  number bones;\n  number fetch(number times) { return @this.bones * times; }\n}\n"
            "export enum class Color { Red, Green, Blue }\n"
            "export number area(number w, number h) { return w * h; }\n"
            "export Dog makeDog() { return Dog { legs = 4, bones = 2 }; }\n")
    with open(os.path.join(ws, "other.clp"), "w") as f:
        f.write("export number helper() { return 1; }\n")

    c = Client(EXE, ws)
    check("initialize returns valid capabilities", bool(c.caps) and c.caps.get("completionProvider") is not None, c.caps)
    check("utf-16 position encoding", c.caps and c.caps.get("positionEncoding") == "utf-16")
    check("color previews advertised", c.caps and c.caps.get("colorProvider") is True)

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
    src = 'link "./shapes.clp" as Shapes;\nShapes.Dog d = Shapes.Dog { legs = 4, bones = 2 };\npost(d.)\n'
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
    edit("const auto counter = 1;\nnumber compute(number value) { return value; }\nco\n")
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
    edit("const auto x = 1;\nx = 2;\npost(y);\n")
    time.sleep(0.2)
    diags = c.diags.get(uri, [])
    messages = [d["message"] for d in diags]
    check("diagnostics reported", "cannot assign to immutable binding" in messages and "undefined name" in messages, messages)
    check("no duplicate diagnostic at the same position",
          len({(d["range"]["start"]["line"], d["range"]["start"]["character"]) for d in diags}) == len(diags), diags)

    # 11. quick fix removes const from the resolved declaration
    immutable = [d for d in diags if d["message"] == "cannot assign to immutable binding"]
    r = c.req("textDocument/codeAction", {"textDocument": {"uri": uri}, "range": immutable[0]["range"] if immutable else {},
                                          "context": {"diagnostics": immutable}})
    actions = r["result"] if r else []
    check("quick fix makes the binding mutable", any("mut" in a["title"] for a in actions), actions)
    edits = [e for a in actions for e in a.get("edit", {}).get("changes", {}).get(uri, [])]
    check("quick fix removes only the const keyword", len(edits) == 1 and edits[0]["newText"] == "" and
          edits[0]["range"] == {"start": {"line": 0, "character": 0}, "end": {"line": 0, "character": 5}}, edits)


    edit("const auto value = 1;\nvoid update() {\n  const auto value = 2;\n  value = 3;\n}\n")
    time.sleep(0.2)
    shadowed = [d for d in c.diags.get(uri, []) if d["message"] == "cannot assign to immutable binding"]
    r = c.req("textDocument/codeAction", {"textDocument": {"uri": uri}, "range": shadowed[0]["range"] if shadowed else {},
                                          "context": {"diagnostics": shadowed}})
    shadow_edits = [e for a in (r.get("result") or []) for e in a.get("edit", {}).get("changes", {}).get(uri, [])]
    check("quick fix targets the shadowed declaration", len(shadow_edits) == 1 and
          shadow_edits[0]["range"]["start"] == {"line": 2, "character": 2}, shadow_edits)

    # 12. hover with signature, cross-file definition
    edit('link "./shapes.clp" as Shapes;\npost(Shapes.area(2, 3));\n')
    r = c.pos(uri, "textDocument/hover", 1, 14)
    value = r["result"]["contents"]["value"] if r and r.get("result") else ""
    check("hover shows the signature", "number area(number w, number h)" in value, value)
    r = c.pos(uri, "textDocument/definition", 1, 14)
    res = r.get("result") if r else None
    check("definition jumps into shapes.clp", bool(res) and res["uri"].endswith("/shapes.clp") and res["range"]["start"]["line"] == 11, res)

    # 13. signature help
    r = c.pos(uri, "textDocument/signatureHelp", 1, 17)
    res = r.get("result") if r else None
    check("signature help names parameters", bool(res) and "number w" in res["signatures"][0]["label"] and res["activeParameter"] == 0, res)

    # 14. UTF-8 accents before the cursor (UTF-16 columns)
    edit('let ação = "órfão";\nconst auto número = 1;\npost(nú)\n')
    got = complete(c, uri, 2, 7)
    check("completion after accented identifiers", "número" in got, got)

    # 15. CRLF document
    edit("<< comentário\r\nconst auto total = 3;\r\npost(to)\r\n")
    got = complete(c, uri, 2, 7)
    check("CRLF: comment does not swallow the file", "total" in got, got)

    # 16. rename a local
    edit("auto score = 1;\nscore = score + 1;\npost(score);\n")
    r = c.req("textDocument/rename", {"textDocument": {"uri": uri}, "position": {"line": 2, "character": 6}, "newName": "pontos"})
    changes = (r.get("result") or {}).get("changes", {}).get(uri, []) if r else []
    check("rename edits every use", len(changes) == 4, changes)

    # 17. document symbols (outline)
    edit("def P { number x; }\nnumber f() { return 1; }\nconst auto a = 1;\n")
    r = c.req("textDocument/documentSymbol", {"textDocument": {"uri": uri}})
    names = [s["name"] for s in (r.get("result") or [])]
    check("outline lists struct, function, binding", {"P", "f", "a"} <= set(names), names)

    # 18. unknown request gets MethodNotFound (the client never hangs)
    r = c.req("textDocument/unknownThing", {"textDocument": {"uri": uri}})
    check("unknown request answered with an error", bool(r) and r.get("error", {}).get("code") == -32601, r)

    # 19. unsaved edits of a linked file are used
    other = c.open(ws + "/other.clp", "export number helper() { return 1; }\nexport number brandNew() { return 2; }\n")
    edit('link "./other.clp" as Other;\nOther.\n')
    got = complete(c, uri, 1, 6)
    check("linked file uses the unsaved buffer", "brandNew" in got, got)

    # Inferred factory variables keep their declared return type and members.
    edit("def Player { number Health; }\nPlayer create() { return Player { Health = 100 }; }\nauto player = create();\nplayer.\n")
    got = complete(c, uri, 3, 7)
    check("inferred factory variable exposes properties", "Health" in got, got)
    r = c.pos(uri, "textDocument/hover", 2, 7)
    value = ((r.get("result") or {}).get("contents") or {}).get("value", "")
    check("inferred factory variable has a type", "Player player" in value, value)
    r = c.pos(uri, "textDocument/definition", 3, 2)
    res = r.get("result") if r else None
    check("inferred variable resolves to its declaration", bool(res) and
          res["range"]["start"] == {"line": 2, "character": 5}, res)

    edit('def Outcome = "ongoing" | "team_a" | "team_b";\nOutcome RunBattle() { return "ongoing"; }\n')
    r = c.pos(uri, "textDocument/hover", 1, 3)
    value = ((r.get("result") or {}).get("contents") or {}).get("value", "")
    check("return type hover shows the literal union definition",
          'def Outcome = "ongoing" | "team_a" | "team_b";' in value, value)

    # Optional fields and direct guards must agree with command-line compilation.
    edit('def Human { string name; optional<number> cpf; }\n'
         'bool CreateBankAccount(Human target) { guard(target.cpf) else { return false; } return true; }\n'
         'auto person = Human { name = "Kartz" }; post(CreateBankAccount(person)); post(!person.cpf);\n')
    c.pos(uri, "textDocument/hover", 2, 6)  # response fences the preceding diagnostic publication
    check("omitted optional field and optional guard have no editor diagnostics", not c.diags.get(uri), c.diags.get(uri))
    edit('number Read(optional<number> value) { guard(value) else { return 0; } value = null; return value + 1; }\n')
    c.pos(uri, "textDocument/hover", 0, 13)
    check("editor retains errors after optional refinement is invalidated", bool(c.diags.get(uri)), c.diags.get(uri))

    # Document colors work without semantic compilation and exclude strings/comments.
    palette = "const auto PANEL = 0x141025;\nconst auto PORTRAIT = 0x2D1B4E;\nconst auto HP = 0xFF5A8A;\nconst auto MP = 0x5EC8FF;\n<< 0xFFFFFF\npost(\"0x123456\");\npost(\"😀\"); auto tint=0x80_FF0000;\nauto small=0xFF;\n"
    edit(palette)
    colors = c.req("textDocument/documentColor", {"textDocument": {"uri": uri}}).get("result", [])
    check("palette returns four colors plus transparent packed color", len(colors) == 5, colors)
    first = colors[0] if colors else {}
    check("hex preview has exact literal range", first.get("range") == {
        "start": {"line": 0, "character": 19}, "end": {"line": 0, "character": 27}}, first)
    color = first.get("color", {})
    check("PANEL RGB matches 141025", all(abs(color.get(k, -1)-v/255) < 1e-9
          for k, v in (("red", 20), ("green", 16), ("blue", 37))) and color.get("alpha") == 1, color)
    if colors:
        tint = colors[-1]
        check("packed transparency and UTF-16 range", abs(tint["color"]["alpha"]-127/255) < 1e-9 and
              tint["range"]["start"] == {"line": 6, "character": 22}, tint)
        presentations = c.req("textDocument/colorPresentation", {"textDocument": {"uri": uri},
            "range": first["range"], "color": {"red": 1, "green": 0, "blue": 0, "alpha": 1}}).get("result", [])
        check("color picker creates valid CL++ edit", presentations and presentations[0]["textEdit"] == {
            "range": first["range"], "newText": "0xFF0000"}, presentations)

    # 20. latency
    t0 = time.perf_counter()
    for _ in range(50):
        c.pos(uri, "textDocument/completion", 1, 6)
    ms = (time.perf_counter() - t0) * 1000 / 50
    gate = 'completion latency' if TIMING_BUDGET_MS == 50 else 'instrumented completion responsiveness'
    check(f"{gate} {ms:.1f} ms < {TIMING_BUDGET_MS:g} ms", ms < TIMING_BUDGET_MS, ms)

    c.close()

print(f"{failures} failure(s)")
sys.exit(failures)
