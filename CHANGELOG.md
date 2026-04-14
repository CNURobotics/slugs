# Changelog

This changelog follows a reverse-chronological layout for the `flexbe-synthesis` branch, with newest changes first.


### 2026-04-14
- Added CHANGELOG.md
- Added `tools/mealy2dot.py`, a utility that converts Slugs `.slugsin` specifications plus JSON strategy or counterstrategy output into Graphviz `.dot` visualizations, with optional rendered graph artifacts when Graphviz is available.
- Added `tools/pipeline.sh`, an end-to-end helper that compiles a `.structuredslugs` file, runs Slugs synthesis, falls back to counterstrategy extraction for unrealizable specifications, and invokes `mealy2dot.py` to generate visualization output.
- Added `tools/kill_big.sh`, a watchdog script that monitors an output file and terminates a running process once the file exceeds a configurable size threshold.

### 2026-04-13

- Ported the Python tooling to Python 3, including updates to `print` usage, `xrange` replacements, and related compatibility fixes in the structured parser, report generation, simulator, and helper scripts.
- Added synthesis performance reporting after `checkRealizability()`, including elapsed synthesis time and detailed CUDD manager statistics such as node counts, reorderings, garbage-collection activity, and memory usage.
- Extended explicit strategy and counterstrategy extraction to support JSON output and extraction metrics, including counts for states, transitions, rank coverage, deadlocks, and strategy dump entries.

## 2023-12-29

- Fork base from upstream [`VerifiableRobotics/slugs`](https://github.com/VerifiableRobotics/slugs):
`master`: [`a188d83`](https://github.com/VerifiableRobotics/slugs/commit/a188d83bccc2086e086c26266de659a7f556e48e) (`Merge pull request #25 from slivingston/patch-1`).
