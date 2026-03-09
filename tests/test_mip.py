"""Test the MIP solver for TSPN."""

import pytest
from tspn_bnb2 import Instance, Point, Polygon
from tspn_bnb2.mip import (
    TSPNMIPSolver,
    TSPNNonConvexMIPSolver,
    solve_tspn_mip,
    solve_tspn_nonconvex_mip,
)
from tspn_bnb2.schemas import AnnotatedSolution


def test_three_polygon_mip() -> None:
    """Test MIP solver on three polygons forming a triangle."""
    instance = Instance(
        [
            Polygon([[Point(0, 0), Point(1, 0), Point(0.5, 0.5)]]),
            Polygon([[Point(5, 0), Point(6, 0), Point(5.5, 0.5)]]),
            Polygon([[Point(2.5, 4), Point(3.5, 4), Point(3, 4.5)]]),
        ],
        False,
    )
    result = solve_tspn_mip(instance, time_limit=60, verbose=False)

    assert isinstance(result, AnnotatedSolution)
    assert result.upper_bound > 0
    # Trajectory has 4 points (3 polygons + closing point)
    assert len(result.trajectory.coords) == 4
    assert result.is_tour


def test_square_grid_mip() -> None:
    """Test MIP solver on a 2x2 grid of square polygons."""
    instance = Instance(
        [
            Polygon(
                [
                    [
                        Point(x - 0.2, y - 0.2),
                        Point(x + 0.2, y - 0.2),
                        Point(x + 0.2, y + 0.2),
                        Point(x - 0.2, y + 0.2),
                    ]
                ]
            )
            for x in [0, 2]
            for y in [0, 2]
        ],
        False,
    )
    result = solve_tspn_mip(instance, time_limit=60, verbose=False)

    assert result.upper_bound > 0
    # Trajectory has 5 points (4 polygons + closing point)
    assert len(result.trajectory.coords) == 5
    assert result.is_tour


def test_visiting_points_inside_polygons() -> None:
    """Test that visiting points are inside their respective polygons."""
    from shapely.geometry import Polygon as ShapelyPolygon

    instance = Instance(
        [
            Polygon([[Point(0, 0), Point(2, 0), Point(2, 2), Point(0, 2)]]),
            Polygon([[Point(5, 0), Point(7, 0), Point(7, 2), Point(5, 2)]]),
            Polygon([[Point(2.5, 5), Point(4.5, 5), Point(4.5, 7), Point(2.5, 7)]]),
        ],
        False,
    )
    result = solve_tspn_mip(instance, time_limit=60, verbose=False)

    # The trajectory visits all polygons
    polygons = [
        ShapelyPolygon([(0, 0), (2, 0), (2, 2), (0, 2)]),
        ShapelyPolygon([(5, 0), (7, 0), (7, 2), (5, 2)]),
        ShapelyPolygon([(2.5, 5), (4.5, 5), (4.5, 7), (2.5, 7)]),
    ]
    # Check trajectory intersects each polygon
    for poly in polygons:
        assert result.trajectory.intersects(poly)


def test_tour_edges_form_hamiltonian_cycle() -> None:
    """Test that trajectory forms a valid closed tour."""
    instance = Instance(
        [Polygon([[Point(i, 0), Point(i + 0.5, 0), Point(i + 0.25, 0.5)]]) for i in range(5)],
        False,
    )
    result = solve_tspn_mip(instance, time_limit=3, verbose=False)

    # Verify trajectory is a closed tour (first == last point)
    coords = list(result.trajectory.coords)
    assert coords[0] == coords[-1], "Tour should be closed"
    # 5 polygons + 1 closing point = 6 coordinates
    assert len(coords) == 6


def test_mip_solver_class() -> None:
    """Test the TSPNMIPSolver class directly."""
    instance = Instance(
        [
            Polygon([[Point(0, 0), Point(1, 0), Point(0.5, 0.5)]]),
            Polygon([[Point(3, 0), Point(4, 0), Point(3.5, 0.5)]]),
            Polygon([[Point(1.5, 2), Point(2.5, 2), Point(2, 2.5)]]),
        ],
        False,
    )
    solver = TSPNMIPSolver(instance, time_limit=60, verbose=False)
    result = solver.solve()

    # OPTIMAL, TIME_LIMIT, or SOLUTION_LIMIT
    assert result.statistics["gurobi_status"] in [2, 9, 10]
    assert result.statistics["runtime"] >= 0
    assert result.gap >= 0


