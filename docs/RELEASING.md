# Release Process

## Release Contract

Bengal Market uses Semantic Versioning for the application and an independent
integer version for the JSONL recording format. A release identifies:

- Bengal Market version and source commit;
- exact tagged Bengal release and pinned commit;
- supported read/write recording-format versions;
- Linux architecture and runtime assumptions;
- user-visible CLI or report changes; and
- known protocol or benchmark limitations.

Pre-1.0 minor versions may change the CLI. Incompatible recording changes still
require a new recording-format version and migration notes.

## Preconditions

Before tagging:

1. Ensure dependencies resolve to immutable commits. Bengal must correspond to
   a published release, not a branch or local override.
2. Update `CHANGELOG.md`, including compatibility and dependency changes.
3. Confirm README commands agree with the CLI usage.
4. Run GCC and Clang CI plus ASan/UBSan and TSan.
5. Generate a fixture and confirm matching engine counts and checksums.
6. Complete a short public capture and replay it without credentials.
7. Verify the install tree from a clean build.
8. Confirm no captures, credentials, local paths, or scratch data are included.

```sh
cmake -S . -B build-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBENGAL_MARKET_BUILD_TESTS=ON \
  -DBENGAL_MARKET_ENABLE_SANITIZERS=OFF
cmake --build build-release --parallel
ctest --test-dir build-release --output-on-failure
cmake --install build-release --prefix staging/usr/local
```

Run validation from a clean worktree.

## Tagging

Use an annotated SemVer tag:

```sh
git tag -a v0.1.0 -m "bengal-market 0.1.0"
git push origin v0.1.0
```

Release automation tests, installs, creates a Linux x86-64 archive, generates
`SHA256SUMS`, and publishes both from the tag.

Do not move a published tag. Correct a release with a new patch version.

## Release Notes

The final notes must state:

```text
Bengal Market: 0.1.0
Bengal dependency: vX.Y.Z (commit ...)
Recording format: reads v1, writes v1
Artifact: Linux x86-64, pinned static libcurl with platform OpenSSL runtime
```

Include notable changes, upgrade instructions, known limitations, and evidence
when performance is discussed. Do not include profitability, market-beating,
trading, or commercial broker latency claims.

## Artifact Verification

```sh
sha256sum --check SHA256SUMS
tar -tzf bengal-market-v0.1.0-linux-x86_64.tar.gz
```

After extraction, run fixture generation, replay, and comparison. Inspect
dynamic dependencies and confirm libcurl WebSocket support on the host.

## Reproducibility Notes

Release automation normalizes archive path order, timestamps, owner, and group.
The archive still contains compiler- and distribution-specific binaries and is
not promised to rebuild byte-identically with a different toolchain. Preserve
CI logs and uploaded artifacts as release provenance.
