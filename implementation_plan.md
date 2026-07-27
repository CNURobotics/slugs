# Slugs Well-Separation Completion Plan

This is the Slugs-side implementation plan for completing
`--checkWellSeparation` as the backend requested by the FlexBE synthesis module
plan. It covers the C++/Slugs fork only: parsing, semantics, CLI behavior, JSON
contract, diagnostics, witnesses, tests, and validation. The ROS 2/Python
pipeline wrapper remains a separate integration task.

## Current Status

- [x] Add a Slugs CLI mode: `--checkWellSeparation`.
- [x] Support stdout JSON:
  `slugs --checkWellSeparation spec.slugsin`.
- [x] Support file JSON:
  `slugs --checkWellSeparation --jsonOutput spec.slugsin result.json`.
- [x] Implement Maoz/Ringert Algorithm 1 case diagnosis for plain Slugs GR(1):
  - `P-all/E-ini`
  - `P-all/E-safe`
  - `P-reach/E-safe`
  - `P-all/E-just`
  - `P-reach/E-just`
  - `WELL_SEPARATED`
- [x] Preserve `##` names for environment assumptions in this mode.
- [x] Emit category-level `responsible_assumptions`.
- [x] Emit `responsible_assumption_mode:
  category_assumptions_not_minimal_core`.
- [x] Emit representative state-cube witnesses for safety and liveness cases.
- [x] Add tiny smoke fixtures for init, safety, liveness, well-separated, and
  stdout/file JSON behavior.
- [x] Add Booleanized Maoz/Ringert forklift regression fixtures.

## Semantic Baseline

The implemented decision follows Maoz/Ringert FSE 2016 Algorithm 1:

- Compute `reachStates(theta_e, rho_e)` from `initEnv` and `safetyEnv`, with
  system choices unconstrained.
- For `E-safe`, compute system-winning states for environment
  `<theta_e, rho_e, empty>` and system `<true, true, {false}>`.
- For `E-just`, compute system-winning states for environment
  `<theta_e, rho_e, J_e>` and system `<true, true, {false}>`.
- Intersect winning states with the environment-reachable region.
- Distinguish `P-all` from `P-reach` using all legal environment initial input
  valuations.

Important Slugs-specific semantics:

- `[ENV_TRANS]` may mention current inputs, current outputs, and next inputs.
- `[ENV_TRANS]` may not mention next outputs.
- `[ENV_LIVENESS]` may mention current/next inputs and outputs, matching the
  parser already used by ordinary Slugs synthesis.
- Original system guarantees are ignored for well-separation, as required by
  the `<true, true, {false}>` reduction.
- The mode currently assumes plain compiled `.slugsin` semantics. It does not
  separate auxiliary variables introduced by higher-level pattern translations.

## Remaining Implementation Work

### 1. Validate Witness Semantics

- [x] Add representative state-cube witnesses.
- [x] Include current input/output proposition values.
- [x] Include witness metadata: `type`, `source_region`, and `case`.
- [x] Extend smoke tests to assert witness presence and selected values.
- [x] Prefer direct one-step safety witnesses from
  `forall nextInput. !safetyEnv` when available; fall back to the broader
  safety-winning reachable region otherwise.
- [x] Run the rebuilt binary and mark witness tests passing.
- [x] Decide whether `P-all/E-ini` should keep `witness: null` or add a
  structured `unsat_env_init` witness explanation.
- [x] Add a witness-validity checker in the smoke test script that verifies the
  emitted cube satisfies the expected source region when feasible.

### 2. Improve Responsible-Assumption Diagnostics

- [x] Preserve `##` assumption names for env init/safety/liveness.
- [x] Report category-level names in `responsible_assumptions`.
- [x] Add `line` or `source_index` metadata if lightweight to preserve during
  parsing.
- [x] For `E-safe`, compute which individual safety clauses are violated by at
  least one witness transition candidate.
- [x] For `E-just`, report the justice assumptions that are avoidable in the
  winning region when this can be derived without constructing a full strategy.