def test_mip_result_fields() -> None:
    """Test that all result fields are populated correctly."""
    from shapely.geometry import LineString

    instance = Instance(
        [
            Polygon([[Point(0, 0), Point(1, 0), Point(1, 1), Point(0, 1)]]),
            Polygon([[Point(3, 0), Point(4, 0), Point(4, 1), Point(3, 1)]]),
            Polygon([[Point(1.5, 3), Point(2.5, 3), Point(2.5, 4), Point(1.5, 4)]]),
        ],
        False,
    )
    result = solve_tspn_mip(instance, time_limit=60, verbose=False)

    # Check all fields exist and have reasonable values
    assert isinstance(result.trajectory, LineString)
    assert isinstance(result.upper_bound, float)
    assert isinstance(result.lower_bound, float)
    assert isinstance(result.gap, float)
    assert isinstance(result.is_optimal, bool)
    assert isinstance(result.is_tour, bool)
    assert isinstance(result.statistics, dict)
    assert isinstance(result.statistics["runtime"], float)
    assert isinstance(result.statistics["num_subtour_cuts"], int)
    assert isinstance(result.statistics["gurobi_status"], int)


def test_collinear_polygons() -> None:
    """Test MIP solver on collinear polygons (optimal tour is a line)."""
    instance = Instance(
        [
            Polygon([[Point(0, 0), Point(1, 0), Point(1, 1), Point(0, 1)]]),
            Polygon([[Point(3, 0), Point(4, 0), Point(4, 1), Point(3, 1)]]),
            Polygon([[Point(6, 0), Point(7, 0), Point(7, 1), Point(6, 1)]]),
        ],
        False,
    )
    result = solve_tspn_mip(instance, time_limit=60, verbose=False)

    # For collinear squares, optimal tour visits corners
    # The tour length should be approximately 2 * (distance between outer squares)
    assert result.upper_bound > 0
    # 3 polygons + closing point = 4 coordinates
    assert len(result.trajectory.coords) == 4


def test_mip_gap_parameter() -> None:
    """Test that MIP gap parameter is respected."""
    instance = Instance(
        [
            Polygon(
                [
                    [
                        Point(x - 0.2, y - 0.2),
                        Point(x + 0.2, y - 0.2),
                        Point(x + 0.2, y + 0.2),
                        Point(x - 0.2, y + 0.2),
                    ]
                ]
            )
            for x in range(3)
            for y in range(3)
        ],
        False,
    )
    # Solve with larger gap for faster termination
    result = solve_tspn_mip(instance, time_limit=120, mip_gap=0.05, verbose=False)

    assert result.upper_bound > 0
    # Gap should be at most what we requested (or optimal was found)
    mip_gap = result.statistics["mip_gap"]
    assert mip_gap <= 0.05 + 1e-6 or result.is_optimal


# --- Non-Convex MIP Solver Tests ---


def test_nonconvex_solver_on_convex_polygons() -> None:
    """Test that non-convex solver works correctly on convex polygons (trivial case)."""
    instance = Instance(
        [
            Polygon([[Point(0, 0), Point(1, 0), Point(0.5, 0.5)]]),
            Polygon([[Point(5, 0), Point(6, 0), Point(5.5, 0.5)]]),
            Polygon([[Point(2.5, 4), Point(3.5, 4), Point(3, 4.5)]]),
        ],
        False,
    )
    result = solve_tspn_nonconvex_mip(instance, time_limit=60, verbose=False)

    assert isinstance(result, AnnotatedSolution)
    assert result.upper_bound > 0
    assert len(result.trajectory.coords) == 4
    assert result.is_tour
    assert result.statistics["solver"] == "mip_nonconvex"


def test_nonconvex_l_shaped_polygon() -> None:
    """Test non-convex solver on L-shaped polygons."""
    # Create L-shaped polygons (non-convex)
    l_shape_1 = Polygon(
        [
            [
                Point(0, 0),
                Point(2, 0),
                Point(2, 1),
                Point(1, 1),
                Point(1, 2),
                Point(0, 2),
            ]
        ]
    )
    l_shape_2 = Polygon(
        [
            [
                Point(5, 0),
                Point(7, 0),
                Point(7, 1),
                Point(6, 1),
                Point(6, 2),
                Point(5, 2),
            ]
        ]
    )
    triangle = Polygon([[Point(3, 5), Point(4, 5), Point(3.5, 6)]])

    instance = Instance([l_shape_1, l_shape_2, triangle], False)
    result = solve_tspn_nonconvex_mip(instance, time_limit=60, verbose=False)

    assert result.upper_bound > 0
    assert len(result.trajectory.coords) == 4
    assert result.is_tour
    # Check that decomposition happened
    assert result.statistics["num_decomposition_pieces"] >= 3  # At least 3 pieces total


