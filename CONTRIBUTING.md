# Contributing

Start with a small persistence protocol, an incorrect result, or a clear usability problem. Include a synthetic scenario and the expected result in an issue. Changes to model assumptions deserve discussion before implementation.

## Build and test

Use CMake 3.20+, a C++20 compiler, and Python 3.9+.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

For Clang or GCC sanitizers:

```sh
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DPERSISTScope_SANITIZERS=ON
cmake --build build-sanitize --config Debug
ctest --test-dir build-sanitize -C Debug --output-on-failure
```

Use the repository's clang-format configuration for C++ files. The CI workflow defines the formatter version used for acceptance. Enable `CMAKE_EXPORT_COMPILE_COMMANDS` with a compatible generator to run clang-tidy against your build.

## Review expectations

- Add a regression scenario that fails before a correctness fix.
- Test broken and corrected protocols; a checker that always passes proves nothing.
- Preserve the distinction between FAIL, INCOMPLETE, and invalid input.
- Explain whether a change affects model semantics, enumeration, or only presentation.
- Keep the core free of host I/O and hidden mutable globals.
- Update the model or format version when compatibility requires it.
- Keep fixture data synthetic and dependency additions justified.

The [engineering deep dive](docs/engineering-deep-dive.md) explains invariants worth testing. Pull requests should state the problem, the resulting behavior, relevant limitations, and checks actually run.

By contributing, you agree that your contribution is available under the MIT license. No contributor license agreement is required. Follow the [code of conduct](CODE_OF_CONDUCT.md).


## Test layers

CTest runs targeted engine regressions, CLI integration tests, an independent
Cartesian oracle across 1,024 write/sync protocols, and 10,000 deterministic
generated programs whose failure witnesses are replayed. Linux CI additionally
runs the same harness under Clang libFuzzer with ASan/UBSan. The deterministic
driver is available on hosts whose compiler does not ship the libFuzzer runtime.

Install `requirements-dev.txt` in an isolated Python environment, then run
`python scripts/check_format.py`. Benchmark instructions and observed results are
in [benchmarks](benchmarks/README.md).
