#!/usr/bin/env python3
"""Generate docs/spec/grammar.generated.md from src/parser/grammar.pest."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
pest = (ROOT / "src" / "parser" / "grammar.pest").read_text(encoding="utf-8")
lines = ["---", "title: Grammar (generated)", "description: EBNF derived from src/parser/grammar.pest — 0.7 source of truth.", "---", "", "# Grammar (generated)", "", "Derived from `src/parser/grammar.pest`. Do not invent productions that are not in that file.", "", "```ebnf"]
for raw in pest.splitlines():
    s = raw.strip()
    if not s or s.startswith("//"):
        continue
    if " = " not in s:
        lines.append(s.replace("~", " ").replace("{", "").replace("}", "").strip())
        continue
    name, rest = s.split(" = ", 1)
    silent = rest.startswith("_{") or rest.startswith("@{")
    body = rest
    for prefix in ("_{", "@{", "{"):
        if body.startswith(prefix):
            body = body[len(prefix):]
            break
    body = body.rstrip("}").strip()
    body = body.replace(" ~ ", " ").replace("~", " ")
    marker = "  (* silent *)" if silent else ""
    lines.append(f"{name} ::= {body}{marker}")
lines.append("```")
lines.append("")
out = ROOT / "docs" / "spec" / "grammar.generated.md"
out.write_text("\n".join(lines) + "\n", encoding="utf-8")
print(f"wrote {out}")
