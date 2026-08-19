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
