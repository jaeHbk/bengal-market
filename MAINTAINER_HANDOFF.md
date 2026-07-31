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

## Published Release

- Release: <https://github.com/jaeHbk/bengal-market/releases/tag/v0.2.2>
- Tag commit: `48d7005413be919dbea65a3086c19ef257196380`
- CI: `30667797416`
- Sanitizers: `30667797412` (successful second attempt after a hosted package
  installation stall)
- Release workflow: `30668358455`
- Archive SHA-256:
  `5e21746cffa284f3c4317f745bb0f16da7896bb8c0c74d60e088bd6b7119834c`

The release workflow completed build, tests, installation, benchmark, and both
runtime symbol gates. Its archive step hit Git safe-directory protection in
the job container. The assets were recovered from the exact immutable tag with
the same Rocky Linux 9 build and release commands, then downloaded from GitHub
and verified again on a glibc 2.34 host. The public binary reports 0.2.2,
requires at most `GLIBC_2.34` and `OPENSSL_3.0.0`, and completes a comparable
two-pair benchmark. The workflow on `main` now trusts the container workspace
so future tagged releases can archive automatically.

The 0.2.0 release notes direct binary users to 0.2.2. Do not move or replace
the 0.2.0, 0.2.1, or 0.2.2 tags.

Never report pipeline benchmark results as market latency, profitability, or
market-beating performance. See `docs/BENCHMARKING.md` and
`docs/RELEASING.md` for the evidence and release contracts.

## Next Product Milestone

Keep Bengal Market read-only. The next practical milestone is provider
abstraction plus a second public market-data adapter, backed by captured
protocol fixtures and conformance tests. Preserve raw provider messages,
connection boundaries, data-quality counters, deterministic replay, and
explicit source provenance across adapters.
