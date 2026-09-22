# RFC 0014 — Contracts (`requires` / `ensures`)

Status: Draft — surface reserved

Parent: [RFC 0008](0008-intent-system-roadmap.md)

## Surface (planned)

```clpp
int divide(int a, int b)
  requires(b != 0)
  ensures(result * b == a)
{
  return a / b;
}
```

## Emit

Debug builds: runtime `assert` before/after body. Release: strip unless `--contracts=always`.

## Implementation checklist

See [FEATURE_CHECKLIST](../docs/architecture/FEATURE_CHECKLIST.md). Grammar keywords reserved in intent module; full parse lands with Phase A batch.
