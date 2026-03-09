"""Test TSPN-Path calc with given start and dest polys."""

import pytest
from tspn_bnb2 import Instance, Point, Polygon, branch_and_bound


def test_empty() -> None:
    """Test an empty instance (no polys)."""
    instance = Instance([Point(0, 0), Point(0, 0)], True)
    ub, lb, stats = branch_and_bound(
        instance=instance,
        callback=lambda _: None,
        initial_solution=None,
        timelimit=60,
        branching="FarthestPoly",
        search="DfsBfs",
        root="LongestTriple",
        node_simplification=False,
        rules=[],
        use_cutoff=True,
        num_threads=16,
        decomposition_branch=True,
        eps=0.001,
    )


def test_single_poly() -> None:
    """Test a path through one poly."""
    instance = Instance(
        [Point(0, 0), Point(0, 0), Polygon([[Point(0, 0), Point(1, 0), Point(0, 1)]])], True
    )
    ub, lb, stats = branch_and_bound(
        instance=instance,
        callback=lambda _: None,
        initial_solution=None,
        timelimit=60,
        branching="FarthestPoly",
        search="DfsBfs",
        root="LongestTriple",
        node_simplification=False,
        rules=[],
        use_cutoff=True,
        num_threads=16,
        decomposition_branch=True,
        eps=0.001,
    )


def test_two_polys() -> None:
    """Test a path through two Polys."""
    instance = Instance(
        [
            Point(0, 0),
            Point(0, 0),
            Polygon([[Point(4, 3), Point(5, 4), Point(3, 4)]]),
            Polygon([[Point(10, 0), Point(8, 0), Point(10, 1)]]),
        ],
        True,
    )
    # instance is 2x pythagorean triple (3,4,5)
    ub, lb, stats = branch_and_bound(
        instance=instance,
        callback=lambda _: None,
        initial_solution=None,
        timelimit=60,
        branching="FarthestPoly",
        search="DfsBfs",
        root="LongestTriple",
        node_simplification=False,
        rules=[],
        use_cutoff=True,
        num_threads=16,
        decomposition_branch=True,
        eps=0.001,
    )
    assert ub.get_trajectory().length() == pytest.approx(18)
    assert lb == pytest.approx(18)


def test_three_polys() -> None:
    """Test a path trough three polys."""
    instance = Instance(
        [
            Point(0, 0),
            Point(0, 0),
            Polygon([[Point(4, 3), Point(5, 4), Point(3, 4)]]),
            Polygon([[Point(4, -3), Point(5, -4), Point(3, -4)]]),
            Polygon([[Point(10, 0), Point(8, 0), Point(10, 1)]]),
        ],
        True,
    )
    # instance is 4x pythagorean triple (3,4,5)
    ub, lb, stats = branch_and_bound(
        instance=instance,
        callback=lambda _: None,
        initial_solution=None,
        timelimit=60,
        branching="FarthestPoly",
        search="DfsBfs",
        root="LongestTriple",
        node_simplification=False,
        rules=[],
        use_cutoff=True,
        num_threads=16,
        decomposition_branch=True,
        eps=0.001,
    )
    assert ub.get_trajectory().length() == pytest.approx(20)
    assert lb == pytest.approx(20)


def test_square() -> None:
    """Test a path through four polygons in a square."""
    instance = Instance(
        [
            Point(5, -3),
            Point(5, -3),
            Polygon([[Point(0, 0), Point(1, 0), Point(0, 1)]]),
            Polygon([[Point(10, 0), Point(9, 0), Point(10, 1)]]),
            Polygon([[Point(10, 6), Point(10, 5), Point(9, 6)]]),
            Polygon([[Point(0, 6), Point(1, 6), Point(0, 5)]]),
        ],
        True,
    )
    ub, lb, stats = branch_and_bound(
        instance=instance,
        callback=lambda _: None,
        initial_solution=None,
        timelimit=60,
        branching="FarthestPoly",
        search="DfsBfs",
        root="LongestTriple",
        node_simplification=False,
        rules=[],
        use_cutoff=True,
        num_threads=16,
        decomposition_branch=True,
        eps=0.001,
    )
    assert ub.get_trajectory().length() == pytest.approx(30)
    assert lb == pytest.approx(30)


def test_square_with_middle() -> None:
    """Test square with another poly in the middle (symmetry)."""
    instance = Instance(
        [
            Point(5, 2.4),
            Point(5, 2.4),
            Polygon([[Point(0, 0), Point(1, 0), Point(0, 1)]]),
            Polygon([[Point(10, 0), Point(9, 0), Point(10, 1)]]),
            Polygon([[Point(10, 10), Point(10, 9), Point(9, 10)]]),
            Polygon([[Point(4, 4), Point(6, 4), Point(6, 6), Point(4, 6)]]),
            Polygon([[Point(0, 10), Point(1, 10), Point(0, 9)]]),
        ],
        True,
    )

    ub, lb, stats = branch_and_bound(
        instance=instance,
        callback=lambda _: None,
        initial_solution=None,
        timelimit=60,
        branching="FarthestPoly",
        search="DfsBfs",
        root="LongestTriple",
        node_simplification=False,
        rules=[],
        use_cutoff=True,
        num_threads=16,
        decomposition_branch=True,
        eps=0.001,
    )
    assert ub.get_trajectory().length() == pytest.approx(38.046507)
    assert lb == pytest.approx(38.046507)


def test_4x4() -> None:
    """Test a 4x4 grid of polys."""
    instance = Instance(
        [
            Point(0, 0),
            Point(0, 0),
        ]
        + [
            Polygon(
                [
                    [
                        Point(x - 0.25, y - 0.25),
                        Point(x - 0.25, y + 0.25),
                        Point(x + 0.25, y + 0.25),
                        Point(x + 0.25, y - 0.25),
                    ]
                ]
            )
            for x in range(4)
            for y in range(4)
        ],
        True,
    )
    ub, lb, stats = branch_and_bound(
        instance=instance,
        callback=lambda _: None,
        initial_solution=None,
        timelimit=60,
        branching="FarthestPoly",
        search="DfsBfs",
        root="LongestTriple",
        node_simplification=False,
        rules=[],
        use_cutoff=True,
        num_threads=16,
        decomposition_branch=True,
        eps=0.001,
    )
    assert ub.get_trajectory().length() == pytest.approx(11.3803, rel=1e-4)
    assert lb <= 11.4


def test_4x5() -> None:
    """Test a 4x5 grid of squares."""
    instance = Instance(
        [
            Point(0, 0),
            Point(0, 0),
        ]
        + [
            Polygon(
                [
                    [
                        Point(x - 0.25, y - 0.25),
                        Point(x - 0.25, y + 0.25),
                        Point(x + 0.25, y + 0.25),
                        Point(x + 0.25, y - 0.25),
                    ]
                ]
            )
            for x in range(4)
            for y in range(5)
        ],
        True,
    )

    ub, lb, stats = branch_and_bound(
        instance=instance,
        callback=lambda _: None,
        initial_solution=None,
        timelimit=60,
        branching="FarthestPoly",
        search="DfsBfs",
        root="LongestTriple",
        node_simplification=False,
        rules=[],
        use_cutoff=True,
        num_threads=16,
        decomposition_branch=True,
        eps=0.001,
    )
    assert ub.get_trajectory().length() == pytest.approx(13.8009, rel=1e-3)
    assert lb <= 13.8