def test_nonconvex_star_polygon() -> None:
    """Test non-convex solver on a star-shaped polygon."""
    # Create a simple 4-pointed star (non-convex)
    star = Polygon(
        [
            [
                Point(1, 0),
                Point(1.2, 0.8),
                Point(2, 1),
                Point(1.2, 1.2),
                Point(1, 2),
                Point(0.8, 1.2),
                Point(0, 1),
                Point(0.8, 0.8),
            ]
        ]
    )
    square = Polygon([[Point(5, 0), Point(6, 0), Point(6, 1), Point(5, 1)]])
    triangle = Polygon([[Point(3, 4), Point(4, 4), Point(3.5, 5)]])

    instance = Instance([star, square, triangle], False)
    result = solve_tspn_nonconvex_mip(instance, time_limit=60, verbose=False)

    assert result.upper_bound > 0
    assert len(result.trajectory.coords) == 4
    assert result.is_tour


def test_nonconvex_selected_pieces_in_statistics() -> None:
    """Test that selected pieces are reported in statistics."""
    # L-shaped polygon
    l_shape = Polygon(
        [
            [
                Point(0, 0),
                Point(2, 0),
                Point(2, 1),
                Point(1, 1),
                Point(1, 2),
                Point(0, 2),
            ]
        ]
    )
    triangle = Polygon([[Point(5, 0), Point(6, 0), Point(5.5, 1)]])
    square = Polygon([[Point(2.5, 4), Point(3.5, 4), Point(3.5, 5), Point(2.5, 5)]])

    instance = Instance([l_shape, triangle, square], False)
    result = solve_tspn_nonconvex_mip(instance, time_limit=60, verbose=False)

    # Check that selected_pieces is in statistics
    assert "selected_pieces" in result.statistics
    selected = result.statistics["selected_pieces"]
    # Each polygon should have exactly one piece selected
    assert len(selected) == 3
    for i in range(3):
        assert i in selected


def test_nonconvex_solver_class_directly() -> None:
    """Test the TSPNNonConvexMIPSolver class directly."""
    l_shape = Polygon(
        [
            [
                Point(0, 0),
                Point(2, 0),
                Point(2, 1),
                Point(1, 1),
                Point(1, 2),
                Point(0, 2),
            ]
        ]
    )
    triangle = Polygon([[Point(5, 0), Point(6, 0), Point(5.5, 1)]])
    square = Polygon([[Point(2.5, 3), Point(3.5, 3), Point(3.5, 4), Point(2.5, 4)]])

    # Need at least 3 polygons for valid MIP (degree constraints require 2 edges per node)
    instance = Instance([l_shape, triangle, square], False)
    solver = TSPNNonConvexMIPSolver(instance, time_limit=60, verbose=False)

    # Check pieces were extracted correctly
    assert len(solver.pieces) == 3
    # L-shape should be decomposed into multiple pieces
    assert len(solver.pieces[0]) >= 2
    # Triangle is convex, should be 1 piece
    assert len(solver.pieces[1]) == 1
    # Square is convex, should be 1 piece
    assert len(solver.pieces[2]) == 1

    result = solver.solve()
    assert result.statistics["gurobi_status"] in [2, 9, 10]


def test_nonconvex_matches_convex_on_convex_input() -> None:
    """Test that non-convex solver gives same result as convex solver on convex input."""
    instance = Instance(
        [
            Polygon([[Point(0, 0), Point(1, 0), Point(1, 1), Point(0, 1)]]),
            Polygon([[Point(3, 0), Point(4, 0), Point(4, 1), Point(3, 1)]]),
            Polygon([[Point(1.5, 3), Point(2.5, 3), Point(2.5, 4), Point(1.5, 4)]]),
        ],
        False,
    )

    result_convex = solve_tspn_mip(instance, time_limit=60, verbose=False)
    result_nonconvex = solve_tspn_nonconvex_mip(instance, time_limit=60, verbose=False)

    # Both should find optimal (or near-optimal) solutions
    # Allow small tolerance for numerical differences
    assert abs(result_convex.upper_bound - result_nonconvex.upper_bound) < 1e-4


