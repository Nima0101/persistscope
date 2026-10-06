# Engineering deep dive

The useful question is not whether a rename happened before a crash. It is which namespace and data events could have survived, and whether recovery can make sense of that combination.

## The invariants that carry the design

**Identity survives renaming.** A file's bytes belong to an inode identity, not a path string. Otherwise, renaming a file and then syncing or modifying it can detach its data from the wrong history. Tests should exercise rename, overwrite of a destination, and subsequent data mutation.

**Legal selections are dependency closed.** If an event survives, every prerequisite must survive. Per-inode data chains and conflicts on namespace names supply those prerequisites. A graph with too many dependencies hides bugs; a graph with too few admits states outside the declared model.

**Sync changes the allowed outcomes.** A sync does not merely add an event that may be lost. It constrains all later cuts to retain the relevant preceding events. File sync and directory sync deliberately constrain different event families.

**Acknowledgement is observable, not durable state.** Before acknowledgement, old or new data may satisfy an atomic-replacement contract. After acknowledgement, allowing old data would hide lost acknowledged work. Keeping this bit in the context lets the same snapshot violate one cut's contract while satisfying another.

**Completeness is a result property.** A search with no counterexample can still be unfinished. Node exhaustion and cancellation produce INCOMPLETE. Invalid scenarios must not be accepted just because an earlier prefix already contains a counterexample.

## Why an event DAG?

Enumerating every subset is easy to describe but includes impossible selections and duplicates work. Dependencies allow the explorer to prune selections whose prerequisites were excluded. They also make witness validation independent of how the search reached the witness.

The model still has exponential worst-case behavior. Independent files create independent choices; a chain of ordered writes creates fewer legal combinations. Bounded deterministic exploration is an honest fit for short protocol tests. It is not a scalable substitute for testing an entire storage engine workload.

## Where the abstraction trades precision for use

A record-atomic `write` makes small manifest and replacement examples readable, but it can conceal real torn writes. The separate `patch` operation exposes byte-level intermediate contents while preserving this model's per-inode ordering. Neither describes arbitrary device reordering.

The flat namespace avoids importing complicated directory-tree and hardlink behavior into an initial model. Namespace mutations depend on names they conflict on; unrelated names may persist independently. This is more useful for multi-file protocols than treating the whole namespace as an always-atomic snapshot.

Recovery runs once without interruption. The prepared journal example starts with durable redo files and rolls forward missing replacements. It checks that commit phase, not journal preparation, marker durability, or arbitrary repeated recovery crashes. Expanding those claims requires expanding the scenario and model.

## Concurrency and recovery reasoning

The checker itself uses local state and synchronous callbacks. Independent checks can execute in different threads; shared callback captures need application synchronization. This does not mean it explores concurrent application operations. A scenario supplies a single ordered run.

Recovery sees only the reconstructed crash snapshot. Conditional operations such as `rename_if_exists` let it tolerate work that already survived. An unconditional operation can fail on one reachable image, exposing a missing recovery case. The original acknowledgement context survives recovery because it represents what the caller observed before the crash.

## Verify the claims

Run the README commands. The broken replacement must fail, the fixed replacement must pass, and the journal example must recover consistently. Inspect the JSON witness and replay its event selection using the CLI contract.

Reduce the node budget until the result becomes INCOMPLETE. It must never become PASS simply because the search stopped early. Run tests under ASan/UBSan and on the supported compiler matrix to check memory safety and portability separately from model correctness.

Useful live experiments:

- Remove `sync_dir` from the repaired replacement and explain the resulting witness.
- Remove one redo file from the prepared transaction and predict the failing cut.
- Replace a content write with a patch and choose an invariant that rejects torn values.
- Add a custom C++ invariant relating three files, then construct a missing-sync counterexample.
- Change a model dependency only after writing a test that distinguishes the old and new semantics.

## Security and API trade-offs

Keeping CLI assertions declarative avoids executing user-supplied commands. The C++ API remains flexible through trusted callbacks. Bounded input and exploration protect availability, but callbacks are not sandboxed and reports intentionally retain evidence contents.

The strongest useful claim remains narrow: complete exploration establishes the supplied invariant for the supplied scenario under the named model. Connecting a trace to real application code, additional storage semantics, and crashes during recovery are future work with separate proof obligations.