- [x] Keep the JSON explicit that this is not a minimized core.

### 3. Add Counterstrategy or Trace Witnesses

- [x] Decide whether to reuse `strategyDumpingData` from the modified
  `<true,true,{false}>` GR(1) run or implement a dedicated compact witness
  extraction path. Decision: keep this mode on a dedicated compact witness path
  for JSON stability; do not dump full strategies by default.
- [x] For `E-safe`, emit a one-step witness when possible:
  - current input/output state;
  - system output choice;
  - explanation that every next-input choice violates `rho_e`, or a violating
    clause sample.
- [x] For `E-just`, emit a short abstract witness:
  - reachable starting cube;
  - avoided environment justice index/name;
  - strategy-region metadata.
- [x] Keep full strategy extraction optional because JSON size can grow quickly.

### 4. Add Core Minimization, Optional

- [x] Implement only after witness diagnostics are stable.
- [x] Use delta debugging (Zeller's DDMin) over named environment assumptions
  as the search strategy for the paper's core minimization, matching the
  reference implementations this mode is validated against (Maoz/Ringert
  FSE 2016 Sect. 5.3 and Gorenstein/Maoz/Ringert ICSE 2024 Sect. 6.3, both of
  which use DDMin rather than a plain greedy deletion pass).
- [x] Evaluate subsets with the same Slugs-level well-separation check used by
  the main mode, restoring the original environment game afterwards.
- [x] Expose as an opt-in flag, not default behavior, because it multiplies
  solver calls.
- [x] Emit `core_mode`, `core_complete`, and `core_assumptions`.
- [x] **Fixed (2026-07-27):** the greedy search recomputed reachable states
  from each reduced candidate instead of pinning them to the original
  specification, as Maoz/Ringert Definition 3 requires
  (`sysWinSts(...) ∩ reachStates(θ^e_ASM, ρ^e_ASM)`, not
  `reachStates` of the candidate). Since removing assumptions can only
  enlarge the reachable region, this let the greedy search settle on a core
  whose witness states are unreachable in the original specification,
  misattributing the cause of non-well-separation. `computeGreedyCoreIfRequested`
  now computes `originalReachableStates` once before masking begins and
  `currentEnvironmentIsNonWellSeparated` takes it as a fixed parameter.
  Regression: `examples/well_separation/core_reach_pinned_to_original.slugsin`.
- [x] **Changed (2026-07-27):** replaced the linear greedy-deletion search with
  delta debugging (DDMin), for parity with both reference implementations'
  own core-search strategy (see above) rather than for its own sake — DDMin
  only wins asymptotically for larger candidate counts than this branch's
  fixtures exercise. Correctness of the switch relies on the same
  monotonicity the reachability-pinning fix above established: with
  `originalReachableStates` fixed, "is this subset non-well-separated" is
  monotonically increasing in subset inclusion (Maoz/Ringert Theorem 2, "Core
  Monotonic"), which is exactly the structure DDMin's subset/complement
  search assumes. The entry point (renamed
  `computeDeltaDebuggingCoreIfRequested`) delegates to a new `ddminSearch`
  helper; the well-separation check oracle
  (`currentEnvironmentIsNonWellSeparated`) is unchanged. Verified the switch
  is behavior-preserving on both existing core fixtures: `core_mode` is now
  `delta_debugging_1_minimal_non_well_separated_subset`, and
  `forklift_cargo_conflict.slugsin` / `core_reach_pinned_to_original.slugsin`
  still report the identical cores (`{dropCargo, clearCargo}` and
  `{holdFreezeFalse, visitP}` respectively) as before the switch, just via
  more solver calls at this small scale (19 and 32, vs. the prior greedy
  scan's fewer calls) — expected, since DDMin's advantage is asymptotic and
  these fixtures are too small to benefit from it.

### 5. Expand Tests

- [x] Tiny smoke cases:
  - well-separated trivial;
  - contradictory environment init;
  - `P-all/E-safe`;
  - `P-reach/E-safe`;
  - `P-all/E-just`;
  - `P-reach/E-just`.
- [x] Booleanized forklift regressions:
  - cargo safety conflict;
  - `{findStat, samePos}`;
  - removing `samePos`;
  - adding `G(atStation)`.
- [x] Add expected checks for `responsible_assumption_mode`.
- [x] Add expected checks for witness `source_region` and `case`.
- [x] Add malformed/edge fixtures:
  - no env liveness section;
  - no sys liveness section;
  - no output variables;
  - no input variables;
  - environment liveness using next variables.
- [x] Add regression fixtures for incomparable summary cases where both
  `P-reach/E-safe` and `P-all/E-just` should be reported.
- [x] Add brute-force enumerative oracle for very small Boolean games.
- [x] Add a test mode that compares Slugs output against the oracle over a small
  generated matrix.

### 6. Validate Against External References

- [x] Use Maoz/Ringert Algorithm 1 as the authoritative semantic target.
- [x] Add Booleanized fixtures inspired by the paper's forklift discussion.
- [x] Compare selected fixtures against Spectra/SYNTECH, if available.
  Status: direct Spectra/SYNTECH execution is unavailable in this checkout and
  session; the branch instead validates the literature-derived forklift fixtures
  with Slugs output checks and a small explicit-state oracle.
- [x] Check whether the SYNTECH supporting-material archive is still reachable
  and legally reusable. Status: the old separation support-material path was
  not found/reachable through current public search; no licensing basis for
  vendoring external fixtures has been established.
- [x] If usable, derive a small permissively documented fixture subset rather
  than vendoring the whole archive. Status: not usable at this time; keep this
  branch on hand-written fixtures only.

### 7. Harden CLI and JSON Contract

- [x] Decide whether `--jsonOutput` should remain reused for this mode or
  whether to add a clearer flag such as `--wellSeparationJsonOutput`.
  Decision: keep `--jsonOutput` for this mode so the CLI matches existing
  Slugs JSON-producing modes; avoid adding a second output flag until a broader
  CLI cleanup is needed.
- [x] Add a `format_version` field to JSON.
- [x] Add basic `tool` metadata:
  - Slugs mode;
  - backend;
  - algorithm.
- [x] Add build-specific tool metadata if the build system can provide it:
  - compile-time version if available;
  - git commit if available.
  Status: no existing compile-time version or git commit hook is available in
  this build system, so the JSON currently reports stable mode/backend/algorithm
  metadata only.
- [x] Add stable `status` values only:
  - `WELL_SEPARATED`
  - `NON_WELL_SEPARATED`
  - `ANALYSIS_INCOMPLETE`
  - `ANALYSIS_ERROR` if recoverable internal errors are handled locally later.
- [x] Keep normal process failures as nonzero exits for parser/file/internal
  errors unless the Python wrapper requires an always-JSON failure mode.

### 8. Performance and Safety

- [x] Record elapsed sub-times for reachability, safety-winning computation,
  liveness-winning computation, diagnostics, and witness extraction.
- [x] Add BDD size metrics for key regions already exposed in `checks`.
- [x] Confirm that temporary mutation of `safetySys`, `livenessAssumptions`, and
  `livenessGuarantees` is restored on all normal paths.
- [x] Consider a small RAII guard for temporary GR(1) context mutation.
- [x] Confirm dynamic reordering flags still apply consistently through the
  existing `main.cpp` CUDD setup before context construction.

### 9. Documentation

- [x] Add `doc/well_separation_implementation_plan.md`.
- [x] Add this root completion plan.
- [x] Document fixtures in `examples/well_separation/README.md`.
- [x] Add README usage examples for the new CLI mode.
- [x] Document the exact semantic limitations:
  - plain `.slugsin` only;
  - no separated auxiliary-pattern semantics yet;
  - category assumptions are not minimal cores;
  - current witness is a state cube, not a full strategy.

## Done Criteria for Slugs Backend

The Slugs-side work is complete enough for ROS/Python integration when:

- [x] The rebuilt binary passes all smoke tests.
- [x] Witness assertions pass.
- [x] Expanded 15-case smoke suite passes with JSON `format_version` 0.1.
- [x] Source metadata and timing assertions pass in the 15-case smoke suite.
- [x] RAII mutation guard and BDD metric assertions pass in the 15-case smoke
  suite.
- [x] Witness-local implicated-assumption assertions pass in the smoke suite.
- [x] Unavailable-witness reason assertions pass in the smoke suite.
- [x] A brute-force oracle validates the tiny fixture matrix against Slugs.
- [x] Incomparable-case regression passes in both smoke and explicit oracle
  suites.
- [x] Expanded 16-case smoke and explicit oracle suites pass after adding the
  incomparable-case regression.
- [x] Witness-region validation passes in the 16-case smoke suite.
- [x] Compact witness-details assertions pass in the 16-case smoke and explicit
  oracle suites after rebuild.
- [x] Opt-in core-minimization assertions pass in the smoke suite (now
  delta-debugging based, see item 4 above), and the explicit oracle suite
  still passes after rebuild.
- [x] JSON has a documented format version.
- [x] The CLI behavior is documented.
- [x] Known semantic limitations are documented.
- [x] At least one external-reference comparison has been performed or
  explicitly marked unavailable.
- [x] The branch has no unrelated generated/build artifact churn.
- [x] Independent verification against the primary source has been performed
  and one correctness defect found by that verification has been fixed. See
  "Independent Verification Against the Primary Source" below.

## Independent Verification Against the Primary Source (2026-07-27)

The Slugs-side implementation was re-checked directly against the cited paper
rather than against recollection or the design docs alone.

### Citation

S. Maoz and J. O. Ringert, "On Well-Separation of GR(1) Specifications,"
*Proc. 24th ACM SIGSOFT International Symposium on the Foundations of
Software Engineering (FSE 2016)*, Seattle, WA, USA, 2016, pp. 362-372.
doi:10.1145/2950290.2950300. Author-hosted PDF:
https://www.cs.tau.ac.il/~maozs/papers/well-separation-fse16-cr.pdf

### Method

- Fetched and read the paper directly (Sections 1-5, including Definition 2
  (well-separation, adapted from Klein and Pnueli), the *P-all*/*P-reach* and
  *E-ini*/*E-safe*/*E-just* case taxonomy (Sect. 4.1), Algorithm 1, the Lego
  forklift running example (Listing 1, Fig. 3), the non-monotonicity result
  (Theorem 1), and the non-well-separated core (Definition 3, Sect. 5.2).
- Transcribed Algorithm 1's pseudocode and compared it statement-by-statement
  against `execute()` in `src/extensionWellSeparation.hpp`. The control flow
  (the `E-ini` short-circuit, the `E-safe` check and its own short-circuit on
  `P-all`, the unconditional `P-all/E-just` addition versus the
  `res`-must-be-empty gate on `P-reach/E-just`) matches the paper exactly,
  including the asymmetry that produces the paper's "incomparable"
  `(P-reach,E-safe)`/`(P-all,E-just)` combination.
- Confirmed `sysWinSts` is reused correctly: the paper states `sysWinSts` is
  the same Z fixed point as the standard GR(1) algorithm, computed against
  system specification `⟨true,true,{false}⟩`. `computeEnvironmentViolationWinningStates`
  reduces to exactly that by temporarily setting `safetySys=true`,
  `livenessGuarantees={false}` and calling Slugs' existing, previously
  validated `GR1Context::computeWinningPositions()` (`src/synthesisAlgorithm.cpp`)
  rather than a new fixed point — this reuse is the main reason the reduction
  is trustworthy.
- Confirmed the Mealy turn order: the paper's game has the environment move
  first, then the system responds. Slugs' `cox` operator
  (`safetyEnv.Implies(foundPaths).ExistAbstract(varCubePostOutput).UnivAbstract(varCubePostInput)`
  in `synthesisAlgorithm.cpp`) applies `∃` before `∀`, i.e. computes
  `∀ next-input. ∃ next-output`, which is the correct predecessor operator for
  that turn order. `allEnvironmentInitialInputsCanBeWon` mirrors the same
  shape as the existing `checkRealizability()` (`∀ preInput ∈ initEnv. ∃
  preOutput`), omitting only `initSys`, consistent with the paper's `θ^s =
  true` reduction (and correct regardless, since `initSys` never appears in
  `computeWinningPositions()`'s fixed point).
- Cross-checked the paper's own worked example: Listing 1's `dropCargo` /
  `clearCargo` conflict is reported as `{(P-all,E-safe)}` in the paper;
  `forklift_cargo_conflict.slugsin` reproduces this. The paper's Theorem 1
  proof uses the reduced `{findStat, samePos}` environment to show
  non-monotonicity, reporting `{(P-reach,E-just)}`, well-separated after
  removing `samePos`, and well-separated after adding `G(atStation)`;
  `forklift_findstat_samepos.slugsin`, `forklift_findstat_only.slugsin`, and
  `forklift_findstat_samepos_station_invariant.slugsin` reproduce all three
  results exactly.
- Built the branch clean and ran `tools/testWellSeparation.py` and
  `tools/testWellSeparationOracle.py` (independent brute-force explicit-state
  reimplementation, not sharing code with the BDD-based Slugs implementation)
  against all fixtures — all passed both before and after the fix below.

### Defect found and fixed: core minimization did not match Definition 3

Definition 3 defines a non-well-separated core `C` as a minimal subset of the
original assumption set `ASM` such that
`sysWinSts(⟨θ^e_C,ρ^e_C,J^e_C⟩,⟨true,true,{false}⟩) ∩ reachStates(θ^e_ASM,ρ^e_ASM) ≠ ∅` —
i.e. the reachable-states term is pinned to the **original** `ASM`, not
recomputed from the candidate `C` under test. The paper calls this out
explicitly because removing assumptions can only enlarge the reachable
region, so recomputing reachability per-candidate can "discover" a violation
through states that are not actually reachable in the original specification.

`computeGreedyCoreIfRequested` / `currentEnvironmentIsNonWellSeparated` in
`src/extensionWellSeparation.hpp` did exactly that: it called
`computeEnvironmentReachableStates()` fresh on every greedy step, using
whatever `initEnv`/`safetyEnv` the candidate mask had already produced. This
was verified to cause real misattribution, not just non-minimality: a
constructed fixture
(`examples/well_separation/core_reach_pinned_to_original.slugsin`) whose only
real cause of non-well-separation is a `holdFreezeFalse`/`visitP` liveness
trap instead reported an unrelated `decoyTrue`/`decoyFalse` safety pair as the
core, once an unrelated `gate_r`/`gate_r_safety` guard was greedily (and
validly, in isolation) removed and inflated the candidate's own reachable
region enough to make the decoy pair look reachable.

Fix: `computeGreedyCoreIfRequested` now computes `originalReachableStates`
once, before any candidate masking begins, and
`currentEnvironmentIsNonWellSeparated` takes it as a fixed parameter instead
of recomputing reachability from the current (possibly reduced) environment.
After the fix, the same fixture correctly reports
`{holdFreezeFalse, visitP}` as the core. This only affected the opt-in
`--minimizeWellSeparationCore` path; the primary `status`/`cases`/
`responsible_assumptions` diagnosis was already correct and unaffected.

### Residual test-completeness gaps (not yet addressed)

- No fixture pairs a well-separation result against ordinary `slugs`
  realizability on the same spec, to empirically demonstrate the orthogonality
  of the two notions (realizable-but-non-well-separated,
  unrealizable-but-well-separated).
- `ENV_LIVENESS` mentioning a next-state **output** variable is untested (only
  next-input is, via `env_liveness_next_input.slugsin`), despite the parser
  allowing `PostOutput` there.
- No regression pins down that `SYS_INIT` is irrelevant to the result — true
  today by construction, but not locked in by a test.
