# Product Scope

## Purpose

Bengal Market is a command-line product for collecting public market data and
evaluating bounded C++ event-pipeline behavior with replayable evidence. Its
first supported provider is the public Coinbase Advanced Trade WebSocket feed.
Linux is the primary production and release platform.

The product answers engineering questions:

- Was the source stream complete according to its sequence information?
- Did parsing or overload cause visible errors or drops?
- Can an exact input be replayed with deterministic logical output?
- How do equivalent bounded Bengal and standard C++ pipelines behave under a
  controlled local workload?

It does not answer whether, when, or how to trade.

## Release Surface

The product consists of one CLI with five commands:

| Command | Contract |
|---|---|
| `capture` | Record complete public WebSocket text frames and receipt metadata. |
| `generate` | Produce deterministic offline fixtures from explicit product and size. |
| `replay` | Process one recording through a selected bounded pipeline. |
| `compare` | Compare Bengal and standard C++ pipelines against identical input. |
| `benchmark` | Run alternating fresh-process comparisons and retain evidence. |

A publishable release includes source, a Linux x86-64 installed-tree archive,
SHA-256 checksums, change notes, the recording-format version, and the exact
Bengal release pin. Release CI builds from a clean tag and runs tests before
producing an artifact.

## Non-Goals

Bengal Market does not include:

- order entry, cancellation, routing, or execution;
- authenticated exchange or brokerage account access;
- portfolio, position, wallet, or balance management;
- strategy logic, signals, forecasting, or price prediction;
- backtesting claims about returns or market-beating performance;
- claims that local processing latency is exchange-to-client latency; or
- claims of lower latency than commercial broker or trading applications.

Provider data can be delayed, incomplete, corrected, or unavailable. Pipeline
benchmarks measure local software under stated conditions and have no financial
meaning.

## Dependency Boundary

Bengal Market consumes an immutable commit from a tagged Bengal release rather
than tracking Bengal's default branch. The corresponding release is part of
each Bengal Market release and must be recorded in release notes. Temporary
local source overrides are for downstream validation only and must identify the
exact Bengal commit in resulting reports.

Reusable bounded concurrency, memory, and thread primitives belong in Bengal.
Provider protocols, market schemas, capture storage, and market-data metrics
remain in this repository.

## 0.2 Release Bar

Version 0.2 is ready when:

- GCC and Clang release builds and deterministic tests pass on Linux;
- ASan/UBSan and TSan jobs pass;
- benchmark runs execute in fresh child processes with alternating order;
- evidence retains fixture and executable hashes, environment metadata, raw
  JSON reports, stderr, aggregate distributions, and an HTML summary;
- the benchmark fails closed on any logical mismatch, parse error, or drop;
- live capture finalizes valid recordings after `SIGINT` and `SIGTERM`;
- incomplete data remains under a visible `.part` name and is never silently
  promoted or overwritten;
- a short public capture can be interrupted and replayed without credentials;
- a clean tag produces a checksummed Linux artifact; and
- documentation matches the implemented CLI and format.
