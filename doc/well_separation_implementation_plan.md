# Well-Separation Slugs Implementation Plan

This branch adds a Slugs-native pre-synthesis well-separation analysis mode for
GR(1) specifications. The immediate goal is an experimental but tested command
line analysis mode that can be called by the ROS/FlexBE synthesis pipeline after
`.slugsin` generation and before ordinary synthesis.

No build or install steps are part of this branch work until explicitly
requested.

## Repository Touch Points

- `src/main.cpp`
  - Owns command line flag discovery, permitted option combinations, CUDD
    reordering toggles, context construction, and `execute()`.
  - Add a new mode flag such as `--checkWellSeparation`.
  - Add it to `commandLineArguments` and `optionCombinations`.

- `src/gr1context.hpp`
  - Defines `GR1Context`, the shared base for parser, BDD variables, GR(1)
    clauses, fixed-point helpers, and execution hooks.
  - Reusable protected fields include:
    - `initEnv`, `initSys`
    - `safetyEnv`, `safetySys`
    - `safetyEnvFormulae`, `safetySysFormulae`
    - `livenessAssumptions`, `livenessGuarantees`
    - variable cubes/vectors for pre/post input/output abstraction and swapping.

- `src/synthesisContextBasics.cpp`
  - Parses `.slugsin` sections:
    - `[INPUT]` -> `PreInput`, `PostInput`
    - `[OUTPUT]` -> `PreOutput`, `PostOutput`
    - `[ENV_INIT]` -> `initEnv`
    - `[SYS_INIT]` -> `initSys`
    - `[ENV_TRANS]` -> `safetyEnvFormulae`, `safetyEnv`
    - `[SYS_TRANS]` -> `safetySysFormulae`, `safetySys`
    - `[ENV_LIVENESS]` -> `livenessAssumptions`
    - `[SYS_LIVENESS]` -> `livenessGuarantees`
  - The proposed analyzer should consume this parsed representation, not a
    second parser.

- `src/synthesisAlgorithm.cpp`
  - Implements the ordinary system-winning GR(1) fixed point.
  - Its `cox` shape is:
    `safetyEnv.Implies(foundPaths).ExistAbstract(postOutput).UnivAbstract(postInput)`.
  - Useful as the semantic reference for Slugs' Mealy-style turn order.

- `src/extensionCounterstrategy.hpp`
  - Implements the environment-winning dual algorithm and already shows how to
    make a mode as a templated extension around `GR1Context`.
  - Useful as an implementation reference if well-separation is reduced to a
    modified GR(1) game.

- `src/extensionAnalyzeAssumptions.hpp`
  - Demonstrates preserving per-assumption names from `##` comments while
    parsing.
  - Useful later for richer diagnostics, but the initial implementation should
    stay conservative unless the parser reuse is clean.

## Formal Target

Maoz and Ringert's well-separation question is specification-level: can the
system win by forcing the environment to violate its assumptions? This differs
from ordinary realizability, environment-assumption satisfiability, and
post-synthesis auditing of one extracted strategy.

The initial implementation will avoid claiming more than it proves. It should
first implement the standard diagnostic checks against Slugs' parsed GR(1)
semantics and report the semantic mode explicitly. The implementation must
verify each of these against Slugs' actual turn order:

- environment initialization: no legal environment initial valuation;
- environment safety: system can choose outputs so all environment next-input
  choices violate at least one environment transition constraint;
- environment liveness: system can keep play within environment-safety-respecting
  behavior while preventing an environment justice assumption forever;
- all-state versus reachable-state variants, reported separately if both are
  implemented.

The Slugs parser allows environment liveness assumptions to mention pre/post
inputs and outputs, and environment transition assumptions to mention current
inputs, current outputs, and next inputs. The analyzer must keep that distinction
because next-output mentions in environment safety are intentionally rejected by
the parser.

## Implementation Strategy

1. Add `src/extensionWellSeparation.hpp`.
   - Derive from `GR1Context` as other modes do.
   - Override `execute()`.
   - Emit a stable, machine-readable JSON object to stdout.
   - Keep human notes, if any, on stderr.

