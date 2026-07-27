#!/usr/bin/env python3
"""Smoke tests for the experimental Slugs well-separation mode.

This script intentionally does not build or install Slugs. Point SLUGS_BINARY at
an already-built binary, or leave it unset to use src/slugs from the checkout.
"""

import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

from testWellSeparationOracle import ExplicitGame, read_spec, state_key


REPO_ROOT = Path(__file__).resolve().parents[1]
SLUGS = Path(os.environ.get("SLUGS_BINARY", REPO_ROOT / "src" / "slugs"))
FIXTURE_DIR = REPO_ROOT / "examples" / "well_separation"

CASES = [
    (
        "well_separated_trivial.slugsin",
        {
            "status": "WELL_SEPARATED",
            "complete": True,
            "violated_assumption_type": "none",
            "witness_available": False,
            "witness_unavailable_reason": "well_separated",
            "_check_env_liveness_checked": True,
            "core_mode": "disabled",
            "core_complete": False,
            "core_enabled": False,
        },
    ),
    (
        "env_init_contradiction.slugsin",
        {
            "status": "NON_WELL_SEPARATED",
            "complete": True,
            "violated_assumption_type": "env_init",
            "cases": ["P-all/E-ini"],
            "witness_available": False,
            "witness_unavailable_reason": "unsat_env_init",
            "_check_env_liveness_checked": False,
            "core_mode": "disabled",
            "core_complete": False,
            "core_enabled": False,
        },
    ),
    (
        "env_safety_forced_initial.slugsin",
        {
            "status": "NON_WELL_SEPARATED",
            "complete": True,
            "violated_assumption_type": "env_safety",
            "cases": ["P-all/E-safe"],
            "witness_available": True,
            "responsible_assumption_mode": "category_assumptions_not_minimal_core",
            "_check_env_liveness_checked": False,
            "core_mode": "disabled",
            "core_complete": False,
            "core_enabled": False,
            "_witness_case": "P-all/E-safe",
            "_witness_source_region": "direct_safety_violation_reachable",
            "_witness_details_kind": "one_step_safety_violation",
            "_witness_all_next_inputs_violate_env_safety": True,
        },
    ),
    (
        "env_safety_forced_some_state.slugsin",
        {
            "status": "NON_WELL_SEPARATED",
            "complete": True,
            "violated_assumption_type": "env_safety",
            "cases": ["P-reach/E-safe"],
            "witness_available": True,
            "responsible_assumption_mode": "category_assumptions_not_minimal_core",
            "_check_env_liveness_checked": True,
            "_witness_case": "P-reach/E-safe",
            "_witness_source_region": "direct_safety_violation_reachable",
            "_witness_details_kind": "one_step_safety_violation",
            "_witness_all_next_inputs_violate_env_safety": True,
        },
    ),
    (
        "env_liveness_prevented_initial.slugsin",
        {
            "status": "NON_WELL_SEPARATED",
            "complete": True,
            "violated_assumption_type": "env_liveness",
            "cases": ["P-all/E-just"],
            "witness_available": True,
            "responsible_assumption_mode": "category_assumptions_not_minimal_core",
            "_check_env_liveness_checked": True,
            "_witness_case": "P-all/E-just",
            "_witness_source_region": "full_winning_reachable",
            "_witness_details_kind": "abstract_liveness_trap",
        },
    ),
    (
        "env_liveness_prevented_reachable.slugsin",
        {
            "status": "NON_WELL_SEPARATED",
            "complete": True,
            "violated_assumption_type": "env_liveness",
            "cases": ["P-reach/E-just"],
            "witness_available": True,
            "responsible_assumption_mode": "category_assumptions_not_minimal_core",
            "_check_env_liveness_checked": True,
            "_witness_case": "P-reach/E-just",
            "_witness_source_region": "full_winning_reachable",
            "_witness_details_kind": "abstract_liveness_trap",
        },
    ),
    (
        "forklift_cargo_conflict.slugsin",
        {
            "status": "NON_WELL_SEPARATED",
            "complete": True,
            "violated_assumption_type": "env_safety",
            "cases": ["P-all/E-safe"],
            "_check_env_liveness_checked": False,
            "_responsible_names": [
                "dropCargo: if lift is DROP, cargo is true in the next input valuation",
                "clearCargo: if motion is BWD, cargo is false in the next input valuation",
            ],
            "_responsible_lines": [17, 19],
            "_witness_outputs": {
                "mot_BWD": True,
                "lift_DROP": True,
            },
            "_witness_details_kind": "one_step_safety_violation",
            "_witness_all_next_inputs_violate_env_safety": True,
            "_witness_implicated_names": [
                "dropCargo: if lift is DROP, cargo is true in the next input valuation",
                "clearCargo: if motion is BWD, cargo is false in the next input valuation",
            ],
        },
    ),
    (
        "forklift_findstat_samepos.slugsin",
        {
            "status": "NON_WELL_SEPARATED",
            "complete": True,
            "violated_assumption_type": "env_liveness",
            "cases": ["P-reach/E-just"],
            "_responsible_names": ["findStat"],
            "_responsible_lines": [24],
            "_witness_inputs": {
                "atStation": False,
            },
            "_witness_outputs": {
                "mot_STOP": True,
            },
            "_witness_details_kind": "abstract_liveness_trap",
            "_witness_implicated_names": ["findStat"],
        },
    ),
    (
        "forklift_findstat_only.slugsin",
        {
            "status": "WELL_SEPARATED",
            "complete": True,
            "violated_assumption_type": "none",
            "cases": [],
            "witness_available": False,
        },
    ),
    (
        "forklift_findstat_samepos_station_invariant.slugsin",
        {
            "status": "WELL_SEPARATED",
            "complete": True,
            "violated_assumption_type": "none",
            "cases": [],
            "witness_available": False,
        },
    ),
    (
        "no_env_liveness_section.slugsin",
        {
            "status": "WELL_SEPARATED",
            "complete": True,
            "violated_assumption_type": "none",
            "cases": [],
            "witness_available": False,
        },
    ),
    (
        "no_sys_liveness_section.slugsin",
        {
            "status": "WELL_SEPARATED",
            "complete": True,
            "violated_assumption_type": "none",
            "cases": [],
            "witness_available": False,
        },
    ),
    (
        "no_output_variables.slugsin",
        {
            "status": "WELL_SEPARATED",
            "complete": True,
            "violated_assumption_type": "none",
            "cases": [],
            "witness_available": False,
        },
    ),
    (
        "no_input_variables.slugsin",
        {
            "status": "NON_WELL_SEPARATED",
            "complete": True,
            "violated_assumption_type": "env_safety",
            "cases": ["P-all/E-safe"],
            "witness_available": True,
            "_witness_outputs": {
                "sys": True,
            },
        },
    ),
    (
        "env_liveness_next_input.slugsin",
        {
            "status": "NON_WELL_SEPARATED",
            "complete": True,
            "violated_assumption_type": "env_liveness",
            "cases": ["P-all/E-just"],
            "witness_available": True,
            "_witness_inputs": {
                "p": False,
            },
            "_witness_outputs": {
                "hold": True,
            },
        },
    ),
    (
        "core_reach_pinned_to_original.slugsin",
        {
            "status": "NON_WELL_SEPARATED",
            "complete": True,
            "violated_assumption_type": "env_liveness",
            "cases": ["P-all/E-just"],
            "witness_available": True,
            "_responsible_names": ["visitP"],
            "_witness_case": "P-all/E-just",
            "_witness_source_region": "full_winning_reachable",
            "_witness_inputs": {
                "r": False,
                "p": False,
            },
            "_witness_outputs": {
                "hold": True,
            },
            "_witness_details_kind": "abstract_liveness_trap",
            "_witness_implicated_names": ["visitP"],
        },
    ),
    (
        "incomparable_reach_safe_all_just.slugsin",
        {
            "status": "NON_WELL_SEPARATED",
            "complete": True,
            "violated_assumption_type": "env_safety",
            "cases": ["P-reach/E-safe", "P-all/E-just"],
            "witness_available": True,
            "_witness_case": "P-reach/E-safe",
            "_witness_source_region": "direct_safety_violation_reachable",
            "_witness_inputs": {
                "p": True,
            },
            "_witness_outputs": {
                "break_env": True,
            },
            "_witness_details_kind": "one_step_safety_violation",
            "_witness_all_next_inputs_violate_env_safety": True,
            "_witness_implicated_names": [
                "breakQTrue: if p and break_env, q must become true",
                "breakQFalse: if p and break_env, q must become false",
            ],
        },
    ),
]


