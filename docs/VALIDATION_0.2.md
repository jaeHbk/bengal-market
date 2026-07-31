# Bengal Market 0.2 Validation

## Scope

Version 0.2 adds fresh-process benchmark evidence and durable, signal-aware
capture finalization. Recording format v1 and replay report schema v1 are
unchanged. Benchmark manifest schema v1 is new.

## Automated Coverage

The test suite covers:

- deterministic generation and equivalent Bengal/standard replay;
- valid and malformed fixed-point values, timestamps, envelopes, and sequence
  streams;
- atomic output visibility, commit, destination collision, and stale partial
  rejection;
- two-pair benchmark ordering, raw-run retention, fixture and executable
  hashes, aggregate output, and HTML generation;
- publication of a deliberately non-comparable benchmark bundle;
- local WebSocket capture followed by both `SIGINT` and `SIGTERM`;
- conventional signal exit status, partial-file removal, valid replay, and
  refusal to overwrite the committed capture.

Local GCC builds passed Debug, Release, ASan/UBSan, and TSan configurations.
LeakSanitizer process inspection is blocked by the local host policy; the clean
GitHub sanitizer job is the release leak-detection gate.

## Ten-Pair Benchmark

A Release build generated 100,000 deterministic `BTC-USD` events and ran ten
measured pairs after one warm-up pair. Each replay ran in a fresh process and
engine order alternated.

```text
fixture_sha256=60837844965ec5b3c382d4b33cf2ac0c1255a6c9261501ed2719e5cfc338f5a8
measured_processes=20
comparable=true
bengal_elapsed_median_ns=1332567010
standard_elapsed_median_ns=1352865214
bengal_stage_p99_median_ns=2048
standard_stage_p99_median_ns=2048
backpressure=0
dropped=0
```

The host used an Intel Xeon Platinum 8259CL, 16 logical CPUs, Linux
`6.12.94-123.192.amzn2023.x86_64`, and GCC 11.5. These numbers are diagnostic
for this host and fixture. They do not establish a general performance
advantage.

## Public Feed Smoke Test

A five-second unauthenticated Coinbase Advanced Trade capture completed through
the Release binary:

```text
frames=21
reconnects=0
events=126
parse_errors=0
sequence_gaps=0
out_of_order=0
backpressure=0
dropped=0
checksum=8490280818482931232
benchmark_measured_processes=6
benchmark_comparable=true
```

The finalized recording replayed successfully and its three-pair benchmark
bundle was comparable. The capture is not distributed because provider terms
can restrict market-data redistribution.

## Package Validation

The installed-tree binary reported `bengal-market 0.2.0`, generated a fixture,
and produced a comparable six-process benchmark bundle. The archive contained
the executable, MIT license, security policy, third-party notices, operations
guide, benchmark methodology, benchmark schema, and validation documents.

Runtime inspection found no dynamic dependency on libcurl, libssh, nghttp2,
libpsl, Brotli, or Zstandard. The supported pinned transport retains only
HTTP(S), WebSocket, and platform OpenSSL runtime dependencies.

## Interpretation

This validation establishes deterministic logical output, observable source
quality, process-isolated local comparison, and graceful recording
finalization. It does not measure exchange-to-client latency, compare Bengal
Market with a broker application, predict returns, or establish financial
performance.
