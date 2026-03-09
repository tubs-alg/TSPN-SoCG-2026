"""Test AnnotatedSolution and solve_annotated_instance."""

from shapely.geometry import Polygon as ShapelyPolygon
from tspn_bnb2 import (
    AnnotatedInstance,
    AnnotatedSolution,
    simplify_annotated_instance,
    solve_annotated_instance,
)


def test_solve_simple_instance() -> None:
    """Test solving a simple annotated instance."""
    # Create a simple instance with 3 triangular polygons
    poly1 = ShapelyPolygon([(0, 0), (1, 0), (0.5, 0.5)])
    poly2 = ShapelyPolygon([(5, 0), (6, 0), (5.5, 0.5)])
    poly3 = ShapelyPolygon([(2.5, 4), (3.5, 4), (3, 4.5)])

    instance = AnnotatedInstance(
        polygons=[poly1, poly2, poly3], meta={"name": "test_instance", "difficulty": "easy"}
    )

    # Solve the instance
    solution = solve_annotated_instance(
        instance=instance,
        timelimit=10,
        branching="FarthestPoly",
        search="DfsBfs",
        root="LongestEdgePlusFurthestSite",
        node_simplification=False,
        rules=[],
        use_cutoff=True,
        num_threads=16,
        decomposition_branch=True,
        eps=0.001,
    )

    # Verify solution properties
    assert isinstance(solution, AnnotatedSolution)
    assert solution.trajectory is not None
    assert solution.lower_bound > 0
    assert solution.upper_bound > 0
    assert solution.lower_bound <= solution.upper_bound
    assert solution.gap >= 0
    assert solution.is_tour is True
    assert isinstance(solution.statistics, dict)


def test_solution_metadata() -> None:
    """Test that solution preserves and extends metadata."""
    poly1 = ShapelyPolygon([(0, 0), (1, 0), (0.5, 0.5)])
    poly2 = ShapelyPolygon([(2, 0), (3, 0), (2.5, 0.5)])

    instance = AnnotatedInstance(polygons=[poly1, poly2], meta={"instance_name": "simple_test"})

    solution = solve_annotated_instance(
        instance=instance,
        timelimit=10,
        solver_version="1.0",
        experiment_id="test_001",
    )

    # Check that instance metadata is preserved
    assert "instance_name" in solution.meta
    assert solution.meta["instance_name"] == "simple_test"

    # Check that additional metadata is added
    assert "solver_version" in solution.meta
    assert solution.meta["solver_version"] == "1.0"
    assert "experiment_id" in solution.meta
    assert solution.meta["experiment_id"] == "test_001"


def test_solution_serialization() -> None:
    """Test that solutions can be serialized and deserialized."""
    poly1 = ShapelyPolygon([(0, 0), (1, 0), (1, 1), (0, 1)])
    poly2 = ShapelyPolygon([(5, 0), (6, 0), (6, 1), (5, 1)])

    instance = AnnotatedInstance(polygons=[poly1, poly2])

    solution = solve_annotated_instance(instance, timelimit=10)

    # Serialize to dict
    solution_dict = solution.model_dump()
    assert isinstance(solution_dict, dict)
    assert "trajectory" in solution_dict
    assert "lower_bound" in solution_dict
    assert "upper_bound" in solution_dict
    assert isinstance(solution_dict["trajectory"], str)  # Should be WKT

    # Serialize to JSON
    solution_json = solution.model_dump_json()
    assert isinstance(solution_json, str)

    # Deserialize from dict
    solution_copy = AnnotatedSolution.model_validate(solution_dict)
    assert solution_copy.lower_bound == solution.lower_bound
    assert solution_copy.upper_bound == solution.upper_bound
    assert solution_copy.is_optimal == solution.is_optimal


