# Reproducibility

## Reproducible Logical Results

Given the same tagged source, dependency pins, release build, recording, and
CLI options, replay must produce identical logical counts and checksum.
Wall-clock duration and latency are measurements and are not expected to be
bit-for-bit identical.

Fixture generation additionally requires the same product, event count, and
generator version. It must not depend on wall-clock time, process ID, host
entropy, locale, or unordered iteration.

## Build From a Tag

```sh
git clone --branch v0.1.0 --depth 1 \
  https://github.com/jaeHbk/bengal-market.git
cd bengal-market

cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBENGAL_MARKET_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

This example becomes valid when `v0.1.0` is published. Verify release checksums
against GitHub before using a downloaded artifact.

## Dependency Pins

Release source pins Bengal and fetched third-party dependencies to immutable
commits associated with published releases. It must not fetch a moving branch.
Record resolved revisions from configure output and preserve `CMakeCache.txt`.

A local Bengal override changes the experiment:

```sh
cmake -S . -B build \
  -DBENGAL_SOURCE_DIR=/absolute/path/to/bengal
```

Record the path, Bengal `git rev-parse HEAD`, dirty state, and resulting binary
hash. Do not describe an override build as an unmodified release.

## Generate and Verify a Fixture

```sh
LC_ALL=C ./build/bengal-market generate \
  --product BTC-USD --events 1000000 \
  --output fixture-1m.jsonl
sha256sum fixture-1m.jsonl

LC_ALL=C ./build/bengal-market replay \
  --input fixture-1m.jsonl --engine bengal
LC_ALL=C ./build/bengal-market replay \
  --input fixture-1m.jsonl --engine standard
```

Both replays must process the same events and produce the same checksum. Save
stdout, stderr, exit status, and fixture digest.

## Evidence Bundle

```text
evidence/
  README.txt
  source.txt
  configure.txt
  CMakeCache.txt
  environment.txt
  fixture.sha256
  command.txt
  run-01.json
  run-02.json
  ...
```

`source.txt` identifies the Bengal Market tag/commit, dirty state, Bengal pin,
and dependency pins. `environment.txt` contains metadata listed in
`BENCHMARKING.md`. Never hand-edit raw run files.

## Release Artifact Verification

```sh
sha256sum --check SHA256SUMS
tar -tzf bengal-market-v0.1.0-linux-x86_64.tar.gz
```

Release binaries are convenience artifacts. Building tagged source in a
controlled toolchain provides stronger reproducibility than expecting binaries
from different compiler and libc versions to be identical.

## Limits

The public feed is external and mutable. A live capture cannot be regenerated
from a timestamp or sequence range, and provider terms can limit redistribution.
Use generated fixtures for portable tests and capture once/replay many for
provider-specific investigations.

Scheduling, kernel behavior, compiler code generation, and system load affect
timing. Separate stable logical output from variable performance measurements.
