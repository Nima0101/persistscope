# Security policy

Security fixes target the latest released 0.1.x version. Earlier prereleases and unreleased commits have no separate support commitment.

Report vulnerabilities through [GitHub private vulnerability reporting](https://github.com/Nima0101/persistscope/security/advisories/new). Include a minimal synthetic scenario, the version, build settings, and observed behavior. Do not put credentials or sensitive file contents in reports. If private reporting is unavailable, open a public issue requesting a private contact without disclosing exploit details.

We aim to acknowledge reports within seven days; this is a maintainer goal, not a service guarantee.

## Boundaries

PersistScope is an in-memory model checker. Scenario names do not access host files. The CLI reads the explicitly selected input file and emits results to standard output; it has no networking, shell invocation, dynamic plugin loading, or filesystem adapter.

The parser and library reject oversized and malformed inputs. Exploration has an explicit decision-node budget. These bounds reduce resource exposure but do not make the process a sandbox. Run untrusted files with ordinary process limits where necessary.

C++ invariant and cancellation callbacks are trusted application code. They can block, allocate memory, or perform arbitrary application actions. Exceptions propagate to the embedding caller.

Results and witnesses contain input values. Treat reports with the same confidentiality as their scenarios. Use synthetic contents in public bug reports.

A false PASS caused by an implementation defect is a correctness issue with potential security consequences. A result outside the documented persistence assumptions is not evidence of application durability. See [the threat model](docs/threat-model.md).
