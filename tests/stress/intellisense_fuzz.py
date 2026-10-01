"""Property matrix and JSON-RPC executor for the CL++ language server.

The generator builds 10000 completion simulations. The executor is one
clpp --lsp process: didOpen, bursts of didChange, then textDocument/completion.
"""

import json
import subprocess
import sys
import time
from pathlib import Path

TARGET = 10000
URI = "file:///C:/clpp/fuzz.clp"
SERVER = Path(__file__).resolve().parents[1] / "ucrt64" / "src" / "clpp.exe"


def cursor(text):
    mark = text.index("@@")
    body = text[:mark] + text[mark + 2 :]
    line = body[:mark].count("\n")
    previous = body.rfind("\n", 0, mark)
    character = mark - (previous + 1)
    return body, {"line": line, "character": character}


def case(category, snippet, expected, unexpected, trigger=".", changes=None, unique=False):
    code, position = cursor(snippet)
    item = {
        "category": category,
        "code_snippet": code,
        "cursor_position": position,
        "trigger_character": trigger,
        "expected_completions": expected,
        "unexpected_completions": unexpected,
        "max_allowed_latency_ms": 50,
        "expect_unique": unique,
    }
    if changes:
        item["changes"] = changes
    return item


def generate():
    fields = ["health", "armor", "speed", "score", "mana", "focus"]
    owners = ["player", "actor", "unit", "pawn", "hero"]
    kinds = ["Fighter", "Mage", "Rover", "Guard", "Scout"]
    cases = []

    def add(item):
        if len(cases) >= TARGET:
            return
        item["test_id"] = f"TC_INTELLI_{len(cases) + 1:05d}"
        item["code_snippet"] = f"<< {item['test_id']}\n" + item["code_snippet"]
        item["cursor_position"]["line"] += 1
        if "changes" in item:
            item["changes"] = [f"<< {item['test_id']}\n" + step for step in item["changes"]]
        cases.append(item)

    for kind in kinds:
        for owner in owners:
            for left in fields:
                for right in fields:
                    if left == right or len(cases) >= 1800:
                        continue
                    add(case(
                        "Resilient_Parsing_Incomplete_Member_Access",
                        f"struct {kind} {{ int {left}; int {right}; }}\n{kind} {owner} = {kind}(1, 2);\n{owner}.@@",
                        [left, right],
                        [kind, "string", "let"],
                        unique=True,
                    ))

    for kind in kinds:
        for owner in owners:
            for field in fields:
                if len(cases) >= 3200:
                    break
                add(case(
                    "Fuzzy_Subsequence",
                    f"struct {kind} {{ int {field}; }}\n{kind} {owner} = {kind}(1);\n{owner}.{field[0]}{field[-1]}@@",
                    [field],
                    ["let", kind],
                ))

    for index in range(700):
        name = f"local{index % 40}"
        add(case(
            "Scope_Block_Local_Visible",
            f"if (1) {{\n  let {name} = {index % 7};\n  {name[:2]}@@\n}}\n",
            [name],
            ["while"],
        ))
        add(case(
            "Scope_Block_Local_Hidden",
            f"if (1) {{\n  let {name} = 1;\n}}\npost({name[:2]}@@);\n",
            [],
            [name],
        ))

    for index in range(600):
        add(case(
            "Inheritance_Chain_No_Duplicates",
            "struct Base { int health; func speak() { return 1; } }\n"
            "struct Mid : Base { int armor; }\n"
            "struct Top : Mid { int speed; }\n"
            f"Top unit{index % 5} = Top(1, 2, 3);\nunit{index % 5}.@@",
            ["health", "armor", "speed", "speak"],
            ["Base", "string"],
            unique=True,
        ))

    for index in range(500):
        add(case(
            "Enum_Variants",
            "enum Color { Red, Green, Blue }\nColor.@@",
            ["Red", "Green", "Blue"],
            ["let", "health"],
            unique=True,
        ))

    for index in range(400):
        add(case(
            "Type_Alias_In_Scope",
            'type State = "Idle" | "Running";\nSt@@',
            ["State"],
            ["health", "Red"],
        ))

    for index in range(500):
        add(case(
            "Type_Does_Not_Leak_Foreign_Members",
            "struct PlayerData { int health; }\nint count = 1;\ncount.@@",
            [],
            ["health", "Name", "speak"],
        ))

    for index in range(400):
        add(case(
            "Comment_Suppresses_Completion",
            "let health = 1;\n<< hea@@",
            [],
            ["health", "let"],
            trigger=" ",
        ))
        add(case(
            "String_Suppresses_Completion",
            'let health = 1;\n"hea@@',
            [],
            ["health"],
            trigger='"',
        ))

    for index in range(400):
        add(case(
            "Keyword_In_Statement_Position",
            "le@@",
            ["let"],
            ["health"],
            trigger=" ",
        ))

    for index in range(400):
        add(case(
            "Incomplete_Call_Still_Sees_Local",
            "let health = 1;\npost(he@@",
            ["health"],
            ["armor"],
            trigger="(",
        ))
        add(case(
            "Junk_Token_Before_Cursor",
            "let health = 1;\n# he@@",
            ["health"],
            ["armor"],
        ))

    for index in range(300):
        add(case(
            "Unclosed_Block_Keeps_Local",
            "if (1) {\n  let health = 1;\n  he@@",
            ["health"],
            ["armor"],
        ))

    for index in range(200):
        steps = [
            "let health = 1;\n",
            "let health = 1;\nhe",
            "let health = 1;\nhea",
        ]
        add(case(
            "Rapid_DidChange_Uses_Latest_Text",
            "let health = 1;\nhea@@",
            ["health"],
            ["armor"],
            changes=steps,
        ))

    for index in range(80):
        nest = "if (1) {\n" * 20 + "  let health = 1;\n  he@@\n" + "}\n" * 20
        add(case(
            "Deep_Nesting",
            nest,
            ["health"],
            ["armor"],
        ))

    lines = [f"let value{i} = {i % 9};" for i in range(10000)]
    lines.append("let health = 1;")
    lines.append("he@@")
    wide = case(
        "Large_File_Completion",
        "\n".join(lines),
        ["health"],
        ["armor"],
    )
    wide["max_allowed_latency_ms"] = 8000
    add(wide)

    while len(cases) < TARGET:
        index = len(cases)
        field = fields[index % len(fields)]
        add(case(
            "Resilient_Parsing_Incomplete_Member_Access",
            f"struct Box{index} {{ int {field}; }}\nBox{index} item = Box{index}(1);\nitem.@@",
            [field],
            ["let", "string"],
            unique=True,
        ))
    return cases[:TARGET]


