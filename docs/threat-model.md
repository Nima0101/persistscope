# Threat model

## Assets and trust

The assets are correct verification results, process availability, and scenario confidentiality. The CLI may receive a malicious scenario. The embedding application's C++ code, compiler, standard library, and host OS are trusted.

An attacker can influence scenario text, virtual filenames, contents, operation counts, and requested exploration bounds. They cannot supply executable callbacks through the CLI.

## Defenses and limits

| Threat | Boundary |
|---|---|
| Path traversal or overwriting real files | Names are flat validated virtual identifiers. Modeled operations never perform host I/O. |
| Command injection | No shell, subprocess runner, or executable plugin interface exists. |
| Parser ambiguity | Versioned grammar rejects unknown operations, unsupported escapes, malformed arguments, and trailing options. |
| Memory or CPU exhaustion | Input size, value size, operation/event counts, and exploration nodes are bounded. |
| False success on resource exhaustion | INCOMPLETE is separate from PASS and has a nonzero CLI exit code. |
| Invalid replay evidence | Replay rechecks event ranges, dependency closure, and forced durability. |
| Sensitive report contents | Reports include input contents; users must protect them as input data. |
| Unsafe callback | Embedded callbacks are trusted code running synchronously with host privileges. |

The default search budget is 100,000 nodes; the maximum accepted budget is 10,000,000. Scenario text is limited to 64 KiB, content values to 4,096 bytes, each operation program to 128 operations, and run mutation events to 256. Names have a restricted ASCII grammar and at most 64 characters. See the versioned [CLI contract](protocol.md) for input details.

Resource bounds are not sandboxing or a wall-clock deadline. A trusted callback may hang, and a large permitted search can consume meaningful CPU. Embedders can use the cancellation callback and host process limits.

## Correctness threats

The largest risk is an unwarranted interpretation of PASS. The modeled trace may omit a real operation; the persistence assumptions may be stronger than the target storage system; or the invariant may be too weak. The checker cannot establish that a handwritten model matches production code.

The implementation must also avoid lost dependencies, incorrect inode/name association, skipped crash cuts, and budget-related false success. Negative controls, witness replay, independent small-model tests, sanitizers, and cross-compiler CI address implementation risk without eliminating it.

## Exclusions

No remote control plane, signatures, credentials, telemetry, or network service exists. Replay means replaying a modeled crash witness, not executing external side effects. Concurrent protocol execution, syscall errors, hostile devices, and crashes during recovery are outside the model.

Report implementation vulnerabilities through [SECURITY.md](../SECURITY.md).

## Optional updater

The exclusions above describe the model checker. The separately built
[safe-update reference](../safe-update/README.md) performs filesystem I/O and
signature verification. Its key provisioning, directory trust, health callback,
anti-downgrade exclusions and storage limitations are documented separately.
