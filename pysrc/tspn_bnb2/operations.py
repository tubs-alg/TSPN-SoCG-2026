"""Operations for solving and simplifying annotated TSPN instances."""

import warnings
from typing import Any

from shapely.geometry import LineString
from shapely.wkt import loads as load_wkt

from .core._tspn_bindings import (
    GeometryAnnotations,
    Instance,
    branch_and_bound,
    set_int_parameter,
)
from .core._tspn_bindings import (
    Polygon as TspnPolygon,
)
from .misc.io import convert_number
from .schemas import AnnotatedInstance, AnnotatedSolution


def _argument_sanity_check(
    MAX_TE_TOGGLE_COUNT: int,
    MAX_GEOM_TOGGLE_COUNT: int,
    use_cutoff: bool,
) -> None:
    """Check for problematic parameter combinations and issue warnings.

    Raises:
        ValueError: For critical parameter errors that would cause incorrect results

    """
    # Warning: MAX_GEOM_TOGGLE_COUNT < MAX_TE_TOGGLE_COUNT doesn't make sense
    if MAX_GEOM_TOGGLE_COUNT < MAX_TE_TOGGLE_COUNT:
        warnings.warn(
            f"MAX_GEOM_TOGGLE_COUNT ({MAX_GEOM_TOGGLE_COUNT}) is less than "
            f"MAX_TE_TOGGLE_COUNT ({MAX_TE_TOGGLE_COUNT}). Since each TourElement "
            f"toggle also increments the geometry toggle counter, the geometry-level "
            f"constraint will always be hit first, making the TE-level constraint "
            f"ineffective. Consider setting MAX_GEOM_TOGGLE_COUNT >= MAX_TE_TOGGLE_COUNT.",
            UserWarning,
            stacklevel=3,
        )

    # Warning: use_cutoff=False is almost never recommended
    if not use_cutoff:
        warnings.warn(
            "use_cutoff=False disables upper bound cutoff in the SOCP solver. "
            "This removes crucial pruning capability and can slow convergence. "
            "Only recommended for debugging purposes.",
            UserWarning,
            stacklevel=3,
        )


def convert_to_tspn_instance(instance: AnnotatedInstance) -> Instance:
    """Convert an AnnotatedInstance to a C++ TSPN Instance."""
    # Convert Shapely polygons to TSPN polygons
    tspn_polygons = [TspnPolygon.from_wkt(poly.wkt) for poly in instance.polygons]

    # Create TSPN instance
    tspn_instance = Instance(tspn_polygons, False)
    # Push order annotations into the C++ geometries
    for idx, ann in instance.annotations.items():
        ga = GeometryAnnotations()
        ga.order_index = ann.hull_index if ann.hull_index >= 0 else None
        ga.overlapping_order_geo_indices = ann.intersections
        tspn_instance.add_annotation(idx, ga)

    return tspn_instance


def solve_annotated_instance(
    instance: AnnotatedInstance,
    timelimit: int = 60,
    branching: str = "FarthestPoly",
    search: str = "DfsBfs",
    root: str = "LongestEdgePlusFurthestSite",
    node_simplification: bool = False,
    rules: list[str] | None = None,
    MAX_TE_TOGGLE_COUNT: int = 1,
    MAX_GEOM_TOGGLE_COUNT: int = 1,
    use_cutoff: bool = True,
    num_threads: int = 16,
    decomposition_branch: bool = True,
    eps: float = 0.001,
    callback: Any = None,
    lazy_callback: Any = None,
    initial_solution: Any = None,
    **meta_kwargs: Any,
) -> AnnotatedSolution:
    """Solve a TSPN instance using branch-and-bound.

    Args:
        instance: The annotated instance to solve.
        timelimit: Maximum runtime in seconds.
        branching: Branching strategy for selecting which geometry to add next.
            - "FarthestPoly": Selects geometry farthest from current relaxed solution
            - "Random": Randomly selects an uncovered geometry
        search: Node selection strategy for tree traversal.
            - "DfsBfs": Depth-first until feasible, then best-first
            - "CheapestChildDepthFirst": Always selects most promising child (DFS)
            - "CheapestBreadthFirst": Maintains sorted list of all open nodes
            - "Random": Random node selection
        root: Root node strategy for initial relaxed solution.
            - "LongestEdgePlusFurthestSite": Longest edge + farthest geometry
            - "LongestTriple": 3 geometries with maximum pairwise distances
            - "LongestTripleWithPoint", "LongestPointTriple", "LongestPair",
              "RandomPair", "OrderRoot", "RandomRoot"
        node_simplification: Remove non-spanning tour elements after trajectory
            computation. Requires node_reactivation=True.
        rules: List of sequence filtering rules.
            - "OrderFiltering": Checks order constraints (O(n²) per branch)
        MAX_TE_TOGGLE_COUNT: Maximum simplification toggles per TourElement.
            Prevents infinite loops. Only relevant if
            node_simplification=True and node_reactivation=True.
        MAX_GEOM_TOGGLE_COUNT: Maximum simplification toggles per geometry.
            Should be >= MAX_TE_TOGGLE_COUNT. Only relevant if
            node_simplification=True and node_reactivation=True.
        use_cutoff: Enable upper bound cutoff in SOCP solver for pruning.
        num_threads: Number of parallel threads for child evaluation. 0 = auto.
        decomposition_branch: True = branch on convex pieces; False = indicator modeling.
        eps: Optimality tolerance: (UB - LB) <= eps * UB + eps.
        callback: Optional callback receiving EventContext during exploration.
        lazy_callback: Optional callback for adding lazy constraints when a node
            is feasible. Use context.add_lazy_site() to add sites that must be
            covered. Must be deterministic.
        initial_solution: Optional initial solution for warm start.
        **meta_kwargs: Additional metadata for AnnotatedSolution.meta.

    Returns:
        AnnotatedSolution with trajectory, bounds, gap, statistics, and metadata.

    Raises:
        ValueError: If no solution found or invalid parameter combination.

    """
    # Check parameter combinations
    _argument_sanity_check(
        MAX_TE_TOGGLE_COUNT=MAX_TE_TOGGLE_COUNT,
        MAX_GEOM_TOGGLE_COUNT=MAX_GEOM_TOGGLE_COUNT,
        use_cutoff=use_cutoff,
    )

    # Set global parameters
    set_int_parameter("MAX_TE_TOGGLE_COUNT", MAX_TE_TOGGLE_COUNT)
    set_int_parameter("MAX_GEOM_TOGGLE_COUNT", MAX_GEOM_TOGGLE_COUNT)

    tspn_instance = convert_to_tspn_instance(instance)

    # Solve with branch and bound
    if rules is None:
        rules = []

    ub, lb, stats = branch_and_bound(
        instance=tspn_instance,
        callback=callback if callback is not None else lambda _: None,
        lazy_callback=lazy_callback,
        initial_solution=initial_solution,
        timelimit=timelimit,
        branching=branching,
        search=search,
        root=root,
        node_simplification=node_simplification,
        rules=rules,
        use_cutoff=use_cutoff,
        num_threads=num_threads,
        decomposition_branch=decomposition_branch,
        eps=eps,
    )

    # clean statistics
    for key, val in stats.items():
        stats[key] = convert_number(val)

    # Create metadata
    meta = dict(instance.meta)  # Copy instance metadata
    meta.update(meta_kwargs)  # Add any additional metadata

    if ub is None:
        return AnnotatedSolution(
            trajectory=LineString(coordinates=None),
            lower_bound=lb,
            upper_bound=float("inf"),
            is_optimal=False,
            gap=float("inf"),
            statistics=stats,
            is_tour=False,
            meta=meta,
        )
    # Extract trajectory and convert to Shapely LineString
    trajectory_points = ub.get_trajectory()
    coords = [(p.x, p.y) for p in trajectory_points]
    trajectory = LineString(coords)

    # Calculate bounds and gap

    upper_bound = trajectory.length
    gap = upper_bound - lb

    is_optimal = upper_bound / lb <= (1 + eps) if lb > 0 else upper_bound == 0

    return AnnotatedSolution(
        trajectory=trajectory,
        lower_bound=lb,
        upper_bound=upper_bound,
        is_optimal=is_optimal,
        gap=gap,
        statistics=stats,
        is_tour=trajectory_points.is_tour(),
        meta=meta,
    )


