# S7 templates

`template<typename T>` is parsed on functions and structs. Defaults (`U = string`) are stored. No specialization, NTTP, or overload. `items[0]` is 0-based in CL++ and emits `[1]` only when the index is a numeric literal 0 translated at emit time for Luau arrays — documented as 0-based source, 1-based Luau tables.