def validate_witness_region(filename, result):
    if not result.get("witness_available"):
        return []

    witness = result.get("witness") or {}
    source_region = witness.get("source_region")
    inputs = witness.get("inputs") or {}
    outputs = witness.get("outputs") or {}

    spec = read_spec(FIXTURE_DIR / filename)
    game = ExplicitGame(spec)
    try:
        state = state_key(inputs, outputs, game.inputs, game.outputs)
    except KeyError as exc:
        return [f"{filename}: witness is missing valuation for {exc.args[0]!r}"]

    reachable = game.reachable_states()
    if state not in reachable:
        return [f"{filename}: witness state is not environment-reachable"]

    if source_region == "direct_safety_violation_reachable":
        for next_input in game.input_vals:
            if game.eval_env_safety(state, next_input):
                return [
                    f"{filename}: direct safety witness allows next input {next_input!r}"
                ]
        return []

    if source_region == "safety_winning_reachable":
        if state not in game.compute_winning([True]):
            return [f"{filename}: witness state is not safety-winning"]
        return []

    if source_region == "full_winning_reachable":
        if state not in game.compute_winning(game.env_liveness):
            return [f"{filename}: witness state is not full-assumption winning"]
        return []

    return [f"{filename}: unknown witness source_region {source_region!r}"]


