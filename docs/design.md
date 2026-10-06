# PersistScope v0.1 design

PersistScope finds crash states that violate an application's persistence protocol, using a portable C++20 library and a bounded, deterministic command-line checker.

## Users and scope

Storage-library authors, developers implementing config/checkpoint replacement, and reviewers of multi-file commit/recovery protocols can turn a short I/O protocol into an executable regression test. No elevated privileges, filesystem mounts, services, or runtime dependencies are needed. The five-minute demonstration shows a rename that acknowledges a save without persisting its directory, then verifies the repaired protocol.

This is an abstract model checker, not syscall interception, a filesystem implementation, a production storage library, or proof about a particular filesystem. It does not model concurrent application threads, failed syscalls, malicious disks, hardlinks, directory trees, or crashes during recovery in v0.1. Hosts running the checker need not implement the modeled persistence semantics.

## Model: ordered-file-v1

A volatile namespace maps flat virtual names to stable inode identities. Each inode contains bytes. Initial files are fully durable. Mutation events form a DAG. Data mutations to one inode depend on the preceding data mutation to that inode. Namespace mutations depend on preceding mutations to each name they touch. A rename is one atomic namespace event touching source and destination. Namespace persistence does not imply data persistence.

A write replaces a file's contents atomically **in this model**. A patch writes one byte per event, in offset order, allowing torn patches. Patch cannot extend a file. Use patch to test in-place update tearing; write's atomic-record assumption must be suitable for the protocol being modeled. Data events on different inodes and namespace events on unrelated names can persist independently. This intentionally separates record-atomic replacement from ordered byte patches; it is not a universal POSIX model.

A successful file sync forces all earlier data events for that inode and their dependencies durable. A directory sync forces all earlier namespace events and their dependencies durable. Neither invents a dependency between data and names. At a crash cut, every dependency-closed subset containing forced events is a legal modeled persistence outcome. Unsynced events may persist. Renamed/unlinked inode identities stay stable. Each cut includes cut zero and the state after every successful operation, including acknowledgement.

The explorer enumerates deterministic exclude-first event decisions. It evaluates assertions against the recovered snapshot, optionally after an uninterrupted recovery program. Acknowledgement is caller-observed metadata: after an ack, stronger assertions apply. Witnesses include the cut, durable event IDs, and resulting file contents. Replay revalidates closure, forced events and bounds. PASS requires complete exploration of every cut. FAIL means a concrete counterexample was found. INCOMPLETE means the search budget was exhausted or cancellation requested, and is never success.

## API and CLI

Public C++ API: value types for operations, scenario, snapshot, limits, result and witness; check and replay functions; caller-supplied invariant and cancellation callbacks, plus a declarative recovery program. Core has no I/O, hidden global state, threads, or shell execution. Inputs own their strings; snapshots own their maps. Callbacks run synchronously and must be trusted and bounded. Independent checks can run concurrently with separate state.

CLI: `persistscope check FILE [--max-nodes N] [--json]`; `persistscope replay FILE CUT EVENT_IDS`; `persistscope --version`. Exit codes: 0 pass/replay valid, 1 invariant violation, 2 invalid input, 3 incomplete. JSON format is versioned. Scenario format starts with `persistscope 1`, followed by initial files, `run`, operations, optional `recover`, and `check` assertions. Unknown versions, operations, options and trailing arguments fail closed. Witnesses may contain input data and should not be fed secrets.

## Resource and security boundaries

Virtual names are 1–64 ASCII letters/digits/underscore/hyphen/dot, excluding dot and dot-dot. No path traversal or host filesystem action exists in the model. Inputs: 64 KiB scenario text, 128 operations per program, 64 initial files, 4096 bytes per content value, at most 256 mutation events per run. Core validates independently of CLI. Default exploration budget is 100000 decision nodes; hard maximum 10000000. Cancellation is checked during traversal. No recursive parser; DFS depth is bounded by 256. Oversized model/recovery input fails with an error. One witness is retained; no unbounded state cache.

## Implementation and portability

C++20, CMake 3.20+, standard library only; Python 3.9+ for integration/benchmark scripts. GCC, Clang and MSVC with warnings-as-errors. ASan/UBSan on Linux and macOS; thread ownership tested via independent concurrent calls. Library install/export supports downstream CMake consumers. CLI reports bytes safely escaped in JSON; normal text does not echo raw control bytes. MIT license minimizes embedding friction; no external runtime code dependencies.

## Performance

Worst-case exploration is exponential, explicitly bounded by decision-node count. Benchmark chains (dependency pruning) and independent events (state explosion) with sample size, explored nodes, wall time and toolchain recorded. No latency promises before measurement. A budget-limited result is a first-class result.
