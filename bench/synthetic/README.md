# Synthetic Bench Notes

This directory is reserved for larger synthetic benchmark campaigns.

The current repository focus is correctness-first, so the active synthetic
fixtures live in `tests/correctness_harness.cpp` and the checked-in deterministic
artifact example lives under `data/synthetic/`.

Use this directory only when adding benchmark suites that are intentionally
larger or slower than the phase-4 regression harness.
