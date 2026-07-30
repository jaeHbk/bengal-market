# Security Policy

## Supported Versions

Security fixes are provided for the latest tagged release. During the pre-1.0
period, users should upgrade to the newest minor release. The default branch is
development code and is not a supported release.

## Reporting a Vulnerability

Use GitHub's private vulnerability reporting:

https://github.com/jaeHbk/bengal-market/security/advisories/new

Do not disclose a suspected vulnerability in a public issue, discussion, pull
request, benchmark artifact, or capture file. Include the affected version,
platform, impact, reproduction steps, and proposed mitigation when available.

## Security Boundary

Bengal Market consumes untrusted public network data and local recording files.
Parsers must reject malformed JSON, unsupported decimal precision, overflow,
oversized messages, invalid UTF-8, and incompatible format versions without
memory-unsafe behavior.

Version 0.1 limits live provider messages to 4 MiB, recording lines to 8 MiB,
and tracked connection sequence streams to 4,096. These are safety
ceilings, not provider guarantees.

The product does not need and must not request:

- Coinbase credentials, JWTs, API keys, or account identifiers;
- brokerage or exchange account permissions;
- wallet secrets or payment information; or
- order placement, cancellation, or modification privileges.

The public endpoint is `wss://advanced-trade-ws.coinbase.com`. Unexpected
redirects, non-TLS endpoint substitution, or disabled certificate verification
should be treated as an error. Release builds rely on the platform CA trust
store through libcurl.

## Operational Guidance

- Run as an unprivileged user with access limited to the output directory.
- Set file-size, process, and runtime limits for unattended capture.
- Treat all recording files as untrusted input.
- Keep the OS, CA certificates, compiler runtime, and libcurl updated.
- Verify release checksums and build tagged source when provenance matters.
- Review a capture before publishing it.

## Privacy

Public market payloads normally contain trades and provider metadata, not
customer account data. Bengal Market adds local receipt timing, connection
identifiers, and capture metadata. Those fields can reveal when a host was
online and which products it monitored. Apply appropriate retention and access
controls.

The project does not intentionally collect telemetry. Live capture sends only
the public subscriptions required for the selected product and channels.
