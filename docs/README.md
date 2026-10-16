# CL++ documentation

The website teaches the author's CL++ language, then its libraries. It is generated from every Markdown page in guide/, handbook/, and reference/. Internal design material is not published.

## Complete language handbook

The fifteen chapters in [handbook/](handbook/) incorporate the author's language specification, with the confirmed corrections: no headers, output through post/warn/report, named initialization with `Field = value` without a leading dot, and def grouping the type forms. All published explanations are in English. The CL++ VM is the only official execution target. A transpiler written in CL++ may generate Luau as an application use case; that does not make Luau a compiler backend.

## Reading path

1. [Learn CL++ from zero](guide/00-language-tour.md): read a source file, a declaration, a condition, a function, and a data definition.
2. [Implementation status](reference/status.md): understand executable examples versus author-defined target examples.
3. [Language contract](reference/language-contract.md), [keyword inventory](reference/keyword-inventory.md) and [syntax map](reference/syntax.md): consolidated rules and every topic independently.
4. [Lexer](reference/lexer.md), [comments](reference/comments.md), [names](reference/identifiers.md), [literals](reference/literals.md), and [punctuation](reference/punctuation.md).
5. Guide chapters 3–13: declarations, expressions, flow, functions, definitions, types, collections, modules, errors, concurrency, and events.
6. Library chapters and complete projects after the language fundamentals.

## Validation

`npm --prefix www run build` checks website content policy, generates all pages and navigation, and builds Astro/Starlight. The policy rejects obsolete declaration syntax in published CL++ blocks and checks repository Markdown links.

`tests/docs/check_docs.py` compiles executable examples and checks expected output/errors. `clp target` blocks specify pending language work and are counted separately, never as passing compiler tests. Historical regression documentation under `tests/docs/legacy` is not the public language specification.
