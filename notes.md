# Notes: EXP01B Record Reliability

## Source Facts
- `RecordModel::rLength[0]` starts at 2, so record length alone does not prove automatic detection.
- Candidate acceptance already has a length-dependent threshold and two existing rejection paths.
- `ContextModelGeneric` calls `RecordModel::mix()` before `RecordResidualModel::mix()`, so a byte-local acceptance event is visible without shared side information.
- `ResidualMap` supports any histogram count and keeps each predictor context separate.

## Verification Evidence
- Static diff review: four source files, 14 insertions and 2 deletions.
- `git diff --check`: passed; Git only reported the repository's expected LF-to-CRLF checkout warning.
- Mixer inputs remain `3 * ResidualMap::MIXERINPUTS = 6`.
- Histogram storage increases from `3 * 16 * 256 * 2 = 24,576` bytes to `3 * 17 * 256 * 2 = 26,112` bytes.
- Release build: `F:\paq8px\paq-default-research-20260820\builds\EXP01B\paq8px.exe`.
- Release SHA-256: `666C2CA9EDF43AF88A0124F7FC3A1F58786FF82BF4ACD142F4E98AA2D1476988`.
- Debug build: `F:\paq8px\paq-default-research-20260820\builds\EXP01B_DEBUG\paq8px.exe`.
- Debug SHA-256: `6B1BE1BFAE7450CC4E1357AE3BCB49BEA1A41461FCE335A48728E361321C9E3B`.
- Debug `compile_commands.json` contains zero `-DNDEBUG` occurrences; assertions are active.
- Release and Debug builds both completed all 146 Ninja steps. Debug warnings are pre-existing v216 warnings; none point to the four changed source files.
- Independent run validator passed 2/2 rows for each of four smoke runs, checking exit codes, lengths, SHA-256 roundtrips, manifests, and peak RAM markers.
- Smoke coverage: `sao` exercises a changed accepted length, `x-ray` exercises same-length reinforcement, `osdb` exercises the unconfirmed histogram, and `xml` exercises non-DEFAULT/skip behavior.
- `xml` block summary is `default:505; text:32263`, so the smoke run crosses from DEFAULT evidence into a non-DEFAULT block within one input.
- Release and Debug archives are byte-identical for all four inputs.
- Release archive sizes at 32 KiB: `sao=18,568`, `x-ray=14,484`, `osdb=11,173`, `xml=1,383` bytes.
- Compared with EXP01A for these smoke cases: `sao -3`, `x-ray +1`, `osdb 0`, `xml +1`, total `-1` byte. This is not a KEEP/REVERT decision; the formal 36-case run remains pending.

## Independent Review
- Approved with no P0/P1 findings.
- Confirmed deterministic encoder/decoder state, histogram IDs `0..16`, exactly three `set()` or `skip()` calls per byte, and the unchanged six-input Mixer contract.
- Confirmed block-first-byte acceptance is retained and sticky confirmation does not leak across blocks.
- Remaining non-blocking gap: no dedicated synthetic `DEFAULT -> non-DEFAULT -> DEFAULT` input; the formal Silesia run and existing `DEFAULT -> TEXT` smoke provide the current coverage.

## Formal Benchmark
- Source revision: `6a37049430baf23d7a284bad5fdcb7b96f457a78`.
- Run: `F:\paq8px\paq-default-research-20260820\runs\EXP01B-record-reliability-auto-r1-20260820T032242`.
- Contract: Silesia 12 files x first 32/64/128 KiB, `-8 auto`, Repeat 1; no full-file cases.
- Independent validator: 36 rows, manifest `COMPLETE`, 36 SHA-256 byte-exact roundtrips, and 36 Peak RAM records all passed.
- Results CSV SHA-256: `C1AC4C1F7990F05703C8D98FF59B9E40A55A465814E776A4FC175D5F7D842DA6`.
- Manifest SHA-256: `028873064B434E1A6CD87532FB60B4A3E30ACC4B564AE143E1C03DDABAAC5CD6`.
- Prefix postcheck: exactly 36 rows; scopes are only 32/64/128 KiB; every input length equals `scope_kib * 1024`; no Full row exists.

## Decision
- Versus EXP01A: all cases `611,913 -> 611,909` bytes (`-4`, `-0.000654%`); target cases `324,044 -> 324,036` bytes (`-8`, `-0.002469%`).
- Versus EXP00: all cases `612,114 -> 611,909` bytes (`-205`, `-0.033490%`); target cases `324,210 -> 324,036` bytes (`-174`, `-0.053669%`).
- Versus EXP01A outcome count: 4 wins, 26 ties, and 6 losses.
- Scope deltas versus EXP01A: 32 KiB `+1`, 64 KiB `-1`, 128 KiB `-4`.
- KEEP because both the primary target sum and all-file safety sum improve deterministically. The marginal gain is only 4 bytes and costs 1,536 bytes of additional ResidualMap storage, so the tradeoff must remain explicit.

## Experiment Ledger Candidate
- Candidate: `F:\paq8px\paq-default-research-20260820\research\ledger-candidates\workspace-with-EXP01A-and-EXP01B-record-reliability-20260820T033911.json`.
- Candidate SHA-256: `69A92C28149609A3184BDBE7F698F93980893D43400E829E578E23D2768469FE`.
- Strict importer result: schema 2, 3 experiments, new EXP01B has 36 rows, 3 tables, 36 PASS, and no Full rows.
- The live workspace and running Experiment Ledger process were not changed.
