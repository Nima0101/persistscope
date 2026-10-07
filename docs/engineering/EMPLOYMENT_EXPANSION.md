# Employment-signal expansion

PersistScope stays a crash-consistency model checker. The expansion adds a separately bounded safe-update reference implementation so the repository demonstrates both formalized failure reasoning and production-oriented C++20 engineering.

## Target evidence
The reference updater should cover staged payload verification, an authenticated manifest/signature boundary, activation, interruption/fault injection, health verification and rollback. Cross-platform claims require Windows, macOS and Linux implementation/tests; otherwise state the narrower support precisely.

## Model connection
Maintain concrete PersistScope scenarios for the update protocol. Include at least one intentionally broken protocol that produces a replayable counterexample and a corrected protocol that passes under the documented model. Real filesystem semantics and modeled semantics must not be conflated.

## Done means
The updater has a library/API and runnable example, deterministic tests, interruption injection at meaningful state transitions, negative verification cases, package/consumer tests and CI coverage. Existing PersistScope behavior and model assumptions remain intact.