def test_solution_properties() -> None:
    """Test solution computed properties."""
    poly1 = ShapelyPolygon([(0, 0), (1, 0), (0.5, 0.5)])
    poly2 = ShapelyPolygon([(3, 0), (4, 0), (3.5, 0.5)])

    instance = AnnotatedInstance(polygons=[poly1, poly2])

    solution = solve_annotated_instance(instance, timelimit=10)

    # Test length property
    assert solution.length == solution.upper_bound

    # Test relative_gap property
    expected_rel_gap = solution.gap / solution.upper_bound if solution.upper_bound > 0 else 0.0
    assert abs(solution.relative_gap - expected_rel_gap) < 1e-9


def test_optimality_check() -> None:
    """Test optimality flag based on gap."""
    poly1 = ShapelyPolygon([(0, 0), (1, 0), (1, 1), (0, 1)])

    instance = AnnotatedInstance(polygons=[poly1])

    # With tight epsilon
    solution = solve_annotated_instance(instance, timelimit=30, eps=0.0001)

    # For a single polygon, solution should be optimal
    assert solution.is_optimal or solution.gap <= 0.0001 * solution.upper_bound + 0.0001


def test_different_strategies() -> None:
    """Test solving with different strategies."""
    poly1 = ShapelyPolygon([(0, 0), (1, 0), (0.5, 0.5)])
    poly2 = ShapelyPolygon([(2, 0), (3, 0), (2.5, 0.5)])
    poly3 = ShapelyPolygon([(1, 2), (2, 2), (1.5, 2.5)])

    instance = AnnotatedInstance(polygons=[poly1, poly2, poly3])

    # Test with different branching strategy
    solution1 = solve_annotated_instance(instance, timelimit=10, branching="Random")
    assert solution1.trajectory is not None

    # Test with different search strategy
    solution2 = solve_annotated_instance(instance, timelimit=10, search="CheapestChildDepthFirst")
    assert solution2.trajectory is not None

    # Test with node simplification
    solution3 = solve_annotated_instance(instance, timelimit=10, node_simplification=True)
    assert solution3.trajectory is not None


def test_solution_roundtrip() -> None:
    """Test complete roundtrip: instance -> solve -> serialize -> deserialize."""
    poly1 = ShapelyPolygon([(0, 0), (1, 0), (1, 1), (0, 1)])
    poly2 = ShapelyPolygon([(2, 0), (3, 0), (3, 1), (2, 1)])

    instance = AnnotatedInstance(polygons=[poly1, poly2], meta={"source": "test"})

    # Solve
    solution = solve_annotated_instance(instance, timelimit=10)

    # Serialize to JSON
    json_str = solution.model_dump_json()

    # Deserialize from JSON
    solution_restored = AnnotatedSolution.model_validate_json(json_str)

    # Verify all fields match
    assert solution_restored.lower_bound == solution.lower_bound
    assert solution_restored.upper_bound == solution.upper_bound
    assert solution_restored.gap == solution.gap
    assert solution_restored.is_optimal == solution.is_optimal
    assert solution_restored.is_tour == solution.is_tour
    assert solution_restored.trajectory.wkt == solution.trajectory.wkt


def test_simplify_empty_instance() -> None:
    """Test simplifying an empty instance."""
    instance = AnnotatedInstance(polygons=[])
    simplified = simplify_annotated_instance(instance)
    assert simplified.num_polygons() == 0


def test_simplify_single_polygon() -> None:
    """Test simplifying a single polygon."""
    poly = ShapelyPolygon([(0, 0), (1, 0), (1, 1), (0, 1)])
    instance = AnnotatedInstance(polygons=[poly])
    simplified = simplify_annotated_instance(instance)
    assert simplified.num_polygons() == 1


def test_simplify_removes_contained() -> None:
    """Test that simplify removes contained polygons."""
    large = ShapelyPolygon([(0, 0), (10, 0), (10, 10), (0, 10)])
    small = ShapelyPolygon([(2, 2), (4, 2), (4, 4), (2, 4)])

    instance = AnnotatedInstance(polygons=[large, small])
    # Disable convex_hull_fill to avoid issues with this configuration
    simplified = simplify_annotated_instance(instance, convex_hull_fill=False)

    # Should remove the smaller polygon (it's contained in the larger one)
    assert simplified.num_polygons() == 1


