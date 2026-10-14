# CL++ performance checks

The fixtures use the current CL++ language. `run.py` compares five programs
with their Python counterparts, rejects nonzero exits and unstable output, and
validates results before reporting timings. A single finite numeric result may
differ only within a small floating-point tolerance; general text is exact.

Run `python benchmarks/run.py <compiler> --runs 5 --markdown`.
The reported runtime includes process startup, compilation and execution.
The separate `--check` column includes startup and compile checking without
executing the program. These values are not isolated VM throughput.

Run `python benchmarks/check_scaling.py <compiler> --runs 3 --report report.json`
to measure 100, 1,000 and 10,000 file-scope declarations independently of runtime.
`build/ucrt64/tests/frontend_profile.exe 10000` measures lexer, parser, binder
and typechecker phases within one process. Build the `frontend_profile` target
with stress tools enabled. Phase profiling includes the file-scope module slots
used by normal analysis.

Record compiler configuration, sample count and host workload with results.
The current development build is Debug; do not present its timings as release
performance. Compare medians over repeated samples on an idle machine.

The completion matrix is `tests/stress/intellisense_fuzz.py`. It reports
functional failures separately from latency budgets, and accepts
`--strict-latency` to fail on any budget violation. Requests have a 30-second
timeout. `--category Large_File_Completion` isolates the 10,000-binding case.
`--report` saves JSON metrics and individual latency outliers.

`tests/stress/core_validation.py <compiler> --fuzz 250 --report report.json`
checks current features, rejected syntax and deterministic malformed inputs
in separate processes with timeouts. Successful bounded cases are evidence
for those inputs, not proof that all vulnerabilities have been eliminated.
