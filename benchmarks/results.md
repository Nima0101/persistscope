# Measured results — 2026-10-06

Environment: macOS 26.6.2, arm64 (Apple Silicon); AppleClang 21.0.0.21000101;
CMake 4.4.3; Ninja; CMake Release configuration. CPU brand information was not
available inside the execution sandbox. This was a shared development machine,
not an isolated benchmark host; other validation activity may affect timings.

Command: `./build-release/bin/persistscope_bench`, after configuring with
`-DCMAKE_BUILD_TYPE=Release -DPERSISTScope_BENCHMARKS=ON`.
Seven samples per row, full exploration, one-million-node bound. No samples were
discarded. Durations include scenario validation, every cut, snapshot materialization,
and the accepting invariant. They exclude process startup and scenario construction.

| Shape | Events | Nodes | Outcomes | Median ms | Min ms | Max ms |
|---|---:|---:|---:|---:|---:|---:|
| chain | 8 | 165 | 45 | 0.142 | 0.122 | 0.335 |
| chain | 12 | 455 | 91 | 0.252 | 0.246 | 0.271 |
| chain | 16 | 969 | 153 | 0.424 | 0.421 | 0.435 |
| independent | 8 | 1013 | 511 | 6.652 | 6.023 | 9.469 |
| independent | 12 | 16369 | 8191 | 92.527 | 62.610 | 139.607 |
| independent | 16 | 262125 | 131071 | 3144.424 | 2923.076 | 3247.745 |

The counts are reproducible model results; wall times depend on compiler, CPU and
load. In particular, the default 100,000-node CLI budget cannot completely explore
the 16-independent-event case. Increasing the budget buys coverage, not a change
in the exponential bound. No comparison against another tool is implied.
