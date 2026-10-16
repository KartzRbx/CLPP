# Core and distribution validation

Updated 2026-10-16. The current source contract and release checklist are in
[RELEASE_GATES.md](RELEASE_GATES.md). Exact results and known limitations are in
[the QA report](../../.qa/REPORT.md); historical counts are not current totals.

The current normal suite passed 544 checks. New native installer discovery passed
three isolated scenarios; 192 generated arithmetic, control-flow and constrained
generic-closure scenarios matched independent Python expectations in both Debug
and Release. The final instrumented/platform/artifact runs are being completed.

The current documentation corpus retains 19 explicitly excluded target blocks.
Three syntax proposals were deferred directly by the author. Other excluded
blocks require physical/native host effects or external application policies.
They are documented limitations, never passing-test evidence. No Cluaupp-specific
compiler protocol, Roblox policy or new official Luau backend is added to the VM.

Delivery uses a distinct clpp-vm executable, portable platform ZIPs, a pinned
checksum-verifying Windows installer and the matching VSIX. The publication
workflow requires successful CI and documentation for the exact main commit,
checks archive/extension identity and compares the Windows compiler bytes before
publishing. Actual downloads and the deployed site must then be checked.
