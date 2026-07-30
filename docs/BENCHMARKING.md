# Benchmark Methodology

## What Is Measured

Bengal Market compares local bounded event-pipeline implementations. It can
measure parsing and handoff throughput, queue pressure, processing counts, and
in-process stage latency. It cannot infer investment performance or accurately
measure the complete path from an exchange matching engine to a user.

Provider timestamps and host receipt timestamps use different clocks. Their
difference includes clock error, provider batching, network transit, TLS,
kernel scheduling, and application work. It is not a defensible one-way network
latency measurement without synchronized clocks and controlled infrastructure.

## Correctness Before Speed

A comparison is valid only when both implementations report:

- the same input frame and parsed-event counts;
- the same normalized-output checksum;
- the same queue capacity and overload policy;
- no unreported parse errors or drops; and
- successful drain at end-of-stream.

If one run drops events, fails parsing, or produces a different checksum,
report it as a behavioral difference rather than a performance win.

## Required Procedure

1. Build one tagged revision in `Release` mode.
2. Record the source commit, Bengal pin, compiler, CMake, libcurl, and flags.
3. Record kernel, CPU, memory, power governor, and virtualization.
4. Hash the exact JSONL fixture and record command-line options.
5. Stop unrelated high-load work and use a stable CPU/power policy.
6. Execute at least one unreported warm-up.
7. Run at least 10 measured process invocations, alternating engine order.
8. Retain every raw report, not only the fastest result.
9. Report median and dispersion across runs with per-run p50/p95/p99.
10. Repeat on another machine before making a general claim.

Example:

```sh
./build/bengal-market generate \
  --product BTC-USD --events 1000000 \
  --output fixture-1m.jsonl
sha256sum fixture-1m.jsonl

./build/bengal-market replay --input fixture-1m.jsonl --engine bengal
./build/bengal-market replay --input fixture-1m.jsonl --engine standard
./build/bengal-market compare --input fixture-1m.jsonl
```

The current CLI performs one run per invocation. A benchmark harness must start
fresh processes and retain each JSON report.

## Environment Capture

```sh
git rev-parse HEAD
git describe --tags --always --dirty
cmake --version
c++ --version
curl-config --version
uname -a
lscpu
free -h
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || true
sha256sum fixture-1m.jsonl
```

Also retain the configure command and `CMakeCache.txt`. State whether the run
used a container, virtual machine, CPU affinity, elevated scheduling policy, or
modified kernel settings.

## Metrics

| Metric | Interpretation |
|---|---|
| frames/events | Source records and completed logical work. |
| parse errors | Provider payloads rejected by the parser. |
| sequence gaps/out-of-order | Source-stream quality indicators. |
| backpressure | Queue-full observations by the producer. |
| dropped | Work not completed by the pipeline. |
| elapsed time | Measured replay interval for the reported engine. |
| p50/p95/p99 stage latency | Quantiles from bounded latency aggregation. |
| checksum | Deterministic digest of normalized logical output. |

Percentiles are estimates over observed stage samples. Reports must state the
clock source and whether file I/O, parsing, drain, or teardown is included.

## Interpreting Results

Prefer claims of this form:

> On the stated host, compiler, binary, fixture, and options, pipeline A had a
> lower median p99 stage latency across 10 runs, with matching checksums and
> zero drops. Raw results are attached.

Do not claim:

- a pipeline is universally faster based on one host or best run;
- lower local queue latency means faster market data than a broker;
- replay throughput predicts live network behavior;
- low p99 guarantees a worst-case bound; or
- technical results imply profitable or market-beating trading.

## Live Capture

Live data validates protocol behavior and replay compatibility, not fair A/B
timing. Capture once, hash the file, then replay that same file for comparisons.

Do not publish provider data unless its terms permit redistribution.
Deterministic generated fixtures are the preferred portable benchmark input.
