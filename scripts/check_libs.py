#!/usr/bin/env python3
"""Fail CI if libs.json symbols are missing from the mapped headers."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
libs = json.loads((ROOT / "stdlib" / "libs.json").read_text(encoding="utf-8"))
missing = []
for name, spec in libs.items():
    header = ROOT / spec["header"]
    if not header.is_file():
        missing.append(f"{name}: missing header {spec['header']}")
        continue
    text = header.read_text(encoding="utf-8")
    for sym in spec.get("symbols", []):
        if spec.get("map") and spec["map"].get(sym) and spec["map"][sym] in text:
            continue
        if sym not in text:
            missing.append(f"{name}: symbol {sym} not in {spec['header']}")
if missing:
    print("libs.json check failed:")
    for row in missing:
        print(" ", row)
    raise SystemExit(1)
print("libs.json ok")
