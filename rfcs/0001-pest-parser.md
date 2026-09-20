# Pratt parser decision (0.7)

Pest remains the CL++ parser in 0.7. Recovery (`parse_for_ide`, ghost ident `__clpp_complete`, resync to `;` / `}`) is enough for the IDE tests.

An RFC to switch to Pratt is only opened if recovery fails those tests.