2. Implement Algorithm 1 from Maoz/Ringert:
   - `E-ini`: `initEnv` is false.
   - `reachStates(theta_e, rho_e)`: compute the environment-assumption
     reachable region from `initEnv` and `safetyEnv`, with system choices
     unconstrained.
   - `E-safe`: compute Slugs system-winning states for environment
     `<theta_e, rho_e, empty>` and system `<true, true, {false}>`.
   - `E-just`: compute Slugs system-winning states for environment
     `<theta_e, rho_e, J_e>` and system `<true, true, {false}>`.
   - summarize cases using the paper's `P-all`, `P-reach`, `E-ini`, `E-safe`,
     and `E-just` taxonomy.

3. JSON result fields for the first usable mode:
   - `status`: `WELL_SEPARATED`, `NON_WELL_SEPARATED`, or `ANALYSIS_INCOMPLETE`
   - `format_version`: JSON schema version, currently `0.1` until release
   - `tool`: Slugs mode/backend/algorithm metadata
   - `complete`: boolean
   - `elapsed_time` and `timing`: total and per-phase elapsed seconds
   - `method`: string
   - `semantic_mode`: string
   - `violated_assumption_type`: `env_init`, `env_safety`, `env_liveness`, or
     `none`
   - `cases`: e.g. `P-all/E-ini`, `P-all/E-safe`, `P-reach/E-just`
   - `responsible_assumptions`: named assumptions from the responsible
     category, using `##` comments when present, with `source_index` and `line`;
     this is not a minimal core
   - `witness_available`: boolean
   - `witness_unavailable_reason`: reason string when no concrete witness is
     available, e.g. `unsat_env_init`
   - `witness`: representative current-state cube with `source_region`, `case`,
     `inputs`, `outputs`, witness-local `implicated_assumptions`, and compact
     `details`, or `null`
   - `core_*`: disabled by default; with `--minimizeWellSeparationCore`, a
     1-minimal non-well-separated subset of environment assumptions found via
     delta debugging (Zeller's DDMin)
   - `checks`: object with per-check booleans and BDD sizes
   - `warnings`: array

4. Do not disturb ordinary synthesis.
   - The new mode should have an isolated option combination.
   - Existing `GR1Context::execute()` and synthesis output should remain
     unchanged except for shared helper additions that are strictly necessary.

5. Keep tests source-only until build approval.
   - Add `.slugsin` fixtures under an examples/test-oriented path.
   - Add a lightweight shell or Python test driver that documents expected CLI
     invocations without running during this task.
   - Do not modify compiled objects or `src/slugs`.

## Test Fixtures

Initial fixtures should be tiny and hand-checkable:

- well-separated pass with unconstrained environment assumptions;
- `P-all/E-ini`: contradictory environment initialization;
- `P-all/E-safe`: a system output forces contradictory next-input requirements
  from all legal environment initial inputs;
- `P-reach/E-safe`: a reachable, but not all-initial, safety-violation winning
  region;
- `P-all/E-just`: an initial liveness-prevention winning region;
- `P-reach/E-just`: a reachable, but not all-initial, liveness-prevention
  winning region.
- Booleanized Maoz/Ringert forklift regressions:
  - cargo safety conflict from `dropCargo` and `clearCargo`;
  - `{findStat, samePos}` liveness trap;
  - removing `samePos`;
  - adding `G(atStation)` to remove the problematic reachable states.

Expected results should be written next to the fixtures so the future build/test
run can compare JSON fields exactly.

## Commit Plan

1. Commit this plan document.
2. Add the CLI extension skeleton and JSON result plumbing.
3. Add safety/init diagnostics.
4. Add fixtures and source-level test driver.
5. Add liveness/reachability diagnostics after the formal reduction is mapped
   carefully to Slugs' Mealy semantics.

Current branch status:

- Plan document committed in `23560e1`.
- Experimental CLI mode and JSON plumbing committed in `2001ba9` and
  `6ba4be0`.
- Smoke fixtures committed in `8b52b94`.
- Algorithm 1 case diagnosis committed in `70c49b3`.
- Forklift-derived regression fixtures committed in `474d4fb`.
