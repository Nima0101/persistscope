# Exploration benchmark

This measures the model checker, not disk performance. The benchmark explores
all cuts in sequences of 8, 12 and 16 writes. A chain repeatedly writes one
inode; the independent case writes a different inode each time. The invariant
always accepts, so no early counterexample shortens the search.

```sh
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DPERSISTScope_BENCHMARKS=ON
cmake --build build-bench --config Release
./build-bench/bin/persistscope_bench
```

Windows uses `build-bench\bin\persistscope_bench.exe`. Each row has seven samples,
no warm-up exclusion, and a one-million-node budget. CSV includes median, minimum,
maximum, exact nodes and outcomes. All rows must finish with `pass`.

The independent case grows exponentially: 16 events across all cuts produce
131,071 outcomes. Dependencies prune the chain to 153 outcomes. No deduplication
cache hides memory growth; each outcome includes snapshot construction and an
uninterrupted recovery pass. Budget nodes include internal DFS decisions, not
just outcomes.

See [measured results](results.md). Measurements are local observations, not
throughput promises or comparisons with other tools.
