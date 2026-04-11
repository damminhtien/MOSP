# Historical NYC Benchmark Summary From Notes

Date compiled: 2026-04-11

This file preserves the important benchmark results that were previously only referenced from `/tmp` artifacts and written discussion notes.

Important limitation:
- the original raw `/tmp` CSV files were already gone when this archive pass was made
- therefore this document is a **summary reconstruction**, not a raw-data replacement
- use it for context and experiment memory, not as the primary raw evidence source

## Source Notes

- [Agent Memory Log](/workspaces/SOPMOA/notes/agent_memory_log.md)
- [Paper Note](/workspaces/SOPMOA/notes/paper_note_ltmoa_fast_parallel_2026-04-10.md)

## Reconstructed Highlights

### Earlier `LTMOA_array` vs `LTMOA_array_superfast` sweep

Referenced historical artifact names:
- `/tmp/ltmoa_superfast_afterpass.csv`
- `/tmp/ltmoa_superfast_compare.csv`

Representative recorded numbers:

| Budget | Baseline frontier | Historical `superfast` frontier | Baseline first solution (ms) | Historical `superfast` first solution (ms) |
| --- | ---: | ---: | ---: | ---: |
| `0.25s` | `128` | `209` | `1.42` | `3.83` |
| `0.5s` | `206` | `316` | `1.42` | `6.24` |
| `1.0s` | `285` | `437` | `1.40` | `6.24` |

Interpretation preserved in notes:
- this earlier `superfast` state reduced per-label processing cost
- but it still did not beat `LTMOA_array` on first-solution timing

### `LTMOA_array_superfast_anytime` experiment

Referenced historical artifact name:
- `/tmp/ltmoa_anytime_compare.csv`

Representative recorded numbers:

| Budget | Historical `superfast_anytime` frontier | Historical `superfast_anytime` first solution (ms) |
| --- | ---: | ---: |
| `0.25s` | `5` | `3.62` |
| `0.5s` | `5` | `3.86` |
| `1.0s` | `8` | `3.63` |

Interpretation preserved in notes:
- slight improvement in first solution
- severe collapse in anytime frontier coverage

### `LTMOA_array_superfast_lb` timeout growth check

Referenced historical artifact name:
- `/tmp/ltmoa_superfast_lb_compare.csv`

Representative recorded numbers:

| Budget | Historical `superfast_lb` frontier | Historical `superfast_lb` first solution (ms) | Other note |
| --- | ---: | ---: | --- |
| `1.0s` | `64` | `40.7` | canonical benchmark |
| `2.0s` | `94` solutions | n/a | legacy run, `366,893` expansions |
| `5.0s` | `110` solutions | n/a | legacy run, `673,243` expansions |

Interpretation preserved in notes:
- more timeout did not rescue the lower-bound experiment
- search effort grew without converting efficiently into target skyline points

### Direct in-place `LTMOA_array_superfast` scheduling experiment

Referenced historical artifact name:
- `/tmp/ltmoa_superfast_mixed_compare.csv`

Representative recorded numbers:

| Budget | Baseline frontier | Earlier `superfast` frontier | Mixed-schedule `superfast` frontier | Baseline first solution (ms) | Earlier `superfast` first solution (ms) | Mixed-schedule `superfast` first solution (ms) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `0.25s` | `145` | `209` | `101` | `1.203` | `3.835` | `5.747` |
| `0.5s` | `231` | `316` | `174` | `1.215` | `6.240` | `5.793` |

Interpretation preserved in notes:
- exactness stayed intact
- removing upfront warm-start plus adding the mixed side heap was a regression on the NYC anytime benchmark

### Earlier local `LTMOA_parallel` vs `SOPMOA_relaxed` note

Referenced historical note source:
- [paper_note_ltmoa_fast_parallel_2026-04-10.md](/workspaces/SOPMOA/notes/paper_note_ltmoa_fast_parallel_2026-04-10.md)

Representative recorded median numbers at `0.5s`:

| Solver | Runtime (s) | Frontier | First solution (ms) |
| --- | ---: | ---: | ---: |
| `SOPMOA_relaxed` | `0.506` | `58` | `78.178` |
| `LTMOA_parallel` | `0.518` | `80` | `19.280` |

Interpretation preserved in notes:
- this was the strongest early positive signal for the new parallel design
- still a timeout-limited anytime comparison, not a completed exact real-map benchmark
