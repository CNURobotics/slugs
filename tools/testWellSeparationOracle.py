#!/usr/bin/env python3
"""Explicit-state oracle checks for tiny well-separation fixtures.

This is a test helper, not a production analyzer. It parses the small Boolean
fixtures in examples/well_separation, evaluates GR(1) assumptions by exhaustive
enumeration, implements the Maoz/Ringert case diagnosis over explicit sets, and
compares the result with Slugs' --checkWellSeparation JSON.
"""

import json
import os
import subprocess
import sys
from itertools import product
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
SLUGS = Path(os.environ.get("SLUGS_BINARY", REPO_ROOT / "src" / "slugs"))
FIXTURE_DIR = REPO_ROOT / "examples" / "well_separation"

FIXTURES = [
    "well_separated_trivial.slugsin",
    "env_init_contradiction.slugsin",
    "env_safety_forced_initial.slugsin",
    "env_safety_forced_some_state.slugsin",
    "env_liveness_prevented_initial.slugsin",
    "env_liveness_prevented_reachable.slugsin",
    "forklift_cargo_conflict.slugsin",
    "forklift_findstat_samepos.slugsin",
    "forklift_findstat_only.slugsin",
    "forklift_findstat_samepos_station_invariant.slugsin",
    "no_env_liveness_section.slugsin",
    "no_sys_liveness_section.slugsin",
    "no_output_variables.slugsin",
    "no_input_variables.slugsin",
    "env_liveness_next_input.slugsin",
    "incomparable_reach_safe_all_just.slugsin",
    "core_reach_pinned_to_original.slugsin",
]


class Parser:
    def __init__(self, text):
        self.tokens = text.split()
        self.pos = 0

    def parse(self):
        if self.pos >= len(self.tokens):
            raise ValueError("empty formula")
        token = self.tokens[self.pos]
        self.pos += 1
        if token in {"&", "|", "^"}:
            left = self.parse()
            right = self.parse()
            return (token, left, right)
        if token == "!":
            return (token, self.parse())
        if token in {"0", "1"}:
            return token == "1"
        return ("var", token)


def parse_formula(text):
    parser = Parser(text)
    expr = parser.parse()
    if parser.pos != len(parser.tokens):
        raise ValueError(f"trailing tokens in formula {text!r}")
    return expr


def eval_formula(expr, valuation):
    if isinstance(expr, bool):
        return expr
    op = expr[0]
    if op == "var":
        return bool(valuation[expr[1]])
    if op == "!":
        return not eval_formula(expr[1], valuation)
    if op == "&":
        return eval_formula(expr[1], valuation) and eval_formula(expr[2], valuation)
    if op == "|":
        return eval_formula(expr[1], valuation) or eval_formula(expr[2], valuation)
    if op == "^":
        return eval_formula(expr[1], valuation) != eval_formula(expr[2], valuation)
    raise ValueError(f"unknown op {op!r}")


def read_spec(path):
    sections = {
        "INPUT": [],
        "OUTPUT": [],
        "ENV_INIT": [],
        "SYS_INIT": [],
        "ENV_TRANS": [],
        "SYS_TRANS": [],
        "ENV_LIVENESS": [],
        "SYS_LIVENESS": [],
    }
    current = None
    for raw_line in path.read_text().splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("[") and line.endswith("]"):
            current = line[1:-1]
            if current not in sections:
                raise ValueError(f"unknown section {current!r}")
            continue
        if current is None:
            raise ValueError(f"line outside section: {line!r}")
        sections[current].append(line)

    return {
        "inputs": sections["INPUT"],
        "outputs": sections["OUTPUT"],
        "env_init": [parse_formula(f) for f in sections["ENV_INIT"]] or [True],
        "env_trans": [parse_formula(f) for f in sections["ENV_TRANS"]] or [True],
        "env_liveness": [parse_formula(f) for f in sections["ENV_LIVENESS"]] or [True],
    }


def valuations(names):
    result = []
    for bits in product([False, True], repeat=len(names)):
        result.append(dict(zip(names, bits)))
    return result


def state_key(input_value, output_value, inputs, outputs):
    return tuple(input_value[name] for name in inputs) + tuple(output_value[name] for name in outputs)


def state_valuation(state, inputs, outputs):
    valuation = {}
    for i, name in enumerate(inputs):
        valuation[name] = state[i]
    offset = len(inputs)
    for i, name in enumerate(outputs):
        valuation[name] = state[offset + i]
    return valuation


def transition_valuation(state, next_input, next_output, inputs, outputs):
    valuation = state_valuation(state, inputs, outputs)
    for name in inputs:
        valuation[f"{name}'"] = next_input[name]
    for name in outputs:
        valuation[f"{name}'"] = next_output[name]
    return valuation


