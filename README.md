# Bengal Market

[![CI](https://github.com/jaeHbk/bengal-market/actions/workflows/ci.yml/badge.svg)](https://github.com/jaeHbk/bengal-market/actions/workflows/ci.yml)
[![Sanitizers](https://github.com/jaeHbk/bengal-market/actions/workflows/sanitizers.yml/badge.svg)](https://github.com/jaeHbk/bengal-market/actions/workflows/sanitizers.yml)

Bengal Market is a Linux-first, read-only market-data capture and replay tool.
It records public Coinbase Advanced Trade WebSocket frames, replays recordings
through bounded event pipelines, and compares a Bengal pipeline with an
equivalent standard C++ baseline under a reproducible workload.

The product is an engineering and measurement tool. It does not place orders,
implement trading strategies, predict prices, or make profitability,
market-beating, or broker-latency claims.

## Capabilities

- Capture public WebSocket text payloads as portable JSON Lines.
- Generate deterministic offline fixtures without network access.
- Replay the same input through a selected bounded pipeline.
- Compare Bengal and standard C++ implementations using the same data.
- Report sequence gaps, parse failures, backpressure, drops, checksums, and
  bounded stage-latency percentiles.

## Requirements

- Linux on x86-64 or ARM64
- C++20 compiler: GCC 11 or Clang 14, or newer
- CMake 3.20 or newer
- OpenSSL development headers for the pinned live-capture transport
- Git for resolving pinned source dependencies

Ubuntu and Debian users can install build dependencies with:

```sh
sudo apt-get update
sudo apt-get install -y build-essential cmake git libssl-dev ninja-build
```

## Build

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBENGAL_MARKET_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Live capture is enabled by default and builds pinned libcurl `8.17.0` with
WebSocket support. An offline build can omit that dependency:

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBENGAL_MARKET_BUILD_LIVE=OFF
```

A compatible system libcurl can be selected with
`-DBENGAL_MARKET_USE_SYSTEM_CURL=ON`; configuration rejects system builds where
the WebSocket API returns `CURLE_NOT_BUILT_IN`.

Tagged source pins Bengal `v1.0.0` and nlohmann/json to immutable commits. To
validate a local Bengal checkout, configure with
`-DBENGAL_SOURCE_DIR=/absolute/path/to/bengal`. Results made with an override
must identify the Bengal commit and must not be presented as release results.

## Quick Start

Generate a deterministic fixture and replay it without network access:

```sh
./build/bengal-market generate \
  --product BTC-USD --events 100000 \
  --output fixture.jsonl

./build/bengal-market replay \
  --input fixture.jsonl --engine bengal
```

Compare both pipeline implementations against the same recording:

```sh
./build/bengal-market compare --input fixture.jsonl
```

Capture the public Coinbase feed for a bounded interval:

```sh
./build/bengal-market capture \
  --product BTC-USD --duration 60 \
  --output coinbase-btc-usd.jsonl
```

Capture connects to `wss://advanced-trade-ws.coinbase.com` and subscribes to
`market_trades` plus `heartbeats`. Public capture requires no Coinbase API key
or JWT. `--duration` is an integer number of seconds.

## Commands

### `capture`

`capture --output FILE [--product ID] [--duration SECONDS]` connects to the
public feed and records complete text messages. The default product is
`BTC-USD`; the default duration is 30 seconds. Reconnection creates a visible
connection boundary and does not fabricate missing data.

### `generate`

`generate --output FILE [--events N] [--product ID]` creates a deterministic,
schema-valid fixture. The defaults are 10,000 events and `BTC-USD`. It is for
CI, regression tests, and portable comparisons, not market simulation.

### `replay`

`replay --input FILE [--engine bengal|standard]` runs one bounded pipeline and
writes a JSON report to standard output. The default engine is `bengal`.
Repeated runs must produce the same event count and checksum; timing varies.

### `compare`

`compare --input FILE` runs the Bengal and standard C++ bounded pipelines
against identical input and writes a combined JSON report. It exits nonzero
when event counts or checksums differ. It does not interpret a local pipeline
result as end-to-end market latency or financial performance.

## Data and Metrics

Recordings use versioned JSON Lines: one metadata record followed by raw frame
records. A frame stores a receipt timestamp, connection identity, and the
complete WebSocket payload as a JSON string. See
[Data Format](docs/DATA_FORMAT.md) for the normative format and compatibility
rules.

Latency reports include p50, p95, and p99 stage latency. Counts and checksums
are correctness gates; a faster run with mismatched output or drops is not a
valid improvement. See [Benchmarking](docs/BENCHMARKING.md) and
[Reproducibility](docs/REPRODUCIBILITY.md).

## Documentation

- [Product scope](docs/PRODUCT.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Data format](docs/DATA_FORMAT.md)
- [Benchmark methodology](docs/BENCHMARKING.md)
- [Operations](docs/OPERATIONS.md)
- [Reproducibility](docs/REPRODUCIBILITY.md)
- [0.1 release validation](docs/VALIDATION_0.1.md)
- [Release process](docs/RELEASING.md)
- [Security policy](SECURITY.md)
- [Contributing](CONTRIBUTING.md)

## Status

Bengal Market is pre-1.0 software. Recording format changes are versioned, but
CLI and report fields may change between minor releases until 1.0. Release
notes identify compatibility changes and the exact Bengal release consumed.

## License

Bengal Market is available under the [MIT License](LICENSE).
Dependency licenses are reproduced in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