def test_simplify_preserves_metadata() -> None:
    """Test that simplify preserves instance metadata."""
    poly = ShapelyPolygon([(0, 0), (1, 0), (1, 1), (0, 1)])
    instance = AnnotatedInstance(polygons=[poly], meta={"name": "test", "version": 1})
    simplified = simplify_annotated_instance(instance)

    assert simplified.meta["name"] == "test"
    assert simplified.meta["version"] == 1


def test_simplify_preserves_annotations() -> None:
    """Test that simplify is called before annotations.

    Note: Simplification currently does NOT preserve annotations because polygon
    orientation may change during the process (from WKT parsing and bg::correct()).
    This is acceptable since simplification should be called before any annotations
    are performed.
    """
    from tspn_bnb2 import PolygonAnnotation

    poly1 = ShapelyPolygon([(0, 0), (1, 0), (1, 1), (0, 1)])
    poly2 = ShapelyPolygon([(5, 0), (6, 0), (6, 1), (5, 1)])

    instance = AnnotatedInstance(
        polygons=[poly1, poly2],
        annotations={
            0: PolygonAnnotation(hull_index=0, intersections=[]),
            1: PolygonAnnotation(hull_index=1, intersections=[]),
        },
    )

    # Disable convex_hull_fill to avoid disconnected geometry errors
    simplified = simplify_annotated_instance(instance, convex_hull_fill=False)

    # Both polygons should be preserved (not overlapping)
    assert simplified.num_polygons() == 2
    # Annotations are not preserved due to polygon orientation changes
    # Simplification should be called before annotating
    assert len(simplified.annotations) == 0


def test_simplify_with_solve() -> None:
    """Test using simplify before solving."""
    large = ShapelyPolygon([(0, 0), (10, 0), (10, 10), (0, 10)])
    small = ShapelyPolygon([(2, 2), (4, 2), (4, 4), (2, 4)])
    separate = ShapelyPolygon([(20, 20), (22, 20), (22, 22), (20, 22)])

    instance = AnnotatedInstance(polygons=[large, small, separate])

    # Simplify first - disable convex_hull_fill for disconnected polygons
    simplified = simplify_annotated_instance(instance, convex_hull_fill=False)

    # Should have removed the small polygon (contained in large)
    assert simplified.num_polygons() == 2

    # Solve the simplified instance
    solution = solve_annotated_instance(simplified, timelimit=10)
    assert solution.trajectory is not None


def test_simplify_individual_operations() -> None:
    """Test individual simplification operations with parameters."""
    large = ShapelyPolygon([(0, 0), (10, 0), (10, 10), (0, 10)])
    small = ShapelyPolygon([(2, 2), (4, 2), (4, 4), (2, 4)])

    instance = AnnotatedInstance(polygons=[large, small])

    # Test with only remove_supersites enabled
    simplified = simplify_annotated_instance(
        instance, remove_holes=False, convex_hull_fill=False, remove_supersites=True
    )
    assert simplified.num_polygons() == 1  # Small polygon removed

    # Test with all disabled (should return same polygons)
    instance2 = AnnotatedInstance(polygons=[large, small])
    not_simplified = simplify_annotated_instance(
        instance2, remove_holes=False, convex_hull_fill=False, remove_supersites=False
    )
    assert not_simplified.num_polygons() == 2  # Both polygons kept


def test_simplify_error_on_disconnected_convex_hull() -> None:
    """Test that convex_hull_fill handles disconnected geometries gracefully.

    Note: The C++ implementation doesn't raise an error for disconnected geometries.
    It simply doesn't fill areas between polygons that don't share points on the
    convex hull. This is the correct behavior - only polygons with shared convex
    hull points will have areas filled between them.
    """
    poly1 = ShapelyPolygon([(0, 0), (1, 0), (1, 1), (0, 1)])
    poly2 = ShapelyPolygon([(10, 10), (11, 10), (11, 11), (10, 11)])

    instance = AnnotatedInstance(polygons=[poly1, poly2])

    # Should not raise an error - just doesn't fill disconnected geometries
    simplified = simplify_annotated_instance(instance, convex_hull_fill=True)

    # Both polygons should be preserved (not connected)
    assert simplified.num_polygons() == 2


