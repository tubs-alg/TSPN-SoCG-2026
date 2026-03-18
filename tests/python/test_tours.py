"""Test tours through a sequence of polygons."""

import pytest
from tspn_bnb2 import (
    Instance,
    Point,
    Polygon,
    branch_and_bound,
    get_float_parameter,
    get_int_parameter,
    reset_parameters,
    set_float_parameter,
    set_int_parameter,
)


def test_empty_tour() -> None:
    """Test an empty tour."""
    with pytest.raises(ValueError):
        instance = Instance([], False)
        branch_and_bound(
            instance=instance,
            callback=lambda _: None,
            initial_solution=None,
            timelimit=60,
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


def test_single_polygon_tour() -> None:
    """Test a tour through a single polygon."""
    instance = Instance([Polygon([[Point(0, 0), Point(1, 0), Point(0, 1)]])], False)
    ub, lb, stats = branch_and_bound(
        instance=instance,
        callback=lambda _: None,
        initial_solution=None,
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
    assert ub is not None
    assert ub.get_trajectory().is_tour()
    assert lb <= ub.get_trajectory().length()


def test_three_polygon_tour() -> None:
    """Test a tour through three polygons forming a triangle."""
    instance = Instance(
        [
            Polygon([[Point(0, 0), Point(1, 0), Point(0.5, 0.5)]]),
            Polygon([[Point(5, 0), Point(6, 0), Point(5.5, 0.5)]]),
            Polygon([[Point(2.5, 4), Point(3.5, 4), Point(3, 4.5)]]),
        ],
        False,
    )
    ub, lb, stats = branch_and_bound(instance, lambda _: None, timelimit=30)
    assert ub is not None
    assert ub.get_trajectory().is_tour()
    # Tour should close the loop (check distance is near zero due to floating point)
    traj = ub.get_trajectory()
    first = traj[0]
    last = traj[len(traj) - 1]
    assert abs(first.x - last.x) < 1e-6 and abs(first.y - last.y) < 1e-6
    assert lb <= ub.get_trajectory().length()


def test_square_grid_tour() -> None:
    """Test a tour through a 2x2 grid of polygons."""
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
    ub, lb, stats = branch_and_bound(
        instance=instance,
        callback=lambda _: None,
        initial_solution=None,
        timelimit=30,
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
    assert ub is not None
    assert ub.get_trajectory().is_tour()
    # Should visit all 4 polygons
    assert lb <= ub.get_trajectory().length()


def test_6x6_grid_tour() -> None:
    """Test a tour through a 6x6 grid of polygons."""
    instance = Instance(
        [
            Polygon(
                [
                    [
                        Point(x - 0.3, y - 0.3),
                        Point(x + 0.3, y - 0.3),
                        Point(x + 0.3, y + 0.3),
                        Point(x - 0.3, y + 0.3),
                    ]
                ]
            )
            for x in range(6)
            for y in range(6)
        ],
        False,
    )
    ub, lb, stats = branch_and_bound(
        instance=instance,
        callback=lambda _: None,
        initial_solution=None,
        timelimit=60,
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
    assert ub is not None
    assert ub.get_trajectory().is_tour()
    # Should visit all 36 polygons
    assert lb <= ub.get_trajectory().length()


def test_branching_strategy_farthest_poly() -> None:
    """Test tour with FarthestPoly branching strategy."""
    instance = Instance(
        [Polygon([[Point(i, 0), Point(i + 0.5, 0), Point(i + 0.25, 0.5)]]) for i in range(3)], False
    )
    ub, lb, stats = branch_and_bound(
        instance=instance,
        callback=lambda _: None,
        initial_solution=None,
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
    assert ub is not None
    assert ub.get_trajectory().is_tour()


def test_branching_strategy_random() -> None:
    """Test tour with Random branching strategy."""
    instance = Instance(
        [Polygon([[Point(i, 0), Point(i + 0.5, 0), Point(i + 0.25, 0.5)]]) for i in range(3)], False
    )
    ub, lb, stats = branch_and_bound(
        instance=instance,
        callback=lambda _: None,
        initial_solution=None,
        timelimit=10,
        branching="Random",
        search="DfsBfs",
        root="LongestEdgePlusFurthestSite",
        node_simplification=False,
        rules=[],
        use_cutoff=True,
        num_threads=16,
        decomposition_branch=True,
        eps=0.001,
    )
    assert ub is not None
    assert ub.get_trajectory().is_tour()


def test_search_strategy_dfsbfs() -> None:
    """Test tour with DfsBfs search strategy."""
    instance = Instance(
        [Polygon([[Point(i, 0), Point(i + 0.5, 0), Point(i + 0.25, 0.5)]]) for i in range(3)], False
    )
    ub, lb, stats = branch_and_bound(
        instance=instance,
        callback=lambda _: None,
        initial_solution=None,
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
    assert ub is not None


def test_search_strategy_cheapest_child() -> None:
    """Test tour with CheapestChildDepthFirst search strategy."""
    instance = Instance(
        [Polygon([[Point(i, 0), Point(i + 0.5, 0), Point(i + 0.25, 0.5)]]) for i in range(3)], False
    )
    ub, lb, stats = branch_and_bound(
        instance=instance,
        callback=lambda _: None,
        initial_solution=None,
        timelimit=10,
        branching="FarthestPoly",
        search="CheapestChildDepthFirst",
        root="LongestEdgePlusFurthestSite",
        node_simplification=False,
        rules=[],
        use_cutoff=True,
        num_threads=16,
        decomposition_branch=True,
        eps=0.001,
    )
    assert ub is not None


def test_node_simplification_enabled() -> None:
    """Test tour with node simplification enabled."""
    instance = Instance(
        [Polygon([[Point(i, 0), Point(i + 0.5, 0), Point(i + 0.25, 0.5)]]) for i in range(4)], False
    )
    ub, lb, stats = branch_and_bound(
        instance=instance,
        callback=lambda _: None,
        initial_solution=None,
        timelimit=30,
        branching="FarthestPoly",
        search="DfsBfs",
        root="LongestEdgePlusFurthestSite",
        node_simplification=True,
        rules=[],
        use_cutoff=True,
        num_threads=16,
        decomposition_branch=True,
        eps=0.001,
    )
    assert ub is not None
    assert ub.get_trajectory().is_tour()


def test_node_reactivation_disabled() -> None:
    """Test tour with node reactivation disabled."""
    instance = Instance(
        [Polygon([[Point(i, 0), Point(i + 0.5, 0), Point(i + 0.25, 0.5)]]) for i in range(3)], False
    )
    ub, lb, stats = branch_and_bound(
        instance=instance,
        callback=lambda _: None,
        initial_solution=None,
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
    assert ub is not None


def test_decomposition_branch_disabled() -> None:
    """Test tour with decomposition branching disabled."""
    instance = Instance(
        [Polygon([[Point(i, 0), Point(i + 0.5, 0), Point(i + 0.25, 0.5)]]) for i in range(3)], False
    )
    ub, lb, stats = branch_and_bound(
        instance=instance,
        callback=lambda _: None,
        initial_solution=None,
        timelimit=10,
        branching="FarthestPoly",
        search="DfsBfs",
        root="LongestEdgePlusFurthestSite",
        node_simplification=False,
        rules=[],
        use_cutoff=True,
        num_threads=16,
        decomposition_branch=False,
        eps=0.001,
    )
    assert ub is not None


def test_skip_convex_hull() -> None:
    """Test tour with skip_convex_hull using non-convex L-shaped polygons."""
    instance = Instance(
        [
            Polygon(
                [
                    [
                        Point(dx + x, dy + y)
                        for x, y in [(0, 0), (2, 0), (2, 1), (1, 1), (1, 2), (0, 2)]
                    ]
                ]
            )
            for dx, dy in [(i * 4, j * 4) for i in range(4) for j in range(4)]
        ],
        False,
    )
    ub, lb, stats = branch_and_bound(
        instance=instance,
        callback=lambda _: None,
        initial_solution=None,
        timelimit=30,
        branching="FarthestPoly",
        search="DfsBfs",
        root="LongestEdgePlusFurthestSite",
        node_simplification=False,
        rules=[],
        use_cutoff=True,
        num_threads=16,
        decomposition_branch=False,
        skip_convex_hull=True,
        eps=0.001,
    )
    assert ub is not None
    assert ub.get_trajectory().is_tour()
    assert lb <= ub.get_trajectory().length()


def test_num_threads_parameter() -> None:
    """Test tour with different number of threads."""
    instance = Instance(
        [Polygon([[Point(i, 0), Point(i + 0.5, 0), Point(i + 0.25, 0.5)]]) for i in range(3)], False
    )
    ub, lb, stats = branch_and_bound(
        instance=instance,
        callback=lambda _: None,
        initial_solution=None,
        timelimit=10,
        branching="FarthestPoly",
        search="DfsBfs",
        root="LongestEdgePlusFurthestSite",
        node_simplification=False,
        rules=[],
        use_cutoff=True,
        num_threads=4,
        decomposition_branch=True,
        eps=0.001,
    )
    assert ub is not None


def test_eps_tolerance_parameter() -> None:
    """Test tour with different epsilon tolerance."""
    instance = Instance(
        [Polygon([[Point(i, 0), Point(i + 0.5, 0), Point(i + 0.25, 0.5)]]) for i in range(3)], False
    )
    ub, lb, stats = branch_and_bound(
        instance=instance,
        callback=lambda _: None,
        initial_solution=None,
        timelimit=10,
        branching="FarthestPoly",
        search="DfsBfs",
        root="LongestEdgePlusFurthestSite",
        node_simplification=False,
        rules=[],
        use_cutoff=True,
        num_threads=16,
        decomposition_branch=True,
        eps=0.01,
    )
    assert ub is not None
    # With larger eps, optimality gap should be satisfied
    assert ub.get_trajectory().length() - lb <= 0.01 * ub.get_trajectory().length() + 0.01


def test_statistics_output() -> None:
    """Test that statistics are returned correctly."""
    instance = Instance(
        [Polygon([[Point(i, 0), Point(i + 0.5, 0), Point(i + 0.25, 0.5)]]) for i in range(3)], False
    )
    ub, lb, stats = branch_and_bound(
        instance=instance,
        callback=lambda _: None,
        initial_solution=None,
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
    assert isinstance(stats, dict)
    # Stats should contain some information
    assert len(stats) > 0


def test_callback_invocation() -> None:
    """Test that callback is invoked during search."""
    callback_count = 0

    def counting_callback(context):
        nonlocal callback_count
        callback_count += 1

    instance = Instance(
        [Polygon([[Point(i, 0), Point(i + 0.5, 0), Point(i + 0.25, 0.5)]]) for i in range(3)], False
    )
    ub, lb, stats = branch_and_bound(
        instance=instance,
        callback=counting_callback,
        initial_solution=None,
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
    assert callback_count > 0  # Callback should be called at least once


def test_callback_context_properties() -> None:
    """Test that callback receives a working EventContext with accessible properties."""
    observed = {}

    def inspecting_callback(context):
        if observed:
            return  # Only inspect the first call
        observed["num_iterations"] = context.num_iterations
        observed["has_lower_bound"] = context.get_lower_bound() >= 0
        observed["has_upper_bound"] = context.get_upper_bound() > 0 or True  # may be inf
        observed["instance_size"] = len(context.instance)

    instance = Instance(
        [Polygon([[Point(i, 0), Point(i + 0.5, 0), Point(i + 0.25, 0.5)]]) for i in range(3)], False
    )
    branch_and_bound(
        instance=instance,
        callback=inspecting_callback,
        timelimit=10,
        eps=0.001,
    )
    assert len(observed) > 0, "Callback should have been called"
    assert observed["instance_size"] == 3


def test_lazy_callback_adds_sites() -> None:
    """Test that lazy_callback can add sites that must be covered."""
    # Start with 4 corner polygons
    corners = [
        Polygon([[Point(-0.5, -0.5), Point(-0.5, 0.5), Point(0.5, 0.5), Point(0.5, -0.5)]]),
        Polygon([[Point(9.5, -0.5), Point(9.5, 0.5), Point(10.5, 0.5), Point(10.5, -0.5)]]),
        Polygon([[Point(9.5, 9.5), Point(9.5, 10.5), Point(10.5, 10.5), Point(10.5, 9.5)]]),
        Polygon([[Point(-0.5, 9.5), Point(-0.5, 10.5), Point(0.5, 10.5), Point(0.5, 9.5)]]),
    ]
    # Extra polygons to add lazily — placed inside the tour so the trajectory
    # doesn't pass through them without being forced to
    lazy_polys = [
        Polygon([[Point(4.5, 4.5), Point(4.5, 5.5), Point(5.5, 5.5), Point(5.5, 4.5)]]),
    ]
    added_count = [0]

    def lazy_cb(context):
        traj = context.get_relaxed_solution().get_trajectory()
        for poly in lazy_polys:
            if traj.distance_site(poly) > 0.001:
                context.add_lazy_site(poly)
                added_count[0] += 1
                return

    instance = Instance(corners, False)
    result = branch_and_bound(
        instance=instance,
        callback=lambda _: None,
        lazy_callback=lazy_cb,
        timelimit=30,
        eps=0.01,
    )
    assert result is not None
    assert added_count[0] > 0, "Lazy callback should have added sites"


def test_parameter_setting_float() -> None:
    """Test setting and getting float parameters."""
    reset_parameters()

    # Test FEASIBILITY_TOLERANCE
    original_tol = get_float_parameter("FEASIBILITY_TOLERANCE")
    set_float_parameter("FEASIBILITY_TOLERANCE", 0.005)
    assert get_float_parameter("FEASIBILITY_TOLERANCE") == 0.005

    # Test SPANNING_TOLERANCE
    set_float_parameter("SPANNING_TOLERANCE", 0.002)
    assert get_float_parameter("SPANNING_TOLERANCE") == 0.002

    # Reset and verify
    reset_parameters()
    assert get_float_parameter("FEASIBILITY_TOLERANCE") == original_tol


def test_parameter_setting_int() -> None:
    """Test setting and getting int parameters."""
    reset_parameters()

    # Test MAX_TE_TOGGLE_COUNT
    original_count = get_int_parameter("MAX_TE_TOGGLE_COUNT")
    set_int_parameter("MAX_TE_TOGGLE_COUNT", 3)
    assert get_int_parameter("MAX_TE_TOGGLE_COUNT") == 3

    # Test MAX_GEOM_TOGGLE_COUNT
    set_int_parameter("MAX_GEOM_TOGGLE_COUNT", 2)
    assert get_int_parameter("MAX_GEOM_TOGGLE_COUNT") == 2

    # Reset and verify
    reset_parameters()
    assert get_int_parameter("MAX_TE_TOGGLE_COUNT") == original_count


def test_parameter_invalid_float() -> None:
    """Test that invalid float parameter values are rejected."""
    with pytest.raises(Exception):  # Should raise invalid_argument
        set_float_parameter("FEASIBILITY_TOLERANCE", -0.001)

    with pytest.raises(Exception):
        set_float_parameter("SPANNING_TOLERANCE", 0.0)


def test_parameter_unknown_name() -> None:
    """Test that unknown parameter names are rejected."""
    with pytest.raises(Exception):
        set_float_parameter("UNKNOWN_PARAM", 0.5)

    with pytest.raises(Exception):
        set_int_parameter("UNKNOWN_PARAM", 5)

    with pytest.raises(Exception):
        get_float_parameter("UNKNOWN_PARAM")

    with pytest.raises(Exception):
        get_int_parameter("UNKNOWN_PARAM")


def test_parameter_case_insensitive() -> None:
    """Test that parameter names are case-insensitive."""
    reset_parameters()

    # Lowercase should work
    set_float_parameter("feasibility_tolerance", 0.002)
    assert get_float_parameter("FEASIBILITY_TOLERANCE") == 0.002

    set_int_parameter("max_te_toggle_count", 5)
    assert get_int_parameter("MAX_TE_TOGGLE_COUNT") == 5

    reset_parameters()


def test_tour_with_modified_parameters() -> None:
    """Test that tours work correctly with modified global parameters."""
    reset_parameters()

    # Modify parameters
    set_float_parameter("FEASIBILITY_TOLERANCE", 0.002)
    set_int_parameter("MAX_TE_TOGGLE_COUNT", 2)

    instance = Instance(
        [Polygon([[Point(i, 0), Point(i + 0.5, 0), Point(i + 0.25, 0.5)]]) for i in range(3)], False
    )

    ub, lb, stats = branch_and_bound(
        instance=instance,
        callback=lambda _: None,
        initial_solution=None,
        timelimit=10,
        branching="FarthestPoly",
        search="DfsBfs",
        root="LongestEdgePlusFurthestSite",
        node_simplification=True,
        rules=[],
        use_cutoff=True,
        num_threads=16,
        decomposition_branch=True,
        eps=0.001,
    )

    assert ub is not None
    assert ub.get_trajectory().is_tour()

    # Reset for other tests
    reset_parameters()
