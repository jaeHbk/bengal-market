# Operations

## Deployment Model

Bengal Market is a single-host command-line application. Run it as an
unprivileged user on Linux. Live capture needs outbound TCP/TLS access and write
access to the selected output directory. Replay, generation, and comparison
need no network access after dependencies and input files are available.

No credential, account, wallet, or order permission is required. Do not put API
keys in environment files or service definitions for this product.

## Preflight

Before live capture:

1. Record the release or source commit and recording format.
2. Confirm system time synchronization and note its status.
3. Check available disk and inode capacity.
4. Verify the output directory has suitable permissions.
5. Confirm outbound TLS access to
   `wss://advanced-trade-ws.coinbase.com`.
6. Start with a bounded duration and one product.

```sh
df -h .
timedatectl status
./bengal-market capture \
  --product BTC-USD --duration 60 \
  --output capture.jsonl
```

## Expected Connection Behavior

The client subscribes to `market_trades` and `heartbeats` immediately after
connection. A subscription not sent within five seconds can be disconnected by
the provider. Heartbeats keep subscriptions active during quiet periods.

Transient disconnects are expected. Reconnect attempts produce a new
connection ID. Monitor reconnects and sequence gaps; the tool does not conceal
or repair missing source messages.

Persistent TLS, DNS, schema, or disk-write errors should stop capture with a
nonzero exit status rather than retry forever.

## Capacity and Retention

JSONL stores each payload plus envelope overhead and can grow quickly. Capacity
planning must use observed bytes per minute for selected products and retain
headroom for bursts. Prefer a dedicated filesystem or enforced service quota
for unattended capture.

Rotate by stopping cleanly and starting a new recording. Do not concatenate
files because each requires exactly one first metadata record. Hash a closed
recording before compression or transfer:

```sh
sha256sum capture.jsonl > capture.jsonl.sha256
gzip --keep capture.jsonl
```

Deleting or truncating an open output file is unsupported. Disk-full, short
write, flush, and close failures must be visible in process status.

## Shutdown and Recovery

Allow the configured duration to expire for a finalized recording. A signal or
forced termination can interrupt a write and leave a truncated final line;
replay counts malformed lines as parse errors.

After an abnormal stop:

1. preserve the original file;
2. verify its hash if captured externally;
3. run replay validation without modifying it; and
4. document any recovery performed on a copy.

Never silently remove malformed records from benchmark evidence.

## Monitoring

Retain:

- process exit status and source version;
- bytes and complete frames written;
- reconnect count and connection ID;
- sequence gaps and out-of-order observations;
- parse errors;
- backpressure and drops; and
- output path, format version, start time, and duration.

An increase in gaps, parser errors, or reconnects invalidates completeness
assumptions and should be investigated before publishing replay results.

## Privacy and Data Handling

Provider frames are public market data, but the envelope records host receipt
times, monitored products, and connection history. Output paths, shell history,
and benchmark metadata can add hostnames or usernames. Review files before
sharing and follow provider terms for data redistribution.

Bengal Market emits no intentional telemetry. Network infrastructure and the
provider can still observe the host connection and its public subscriptions.
