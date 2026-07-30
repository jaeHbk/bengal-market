# Changelog

All notable changes are recorded here. This project follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and uses
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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

[Unreleased]: https://github.com/jaeHbk/bengal-market/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/jaeHbk/bengal-market/releases/tag/v0.1.0
