from pathlib import Path

import pytest
from tspn_bnb2 import AnnotatedInstance
from tspn_bnb2.operations import simplify_annotated_instance, solve_annotated_instance
from tspn_bnb2.order_annotation import add_order_annotations


def _simple_instance():
    poly1 = "POLYGON((0 0, 2 0, 2 2, 0 2, 0 0))"
    poly2 = "POLYGON((4 0, 6 0, 6 2, 4 2, 4 0))"
    return AnnotatedInstance(polygons=[poly1, poly2])


def _grid_instance(n: int = 5, size: float = 1.0):
    polys = []
    for i in range(n):
        for j in range(n):
            x0, y0 = i * size, j * size
            x1, y1 = x0 + size, y0 + size
            polys.append(f"POLYGON(({x0} {y0}, {x1} {y0}, {x1} {y1}, {x0} {y1}, {x0} {y0}))")
    return AnnotatedInstance(polygons=polys)


def test_simplify_then_order_then_solve():
    instance = _simple_instance()

    simplified = simplify_annotated_instance(
        instance,
        remove_holes=True,
        convex_hull_fill=False,
        remove_supersites=False,
    )
    annotated = add_order_annotations(simplified)

    solution = solve_annotated_instance(
        annotated,
        timelimit=5,
        branching="FarthestPoly",
        search="DfsBfs",
        root="LongestPair",
        rules=["OrderFiltering"],
        num_threads=1,
        use_cutoff=True,
    )

    assert solution.upper_bound >= 0
    assert solution.lower_bound >= 0
    assert solution.trajectory.length == pytest.approx(solution.upper_bound)


def test_grid_instance_order_and_solve():
    """Run pipeline on a 4x4 unit grid."""
    instance = _grid_instance()
    simplified = simplify_annotated_instance(
        instance,
        remove_holes=True,
        convex_hull_fill=False,
        remove_supersites=False,
    )
    annotated = add_order_annotations(simplified)
    assert len(annotated.annotations) == 16
    assert all(ann.hull_index is not None for ann in annotated.annotations.values())
    solution = solve_annotated_instance(
        annotated,
        timelimit=5,
        branching="FarthestPoly",
        search="DfsBfs",
        root="LongestPair",
        rules=["OrderFiltering"],
        num_threads=1,
        use_cutoff=True,
    )

    assert solution.upper_bound >= 0
    assert solution.lower_bound >= 0


def test_grid_instance_orderroot():
    """Run pipeline on a 5x5 unit grid using order-based root."""
    instance = _grid_instance()
    annotated = add_order_annotations(instance)
    assert len(annotated.annotations) == 16
    solution = solve_annotated_instance(
        annotated,
        timelimit=5,
        branching="FarthestPoly",
        search="DfsBfs",
        root="OrderRoot",
        rules=["OrderFiltering"],
        num_threads=1,
        use_cutoff=True,
    )
    assert solution.upper_bound >= 0
    assert solution.lower_bound >= 0


def test_grid_instance_orderroot_large():
    """Run pipeline on a 7x7 unit grid using order-based root."""
    instance = _grid_instance(7)
    annotated = add_order_annotations(instance)
    assert len(annotated.annotations) == 24
    solution = solve_annotated_instance(
        annotated,
        timelimit=5,
        branching="FarthestPoly",
        search="DfsBfs",
        root="OrderRoot",
        rules=["OrderFiltering"],
        num_threads=24,
        use_cutoff=True,
    )
    assert solution.upper_bound >= 0
    assert solution.lower_bound >= 0


def test_ordered_stats():
    instance = _grid_instance(7)
    annotated = add_order_annotations(instance)
    solution = solve_annotated_instance(
        annotated,
        timelimit=5,
        branching="FarthestPoly",
        search="DfsBfs",
        root="LongestPair",
        rules=["OrderFiltering"],
        num_threads=24,
        use_cutoff=True,
    )

    assert "num_discarded_sequences" in solution.statistics
    assert solution.statistics["num_discarded_sequences"] > 0
    assert solution.upper_bound >= 0
    assert solution.lower_bound >= 0


def test_order_annotation_tessellation():
    with open(Path(__file__).parent / "instances" / "euro-night-00000050.json") as f:
        instance = AnnotatedInstance.model_validate_json(f.read())

    annotated = add_order_annotations(instance)

    ordering = [-1] * len(annotated.annotations)

    for key, value in annotated.annotations.items():
        ordering[value.hull_index] = key

    assert ordering == [4, 15, 16, 31, 33, 45, 42, 49, 46, 47, 36, 12, 8, 3, 0, 2, 5, 1]
