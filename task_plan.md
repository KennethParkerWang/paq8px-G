# Task Plan: EXP01B Record Reliability

## Goal
Implement the smallest attributable RecordModel reliability conditioning on top of EXP01A, then build and verify byte-exact Release and Debug codecs.

## Phases
- [x] Phase 1: Create an isolated worktree from EXP01A commit 219bb41.
- [x] Phase 2: Add the accepted-record event and sticky DEFAULT-block confirmation state.
- [x] Phase 3: Build Release and assertion-enabled Debug binaries.
- [x] Phase 4: Run compression/decompression smoke tests and compare bytes.
- [x] Phase 5: Review the diff, finalize metadata, and commit.

## Constraints
- Keep the three existing P0/P1/P2 predictors unchanged.
- Keep confirmed error histograms 0..15 unchanged.
- Route all unconfirmed DEFAULT samples to histogram 16.
- Do not use file names, fixed lengths, SimilarityModel, phase, or a new threshold.
- Do not change Mixer input or context counts.
- Do not modify the live experiment-ledger workspace.

## Decisions Made
- Base: EXP01A commit 219bb41a5a02036decc58c7d0ae3fffed8a49a51.
- Reliability signal: a detector candidate that actually changes or reconfirms the current record length after all existing rejection checks.
- Reset boundary: sticky confirmation resets at each block and remains false on non-DEFAULT blocks.
- Histogram layout: 17 per predictor; 0..15 confirmed error classes, 16 unconfirmed.
- Independent code review: approved with no P0/P1 findings; encoder/decoder synchronization, histogram bounds, Mixer slots, and block reset behavior were checked.

## Errors Encountered
- A read-only `rg` check passed Windows wildcard paths as literal path names and returned an invalid-path error. Re-running with directory plus `--glob` is the correct form; source files were unaffected.
- A read-only PowerShell summary attempted to pipe directly from a nested `foreach` expression and failed parsing. Collecting the loop output in an array before formatting fixed the command; artifacts were unaffected.

## Status
**Implementation complete** - ready to commit and hand off for the formal 36-case benchmark.
