# Architecture

## System Boundary

Bengal Market separates network capture from deterministic processing. Live
capture is an adapter around libcurl's WebSocket API. Recording, parsing,
metrics, replay, and pipeline comparison are independently testable without a
network connection.

```text
Coinbase public WebSocket
          |
          v
  receive and reassemble
          |
          +----> versioned raw JSONL recording
                         |
                         v
              reader and schema parser
                         |
                  bounded handoff
                         |
              metrics and checksum sink
```

The same recording reader and event representation feed the Bengal and
standard C++ pipeline implementations. A comparison is valid only when both
produce equivalent logical output and report overload independently.

## Capture Path

`capture` connects to:

```text
wss://advanced-trade-ws.coinbase.com
```

It sends public subscriptions for `market_trades` and `heartbeats` within five
seconds of opening the connection. No JWT or API key is used. Heartbeats keep
quiet subscriptions open and contribute sequence information.

libcurl can return partial sends and fragmented receives. The adapter must:

1. continue a subscription send until the complete message is accepted;
2. wait for socket readiness after `CURLE_AGAIN`;
3. accumulate fragments until `bytesleft` reaches zero;
4. distinguish text, binary, continuation, ping/pong, and close frames;
5. record only complete provider text messages.

A reconnect increments `connection_id`, repeats both subscriptions, and waits
250 milliseconds before retrying while the requested duration remains.
Sequence state resets at that visible connection boundary. The recorder never
hides a reconnect or synthesizes data.

## Recording and Parsing

The raw provider payload is written before market parsing. JSONL framing adds
receipt metadata but preserves the payload string after outer JSON decoding.
This allows parser regressions to be investigated against original input.

Numeric prices and sizes use checked fixed-point conversion at scale `1e8`
rather than binary floating point in deterministic logical output. Unsupported
precision, overflow, missing fields, wrong types, and malformed JSON increment
explicit errors or reject the recording according to command policy.

## Bounded Pipelines

The Bengal path uses Bengal's fixed-capacity SPSC queue. The standard path uses
an equivalent bounded handoff built from standard C++ synchronization
primitives. Both paths use the same:

- input records and parsed event representation;
- queue capacity and full-queue policy;
- number and role of stages;
- checksum and metric definitions; and
- stop, drain, and end-of-stream behavior.

Queue capacity is 4096 events. Queue full is observable as backpressure; any
policy that drops data increments a drop counter. Neither implementation may
silently become unbounded.

## Metrics

Data-quality metrics include frames, events, parse failures, sequence gaps,
out-of-order sequence values, and reconnects. Pipeline metrics include
processed events, backpressure, drops, checksum, elapsed time, and stage
latency.

Latency uses fixed-space aggregation rather than retaining one sample per
event. Reports include minimum, p50, p95, p99, maximum, and sample count.
Logical output includes a deterministic checksum over normalized processed
events. Metrics are reported separately for each implementation.

## Ownership and Shutdown

Every queue has one producer and one consumer. Worker lifetime is RAII-owned,
and normal replay drains accepted work before returning.

Capture finalizes the output after the requested duration. Forced termination
can interrupt a write, so a truncated final line remains possible and must be
treated as a parse error during replay.
