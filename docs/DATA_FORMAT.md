# Recording Data Format

## Overview

Bengal Market recordings are UTF-8 JSON Lines (`.jsonl`). Each line is one
complete JSON object followed by `\n`. Version 1 contains exactly one metadata
record as the first line and zero or more frame records after it.

The JSONL envelope is owned by Bengal Market. The `payload` field is owned by
the data provider and is retained as a string rather than normalized into the
envelope.

## Metadata Record

Live-capture example, expanded for readability:

```json
{
  "type": "metadata",
  "format": "bengal-market-capture",
  "version": 1,
  "source": "coinbase-advanced-trade",
  "endpoint": "wss://advanced-trade-ws.coinbase.com",
  "product_ids": ["BTC-USD"]
}
```

Required fields:

| Field | Type | Meaning |
|---|---|---|
| `type` | string | Must be `metadata`. |
| `format` | string | Must be `bengal-market-capture`. |
| `version` | unsigned integer | Envelope compatibility version; currently 1. |
| `source` | string | `coinbase-advanced-trade` or `deterministic-generator`. |
| `product_ids` | array of strings | Requested provider product IDs. |

Live captures also include `endpoint`. A generated fixture omits `endpoint`.
Its metadata and payloads derive only from the product and requested event
count, so identical command arguments produce identical file content.

## Frame Record

One physical line resembles:

```json
{"type":"frame","received_ns":1785412800123456789,"connection_id":1,"payload":"{\"channel\":\"market_trades\",\"timestamp\":\"2026-07-30T12:00:00Z\",\"sequence_num\":7,\"events\":[]}"}
```

Required fields:

| Field | Type | Meaning |
|---|---|---|
| `type` | string | Must be `frame`. |
| `received_ns` | unsigned integer | Host realtime receipt time in Unix nanoseconds. |
| `connection_id` | unsigned integer | Starts at 1 and increments after reconnect. |
| `payload` | string | Complete provider text-message payload. |

`received_ns` can jump if the host clock is adjusted and does not measure
provider-to-client latency. Generated fixtures use deterministic synthetic
values rather than host time.

The payload is escaped as required by JSON. Parsing the outer JSON string must
recover the UTF-8 bytes delivered in the complete WebSocket text message.
Implementations must not reserialize, reorder, or normalize the provider JSON
before writing it.

## Provider Payload

For Coinbase market trades, a supported payload has top-level fields including
`channel`, `timestamp`, `sequence_num`, and `events`. Market-trade events contain
`type` and `trades`; each trade includes `trade_id`, `product_id`, `price`,
`size`, `side`, and `time`.

Provider fields can evolve independently of this envelope version. Unknown
provider fields remain in `payload`. Missing required fields, type mismatches,
unsupported decimal precision, and numeric overflow are parse errors.

## Sequence Semantics

Coinbase sequence numbers are data-quality evidence:

- a value above the next expected number records a sequence gap;
- a lower value records an out-of-order observation;
- a new `connection_id` starts new sequence tracking; and
- sequence tracking spans interleaved provider channels on one connection; and
- a gap is reported, not repaired or filled with synthetic trades.

Consumers must not infer that a recording is complete solely because every
record is valid JSON.

## Reader Requirements

Readers require metadata before frames, recognize only metadata and frame
records, and count malformed lines or unsupported records as parse errors.
A file with no valid metadata record fails replay. Local recording files are
untrusted input.

Version 1 readers may ignore unknown envelope fields. They must validate
`type`, `format`, and `version`.

## Compatibility

Adding optional fields is backward compatible within format version 1.
Changing required field meaning, removing a required field, or changing record
framing requires a new `version` value.

The `version` field is independent from the Bengal Market software version.
Release notes list every readable and writable format version.

## Integrity and Compression

The first release writes uncompressed JSONL. Compress completed recordings as
an external post-processing step and retain a SHA-256 digest:

```sh
sha256sum capture.jsonl > capture.jsonl.sha256
gzip --keep capture.jsonl
```

Replay inputs must be decompressed before use.
