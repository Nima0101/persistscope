# Safe-update reference v0.1

A separate C++20 library implements signed, staged payload activation on POSIX
filesystems. PersistScope's in-memory checker, CLI and dependency-free core are
unchanged. This reference performs real filesystem I/O and uses OpenSSL 3's EVP
API for SHA-256 and Ed25519. Windows is explicitly unsupported.

## Invariant and state transitions

Only a payload whose canonical manifest matches its bytes and whose Ed25519
signature verifies against the caller's trusted public key may become a trial.
A single state record holds the stable and trial content hashes. A trial is active
until a health callback confirms it; failure or `recover()` rolls back to stable.
A restart must call `recover()` before exposing `active()` to consumers. A crash
after a healthy confirmation may retain the new stable value; before confirmation,
recovery restores the old one. Before first confirmation, no stable version exists.

The caller creates a trusted directory and constructs `safe_update::Store`.
A lifetime exclusive nonblocking `flock` rejects competing updater processes.
The object is noncopyable and is not thread-safe. `stage(payload, manifest,
signature, public_key)` authenticates before writing, syncs payload data and its
directory entry, then atomically replaces the state file with a trial record.
`confirm(health)` reads/hash-checks the active bytes, calls the trusted callback,
and persists either the new stable hash or rollback. Callback exceptions leave
a recoverable trial. `recover()` is idempotent and clears unfinished trials.

Every state replacement writes a temporary file, calls `fsync`, renames within
the same directory, then syncs the directory before returning. Errors propagate;
callers must stop and reopen/recover after an I/O exception because the operation
may have taken effect. Content-addressed payload files are retained for rollback;
garbage collection is not implemented. Reads recheck the selected payload hash.

## Signed manifest contract

The exact signed bytes are `safe-update-v1\n` + 64 lowercase hexadecimal SHA-256
digits + `\n`. Payloads are at most 1 MiB, the raw Ed25519 public key is 32 bytes,
and signature is 64 bytes. No parsing ambiguity, paths, shell commands, archive
extraction or executable health scripts are accepted through the manifest.
Provisioning the trusted key is the embedding application's responsibility.
Do not accept a key supplied by the same untrusted source as the update.

This format authenticates content, not release freshness, intended product,
semantic version or update authorization. It has no anti-downgrade counter,
key rotation or revocation mechanism. Use a distinct key per trust domain.
Health callbacks are trusted application code and must impose their own deadline.

## Build, tests and consumer

From the repository root with CMake, C++20, Python and OpenSSL 3 development files:

```sh
cmake -S . -B build-agent-verify -DCMAKE_BUILD_TYPE=Release
cmake --build build-agent-verify --parallel 2
safe-update/scripts/verify-local.sh
```

Set `OPENSSL_ROOT_DIR` at CMake configure time if OpenSSL is outside its search
path. The major version is constrained to 3; use a maintained vendor patch release.
The script runs the real updater tests, installs the library, builds/runs the
separate `find_package(SafeUpdate)` consumer and checks/replays the model pair.
The installed-library consumer generates an ephemeral demonstration key, stages
signed payloads, rejects a synthetic unhealthy candidate, reopens the store to
prove restart rollback, and confirms a healthy replacement. Run
`safe-update/build/consumer/consumer` after the script to repeat that example.
Its content-based health predicate is synthetic; an embedding application must
supply its own health check. The integration tests add abrupt interruptions.

Tests reject bad signatures, wrong publisher keys, payload/hash mismatch, unknown
manifest versions, oversized payloads, overlapping trials, a second writer,
symlink state files, corrupt payloads and I/O failure. Ten forked child processes
exit at explicit persistence/health boundaries; a new Store recovers and checks
the selected bytes. These are abrupt **process** interruptions, not power cuts.

## Model connection and limitations

`models/activation-fixed.pscope` abstracts the `save()` sequence. The broken control
omits directory sync and must produce FAIL; the test replays its exact witness.
The fixed scenario must exhaust exploration with PASS. The model reasons about
atomic state-record content replacement under `ordered-file-v1`; it does not
prove cryptography, the callback, full multi-file staging, OS syscalls or hardware.
The real implementation additionally persists the payload before state publication.

Trusted local directory ancestry, storage, OS, OpenSSL and application callbacks
are assumptions. Leaf opens reject symlinks, but this is not a sandbox against
an attacker modifying parent directories or hardlinks. A database-like secure
store or arbitrary hostile update directory is outside scope. Temporary files
and obsolete payloads may remain after interruption and are not automatically
removed; retry safely overwrites temporary files in the trusted directory.

The code uses POSIX fsync/rename/flock. Local macOS or hosted Linux tests do not
establish physical power-loss durability, network-filesystem behavior, or Windows
support. macOS `F_FULLFSYNC` and device cache barriers are not implemented; no
hardware durability claim is made. Compare the documented model assumptions
with the actual target filesystem before integrating it.