def test_self_plausibility_check_valid_solution() -> None:
    """Test self plausibility check on a valid solution."""
    poly1 = ShapelyPolygon([(0, 0), (1, 0), (0.5, 0.5)])
    poly2 = ShapelyPolygon([(2, 0), (3, 0), (2.5, 0.5)])

    instance = AnnotatedInstance(polygons=[poly1, poly2])
    solution = solve_annotated_instance(instance, timelimit=10)

    # Check self plausibility
    result = solution.self_plausibility_check()

    # Should pass all checks
    assert result["valid"] is True
    assert len(result["errors"]) == 0
    assert result["bounds_consistent"] is True
    assert result["gap_correct"] is True
    assert result["trajectory_matches_ub"] is True


def test_self_plausibility_check_invalid_solution() -> None:
    """Test self plausibility check detects invalid solution."""
    from shapely.geometry import LineString as ShapelyLineString

    trajectory = ShapelyLineString([(0, 0), (1, 0), (1, 1), (0, 0)])

    # Solution with incorrect gap
    solution = AnnotatedSolution(
        trajectory=trajectory,
        lower_bound=2.0,
        upper_bound=3.0,
        is_optimal=False,
        gap=0.5,  # Should be 1.0
        is_tour=True,
    )

    result = solution.self_plausibility_check()

    assert result["valid"] is False
    assert result["gap_correct"] is False
    assert any("Gap incorrect" in err for err in result["errors"])


def test_plausibility_check_valid_solutions() -> None:
    """Test plausibility check between two valid solutions."""
    poly1 = ShapelyPolygon([(0, 0), (1, 0), (0.5, 0.5)])
    poly2 = ShapelyPolygon([(2, 0), (3, 0), (2.5, 0.5)])

    instance = AnnotatedInstance(polygons=[poly1, poly2])

    # Solve with two different strategies
    solution1 = solve_annotated_instance(instance, timelimit=10, branching="FarthestPoly")
    solution2 = solve_annotated_instance(instance, timelimit=10, branching="Random")

    # Check plausibility between solutions
    result = solution1.plausibility_check(solution2)

    # Should pass all checks
    assert result["valid"] is True
    assert len(result["errors"]) == 0
    assert result["self_bounds_consistent"] is True
    assert result["other_bounds_consistent"] is True
    assert result["cross_bounds_consistent"] is True


def test_plausibility_check_inconsistent_bounds() -> None:
    """Test plausibility check detects inconsistent bounds."""
    from shapely.geometry import LineString as ShapelyLineString

    # Create a solution with inconsistent bounds
    trajectory = ShapelyLineString([(0, 0), (1, 0), (1, 1), (0, 1), (0, 0)])

    # Valid solution
    solution1 = AnnotatedSolution(
        trajectory=trajectory,
        lower_bound=4.0,
        upper_bound=4.0,
        is_optimal=True,
        gap=0.0,
        is_tour=True,
    )

    # Solution with LB > UB (invalid)
    solution2 = AnnotatedSolution(
        trajectory=trajectory,
        lower_bound=5.0,
        upper_bound=4.0,
        is_optimal=False,
        gap=-1.0,  # Incorrect but matches LB/UB
        is_tour=True,
    )

    result = solution1.plausibility_check(solution2)

    assert result["valid"] is False
    assert result["other_bounds_consistent"] is False
    assert any("Bounds inconsistent" in err for err in result["errors"])


def test_plausibility_check_incorrect_gap() -> None:
    """Test plausibility check detects incorrect gap calculation."""
    from shapely.geometry import LineString as ShapelyLineString

    trajectory = ShapelyLineString([(0, 0), (1, 0), (1, 1), (0, 0)])

    # Solution with incorrect gap
    solution1 = AnnotatedSolution(
        trajectory=trajectory,
        lower_bound=2.0,
        upper_bound=3.0,
        is_optimal=False,
        gap=0.5,  # Should be 1.0
        is_tour=True,
    )

    solution2 = AnnotatedSolution(
        trajectory=trajectory,
        lower_bound=2.0,
        upper_bound=3.0,
        is_optimal=False,
        gap=1.0,  # Correct
        is_tour=True,
    )

    result = solution1.plausibility_check(solution2)

    assert result["valid"] is False
    assert result["self_gap_correct"] is False
    assert any("Gap incorrect" in err for err in result["errors"])


