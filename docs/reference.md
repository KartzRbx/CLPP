---
title: Language reference
toc_min_heading_level: 2
toc_max_heading_level: 2
---

# Language reference

Every utility has its own page: syntax, parameters, return value, Luau emit, example, and see-also. Same job as [cplusplus.com/reference](https://cplusplus.com/reference/). Generated class pages live under the **API** tab.

<div class="clpp-ref-quick" aria-label="Quick access">

[post](reference/post) · [warn](reference/warn) · [report](reference/report) · [GetService](reference/GetService) · [new](reference/new) · [::](reference/operator-method) · [.:](reference/operator-concat) · [~>](reference/operator-janitor) · [guard](reference/guard) · [match](reference/match) · [signal](reference/signal-type) · [Fire](reference/Fire) · [init](reference/init) · [#include](reference/include) · [struct](reference/struct) · [cheatsheet](cheatsheet)

</div>

<div class="clpp-ref-split">

<div class="clpp-ref-col">

## I/O

- [post](reference/post)
- [warn](reference/warn)
- [report](reference/report)
- [cout / endl](reference/cout)

## Builtins

- [GetService](reference/GetService)
- [pcall](reference/pcall)
- [new](reference/new)
- [static_cast](reference/static_cast)
- [string_concat](reference/string_concat)
- [to_string](reference/to_string)
- [to_number](reference/to_number)
- [to_bool](reference/to_bool)
- [null](reference/null)
- [void init()](reference/init)
- [this](reference/this)

## Types

- [int](reference/int)
- [float](reference/float)
- [double](reference/double)
- [bool](reference/bool)
- [string](reference/string)
- [void](reference/void)
- [func](reference/func)
- [auto](reference/auto)
- [optional&lt;T&gt;](reference/optional)
- [array&lt;T&gt;](reference/array)
- [vector&lt;T&gt;](reference/vector)
- [dictionary&lt;K,V&gt;](reference/dictionary)
- [T* (Instance)](reference/instance-pointer)
- [const](reference/const)
- [signal&lt;T...&gt;](reference/signal-type)
- [observable T](reference/observable)

## Operators

- [:: static / manual Connect](reference/operator-method)
- [: type / protected call](reference/operator-table)
- [. property / instance method](reference/operator-property)
- [.: concat](reference/operator-concat)
- [~&gt; janitor](reference/operator-janitor)
- [+ − * /](reference/operator-arithmetic)
- [== != &lt; &gt;](reference/operator-comparison)
- [&amp;&amp; || !](reference/operator-logic)
- [= += …](reference/operator-assignment)
- [++ −−](reference/operator-increment)
- [.Field =](reference/operator-designated)

## Control flow

- [if / else if / else](reference/if)
- [while](reference/while)
- [for (C-style)](reference/for)
- [for (T x in list)](reference/range-for)
- [switch](reference/switch)
- [guard](reference/guard)
- [match](reference/match)
- [break](reference/break)
- [return](reference/return)

</div>

<div class="clpp-ref-col">

## Functions &amp; OOP

- [function](reference/function)
- [`func (...)`](reference/lambda)
- [struct / class](reference/struct)
- [Class::Method](reference/class-method)
- [public / private](reference/access-labels)

## Signals

- [Fire](reference/Fire)
- [Connect](reference/Connect)
- [Once](reference/Once)
- [Wait](reference/Wait)
- [GetPropertyChangedSignal](reference/GetPropertyChangedSignal)
- [OnChange](reference/OnChange)

## Concurrency

- [async](reference/async)
- [await](reference/await)
- [spawn](reference/spawn)
- [parallel](reference/parallel)
- [`auto [a, b] =`](reference/destructure)

## Attributes &amp; preprocessor

- [`[[server]]`](reference/attr-server)
- [`[[client]]`](reference/attr-client)
- [#include](reference/include)
- [#pragma strict](reference/pragma-strict)
- [#pragma nostrict](reference/pragma-nstrict)
- [#pragma native](reference/pragma-native)
- [#pragma optimize](reference/pragma-optimize)
- [#pragma once](reference/pragma-once)

## Headers

- [&lt;clpp/roblox.clh&gt;](reference/header-roblox)
- [&lt;clpp/generated/instances.clh&gt;](reference/header-instances)
- [&lt;clpp/datatypes.clh&gt;](reference/header-datatypes)
- [&lt;clpp/libs/janitor.clh&gt;](reference/header-janitor)
- [&lt;clpp/libs/dataservice.clh&gt;](reference/header-dataservice)

## Tooling

- [`clpp` language id](reference/language-id)

</div>

</div>

## Not in the language

See [What CL++ does not do](unsupported): `->`, `continue`, ternary, `do/while`, `try/catch`, `goto`, pointer arithmetic, C++ captures, generic templates, `std::`.