class LanguageServer:
    def __init__(self, path):
        self.proc = subprocess.Popen(
            [str(path), "--lsp"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
        )
        self.next_id = 1

    def close(self):
        if self.proc.poll() is None:
            self.notify("exit", {})
            self.proc.kill()
            self.proc.wait(timeout=5)

    def _read(self):
        header = b""
        while b"\r\n\r\n" not in header:
            chunk = self.proc.stdout.read(1)
            if not chunk:
                raise RuntimeError("language server closed")
            header += chunk
        length = 0
        for line in header.decode("ascii", "replace").split("\r\n"):
            if line.lower().startswith("content-length:"):
                length = int(line.split(":", 1)[1].strip())
        body = b""
        while len(body) < length:
            part = self.proc.stdout.read(length - len(body))
            if not part:
                raise RuntimeError("truncated language server message")
            body += part
        return json.loads(body.decode("utf-8"))

    def _write(self, payload):
        raw = json.dumps(payload, separators=(",", ":")).encode("utf-8")
        frame = f"Content-Length: {len(raw)}\r\n\r\n".encode("ascii") + raw
        self.proc.stdin.write(frame)
        self.proc.stdin.flush()

    def notify(self, method, params):
        self._write({"jsonrpc": "2.0", "method": method, "params": params})

    def request(self, method, params):
        message_id = self.next_id
        self.next_id += 1
        started = time.perf_counter()
        self._write({"jsonrpc": "2.0", "id": message_id, "method": method, "params": params})
        while True:
            message = self._read()
            if message.get("id") == message_id:
                elapsed = (time.perf_counter() - started) * 1000
                return message, elapsed

    def start(self):
        self.request("initialize", {"capabilities": {}})
        self.notify("initialized", {})
        self.notify("textDocument/didOpen", {"textDocument": {"uri": URI, "text": ""}})

    def change(self, text):
        self.notify("textDocument/didChange", {"textDocument": {"uri": URI}, "contentChanges": [{"text": text}]})

    def complete(self, text, position, changes):
        for step in changes or []:
            self.change(step)
        self.change(text)
        response, elapsed = self.request(
            "textDocument/completion",
            {"textDocument": {"uri": URI}, "position": position},
        )
        items = ((response.get("result") or {}).get("items")) or []
        return [item.get("label", "") for item in items], elapsed


def evaluate(labels, item, elapsed):
    missing = [label for label in item["expected_completions"] if label not in labels]
    leaked = [label for label in item["unexpected_completions"] if label in labels]
    duplicated = []
    if item.get("expect_unique"):
        duplicated = [label for label in item["expected_completions"] if labels.count(label) > 1]
    slow = elapsed > item["max_allowed_latency_ms"]
    if not missing and not leaked and not duplicated and not slow:
        return None
    return {
        "id": item["test_id"],
        "category": item["category"],
        "missing": missing,
        "leaked": leaked,
        "duplicated": duplicated,
        "elapsed_ms": round(elapsed, 2),
    }


def main():
    server_path = Path(sys.argv[1]) if len(sys.argv) > 1 else SERVER
    if not server_path.exists():
        print(f"missing language server: {server_path}")
        return 1
    cases = generate()
    if len(sys.argv) > 2 and sys.argv[2] == "--dump":
        print(json.dumps(cases[0], indent=2))
        return 0
    server = LanguageServer(server_path)
    failures = []
    slowest = 0.0
    try:
        server.start()
        for item in cases:
            labels, elapsed = server.complete(item["code_snippet"], item["cursor_position"], item.get("changes"))
            slowest = max(slowest, elapsed)
            problem = evaluate(labels, item, elapsed)
            if problem is not None:
                if len(failures) < 12:
                    failures.append(problem)
                else:
                    failures.append({"id": item["test_id"]})
    finally:
        server.close()
    print(f"ran {len(cases)} failures {len(failures)} slowest_ms {slowest:.2f}")
    for problem in failures:
        if "category" in problem:
            print(problem)
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
