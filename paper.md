# Well-Separation of GR(1) Specifications: Background and a Slugs Implementation

This document has two parts. Part 1 summarizes the theory of well-separation
for GR(1) specifications and the literature behind this implementation. Part 2
describes how that theory is realized as an experimental analysis mode in
Slugs (`--checkWellSeparation`, `src/extensionWellSeparation.hpp`).

## Part 1: Background

### 1.1 GR(1) synthesis

Generalized Reactivity(1), or GR(1), is a fragment of LTL used for reactive
synthesis. A GR(1) specification is a tuple of an environment part
`⟨θ^e, ρ^e, J^e⟩` and a system part `⟨θ^s, ρ^s, J^s⟩`, where `θ` is an initial
condition, `ρ` is a safety (transition) relation, and `J` is a set of justice
(recurrence) requirements. Two notions of realizability are used:

- **Implication realizability** (`ψ^→`): the system guarantees hold whenever
  the environment assumptions hold — `(θ^e ∧ Gρ^e ∧ ⋀ GFJ_i^e) → (θ^s ∧ Gρ^s ∧
  ⋀ GFJ_j^s)`.
- **Strict realizability** (`ψ^sr`): a stronger notion, also requiring that
  the system does not simply wait for the environment to break its own
  assumptions.

GR(1) synthesis is efficient (polynomial-time symbolic fixed-point
computation) because it restricts to this initial/safety/justice structure
rather than full LTL. Slugs implements this fixed point in
`GR1Context::computeWinningPositions()` (`src/synthesisAlgorithm.cpp`), which
the Slugs implementation reuses unmodified — see Part 2.

### 1.2 The well-separation problem

A specification is **realizable** (in the `ψ^→` sense) whenever the system
guarantees hold under the environment assumptions — including trivially, by a
controller that forces the environment to break its own assumptions rather
than by actually satisfying its guarantees. Such a controller is a valid
implication-realizability witness but is not useful: it "solves" the
synthesis problem by making the environment lose. This is the phenomenon
**well-separation** is meant to characterize and rule out.

Klein and Pnueli (`HVC 2010`, "Revisiting Synthesis of GR(1) Specifications")
first defined well-separation as a sufficient condition under which
implication realizability and strict realizability coincide, so that a
controller synthesized under the (efficient, standard) implication semantics
behaves as one would want under the (stronger, more intuitive) strict
semantics.

Maoz and Ringert restate this definition precisely and give it an algorithmic
treatment:

> **Definition (Well-Separation).** A GR(1) environment specification
> `⟨θ^e, ρ^e, J^e⟩` is well-separated iff `ψ^sr` has no reachable system
> winning states for the system specification `⟨true, true, {false}⟩`.

The system specification `⟨true, true, {false}⟩` means: the system's
initial and safety constraints are unconstrained, and its only justice
requirement is `false` — i.e., unsatisfiable. A system playing against this
trivial specification can only "win" by forcing the *environment* to violate
one of its own assumptions (initial, safety, or justice); there is no other
way for the left-hand side of `ψ^sr` to fail. So well-separation is exactly
the question: from some state the environment can actually reach, can the
system force an assumption violation? Note this is a property of the
**environment** part of the specification only — the system's guarantees do
not enter into Definition 2 at all.

### 1.3 Cases of non-well-separation (Maoz/Ringert, FSE 2016)

S. Maoz and J. O. Ringert, "On Well-Separation of GR(1) Specifications,"
*Proc. 24th ACM SIGSOFT International Symposium on the Foundations of
Software Engineering (FSE 2016)*, Seattle, WA, USA, 2016, pp. 362–372.
doi:10.1145/2950290.2950300.

This is the primary reference for this implementation. It extends Klein/Pnueli's
definition with a fine-grained case analysis, a diagnosis algorithm, strategy
construction for demonstrating each case, and a notion of *core* (a minimal
subset of assumptions responsible for non-well-separation).

**Case taxonomy.** Non-well-separation is classified along two independent
axes:

- **Winning positions** — from which states can the system force a
  violation?
  - `P-all`: from *all* initial environment choices.
  - `P-reach`: from *some* reachable state, but not necessarily all initial
    ones.
- **Responsible assumption part** — *which* assumption is forced to be
  violated?
  - `E-ini`: the initial assumption itself is contradictory
    (`θ^e ≡ false`) — a specification-level deadlock, independent of any
    system strategy.
  - `E-safe`: a safety assumption `ρ^e` can be forced to be violated.
  - `E-just`: a justice assumption `J_i^e` can be forced to never hold again.

