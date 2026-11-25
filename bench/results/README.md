# Raw Experiment Results

## Real-model runs (executed)

Provider: DeepSeek OpenAI-compatible. Requested `deepseek-chat`; provider reported
`deepseek-flash`. No API key, Authorization header or Bearer token is stored in
these files.

| Directory | Calls | Result |
| --- | --- | --- |
| `smoke-001/` | 1 | local-edit pass |
| `factorial-smoke-001/` | 7 | A0/A1 pass; B/C compile errors; C1 repair failed |
| `learnability-001/` | 40 | LCP v1: L0 0/6, L1 2/6, L2 3/6, L3 4/6, L4 4/6 |
| `learnability-002/` | 39 | LCP v1.1: L0 0/6, L1 2/6, L2 5/6, L3 5/6, L4 5/6; + prompt-v2 pass |
| `pilot-001/` | 30 | six arms, 5 tasks each: 30/30 pass (ceiling) |

Total: 117 calls, 264,999 tokens, 169,828 ms measured latency.

## Interpretation limits

- `pilot-001` prompts expose exact target observations; all arms pass by
  transcription. This is a ceiling effect and **not** evidence that one
  representation is better.
- `learnability-002` is development-set diagnostics, not a confirmatory test.
- Held-out tasks were not used in prompt tuning or in this run.
- No artistic-quality evaluation was performed.

## Offline Context Benchmark

`context-accessibility-001/context-accessibility.json` is a completely offline
comparison of Full/Domain/Adaptive contexts. It makes no model calls and does
not measure model success.

## Formal held-out Pilot

Held-out six-arm evaluation has **not** been executed. Do not merge these
development/smoke counts into held-out success rates.

## Storage convention

Each experiment result JSON includes per-arm status, tokens, latency, repair
attempts and verification evidence. Task definitions and `bench/freeze.json`
must not change between the recorded pilot and held-out evaluation.

## V0.5.0-beta Offline Benchmarks

- sma-001/results.json: 10 self-hosted MPL algorithms, 8 pass / 2 unsupported, zero model calls.
