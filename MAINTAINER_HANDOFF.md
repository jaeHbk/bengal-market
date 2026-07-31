# Maintainer Handoff

## Project

- Repository: <https://github.com/jaeHbk/bengal-market>
- Primary branch: `main`
- Bengal dependency: `v1.0.0`
  (`1bd5389db9fd802e4c836426dc94288f268cb543`)
- Recording format: version 1
- Replay report schema: version 1
- Benchmark manifest schema: version 1

## Current Release Work

Version 0.2.0 delivered fresh-process benchmark evidence, atomic live capture,
signal-safe finalization, and a reduced pinned curl dependency surface. Its
source and checksums are valid, but the Ubuntu 24.04-built convenience binary
requires glibc 2.38 and does not run on the documented glibc 2.34 baseline.
Do not move or replace the published `v0.2.0` tag or assets.

Version 0.2.1 attempted the corrective binary-portability release, but its
release workflow failed before checkout because `ninja-build` was unavailable
in Rocky Linux 9's enabled repositories. The tag is immutable and has no
release assets.

Version 0.2.2 is the corrective binary-portability release. Its release job
uses base-repository build tools and rejects imported glibc symbol versions
newer than 2.34 or OpenSSL symbol versions newer than 3.0.0. Product behavior
and data formats are unchanged.

## Release Completion

For an untagged 0.2.2 commit:

1. Confirm CI and sanitizer workflows pass on `main`.
2. Create and push annotated tag `v0.2.2`.
3. Wait for the Release workflow, then download the Linux package artifact and
   run it on a glibc 2.34 host.
4. Inspect `readelf --version-info`; no GLIBC requirement may exceed 2.34 and
   no OpenSSL requirement may exceed 3.0.0.
5. Verify `SHA256SUMS`, extraction,
   `--version`, fixture generation, replay, and a two-pair benchmark.
6. Add a note to the GitHub 0.2.0 release that its Linux binary is superseded
   by 0.2.2.

Never report pipeline benchmark results as market latency, profitability, or
market-beating performance. See `docs/BENCHMARKING.md` and
`docs/RELEASING.md` for the evidence and release contracts.

## Next Product Milestone

Keep Bengal Market read-only. The next practical milestone is provider
abstraction plus a second public market-data adapter, backed by captured
protocol fixtures and conformance tests. Preserve raw provider messages,
connection boundaries, data-quality counters, deterministic replay, and
explicit source provenance across adapters.