`E-ini` and `E-safe` both imply `E-just` (any way to force a safety
violation is a fortiori a way to force *some* justice violation, and a
contradictory initial condition means the environment cannot even start), so
the paper summarizes results by their `⊑`-smallest representative
combination. Because `P-all/E-safe` and `P-reach/E-safe` don't compare
against `P-all/E-just` in the same partial order, one incomparable
combination is possible in practice:
`{(P-reach, E-safe), (P-all, E-just)}` — a safety violation is forceable
only from some reachable states, but a (different) justice violation is
forceable from every initial state.

**Algorithm 1** (paraphrased from the paper) diagnoses these cases:

```
if θ^e ≡ false:                                          # E-ini
    return {(P-all, E-ini)}

reach ← reachStates(θ^e, ρ^e)                             # forward reachability,
                                                            # system choices unconstrained
win_s ← sysWinSts(⟨θ^e, ρ^e, ∅⟩, ⟨true, true, {false}⟩)   # E-safe only
res ← {}
if win_s ∩ reach ≠ ∅:
    if sysWinAllIni(win_s, θ^e):
        return {(P-all, E-safe)}                          # strongest possible result: stop here
    res ← res ∪ {(P-reach, E-safe)}

win ← sysWinSts(⟨θ^e, ρ^e, J^e⟩, ⟨true, true, {false}⟩)   # E-safe or E-just
if win ∩ reach ≠ ∅:
    if sysWinAllIni(win, θ^e):
        res ← res ∪ {(P-all, E-just)}                     # added unconditionally
    else if res = ∅:
        res ← res ∪ {(P-reach, E-just)}                   # only if nothing stronger already found

return res                                                 # empty iff well-separated
```

`sysWinSts(env, sys)` is the ordinary GR(1) system-winning-states fixed
point (the same `Z` computation used for regular synthesis), evaluated
against the reduced system specification. `sysWinAllIni(win, θ^e)` checks
whether *every* legal environment initial input choice is covered by the
winning region (`∀ preInput ∈ θ^e. ∃ preOutput. win`). Both the `E-ini`
short-circuit and the `P-all/E-safe` early return are load-bearing: they
determine which of the summarized case combinations is reported, including
the incomparable case above (only reachable because `(P-reach, E-safe)` does
*not* short-circuit the `E-just` check, while `(P-all, E-safe)` does).

**Strategies.** Once a case is identified, a concrete strategy demonstrating
it can be extracted from the same game memory used during the
`sysWinSts` fixed-point computation — no separate solve is required.

### 1.4 Non-monotonicity and the non-well-separated core

Non-well-separation is not, in general, monotonic with respect to adding or
removing assumptions — a specification can become well-separated by *either*
removing an assumption (shrinking the winning region) or adding one
(shrinking the reachable region), and either operation can also do the
opposite (Theorem 1 in the paper, proved with the running forklift example:
removing the `samePos` safety assumption fixes non-well-separation caused by
`{findStat, samePos}`, and separately, *adding* `G(atStation)` also fixes
it). This non-monotonicity is exactly what makes "core" a nontrivial notion
to define correctly.

The paper's fix is to define a **non-well-separated core** relative to the
*original* specification's reachable states, not the candidate subset's own:

> **Definition (Non-Well-Separated Core).** A non-well-separated core for a
> set of assumptions `ASM` is a minimal set `C ⊆ ASM` such that
> `sysWinSts(⟨θ^e_C, ρ^e_C, J^e_C⟩, ⟨true, true, {false}⟩) ∩
> reachStates(θ^e_ASM, ρ^e_ASM) ≠ ∅`.

Pinning the reachability term to `ASM` (not `C`) restores monotonicity of
the *check* with respect to adding assumptions back into a candidate core
(**Theorem 2, "Core Monotonic"**), since removing assumptions can only
enlarge the true reachable region — using the frozen, original reachable set
as the intersection target avoids the search "discovering" a spurious
violation through states that are not actually reachable in the original
specification. This monotonicity is what licenses using a deletion-based
minimization search (delta debugging or greedy deletion) at all: without it,
the search could settle on a "core" that isn't actually a reason for
non-well-separation of the original specification.