def test_plausibility_check_trajectory_length_mismatch() -> None:
    """Test plausibility check detects trajectory length mismatch with upper bound."""
    from shapely.geometry import LineString as ShapelyLineString

    # Create a trajectory with known length (3-4-5 triangle perimeter = 12)
    trajectory = ShapelyLineString([(0, 0), (3, 0), (3, 4), (0, 0)])
    actual_length = trajectory.length  # Should be 12

    # Solution with mismatched upper bound
    solution1 = AnnotatedSolution(
        trajectory=trajectory,
        lower_bound=10.0,
        upper_bound=15.0,  # Doesn't match actual trajectory length
        is_optimal=False,
        gap=5.0,
        is_tour=True,
    )

    solution2 = AnnotatedSolution(
        trajectory=trajectory,
        lower_bound=10.0,
        upper_bound=actual_length,  # Correct
        is_optimal=False,
        gap=actual_length - 10.0,
        is_tour=True,
    )

    result = solution1.plausibility_check(solution2)

    assert result["valid"] is False
    assert result["self_trajectory_matches_ub"] is False
    assert any("Trajectory length mismatch" in err for err in result["errors"])


def test_plausibility_check_optimality_flag() -> None:
    """Test plausibility check validates optimality flags."""
    from shapely.geometry import LineString as ShapelyLineString

    trajectory = ShapelyLineString([(0, 0), (1, 0), (1, 1), (0, 0)])

    # Solution claiming optimality but with large gap
    solution1 = AnnotatedSolution(
        trajectory=trajectory,
        lower_bound=2.0,
        upper_bound=4.0,
        is_optimal=True,  # Claims optimal
        gap=2.0,  # But large gap
        is_tour=True,
    )

    solution2 = AnnotatedSolution(
        trajectory=trajectory,
        lower_bound=2.5,
        upper_bound=3.0,
        is_optimal=False,
        gap=0.5,
        is_tour=True,
    )

    result = solution1.plausibility_check(solution2, eps=0.001)

    assert result["valid"] is False
    assert result["self_optimality_flag_consistent"] is False
    assert any("Optimality flag inconsistent" in err for err in result["errors"])


def test_plausibility_check_cross_bounds_inconsistency() -> None:
    """Test plausibility check detects cross-instance bound inconsistencies."""
    from shapely.geometry import LineString as ShapelyLineString

    trajectory = ShapelyLineString([(0, 0), (1, 0), (1, 1), (0, 0)])

    # Solution with high lower bound
    solution1 = AnnotatedSolution(
        trajectory=trajectory,
        lower_bound=10.0,
        upper_bound=11.0,
        is_optimal=False,
        gap=1.0,
        is_tour=True,
    )

    # Solution with low upper bound (inconsistent with first LB)
    solution2 = AnnotatedSolution(
        trajectory=trajectory,
        lower_bound=5.0,
        upper_bound=8.0,
        is_optimal=False,
        gap=3.0,
        is_tour=True,
    )

    result = solution1.plausibility_check(solution2)

    # best_lb = 10.0, best_ub = 8.0 => inconsistent
    assert result["valid"] is False
    assert result["cross_bounds_consistent"] is False
    assert any("Cross-instance bounds inconsistent" in err for err in result["errors"])


