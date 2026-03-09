"""Test optimal solutions with different solver configurations.

This module loads pre-computed optimal solutions from tests/instances/optimal_solutions.json.xz
and verifies that the BnB solver finds optimal solutions across various strategy configurations.
"""

import json
import lzma
from pathlib import Path

import numpy as np

from tspn_bnb2.operations import solve_annotated_instance
from tspn_bnb2.schemas import AnnotatedInstance, AnnotatedSolution


def load_optimal_solutions() -> list[tuple[str, AnnotatedInstance, AnnotatedSolution]]:
    """Load pre-computed optimal solutions from the compressed JSON file.

    Returns:
        List of tuples containing (name, instance, expected_solution).

    """
    path = Path(__file__).parent / "instances" / "optimal_solutions.json.xz"
    with lzma.open(path, "rt", encoding="utf-8") as f:
        data = json.load(f)
    return [
        (
            item["name"],
            AnnotatedInstance.model_validate_json(item["instance"]),
            AnnotatedSolution.model_validate_json(item["optimal_solution"]),
        )
        for item in data
    ]


def _check_solution_correct(name: str, solution: AnnotatedSolution, instance: AnnotatedInstance, eps: float = 0.001):
    assert solution.is_tour, f"[{name}] Not a valid tour"

    assert solution.lower_bound <= solution.upper_bound + eps, (
        f"[{name}] LB > UB: {solution.lower_bound} > {solution.upper_bound}"
    )

    assert solution.check_feasibility(instance), (
        f"[{name}] Trajectory doesn't visit all polygons"
    )

    assert np.isclose(solution.trajectory.length, solution.upper_bound, rtol=eps), (
        f"[{name}] Trajectory length mismatch: "
        f"actual={solution.trajectory.length:.6f}, expected={solution.upper_bound:.6f}"
    )


def _verify_solution(
        name: str,
        instance: AnnotatedInstance,
        solution: AnnotatedSolution,
        expected: AnnotatedSolution,
        eps: float = 0.001,
) -> None:
    """Verify that a solution is valid and optimal.

    Args:
        name: Instance name for error messages.
        instance: The problem instance.
        solution: The AnnotatedSolution from solve_annotated_instance.
        expected: The expected optimal solution for comparison.
        eps: Optimality tolerance.

    Raises:
        AssertionError: If any verification check fails.

    """
    _check_solution_correct(name, solution, instance, eps)
    _check_solution_correct(name, expected, instance, eps)

    assert solution.alg_is_optimal(eps), (
        f"[{name}] Not optimal: UB/LB = {solution.alg_relative_gap:.6f} > {1 + eps}"
    )

    best_lb = max(solution.lower_bound, expected.lower_bound)
    best_ub = min(solution.upper_bound, expected.upper_bound)
    assert best_lb <= best_ub * (1+eps), ( # 1+eps due to numerical inaccuracies.
        f"[{name}] Cross-solution bounds inconsistent: "
        f"best_LB={best_lb:.6f} > best_UB={best_ub:.6f}"
    )

    # 6. Objectives should match (both are optimal for the same instance)
    assert np.isclose(solution.upper_bound, expected.upper_bound, rtol=eps), (
        f"[{name}] Objectives differ: "
        f"solution={solution.upper_bound:.6f}, expected={expected.upper_bound:.6f}"
    )


def test_default_configuration() -> None:
    """Test default: FarthestPoly + DfsBfs + LongestEdgePlusFurthestSite."""
    eps = 0.001
    instances = load_optimal_solutions()

    for name, instance, expected in instances:
        solution = solve_annotated_instance(
            instance,
            timelimit=120,
            branching="FarthestPoly",
            search="DfsBfs",
            root="LongestEdgePlusFurthestSite",
            node_simplification=False,
            rules=[],
            num_threads=16,
            decomposition_branch=True,
            eps=eps,
        )
        _verify_solution(name, instance, solution, expected, eps)


def test_order_root_configuration() -> None:
    """Test with OrderRoot strategy (useful for instances with hull annotations)."""
    eps = 0.001
    instances = load_optimal_solutions()

    for name, instance, expected in instances:
        solution = solve_annotated_instance(
            instance,
            timelimit=120,
            branching="FarthestPoly",
            search="DfsBfs",
            root="OrderRoot",
            node_simplification=False,
            rules=[],
            num_threads=16,
            decomposition_branch=True,
            eps=eps,
        )
        _verify_solution(name, instance, solution, expected, eps)


def test_cheapest_child_configuration() -> None:
    """Test with CheapestChildDepthFirst search strategy."""
    eps = 0.001
    instances = load_optimal_solutions()

    for name, instance, expected in instances:
        solution = solve_annotated_instance(
            instance,
            timelimit=120,
            branching="FarthestPoly",
            search="CheapestChildDepthFirst",
            root="OrderRoot",
            node_simplification=False,
            rules=[],
            num_threads=16,
            decomposition_branch=True,
            eps=eps,
        )
        _verify_solution(name, instance, solution, expected, eps)


def test_cheapest_breadth_first_configuration() -> None:
    """Test with CheapestBreadthFirst search strategy."""
    eps = 0.001
    instances = load_optimal_solutions()

    for name, instance, expected in instances:
        solution = solve_annotated_instance(
            instance,
            timelimit=120,
            branching="FarthestPoly",
            search="CheapestBreadthFirst",
            root="OrderRoot",
            node_simplification=False,
            rules=[],
            num_threads=16,
            decomposition_branch=True,
            eps=eps,
        )
        _verify_solution(name, instance, solution, expected, eps)


def test_node_simplification_configuration() -> None:
    """Test with node simplification enabled."""
    eps = 0.001
    instances = load_optimal_solutions()

    for name, instance, expected in instances:
        solution = solve_annotated_instance(
            instance,
            timelimit=120,
            branching="FarthestPoly",
            search="DfsBfs",
            root="LongestEdgePlusFurthestSite",
            node_simplification=True,
            MAX_TE_TOGGLE_COUNT=1,
            MAX_GEOM_TOGGLE_COUNT=1,
            rules=[],
            num_threads=16,
            decomposition_branch=True,
            eps=eps,
        )
        _verify_solution(name, instance, solution, expected, eps)


def test_order_filtering_configuration() -> None:
    """Test with OrderFiltering rule enabled."""
    eps = 0.001
    instances = load_optimal_solutions()

    for name, instance, expected in instances:
        solution = solve_annotated_instance(
            instance,
            timelimit=120,
            branching="FarthestPoly",
            search="DfsBfs",
            root="OrderRoot",
            node_simplification=True,
            MAX_TE_TOGGLE_COUNT=1,
            MAX_GEOM_TOGGLE_COUNT=1,
            rules=["OrderFiltering"],
            num_threads=16,
            decomposition_branch=True,
            eps=eps,
        )
        _verify_solution(name, instance, solution, expected, eps)


def test_longest_pair_configuration() -> None:
    """Test with LongestPair root strategy."""
    eps = 0.001
    instances = load_optimal_solutions()

    for name, instance, expected in instances:
        solution = solve_annotated_instance(
            instance,
            timelimit=120,
            branching="FarthestPoly",
            search="DfsBfs",
            root="LongestPair",
            node_simplification=False,
            rules=[],
            num_threads=16,
            decomposition_branch=True,
            eps=eps,
        )
        _verify_solution(name, instance, solution, expected, eps)
