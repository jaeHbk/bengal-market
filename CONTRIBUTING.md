# Contributing

Contributions should preserve Bengal Market's narrow product boundary: public,
read-only market-data capture, deterministic replay, and honest pipeline
measurement. Order execution, account access, trading strategies, forecasts,
and profitability claims are out of scope.

## Before Opening a Change

Open an issue for a new command, recording-format change, dependency, or broad
architectural change. Security reports must follow [SECURITY.md](SECURITY.md)
and should not be filed as public issues.

General-purpose bounded-pipeline improvements belong in
[Bengal](https://github.com/jaeHbk/bengal). Provider adapters, recording,
market-data parsing, and product-specific metrics belong here.

## Build and Test

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBENGAL_MARKET_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Run the project-provided sanitizer configuration before requesting review:

```sh
cmake -S . -B build-sanitize -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBENGAL_MARKET_BUILD_TESTS=ON \
  -DBENGAL_MARKET_ENABLE_SANITIZERS=ON
cmake --build build-sanitize --parallel
ctest --test-dir build-sanitize --output-on-failure
```

## Change Requirements

- Use C++20 and keep warnings clean under supported GCC and Clang versions.
- Keep capture input raw and replayable; parsing must not replace the recorded
  provider payload.
- Avoid unbounded allocation and per-event allocation in measured stages.
- Expose queue-full, parse-error, drop, and sequence-discontinuity behavior.
- Add deterministic tests for parsing, replay, and malformed input.
- Update the format version and `docs/DATA_FORMAT.md` for incompatible changes.
- Do not add credentials, private account endpoints, order APIs, or strategies.
- Do not submit benchmark claims without raw output and environment metadata.

## Benchmark Changes

Performance changes need an A/B comparison using the same release build,
fixture, options, machine, CPU policy, and run count. Include output checksums
and drop counts. Report distributions rather than a single best run. Follow
[docs/BENCHMARKING.md](docs/BENCHMARKING.md).

## Pull Requests

Keep changes focused and describe:

- user-visible and failure behavior;
- tests run, compiler, and operating system;
- recording-format or CLI compatibility impact;
- dependency or supply-chain impact; and
- benchmark evidence, when performance is claimed.

By contributing, you agree that your contribution is licensed under the MIT
License in this repository.
