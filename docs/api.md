# C++ API and extension points

Include `<persistscope/persistscope.hpp>` and link `PersistScope::core`.
The core uses only the C++20 standard library. It performs no host I/O and
starts no threads. All strings, operations and snapshots own their values.

```sh
cmake --install build --config Release --prefix install
cmake -S examples/embedding -B consumer -DCMAKE_PREFIX_PATH="$PWD/install"
cmake --build consumer --config Release
ctest --test-dir consumer -C Release --output-on-failure
```

On PowerShell use `-DCMAKE_PREFIX_PATH="$pwd/install"`.
The [embedding example](../examples/embedding/main.cpp) is also tested by CI.
The installed CMake package supports `find_package(PersistScope 0.1 CONFIG REQUIRED)`.

## Inputs

`Scenario` contains a durable initial `Snapshot` (`map<string, string>`), a run
program, and an optional recovery program. `Operation` is `{kind, path, value,
offset}`. `value` is replacement content for `write`, byte content for `patch`,
and destination for `rename`/`rename_if_exists`. Other operations require an empty
value; only `patch` accepts a nonzero offset. `create` creates an empty file.
Conditional operations are recovery-only. The [protocol contract](protocol.md)
describes the same operations in text form.

The CLI accepts printable ASCII values; the C++ API accepts arbitrary bytes,
including NUL. Virtual names have the same restricted ASCII syntax in both.
The text input limit does not apply to the API; file, value, operation and event
limits do. Recovery's 256-event bound is conservatively checked by summing patch
bytes and potential mutating operations, including conditional operations that
might turn out to be no-ops.

## Checking and replay

`check(scenario, invariant, limits)` returns `Result` with `Status::pass`,
`Status::fail`, or `Status::incomplete`. It validates the entire run before
exploration, even if an early invariant could fail. Invalid input throws `Error`.
An operational recovery error is a failure witness; `recovered` then contains
the state after recovery's last successful operation.

The invariant receives `const Snapshot&` and `const Context&`. Return
`std::nullopt` to accept, or an explanation to reject. An empty explanation still
rejects. The context's `cut` counts completed run operations and `acknowledged`
indicates whether a run acknowledgement occurred before the crash. A recovery
acknowledgement does not rewrite what the original caller observed.

`Limits::max_nodes` defaults to 100,000 and accepts 1–10,000,000. Optional
`Limits::cancelled` is polled before each search node. It must return promptly.
Exhaustion/cancellation returns incomplete unless a violation was already found.
All callback exceptions propagate unchanged. Recovery and invariant evaluation
are synchronous; there is no preemptive timeout for application callbacks.

`Result::outcomes` counts evaluated durable-event sets, including sets that
materialize identical snapshots. `nodes` counts all DFS visits, including internal
decisions; `cuts_completed` counts fully explored prefixes. No state cache or
probabilistic sampling contributes to PASS. The first violation stops exploration.

`replay(scenario, cut, durable_events, invariant)` checks the witness set is
ascending, in range, dependency-closed, and contains all forced events. It returns
one `Witness`, including before/after recovery snapshots and any violation reason.
Replay validates one outcome; it does not establish a complete PASS. Event IDs
are deterministic for the same scenario and model version, not stable across
scenario edits. Keep the scenario with a witness.

## Extend without a plugin loader

Supply domain-specific invariants as C++ callbacks: parse a modeled manifest,
check checksums, relate multiple files, or validate generation counters. Construct
scenarios from your test workload generator. Callbacks are trusted native code;
there is no dynamic plugin or scripting engine in the CLI.

Independent checks can execute concurrently. The library has no shared mutable
state, but callbacks must synchronize their own shared captures. Do not mutate
scenario data from a callback or another thread while a check is running.

The initial release follows semantic versioning with an explicit model and CLI
schema identifier. Before 1.0, incompatible C++ API changes require a minor-version
bump. Bug fixes may change outcomes for previously mischecked scenarios and will
be documented; never silently change the model assumptions under the same name.
