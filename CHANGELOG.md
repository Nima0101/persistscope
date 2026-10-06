# Changelog

Changes follow semantic versioning. During 0.x, a minor release may change the public API or model contract; patch releases preserve them except for documented correctness fixes.

## 0.1.0

Initial release:

- C++20 library and CLI for bounded crash-state exploration.
- Versioned ordered-file model with stable inode identity and namespace dependency tracking.
- Record-atomic replacement and byte-granular ordered patches.
- Explicit sync boundaries, acknowledgement, and uninterrupted recovery programs.
- Deterministic counterexamples and validated witness replay.
- Separate pass, fail, invalid-input, and incomplete outcomes.
- Synthetic broken/fixed replacement and multi-file recovery examples.
