# Benchmark Report Format

## Bundle Layout

The `benchmark` command creates a new evidence directory:

```text
evidence/
  fixture.sha256
  manifest.json
  report.html
  runs/
    run-001-order-01-bengal.json
    run-001-order-01-bengal.stderr.txt
    run-001-order-02-standard.json
    run-001-order-02-standard.stderr.txt
    ...
```

The sibling path `evidence.part` is used while work is in progress. The final
directory appears only after all child processes succeed, every raw report is
validated, and the manifest and HTML summary are written. Neither path is
overwritten.

## Manifest Schema 1

`manifest.json` is the normative machine-readable index. Its top-level fields
are:

| Field | Meaning |
|---|---|
| `schema_version` | Benchmark manifest schema; currently `1`. |
| `created_at_utc` | Report creation time in UTC. |
| `tool` | Bengal Market/Bengal versions, source revision, executable hash. |
| `benchmark` | Measured pairs, warm-ups, isolation, and ordering policy. |
| `fixture` | Input basename, byte size, and SHA-256. |
| `environment` | Kernel, architecture, CPU, memory, governor, and compiler. |
| `comparable` | Whether every raw report passed logical equivalence gates. |
| `summary` | Per-engine distributions over measured runs. |
| `runs` | Ordered index of raw report and stderr files. |
| `scope` | Required interpretation boundary for the measurements. |

Each summary distribution contains `samples`, `min`, `p25`, `median`, `p75`,
`p95`, and `max`. Percentiles use nearest-rank selection over measured process
results. They aggregate per-process metrics; they do not merge individual
event-latency histograms.

## Comparability

`comparable` is true only when every process reports:

- report schema version 1 and the requested engine;
- equal source frame, sequence-gap, and out-of-order counts;
- zero parse errors and drops;
- equal event counts and deterministic checksums; and
- exactly one stage-latency sample per completed event.

Backpressure can differ and remains visible in the summary. A false value
causes the command to exit nonzero and must not be represented as a performance
comparison.

## Raw Evidence

Raw JSON files are unmodified `replay` stdout. A separate stderr file is
retained for every measured process, including successful empty streams.
Warm-up output is validated and then removed.

`report.html` is a static convenience view generated from the manifest. It is
not normative and contains no remote scripts or assets.

## Compatibility

Benchmark manifests are versioned independently from capture recordings and
single-replay reports. Pre-1.0 releases can add fields without changing
`schema_version`; removing fields or changing their meaning requires a new
schema version and migration notes.