The paper's own reference implementation computes cores using **delta
debugging** (Zeller's `ddmin`, "Yesterday, my program worked. Today, it does
not. Why?", ESEC/FSE 1999) over the set of named assumptions, rather than a
naive one-at-a-time linear scan.

### 1.5 The forklift running example

Both the background paper and the Slugs test fixtures use a Lego
forklift controller as a running example (a physical robot with `atStation`
and `cargo` sensors, `mot` and `lift` actuators). Two concrete results from
the paper are used as ground truth in the fixtures:

- The safety assumptions `dropCargo` ("if lift is DROP, cargo becomes true
  next") and `clearCargo` ("if motion is BWD, cargo becomes false next")
  jointly force a contradiction on `cargo'` — reported as
  `{(P-all, E-safe)}`.
- After fixing that (making `dropCargo` conditional on motion), the
  assumptions `findStat` ("always eventually reach a station", a justice
  requirement) and `samePos` ("the station reading doesn't change while
  stopped", a safety requirement) together let the system stop forever away
  from a station, forcing `findStat` to be violated — reported as
  `{(P-reach, E-just)}`. Removing `samePos`, or instead adding
  `G(atStation)`, both make the specification well-separated again
  (Theorem 1's non-monotonicity witnesses).

### 1.6 Follow-up work: Kind Controllers (ICSE 2024)

A. Gorenstein, S. Maoz, and J. O. Ringert, "Kind Controllers and Fast
Heuristics for Non-Well-Separated GR(1) Specifications," *Proc. IEEE/ACM 46th
International Conference on Software Engineering (ICSE 2024)*, Lisbon,
Portugal, pp. 28:1–28:12. doi:10.1145/3597503.3608131.

This is the most relevant follow-up to the FSE 2016 paper, but it does not
revise Definition 2, Algorithm 1, or Definition 3 — both are reused verbatim
as ground truth throughout the evaluation ("We used the algorithm of \[FSE16]
as the ground truth for WS of specifications, as it is both sound and
complete"). It contributes two independent, additive things instead:

1. **Kind controllers.** Majumdar et al. ("Environmentally-friendly GR(1)
   synthesis," TACAS 2019) gave a 4-fixed-point algorithm that synthesizes
   controllers that never force a *justice* violation, if a controller with
   that property exists — but such controllers may still force *safety*
   violations, making them "useful... but, in a way, misleading" for
   `E-safe` cases. This paper extends that algorithm, via a reduction that
   adds one auxiliary environment variable and three assumptions/guarantees
   around it, so the synthesized controller never forces *any* assumption
   violation (safety or justice), if such a controller exists at all. This
   is a **controller-synthesis** contribution — a downstream consumer of a
   non-well-separation diagnosis, not a change to the diagnosis itself.
2. **Fast heuristics (T1/T2/T3).** Three sound-but-incomplete, purely
   semantic (BDD-support-based, not syntactic) checks for common
   authoring mistakes that cause non-well-separation — an assumption
   referencing only system variables, a safety assumption missing a
   `next()` on the variable it's supposed to constrain, and pairs/sets of
   assumptions that can never be mutually satisfied. Each is orders of
   magnitude faster than the full realizability-based check because it is a
   linear symbolic scan rather than a fixed-point computation; when a
   heuristic fires, the result is guaranteed correct (sound), but a
   heuristic finding nothing does not guarantee well-separation
   (incomplete). Core computation via these heuristics uses delta debugging
   as well, again validated for correctness against, not replacing, "the
   core computation from \[FSE16]."

The paper also proposes, in passing, a definition of well-separation that
additionally accounts for the system's own guarantees (Section 6.2 of the
FSE16 paper's own extensions section, reused here as "Definition 4"): a
specification is well-separated *with respect to guarantees* if the system
cannot force an assumption violation even when required to also uphold
`⟨θ^s, ρ^s, J^s⟩` alongside `⟨true, true, {false}⟩`'s trivial-justice
reduction. This is a strictly weaker notion of non-well-separation than
Definition 2 (well-separation implies well-separation wrt. guarantees, not
the other way around) and is not implemented here — see §2.5.

## Part 2: The Slugs-side implementation

### 2.1 Scope and goal

This branch adds `--checkWellSeparation`, an experimental, opt-in Slugs mode
that implements Algorithm 1's case diagnosis (§1.3) directly against Slugs'
existing parsed GR(1) representation and existing winning-states fixed
point, plus an opt-in `--minimizeWellSeparationCore` flag implementing
Definition 3's core (§1.4). The intended use is as a pre-synthesis
diagnostic step in the FlexBE/ROS 2 synthesis pipeline, run on compiled
`.slugsin` specifications before ordinary synthesis, to catch
non-well-separated environments before a controller is built from them.

Everything lives in one new file, `src/extensionWellSeparation.hpp`, added
as a templated `GR1Context` extension the same way Slugs' other analysis
modes are (`extensionCounterstrategy.hpp`, `extensionAnalyzeAssumptions.hpp`,
etc.), plus minimal, additive registration changes in `src/main.cpp` and
`src/plugin_combination_enumerator.py` (the generator that produces
`main.cpp`'s option-combination table). **No existing synthesis code path is
modified**: `src/synthesisAlgorithm.cpp` and `src/gr1context.*` — the actual
GR(1) fixed-point implementation — have zero diff against the branch point.

### 2.2 The reduction

The core idea is that Definition 2's system specification
`⟨true, true, {false}⟩` is exactly what Slugs' *existing*,
previously-validated `GR1Context::computeWinningPositions()` computes a
fixed point for — no new fixed-point implementation is needed, only a
temporary substitution of the context's system fields immediately before
calling it:

```cpp
class TemporaryWellSeparationGame {
    // RAII: saves safetySys, livenessAssumptions, livenessGuarantees on
    // construction, restores them on destruction.
    TemporaryWellSeparationGame(context, includeEnvironmentLiveness) {
        context.safetySys = true;
        context.livenessGuarantees = {false};
        if (!includeEnvironmentLiveness) {
            context.livenessAssumptions = {true};   // models J^e = ∅
        }
        // else: leave livenessAssumptions as the real J^e
    }
    ~TemporaryWellSeparationGame() { /* restore all three fields */ }
};
```

Two calls to `computeWinningPositions()` under this substitution give
Algorithm 1's `win_s` (environment liveness excluded, i.e. `J^e = ∅`) and
`win` (environment liveness included, i.e. the real `J^e`). Using a single
trivial `{true}` justice assumption to model "no justice constraints"
(rather than an empty set) matches Slugs' own existing convention for
"no `[ENV_LIVENESS]` section given" and is semantically exact here too: in
the standard GR(1) fixed point, the environment-justice loop contributes a
disjunct `¬J_i^e ∧ cox(X)` per justice requirement, and `¬true = false`
makes that disjunct vacuous — identical to having no justice requirements
at all.

`reachStates(θ^e, ρ^e)` (with system choices unconstrained) is a small,
separate forward-reachability fixed point:

```cpp
BF computeEnvironmentReachableStates() {
    BFFixedPoint muReach(initEnv);
    for (; !muReach.isFixedPointReached(); ) {
        BF nextStates = (muReach.getValue() & safetyEnv)
                             .ExistAbstract(varCubePre)
                             .SwapVariables(varVectorPre, varVectorPost);
        muReach.update(muReach.getValue() | nextStates);
    }
    return muReach.getValue();
}
```

Because `[ENV_TRANS]` (`safetyEnv`) can only mention current inputs, current
outputs, and next inputs — never next outputs, per Slugs' own parser
restriction — abstracting away all current-state variables before swapping
post-labels to pre-labels leaves the successor's output dimension entirely
unconstrained, exactly modeling "system choices unconstrained" without any
special-casing.

`sysWinAllIni` (Algorithm 1's `sysWinAllIni(win, θ^e)`) mirrors the shape of
Slugs' own `checkRealizability()` but omits `initSys`, matching the paper's
`θ^s = true` reduction:

```cpp
bool allEnvironmentInitialInputsCanBeWon(BF winningStates) {
    return initEnv.Implies(winningStates.ExistAbstract(varCubePreOutput))
                  .UnivAbstract(varCubePreInput)
                  .isTrue();
}
```

`execute()` then follows Algorithm 1's control flow statement-by-statement,
including the two short-circuits described in §1.3 (the `E-ini` early
return, and skipping the `E-just` computation entirely once `(P-all,
E-safe)` is already found) and the asymmetric `res = ∅` gate that produces
the incomparable `{(P-reach, E-safe), (P-all, E-just)}` combination when it
occurs.

### 2.3 JSON output and diagnostics

The mode emits a single JSON object (to stdout, or to a file with
`--jsonOutput`) rather than the usual Slugs strategy formats, since its
output is a diagnosis, not a controller. Beyond `status` (`WELL_SEPARATED`
/ `NON_WELL_SEPARATED` / `ANALYSIS_INCOMPLETE`) and the reported `cases`,
the JSON includes:

- **`responsible_assumptions`** — named assumptions (using `##` comments
  when present, falling back to a generated name) from the responsible
  category (`env_init`/`env_safety`/`env_liveness`) for the reported cases.
  This is explicitly a category-level report, not a minimized core
  (`responsible_assumption_mode: category_assumptions_not_minimal_core`).
- **`witness`** — a representative reachable state (a single BDD cube,
  determinized to concrete variable values), preferring a direct one-step
  safety-violation witness (`∀ nextInput. ¬safetyEnv`, i.e. a state from
  which *every* legal next-input choice breaks the transition relation)
  over the broader safety-winning region when one is available, and
  including which named assumptions the witness cube actually intersects
  (`implicated_assumptions`).
- **`checks`** and **`timing`** — per-phase booleans, BDD sizes, and elapsed
  seconds for reachability, safety-winning, liveness-winning, diagnostics,
  and (if requested) core computation, useful for profiling and for the
  Python pipeline wrapper to make timeout decisions.

### 2.4 Core minimization (`--minimizeWellSeparationCore`)

Definition 3's core is computed as an opt-in flag (disabled by default,
since it multiplies solver calls) using **delta debugging (DDMin)** over the
set of named environment assumptions (`env_init`, `env_safety`, and
`env_liveness` entries, excluding the synthetic always-true liveness
assumption Slugs inserts when no `[ENV_LIVENESS]` section is present) —
matching the search strategy of both reference implementations described in
§1.6, rather than a plain linear greedy-deletion scan.

The oracle DDMin searches over is a direct implementation of Definition 3
with the reachability term pinned once, up front:

```cpp
BF originalReachableStates = computeEnvironmentReachableStates();  // fixed for the whole search

bool currentEnvironmentIsNonWellSeparated(BF originalReachableStates) {
    if (initEnv.isFalse()) return true;
    if (!(computeEnvironmentViolationWinningStates(false)
              & originalReachableStates).isFalse()) return true;
    return !(computeEnvironmentViolationWinningStates(true)
                 & originalReachableStates).isFalse();
}
```

Pinning `originalReachableStates` outside the search — rather than
recomputing `reachStates` from whatever reduced candidate is currently under
test — is what Definition 3 actually requires, and is what makes the DDMin
search correct in the first place: with the reachability term fixed,
"is this candidate subset non-well-separated" is monotonically increasing in
subset inclusion (Theorem 2), which is exactly the structure a
deletion/partition-based minimality search assumes. An earlier version of
this search recomputed reachability per-candidate instead; this was found,
independently of the paper text, to cause a real misattribution (a
constructed regression fixture,
`examples/well_separation/core_reach_pinned_to_original.slugsin`, in which
an unrelated "decoy" safety pair became falsely implicated as the core once
an unrelated guard assumption was greedily removed and inflated the
candidate's own reachable region) and was fixed by hoisting the
reachability computation before the search begins.

DDMin itself (`ddminSearch`) is algorithm-agnostic with respect to the
oracle: it operates purely on candidate *indices*, testing ever-finer
partitions of the current working set and their complements before falling
back to finer granularity, exactly as in Zeller's original presentation,
with no "unresolved" outcome needed since the oracle above is a
deterministic, always-terminating boolean predicate. Like a linear scan, it
only guarantees a **1-minimal** core (no single remaining assumption is
individually removable) — not a globally smallest one — and which
1-minimal core is found can depend on assumption order.

### 2.5 Known limitations (by design, not oversight)

These are also summarized in `README.md`:

- **Plain `.slugsin` semantics only.** The mode consumes Slugs' parsed
  representation directly; it does not separate auxiliary variables
  introduced by higher-level specification compilers (patterns, past-time
  LTL translations) from ordinary environment assumptions, unlike the
  extensions in §6.1 of the FSE16 paper or §6.4 of the ICSE24 paper. A
  pattern-introduced auxiliary variable would currently be analyzed as if it
  were an ordinary environment/system variable.
- **Definition 2 only, not Definition 4.** The mode ignores the system's
  own guarantees entirely, as Definition 2 (and the `⟨true, true,
  {false}⟩` reduction it requires) demands. "Well-separation with respect
  to guarantees" (§1.6, §6.2 of the FSE16 paper) is not implemented.
- **Diagnosis only, not synthesis.** The mode reports a state-cube witness
  and (optionally) a minimal responsible-assumption core; it does not
  extract a full strategy, counterstrategy, or trace, and it does not
  synthesize a controller of any kind — kind or otherwise. Downstream
  controller construction (including the ICSE24 "kind controller"
  extension) is out of scope for this mode.
- **Category-level, not minimal-by-default diagnostics.** The default
  `responsible_assumptions` field is a coarse, category-level report;
  minimization is only performed when `--minimizeWellSeparationCore` is
  explicitly requested, because it is significantly more expensive.

### 2.6 Traceability to the reference algorithm

The implementation is intended to stay close to the FSE16 construction:
Algorithm 1's pseudocode maps statement-by-statement onto `execute()`, and the
`sysWinSts` reduction reuses Slugs' existing winning-states fixed point rather
than introducing a second solver. The worked examples that motivate the
implementation are the paper's `dropCargo`/`clearCargo` safety conflict, the
`{findStat, samePos}` liveness trap, and both directions of Theorem 1's
non-monotonicity argument.
