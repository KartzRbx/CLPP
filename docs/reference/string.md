---
title: "string"
sidebar_label: "string"
---

# string

<div class="clpp-ref-meta">Type</div>

UTF-8 text. Write it with `"double quotes"`, `'single quotes'`, or a `` `template` ``. Join pieces with [`.:`](operator-concat) or with commas after a template.

## Syntax

```clpp
string name = "Kartz";
string title = 'Player';
string line = `PlayerName is {player.Name}`;
string also = `PlayerName is `, player.Name, `.`;
```

## Description

- `"..."` and `'...'` are plain text.
- `` `PlayerName is {player.Name}` `` inserts expressions inside `{ }`.
- `` `PlayerName is `, player.Name, `.` `` joins a template with extra values in one expression.

A `const int` cannot be initialized with a string. Every statement ends with `;`.

## Example

```clpp
string name = "Kartz";
string key = name .: "_bag";
post(`online: {name}`);
```

## See also

[operator-concat](operator-concat) · [const](const)
