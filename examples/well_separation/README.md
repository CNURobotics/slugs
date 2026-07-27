# Well-Separation Fixtures

These tiny `.slugsin` files exercise the experimental `--checkWellSeparation`
mode.

Assumption names are taken from `##` comments immediately before an environment
assumption clause. The JSON field `responsible_assumptions` reports named
assumptions from the responsible category; it is not a minimal non-well-separated
core. Use `--minimizeWellSeparationCore` to request the slower delta-debugging
(DDMin) 1-minimal `core_assumptions` diagnostic.

- `well_separated_trivial.slugsin`
  - No init or safety witness exists.
  - Expected status: `WELL_SEPARATED`

- `env_init_contradiction.slugsin`
  - Environment initialization is contradictory.
  - Expected status: `NON_WELL_SEPARATED`
  - Expected case: `P-all/E-ini`

- `env_safety_forced_initial.slugsin`
  - The initial system output forces contradictory next-input requirements.
  - Expected status: `NON_WELL_SEPARATED`
  - Expected case: `P-all/E-safe`

- `env_safety_forced_some_state.slugsin`
  - The contradiction is reachable after the initial state, but not winning from
    all legal environment initial inputs.
  - Expected status: `NON_WELL_SEPARATED`
  - Expected case: `P-reach/E-safe`

- `env_liveness_prevented_initial.slugsin`
  - The system can keep the environment in an initial state where justice `p`
    is never visited.
  - Expected status: `NON_WELL_SEPARATED`
  - Expected case: `P-all/E-just`

- `env_liveness_prevented_reachable.slugsin`
  - The same bad region is reachable but not winning from all legal environment
    initial inputs.
  - Expected status: `NON_WELL_SEPARATED`
  - Expected case: `P-reach/E-just`

- `forklift_cargo_conflict.slugsin`
  - Booleanized excerpt of the Maoz/Ringert Lego forklift assumptions.
  - Choosing `mot_BWD` and `lift_DROP` forces contradictory requirements on
    `cargo'`.
  - Expected status: `NON_WELL_SEPARATED`
  - Expected case: `P-all/E-safe`

- `forklift_findstat_samepos.slugsin`
  - Booleanized `{findStat, samePos}` fixture.
  - Choosing `mot_STOP` in a reachable `!atStation` state prevents `GF
    atStation`.
  - Expected status: `NON_WELL_SEPARATED`
  - Expected case: `P-reach/E-just`

- `forklift_findstat_only.slugsin`
  - Removing `samePos` from the previous fixture removes the liveness trap.
  - Expected status: `WELL_SEPARATED`

- `forklift_findstat_samepos_station_invariant.slugsin`
  - Adding `G(atStation)` removes the problematic reachable states, matching the
    non-monotonicity discussion in the paper.
  - Expected status: `WELL_SEPARATED`

- `no_env_liveness_section.slugsin`
  - Omits `[ENV_LIVENESS]`; Slugs inserts the usual true liveness assumption.
  - Expected status: `WELL_SEPARATED`

- `no_sys_liveness_section.slugsin`
  - Omits `[SYS_LIVENESS]`; Slugs inserts the usual true liveness guarantee.
  - Expected status: `WELL_SEPARATED`

- `no_output_variables.slugsin`
  - Exercises a specification with no controllable propositions.
  - Expected status: `WELL_SEPARATED`

- `no_input_variables.slugsin`
  - Exercises a specification with no input propositions where the sole output
    can immediately violate environment safety.
  - Expected status: `NON_WELL_SEPARATED`
  - Expected case: `P-all/E-safe`

- `env_liveness_next_input.slugsin`
  - Exercises an environment liveness assumption over a next-input proposition.
  - Expected status: `NON_WELL_SEPARATED`
  - Expected case: `P-all/E-just`

- `core_reach_pinned_to_original.slugsin`
  - Regression for Maoz/Ringert Definition 3: a non-well-separated core must be
    checked against the reachable states of the *original* assumption set, not
    the reachable states of whatever reduced candidate the delta-debugging
    search is currently testing. `decoyTrue`/`decoyFalse` only become reachable if
    `gate_r`/`gate_r_safety` are removed first; the real, always-reachable
    cause is the `holdFreezeFalse`/`visitP` liveness trap.
  - Expected status: `NON_WELL_SEPARATED`
  - Expected case: `P-all/E-just`
  - Expected `--minimizeWellSeparationCore` result: `{holdFreezeFalse, visitP}`,
    never the decoy pair.

- `incomparable_reach_safe_all_just.slugsin`
  - Exercises the summary case where a safety violation is only reachable
    (`P-reach/E-safe`) while an environment justice violation is forceable from
    all legal environment initial inputs (`P-all/E-just`).
  - Expected status: `NON_WELL_SEPARATED`
  - Expected cases: `P-reach/E-safe`, `P-all/E-just`