class ExplicitGame:
    def __init__(self, spec):
        self.inputs = spec["inputs"]
        self.outputs = spec["outputs"]
        self.input_vals = valuations(self.inputs)
        self.output_vals = valuations(self.outputs)
        self.states = [
            state_key(inp, out, self.inputs, self.outputs)
            for inp in self.input_vals
            for out in self.output_vals
        ]
        self.env_init = spec["env_init"]
        self.env_trans = spec["env_trans"]
        self.env_liveness = spec["env_liveness"]

    def eval_env_init(self, input_value):
        return all(eval_formula(formula, input_value) for formula in self.env_init)

    def eval_env_safety(self, state, next_input):
        valuation = state_valuation(state, self.inputs, self.outputs)
        for name in self.inputs:
            valuation[f"{name}'"] = next_input[name]
        return all(eval_formula(formula, valuation) for formula in self.env_trans)

    def eval_env_liveness(self, formula, state, next_input, next_output):
        valuation = transition_valuation(state, next_input, next_output, self.inputs, self.outputs)
        return eval_formula(formula, valuation)

    def next_state(self, next_input, next_output):
        return state_key(next_input, next_output, self.inputs, self.outputs)

    def env_init_inputs(self):
        return [inp for inp in self.input_vals if self.eval_env_init(inp)]

    def reachable_states(self):
        reachable = set()
        for inp in self.env_init_inputs():
            for out in self.output_vals:
                reachable.add(state_key(inp, out, self.inputs, self.outputs))
        changed = True
        while changed:
            changed = False
            new_reachable = set(reachable)
            for state in reachable:
                for next_input in self.input_vals:
                    if not self.eval_env_safety(state, next_input):
                        continue
                    for next_output in self.output_vals:
                        new_reachable.add(self.next_state(next_input, next_output))
            if new_reachable != reachable:
                reachable = new_reachable
                changed = True
        return reachable

    def cox(self, transition_predicate):
        winning = set()
        for state in self.states:
            ok = True
            for next_input in self.input_vals:
                if not self.eval_env_safety(state, next_input):
                    continue
                if not any(transition_predicate(state, next_input, next_output) for next_output in self.output_vals):
                    ok = False
                    break
            if ok:
                winning.add(state)
        return winning

    def compute_winning(self, env_liveness):
        all_states = set(self.states)
        z = set(all_states)
        while True:
            next_constraints = set(all_states)

            # There is one synthetic system guarantee: false.
            y = set()
            while True:
                def live_transition(_state, next_input, next_output):
                    return self.next_state(next_input, next_output) in y

                good = set(y)
                for assumption in env_liveness:
                    x = set(all_states)
                    while True:
                        def found_path(state, next_input, next_output, x_ref=x, assumption_ref=assumption):
                            next_in_x = self.next_state(next_input, next_output) in x_ref
                            assumption_holds = self.eval_env_liveness(
                                assumption_ref, state, next_input, next_output
                            )
                            return live_transition(state, next_input, next_output) or (
                                next_in_x and not assumption_holds
                            )

                        new_x = self.cox(found_path)
                        if new_x == x:
                            break
                        x = new_x
                    good |= x

                if good == y:
                    break
                y = good

            next_constraints &= y
            if next_constraints == z:
                break
            z = next_constraints

        return z

    def all_initial_inputs_can_be_won(self, winning_states):
        for input_value in self.env_init_inputs():
            exists_output = False
            for output_value in self.output_vals:
                if state_key(input_value, output_value, self.inputs, self.outputs) in winning_states:
                    exists_output = True
                    break
            if not exists_output:
                return False
        return True

    def diagnose(self):
        if not self.env_init_inputs():
            return "NON_WELL_SEPARATED", ["P-all/E-ini"]

        reachable = self.reachable_states()
        safety_winning = self.compute_winning([True])
        safety_reachable = bool(safety_winning & reachable)

        cases = []
        safety_all_initial = False
        if safety_reachable:
            safety_all_initial = self.all_initial_inputs_can_be_won(safety_winning)
            cases.append("P-all/E-safe" if safety_all_initial else "P-reach/E-safe")

        if not cases or not safety_all_initial:
            full_winning = self.compute_winning(self.env_liveness)
            if full_winning & reachable:
                liveness_all_initial = self.all_initial_inputs_can_be_won(full_winning)
                if liveness_all_initial:
                    cases.append("P-all/E-just")
                elif not cases:
                    cases.append("P-reach/E-just")

        if cases:
            return "NON_WELL_SEPARATED", cases
        return "WELL_SEPARATED", []


def run_slugs(path):
    proc = subprocess.run(
        [str(SLUGS), "--checkWellSeparation", str(path)],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if proc.returncode != 0:
        raise RuntimeError(f"slugs failed for {path.name}:\n{proc.stderr}")
    return json.loads(proc.stdout)


def main():
    failures = 0
    for filename in FIXTURES:
        path = FIXTURE_DIR / filename
        oracle_status, oracle_cases = ExplicitGame(read_spec(path)).diagnose()
        slugs_result = run_slugs(path)

        if slugs_result.get("status") != oracle_status:
            failures += 1
            print(
                f"{filename}: oracle status {oracle_status!r}, slugs {slugs_result.get('status')!r}",
                file=sys.stderr,
            )
        if slugs_result.get("cases") != oracle_cases:
            failures += 1
            print(
                f"{filename}: oracle cases {oracle_cases!r}, slugs {slugs_result.get('cases')!r}",
                file=sys.stderr,
            )

    if failures:
        print(f"{failures} oracle comparison(s) failed.", file=sys.stderr)
        return 1

    print(f"Passed {len(FIXTURES)} explicit-state oracle comparisons.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