def simplify_annotated_instance(
    instance: AnnotatedInstance,
    remove_holes: bool = True,
    convex_hull_fill: bool = True,
    remove_supersites: bool = True,
) -> AnnotatedInstance:
    """Simplify an annotated instance using geometric simplifications.

    This function applies geometric simplifications to reduce the instance size:
    - Removes unnecessary holes in polygons (if remove_holes=True)
    - Fills areas between polygon boundaries and their convex hull (if convex_hull_fill=True)
    - Removes polygons that are contained within other polygons (if remove_supersites=True)

    Args:
        instance: The annotated instance to simplify
        remove_holes: Whether to remove unnecessary holes from polygons
        convex_hull_fill: Whether to fill areas between polygon and convex hull
        remove_supersites: Whether to remove contained polygons

    Returns:
        A new AnnotatedInstance with simplified polygons. Annotations are not preserved.

    Raises:
        RuntimeError: If simplification fails. The error message will contain
            details about which operation failed and why.

    """
    import tspn_bnb2.core._tspn_bindings as _bindings

    # check if input polygons are valid
    for poly in instance.polygons:
        if not poly.is_valid:
            raise ValueError("Input instance has invalid polygons")

    # Convert Shapely polygons to TSPN polygons
    tspn_polygons = [_bindings.Polygon.from_wkt(poly.wkt) for poly in instance.polygons]

    # Apply simplifications in order, with detailed error reporting
    # Note: Each function returns a new list
    try:
        if remove_holes:
            tspn_polygons = _bindings.remove_holes(tspn_polygons)
    except RuntimeError as e:
        msg = (
            f"Failed during remove_holes operation: {e!s}. "
            f"This may indicate invalid polygon geometry."
        )
        raise RuntimeError(msg) from e

    try:
        if convex_hull_fill:
            tspn_polygons = _bindings.convex_hull_fill(tspn_polygons)
    except RuntimeError as e:
        msg = (
            f"Failed during convex_hull_fill operation: {e!s}. "
            f"This typically happens when polygons are disconnected and filling "
            f"the convex hull would create a disconnected geometry. "
            f"Consider setting convex_hull_fill=False if you have widely separated polygons."
        )
        raise RuntimeError(msg) from e

    try:
        if remove_supersites:
            tspn_polygons = _bindings.remove_supersites(tspn_polygons)
    except RuntimeError as e:
        msg = (
            f"Failed during remove_supersites operation: {e!s}. "
            f"This may indicate issues with polygon containment checks."
        )
        raise RuntimeError(msg) from e

    # Convert back to Shapely polygons
    simplified_polygons = []
    for tspn_poly in tspn_polygons:
        # Convert TSPN polygon to WKT then to Shapely
        wkt_str = str(tspn_poly)  # TSPN polygons have __str__ that outputs WKT
        shapely_poly = load_wkt(wkt_str)
        simplified_polygons.append(shapely_poly)

    return AnnotatedInstance(
        polygons=simplified_polygons,
        meta=dict(instance.meta),  # Copy metadata
        annotations={},
    )