def main() -> int:
    failures = 0

    for filename, expected in CASES:
        spec = FIXTURE_DIR / filename
        proc = subprocess.run(
            [str(SLUGS), "--checkWellSeparation", str(spec)],
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if proc.returncode != 0:
            failures += 1
            print(f"{filename}: slugs exited with {proc.returncode}", file=sys.stderr)
            print(proc.stderr, file=sys.stderr)
            continue

        try:
            result = json.loads(proc.stdout)
        except json.JSONDecodeError as exc:
            failures += 1
            print(f"{filename}: invalid JSON: {exc}", file=sys.stderr)
            print(proc.stdout, file=sys.stderr)
            continue

        witness_errors = validate_witness_region(filename, result)
        if witness_errors:
            failures += len(witness_errors)
            for error in witness_errors:
                print(error, file=sys.stderr)

        if result.get("format_version") != "0.1":
            failures += 1
            print(
                f"{filename}: expected format_version='0.1', got {result.get('format_version')!r}",
                file=sys.stderr,
            )
        tool = result.get("tool") or {}
        expected_tool = {
            "name": "slugs",
            "mode": "checkWellSeparation",
            "backend": "slugs-native-bdd",
            "algorithm": "maoz-ringert-algorithm-1",
        }
        for key, expected_value in expected_tool.items():
            if tool.get(key) != expected_value:
                failures += 1
                print(
                    f"{filename}: expected tool.{key}={expected_value!r}, got {tool.get(key)!r}",
                    file=sys.stderr,
                )
        timing = result.get("timing") or {}
        for key in [
            "total",
            "reachability",
            "safety_winning",
            "direct_safety_witness_region",
            "liveness_winning",
            "diagnostics_and_witness",
            "core_minimization",
        ]:
            value = timing.get(key)
            if not isinstance(value, (int, float)) or value < 0:
                failures += 1
                print(
                    f"{filename}: expected nonnegative numeric timing.{key}, got {value!r}",
                    file=sys.stderr,
                )
        checks = result.get("checks") or {}
        for key in [
            "reachable_states_bdd_size",
            "safety_winning_states_bdd_size",
            "direct_safety_violation_states_bdd_size",
            "full_winning_states_bdd_size",
        ]:
            value = checks.get(key)
            if not isinstance(value, int) or value < 0:
                failures += 1
                print(
                    f"{filename}: expected nonnegative integer checks.{key}, got {value!r}",
                    file=sys.stderr,
                )

        for key, expected_value in expected.items():
            if key == "_responsible_names":
                responsible_names = [
                    item.get("name") for item in result.get("responsible_assumptions", [])
                ]
                missing = [
                    name for name in expected_value if name not in responsible_names
                ]
                if missing:
                    failures += 1
                    print(
                        f"{filename}: missing responsible assumption names {missing!r}; got {responsible_names!r}",
                        file=sys.stderr,
                    )
                continue
            if key == "_responsible_lines":
                responsible_lines = [
                    item.get("line") for item in result.get("responsible_assumptions", [])
                ]
                missing = [
                    line for line in expected_value if line not in responsible_lines
                ]
                if missing:
                    failures += 1
                    print(
                        f"{filename}: missing responsible assumption lines {missing!r}; got {responsible_lines!r}",
                        file=sys.stderr,
                    )
                for item in result.get("responsible_assumptions", []):
                    if item.get("index") != item.get("source_index"):
                        failures += 1
                        print(
                            f"{filename}: expected index/source_index to match in {item!r}",
                            file=sys.stderr,
                        )
                continue
            if key == "_witness_inputs":
                actual_inputs = (result.get("witness") or {}).get("inputs", {})
                for name, expected_bit in expected_value.items():
                    if actual_inputs.get(name) != expected_bit:
                        failures += 1
                        print(
                            f"{filename}: expected witness input {name}={expected_bit!r}, got {actual_inputs.get(name)!r}",
                            file=sys.stderr,
                        )
                continue
            if key == "_witness_outputs":
                actual_outputs = (result.get("witness") or {}).get("outputs", {})
                for name, expected_bit in expected_value.items():
                    if actual_outputs.get(name) != expected_bit:
                        failures += 1
                        print(
                            f"{filename}: expected witness output {name}={expected_bit!r}, got {actual_outputs.get(name)!r}",
                            file=sys.stderr,
                        )
                continue
            if key == "_witness_case":
                actual_case = (result.get("witness") or {}).get("case")
                if actual_case != expected_value:
                    failures += 1
                    print(
                        f"{filename}: expected witness case {expected_value!r}, got {actual_case!r}",
                        file=sys.stderr,
                    )
                continue
            if key == "_witness_source_region":
                actual_source_region = (result.get("witness") or {}).get("source_region")
                if actual_source_region != expected_value:
                    failures += 1
                    print(
                        f"{filename}: expected witness source_region {expected_value!r}, got {actual_source_region!r}",
                        file=sys.stderr,
                    )
                continue
            if key == "_witness_details_kind":
                details = (result.get("witness") or {}).get("details") or {}
                actual_kind = details.get("kind")
                if actual_kind != expected_value:
                    failures += 1
                    print(
                        f"{filename}: expected witness details.kind {expected_value!r}, got {actual_kind!r}",
                        file=sys.stderr,
                    )
                continue
            if key == "_witness_all_next_inputs_violate_env_safety":
                details = (result.get("witness") or {}).get("details") or {}
                actual_value = details.get("all_next_inputs_violate_env_safety")
                if actual_value != expected_value:
                    failures += 1
                    print(
                        f"{filename}: expected witness details.all_next_inputs_violate_env_safety {expected_value!r}, got {actual_value!r}",
                        file=sys.stderr,
                    )
                continue
            if key == "_check_env_liveness_checked":
                actual_value = checks.get("env_liveness_checked")
                if actual_value != expected_value:
                    failures += 1
                    print(
                        f"{filename}: expected checks.env_liveness_checked {expected_value!r}, got {actual_value!r}",
                        file=sys.stderr,
                    )
                continue
            if key == "_witness_implicated_names":
                implicated_names = [
                    item.get("name")
                    for item in (result.get("witness") or {}).get("implicated_assumptions", [])
                ]
                missing = [
                    name for name in expected_value if name not in implicated_names
                ]
                if missing:
                    failures += 1
                    print(
                        f"{filename}: missing witness implicated names {missing!r}; got {implicated_names!r}",
                        file=sys.stderr,
                    )
                continue
            actual = result.get(key)
            if actual != expected_value:
                failures += 1
                print(
                    f"{filename}: expected {key}={expected_value!r}, got {actual!r}",
                    file=sys.stderr,
                )

    with tempfile.TemporaryDirectory(prefix="slugs-wellsep-") as tmpdir:
        output_path = Path(tmpdir) / "result.json"
        spec = FIXTURE_DIR / "env_init_contradiction.slugsin"
        proc = subprocess.run(
            [str(SLUGS), "--checkWellSeparation", "--jsonOutput", str(spec), str(output_path)],
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if proc.returncode != 0:
            failures += 1
            print(f"jsonOutput: slugs exited with {proc.returncode}", file=sys.stderr)
            print(proc.stderr, file=sys.stderr)
        elif proc.stdout:
            failures += 1
            print("jsonOutput: expected empty stdout when writing a JSON file", file=sys.stderr)
            print(proc.stdout, file=sys.stderr)
        else:
            try:
                result = json.loads(output_path.read_text())
            except (OSError, json.JSONDecodeError) as exc:
                failures += 1
                print(f"jsonOutput: could not read valid JSON output: {exc}", file=sys.stderr)
            else:
                if result.get("status") != "NON_WELL_SEPARATED":
                    failures += 1
                    print("jsonOutput: unexpected status in output file", file=sys.stderr)
                if result.get("format_version") != "0.1":
                    failures += 1
                    print("jsonOutput: unexpected format_version in output file", file=sys.stderr)

    core_spec = FIXTURE_DIR / "forklift_cargo_conflict.slugsin"
    proc = subprocess.run(
        [str(SLUGS), "--checkWellSeparation", "--minimizeWellSeparationCore", str(core_spec)],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if proc.returncode != 0:
        failures += 1
        print(f"core: slugs exited with {proc.returncode}", file=sys.stderr)
        print(proc.stderr, file=sys.stderr)
    else:
        try:
            result = json.loads(proc.stdout)
        except json.JSONDecodeError as exc:
            failures += 1
            print(f"core: invalid JSON: {exc}", file=sys.stderr)
            print(proc.stdout, file=sys.stderr)
        else:
            expected_core_names = [
                "dropCargo: if lift is DROP, cargo is true in the next input valuation",
                "clearCargo: if motion is BWD, cargo is false in the next input valuation",
            ]
            core_names = [item.get("name") for item in result.get("core_assumptions", [])]
            if result.get("core_mode") != "delta_debugging_1_minimal_non_well_separated_subset":
                failures += 1
                print(
                    f"core: unexpected core_mode {result.get('core_mode')!r}",
                    file=sys.stderr,
                )
            if result.get("core_complete") is not True:
                failures += 1
                print("core: expected core_complete true", file=sys.stderr)
            if result.get("core_enabled") is not True:
                failures += 1
                print("core: expected core_enabled true", file=sys.stderr)
            if sorted(core_names) != sorted(expected_core_names):
                failures += 1
                print(
                    f"core: expected core assumptions {expected_core_names!r}, got {core_names!r}",
                    file=sys.stderr,
                )

    # Regression for the Maoz/Ringert Def. 3 requirement that a core is
    # evaluated against the *original* specification's reachable states, not
    # the reachable states of whatever reduced candidate the core search is
    # currently testing. `decoyTrue`/`decoyFalse` are only reachable if
    # `gate_r`/`gate_r_safety` are removed first; a core search that
    # recomputes reachability per-candidate would wrongly settle on the decoy
    # pair and miss the actual liveness trap (`holdFreezeFalse`/`visitP`).
    reach_pinned_spec = FIXTURE_DIR / "core_reach_pinned_to_original.slugsin"
    proc = subprocess.run(
        [str(SLUGS), "--checkWellSeparation", "--minimizeWellSeparationCore", str(reach_pinned_spec)],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if proc.returncode != 0:
        failures += 1
        print(f"core_reach_pinned: slugs exited with {proc.returncode}", file=sys.stderr)
        print(proc.stderr, file=sys.stderr)
    else:
        try:
            result = json.loads(proc.stdout)
        except json.JSONDecodeError as exc:
            failures += 1
            print(f"core_reach_pinned: invalid JSON: {exc}", file=sys.stderr)
            print(proc.stdout, file=sys.stderr)
        else:
            expected_core_names = [
                "holdFreezeFalse: holding at !p keeps p false",
                "visitP",
            ]
            core_names = [item.get("name") for item in result.get("core_assumptions", [])]
            decoy_names = {
                "decoyTrue: if r and trig, q must become true",
                "decoyFalse: if r and trig, q must become false",
            }
            if decoy_names & set(core_names):
                failures += 1
                print(
                    f"core_reach_pinned: core wrongly includes decoy assumptions reachable "
                    f"only under a reduced candidate: {core_names!r}",
                    file=sys.stderr,
                )
            if sorted(core_names) != sorted(expected_core_names):
                failures += 1
                print(
                    f"core_reach_pinned: expected core assumptions {expected_core_names!r}, got {core_names!r}",
                    file=sys.stderr,
                )

    if failures:
        print(f"{failures} well-separation smoke-test check(s) failed.", file=sys.stderr)
        return 1

    print(f"Passed {len(CASES)} well-separation smoke tests.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