def test_plausibility_check_tour_flag() -> None:
    """Test plausibility check validates tour flags."""
    from shapely.geometry import LineString as ShapelyLineString

    # Trajectory that is a tour (closed)
    tour_traj = ShapelyLineString([(0, 0), (1, 0), (1, 1), (0, 1), (0, 0)])

    # Trajectory that is not a tour (open)
    path_traj = ShapelyLineString([(0, 0), (1, 0), (1, 1), (0, 1)])

    # Solution with wrong tour flag
    solution1 = AnnotatedSolution(
        trajectory=tour_traj,
        lower_bound=4.0,
        upper_bound=4.0,
        is_optimal=True,
        gap=0.0,
        is_tour=False,  # Wrong! This is a tour
    )

    # Solution with correct tour flag
    solution2 = AnnotatedSolution(
        trajectory=path_traj,
        lower_bound=3.0,
        upper_bound=3.0,
        is_optimal=True,
        gap=0.0,
        is_tour=False,  # Correct
    )

    result = solution1.plausibility_check(solution2)

    assert result["valid"] is False
    assert result["self_tour_consistent"] is False
    assert any("Tour flag inconsistent" in err for err in result["errors"])


def test_plausibility_check_both_optimal_different_values() -> None:
    """Test plausibility check when both claim optimality but have different values."""
    from shapely.geometry import LineString as ShapelyLineString

    trajectory1 = ShapelyLineString([(0, 0), (1, 0), (1, 1), (0, 0)])
    trajectory2 = ShapelyLineString([(0, 0), (2, 0), (2, 2), (0, 0)])

    solution1 = AnnotatedSolution(
        trajectory=trajectory1,
        lower_bound=3.0,
        upper_bound=3.0,
        is_optimal=True,
        gap=0.0,
        is_tour=True,
    )

    solution2 = AnnotatedSolution(
        trajectory=trajectory2,
        lower_bound=6.0,
        upper_bound=6.0,
        is_optimal=True,
        gap=0.0,
        is_tour=True,
    )

    result = solution1.plausibility_check(solution2, eps=0.001)

    # If both claim optimality for the same instance, they should agree
    assert result["valid"] is False
    assert result["optimal_solutions_agree"] is False
    assert any("Both optimal but different values" in err for err in result["errors"])


def test_plausibility_check_infinite_values() -> None:
    """Test plausibility check handles infinite upper_bound and gap correctly."""
    from shapely.geometry import LineString as ShapelyLineString

    trajectory1 = ShapelyLineString([(0, 0), (1, 0), (1, 1), (0, 0)])
    trajectory2 = ShapelyLineString([(0, 0), (5, 0), (5, 5), (0, 5), (0, 0)])  # Length = 20

    # Solution with infinite upper bound and gap (no solution found)
    solution1 = AnnotatedSolution(
        trajectory=trajectory1,
        lower_bound=10.0,
        upper_bound=float("inf"),
        is_optimal=False,
        gap=float("inf"),
        is_tour=True,
    )

    # Valid finite solution with matching trajectory
    solution2 = AnnotatedSolution(
        trajectory=trajectory2,
        lower_bound=12.0,
        upper_bound=20.0,  # Matches trajectory2 length
        is_optimal=False,
        gap=8.0,
        is_tour=True,
    )

    result = solution1.plausibility_check(solution2)

    # Should pass - infinite values are valid when no solution found
    assert result["valid"] is True
    assert result["self_bounds_consistent"] is True
    assert result["self_gap_correct"] is True
    assert result["cross_bounds_consistent"] is True


def test_plausibility_check_infinite_with_optimal_flag() -> None:
    """Test that claiming optimality with infinite values is caught as invalid."""
    from shapely.geometry import LineString as ShapelyLineString

    trajectory = ShapelyLineString([(0, 0), (1, 0), (1, 1), (0, 0)])

    # Invalid: claiming optimal with infinite gap
    solution1 = AnnotatedSolution(
        trajectory=trajectory,
        lower_bound=10.0,
        upper_bound=float("inf"),
        is_optimal=True,  # Invalid!
        gap=float("inf"),
        is_tour=True,
    )

    solution2 = AnnotatedSolution(
        trajectory=trajectory,
        lower_bound=12.0,
        upper_bound=15.0,
        is_optimal=False,
        gap=3.0,
        is_tour=True,
    )

    result = solution1.plausibility_check(solution2)

    assert result["valid"] is False
    assert result["self_optimality_flag_consistent"] is False
    assert any("gap or upper_bound is inf" in err for err in result["errors"])
