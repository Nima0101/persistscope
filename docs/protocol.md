# Scenario and CLI contract (v1)

The CLI only reads the named scenario file. All names in scenarios refer to virtual files;
operations never touch those names on the host. Use `persistscope --help` or `--version`.

```text
persistscope check FILE [--max-nodes N] [--json]
persistscope replay FILE CUT EVENT_IDS [--json]
```

`N` is an unsigned decimal integer from 1 through 10000000 (default 100000).
`CUT` counts completed run operations, starting at zero. `EVENT_IDS` is an ascending,
comma-separated list of zero-based mutation event IDs, or `-` for the empty set.
Replay rejects missing forced events, invalid IDs, and non-closed dependency sets.
Use the exact cut and event IDs emitted in a witness; operation numbers and event IDs
differ, particularly for byte patches. Replay evaluates the same recovery and assertions.

Exit codes: **0** complete PASS or replay satisfying assertions; **1** concrete FAIL;
**2** invalid input or I/O failure; **3** INCOMPLETE search. Never treat INCOMPLETE as PASS.
Unknown/repeated options, unsupported versions and trailing arguments are errors.

## Text format

Files are at most 65536 bytes of ASCII. Lines separate instructions. Blank lines and
`#` comments are allowed. Spaces/tabs separate tokens; CRLF is accepted. A double-quoted
token can contain spaces or `#`; only `\"` and `\\` escapes exist. Empty data is `""`.
Tokens containing quotes or backslashes must be quoted. Names are 1–64 ASCII letters,
digits, `_`, `-`, or `.`, excluding `.` and `..`. Contents are limited to 4096 bytes.
Each program has at most 128 operations; the run can generate at most 256 mutation events.
Recovery allows at most 256 potential mutation events (patch bytes plus mutating operations).
Initial state has at most 64 files. The complete scenario is validated before exploration.

```text
persistscope 1
initial config old
run
create staging
write staging new
fsync staging
rename staging config
sync_dir
ack
recover
remove_if_exists staging
check
expect config old new
expect_after_ack config new
```

`initial NAME DATA` declares an existing, fully durable file. Duplicate names fail.
The required `run` section follows initial declarations. The optional `recover` section
follows run. The required `check` section is last. An empty check section deliberately
asserts no application invariant; it still detects recovery errors.

| Run operation | Meaning |
| --- | --- |
| `create NAME` | Create a fresh empty inode and namespace entry; name must be absent. |
| `write NAME DATA` | Replace contents of an existing inode; one atomic data event in this model. |
| `patch NAME OFFSET DATA` | Update existing bytes in order, one event per byte; cannot extend. |
| `rename SOURCE DESTINATION` | Atomically move the name, replacing destination if present. |
| `remove NAME` | Unlink an existing name. |
| `fsync NAME` | Force earlier data mutations on this inode. |
| `sync_dir` | Force earlier namespace mutations. |
| `ack` | Mark caller-observed acknowledgement; remains set at subsequent cuts. |

Recovery supports the same operations plus `rename_if_exists SOURCE DESTINATION` and
`remove_if_exists NAME`. These conditional operations do nothing if the source is absent;
all arguments still undergo validation. Recovery runs once, uninterrupted, on every crash
snapshot. Operational recovery failures are counterexamples. Recovery does not model a
second crash; sync operations there do not prove repeated-crash safety.

| Assertion | Meaning after recovery |
| --- | --- |
| `expect NAME VALUE...` | File exists and equals one of the listed byte strings. |
| `expect_after_ack NAME VALUE...` | Same, only if the run acknowledged before the crash. |
| `exists NAME` | File exists (empty contents are allowed). |
| `equal LEFT RIGHT` | Both files exist with identical contents. |

Assertions are conjunctive and evaluated in source order. The first violated assertion
provides the witness reason. Assertions after acknowledgement are additional constraints,
not replacements for unconditional assertions. See [architecture](architecture.md) for
persistence semantics and [threat model](threat-model.md) for bounds and assumptions.

## JSON output

All output objects carry `schema_version: 1`, `model: "ordered-file-v1"` and `status`.
Check statuses are `PASS`, `FAIL`, `INCOMPLETE`; errors use `INVALID`. Consumers should
ignore unknown object fields but reject unknown schema/model versions. Text diagnostics
and JSON safely escape controls; witnesses disclose modeled content, so do not use secrets.

```json
{"schema_version":1,"model":"ordered-file-v1","status":"FAIL","nodes":28,"outcomes":13,"cuts_completed":5,"witness":{"cut":5,"durable_events":[1],"crashed":{"config":"old"},"recovered":{"config":"old"},"acknowledged":true,"reason":"expect_after_ack failed for config"}}
```

This is the output for `examples/atomic-broken.pscope` in v0.1.0.
`nodes` counts bounded search decisions, `outcomes` counts evaluated snapshots, and
`cuts_completed` counts fully explored cuts. A witness is `null` when none was found.
Replay uses the same envelope with `nodes: 0`, `outcomes: 1`, `cuts_completed: 0` and a
witness even when assertions hold (empty `reason`). `INVALID` has an `error` string and
no search counters. Byte values outside printable ASCII are encoded as `\u00XX` in
JSON; the CLI scenario language itself only accepts ASCII.
