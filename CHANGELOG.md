# Changelog

All notable changes are recorded here. This project follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and uses
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.2.2] - 2026-07-31

### Fixed

- Rocky Linux release packaging uses the base-repository `make` tool instead
  of the unavailable `ninja-build` package.
- The pinned curl transport omits unused verbose diagnostic strings so builds
  against newer OpenSSL headers retain the OpenSSL 3.0 runtime baseline.
- Release automation rejects binaries that require an OpenSSL symbol version
  newer than 3.0.0.

### Compatibility

- Source behavior, CLI, and data formats are unchanged from 0.2.0.
- Linux x86-64 convenience binaries require glibc 2.34 or newer and OpenSSL
  3.0.
- The 0.2.2 binary supersedes the incompatible 0.2.0 binary. No binary was
  published for 0.2.1 because its release workflow failed during dependency
  installation.

## [0.2.1] - 2026-07-31

### Fixed

- Linux release automation builds on Rocky Linux 9 to target glibc 2.34 rather
  than the glibc 2.38 symbols introduced by the Ubuntu 24.04 toolchain.
- Release automation rejects binaries that require a glibc version newer than
  2.34.

### Compatibility

- Source behavior, CLI, and data formats are unchanged from 0.2.0.
- No binary artifact was published because Rocky Linux 9's enabled
  repositories did not provide the configured `ninja-build` dependency.
- The published 0.2.0 tag and artifacts remain immutable.

## [0.2.0] - 2026-07-31

### Added

- Fresh-process `benchmark` command with alternating engine order.
- Atomic evidence bundles containing raw reports, stderr, fixture and
  executable hashes, environment and source metadata, aggregate distributions,
  and a static HTML summary.
- `SIGINT` and `SIGTERM` capture integration coverage using a local WebSocket
  fixture.

### Changed

- Live capture writes to a same-directory `.part` file and atomically publishes
  the requested path only after file and directory synchronization.
- Capture refuses to overwrite either an existing recording or stale partial
  output.
- Handled capture signals finalize valid recording-format v1 output and return
  the conventional signal-derived exit status.
- The pinned curl transport disables unrelated protocols and optional SSH,
  compression, IDN, HTTP/2, and public-suffix dependencies.

### Compatibility

- Recording format remains version 1.
- Replay report schema remains version 1.
- Benchmark manifest schema starts at version 1.
- Bengal remains pinned to `v1.0.0`
  (`1bd5389db9fd802e4c836426dc94288f268cb543`).

## [0.1.0] - 2026-07-30

### Added

- Linux-first public market-data capture product specification.
- Deterministic fixture generation, replay, and pipeline comparison contracts.
- Versioned raw JSON Lines recording format.
- CI, sanitizer, and tagged Linux artifact workflows.

The first release includes:

- read-only Coinbase Advanced Trade WebSocket capture;
- deterministic offline fixture generation and replay;
- Bengal and standard C++ bounded-pipeline comparison;
- data-quality, backpressure, drop, checksum, and latency metrics; and
- reproducible Linux release artifacts.

Dependencies:

- Bengal `v1.0.0` (`1bd5389db9fd802e4c836426dc94288f268cb543`)
- curl `8.17.0` (`400fffa90f30c7a2dc762fa33009d24851bd2016`)
- nlohmann/json `v3.11.3`

[Unreleased]: https://github.com/jaeHbk/bengal-market/compare/v0.2.2...HEAD
[0.2.2]: https://github.com/jaeHbk/bengal-market/compare/v0.2.1...v0.2.2
[0.2.1]: https://github.com/jaeHbk/bengal-market/compare/v0.2.0...v0.2.1
[0.2.0]: https://github.com/jaeHbk/bengal-market/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/jaeHbk/bengal-market/releases/tag/v0.1.0
