# Decision: PersistScope

Selected 2026-10-06 after comparing eight candidates against ten criteria.

**Value proposition:** Find a crash state that breaks your persistence protocol before that protocol reaches production.

A developer maintaining a checkpoint, manifest, journal, or configuration store can encode its update sequence and recovery invariant, get a deterministic counterexample, and keep the corrected protocol in CI. This is useful even without integrating a new runtime library into the application. PersistScope targets protocol review and regression testing; it does not infer or verify application syscalls.

The 90/100 score reflects useful failure demonstrations, a dependency-free C++20 implementation, cross-platform model execution, and a small security boundary. Local linearizability checking scored 82 but competes directly with mature Porcupine implementations. Durable Edge Execution scored 78: valuable, but signed authorization, OS adapters, reconciliation, and operational safety create a much larger v0.1 surface. Its most compelling ideas also overlap existing outcome-ledger projects. These are scope judgments, not claims that other projects are inferior.

## Why this is more than an atomic-rename demo

The reusable engine explores dependency-closed durable event sets. Independent file and namespace mutations may reorder; inode identities survive rename. Byte patches can tear. Multi-file assertions, caller-observed acknowledgement, and an explicit recovery phase expose more than single-file replacement mistakes. The CLI and C++ API share the same model. Search exhaustion is an explicit incomplete result, never a pass.

The novelty is packaging and accessibility, not a new verification algorithm. Existing crash-testing research and tools are credited in the landscape. No claim of physical-filesystem crash safety follows from a passing model.

## Name and license

Public web searches for PersistScope and persist-scope and GitHub repository search for PersistScope found no competing repository on 2026-10-06. The proposed `Nima0101/persistscope` repository did not exist. This is a practical collision check, not trademark clearance.

MIT permits embedding the checker in proprietary and open-source test suites with minimal obligations. All product code is written for this repository; runtime dependencies are limited to the C++ standard library.

## Delivery scope

See [design](../docs/design.md). The release includes a bounded explorer, strict versioned trace parser, replayable witnesses, a CMake library package, examples for replacement/torn writes/multi-file recovery, native tests, CLI integration tests, benchmark harness, security guidance and three-host CI. Broader filesystem models, syscall import and repeated recovery crashes are future work.