def test_halfspace_orientation_ccw() -> None:
    """Test that polygon_to_halfspaces works correctly for CCW vertices."""
    from tspn_bnb2.mip.utils import _is_ccw, polygon_to_halfspaces

    # CCW square: (0,0) -> (1,0) -> (1,1) -> (0,1) going counter-clockwise
    ccw_vertices = [(0, 0), (1, 0), (1, 1), (0, 1)]
    assert _is_ccw(ccw_vertices), "Vertices should be detected as CCW"

    A, b = polygon_to_halfspaces(ccw_vertices)

    # Interior point should satisfy all constraints
    interior = (0.5, 0.5)
    for i in range(len(b)):
        val = A[i, 0] * interior[0] + A[i, 1] * interior[1]
        assert val <= b[i] + 1e-9, f"Interior point should satisfy constraint {i}"

    # Exterior point should violate at least one constraint
    exterior = (2, 2)
    violations = sum(
        1 for i in range(len(b)) if A[i, 0] * exterior[0] + A[i, 1] * exterior[1] > b[i] + 1e-9
    )
    assert violations > 0, "Exterior point should violate at least one constraint"


def test_halfspace_orientation_cw() -> None:
    """Test that polygon_to_halfspaces works correctly for CW vertices."""
    from tspn_bnb2.mip.utils import _is_ccw, polygon_to_halfspaces

    # CW square: (0,0) -> (0,1) -> (1,1) -> (1,0) going clockwise
    cw_vertices = [(0, 0), (0, 1), (1, 1), (1, 0)]
    assert not _is_ccw(cw_vertices), "Vertices should be detected as CW"

    A, b = polygon_to_halfspaces(cw_vertices)

    # Interior point should satisfy all constraints
    interior = (0.5, 0.5)
    for i in range(len(b)):
        val = A[i, 0] * interior[0] + A[i, 1] * interior[1]
        assert val <= b[i] + 1e-9, f"Interior point should satisfy constraint {i}"

    # Exterior point should violate at least one constraint
    exterior = (2, 2)
    violations = sum(
        1 for i in range(len(b)) if A[i, 0] * exterior[0] + A[i, 1] * exterior[1] > b[i] + 1e-9
    )
    assert violations > 0, "Exterior point should violate at least one constraint"


def test_halfspace_duplicate_vertices() -> None:
    """Test that polygon_to_halfspaces removes duplicate vertices."""
    from tspn_bnb2.mip.utils import polygon_to_halfspaces

    # Square with duplicate vertex
    vertices_with_dup = [(0, 0), (1, 0), (1, 0), (1, 1), (0, 1)]

    A, b = polygon_to_halfspaces(vertices_with_dup)

    # Should have 4 constraints (duplicates removed)
    assert A.shape[0] == 4, f"Expected 4 constraints after removing dup, got {A.shape[0]}"

    # Interior point should satisfy all constraints
    interior = (0.5, 0.5)
    for i in range(len(b)):
        val = A[i, 0] * interior[0] + A[i, 1] * interior[1]
        assert val <= b[i] + 1e-9, f"Interior point should satisfy constraint {i}"


def test_nonconvex_mip_lagos_n005_seed3149_feasibility() -> None:
    """Test that MIP solution for lagos_n005_seed3149 is feasible.

    This test reproduces an issue where the non-convex MIP formulation
    produces a solution where one of the visiting points does not
    actually hit its polygon.
    """
    from pathlib import Path

    from shapely.geometry import Point as ShapelyPoint
    from tspn_bnb2.operations import convert_to_tspn_instance
    from tspn_bnb2.schemas import AnnotatedInstance

    # Load the specific instance from tests/fixtures
    instance_path = Path(__file__).parent / "instances" / "lagos_n005_seed3149.json"
    if not instance_path.exists():
        pytest.skip(f"Instance file not found at {instance_path}")

    instance = AnnotatedInstance.model_validate_json(instance_path.read_text())
    tspn_instance = convert_to_tspn_instance(instance)

    # Solve with non-convex MIP
    result = solve_tspn_nonconvex_mip(tspn_instance, time_limit=60, verbose=False, mip_gap=0.01)

    # Check feasibility and identify which polygons are missed
    eps = 0.01
    missed_polygons = []
    coords = list(result.trajectory.coords)
    for i, polygon in enumerate(instance.polygons):
        buffered = polygon.buffer(eps)
        if not result.trajectory.intersects(buffered):
            # Find distance from each trajectory point to this polygon
            min_dist = min(ShapelyPoint(x, y).distance(polygon) for x, y in coords)
            missed_polygons.append((i, min_dist))

    assert len(missed_polygons) == 0, (
        f"MIP solution is infeasible: missed polygons {missed_polygons} "
        f"(index, min_distance_from_trajectory)"
    )
