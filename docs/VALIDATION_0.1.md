# Bengal Market 0.1 Validation

## Scope

This record covers release candidate `82cb9677fe17ae781e5ab14975d49ec8a4ac3432`
and its pinned Bengal `1.0.0` dependency. The final release adds only this
record and changelog status before repeating the automated release build.

## Automated Validation

- CI run
  [30570607127](https://github.com/jaeHbk/bengal-market/actions/runs/30570607127):
  GCC, Clang, deterministic generation/comparison, tests, installation, and
  Linux x86-64 packaging passed.
- Sanitizer run
  [30570607081](https://github.com/jaeHbk/bengal-market/actions/runs/30570607081):
  ASan/UBSan and TSan builds and tests passed.
- Bengal run
  [30569677055](https://github.com/jaeHbk/bengal/actions/runs/30569677055):
  GCC, Clang, Apple Clang, MSVC, ASan/UBSan, TSan, fuzzing, benchmarks,
  installation, and package consumers passed for the pinned commit.

The candidate package SHA-256 was:

```text
7008362eafc39d490881265787e8d140cc88bb28776d24416c958eded09c08b1
```

Its installed tree contained the CLI, product documentation, MIT license, and
third-party notices. It did not contain source dependency archives or
development libraries.

## Live Protocol Smoke Test

The candidate package ran inside Ubuntu 24.04 against the public Coinbase
Advanced Trade WebSocket endpoint for 10 seconds with `BTC-USD`.

```text
frames=19
reconnects=0
events=124
parse_errors=0
sequence_gaps=0
out_of_order=0
dropped=0
backpressure=0
checksum=10111059663114712642
```

Both pipeline engines produced the same event count and checksum, so the
comparison reported `comparable: true`. The capture itself is not distributed
with the project; its SHA-256 was
`432dbf5f0477d9c2e3cc8661d9182ef3aea915d048a707952c2a42f1e7d7521d`.

This smoke test validates protocol compatibility and deterministic replay. It
does not establish exchange-to-client latency, commercial product latency,
investment performance, or market-beating behavior.
