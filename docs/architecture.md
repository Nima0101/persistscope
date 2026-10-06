# Architecture

PersistScope transforms a finite protocol into a family of persistence event graphs. It explores legal crash images and applies an application invariant to each recovered image. The core uses only the C++ standard library and does not access the host filesystem.

## State and identity

The initial snapshot is fully durable. A volatile namespace maps flat names to stable inode identities. File contents belong to inodes, so a rename changes a name without moving or duplicating the data history.

Each run operation is validated and applied to volatile state. Mutations append persistence events. A data event depends on the previous data event for its inode. A namespace event depends on the preceding event for every name it touches. Rename touches both source and destination as one atomic event.

These dependencies define **ordered-file-v1**, an explicit abstraction rather than a universal POSIX guarantee. Data and namespace ordering are independent. A newly durable filename can therefore refer to an inode whose content update did not survive.

## Sync and crash cuts

File sync forces earlier data mutations for the addressed inode. Directory sync forces earlier namespace mutations. Their dependency closures are forced as well.

At cut zero and after each run operation, the checker enumerates dependency-closed event subsets containing all forced events. Unsynced events can either survive or disappear subject to dependencies. Enumeration is deterministic and visits exclusion before inclusion.

Applying selected events to initial durable state reconstructs the crash snapshot. Optional recovery operations run to completion against that image; the invariant then receives the recovered snapshot and the cut's acknowledgement state.

An invalid recovery operation on a reachable image becomes a counterexample. Recovery itself is not interrupted or crash-tested in this version.

## Atomicity choices

`write` is an atomic replacement of a content value in the model. `patch` produces ordered one-byte events within an existing value and cannot extend the file. A patch can persist only partly; later data events retain per-inode ordering.

Neither operation claims to reproduce all possible real storage tearing, sector atomicity, writeback reordering, or device behavior. Choose scenarios whose assumptions fit the code you intend to review.

## Exploration and evidence

`PASS` requires exhausting every legal modeled outcome at every cut. `FAIL` carries one concrete violating image. `INCOMPLETE` means the node budget or cancellation stopped the search before completeness was established.

Witnesses contain the cut, selected event IDs, acknowledgement state, pre-recovery and post-recovery snapshots, and a reason. Replay checks the witness against the scenario, including event bounds, dependencies, and required durable events.

The algorithm can take exponential time in the number of independent events. A node budget measures exploration work, not unique filesystem snapshots; multiple selections may yield identical contents. No unbounded state cache is retained.

## Ownership and extension

The [public API](../include/persistscope/persistscope.hpp) owns strings and containers by value. A check's mutable state is local. Separate checks may run concurrently when their callbacks and captured state are safe to use concurrently.

The extension point is a trusted C++ invariant callback. Scenario recovery remains declarative. There are no dynamically loaded plugins or arbitrary CLI checker commands.

Changing persistence dependencies changes the meaning of PASS. New semantics should receive a new model identifier and regression tests; they should not silently reinterpret an existing trace.
