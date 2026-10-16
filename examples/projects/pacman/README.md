# Pacman core on the CL++ VM

Run `clpp examples/projects/pacman/main.clp` from the repository root. The original author files remain in `docs/internal/design/pacman`.

Entities is a source module with explicit exports, replacing the discarded header proposal. Maze keeps the generic grid, bounds and collision checks, exhaustive direction/tile matches, shared entity mutation, typed score callbacks, optional queued direction, bonus and power timer. The scenario exercises a power pellet, fruit, out-of-bounds movement and exact score notifications; CTest compares its complete output.

The Roblox client, persistence adapter, proposed diagnostics and effect annotations are separate application contracts and are not executed by this VM scenario. A local optional snapshot is used before reading the queued direction; this does not assume mutable properties remain refined across calls or workers.
