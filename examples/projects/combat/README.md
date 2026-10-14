# Deterministic Combat battle

Run `clpp examples/projects/combat/main.clp` from the repository root. The original author reference remains in `docs/internal/design/combat/Combat.clp`.

This runnable adaptation keeps the damage pipeline, reference mutation, captured Park–Miller generator, elemental interactions, statuses, generic sorting, optional decisions and deterministic four-round battle. Integer names, field initializers, collection APIs and optional-presence tests use the current language. The original `@parallel` effect annotation is not implemented; the preview function is kept but does not claim a checked purity guarantee.

CTest compares the complete transcript with the golden output, ending in `team_a`. The author diagnostic and purity contracts still require separate implementation.
