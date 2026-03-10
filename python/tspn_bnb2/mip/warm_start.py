"""Christofides-based warm start generation for MIP solver.

This module provides functions to generate initial solutions for the TSPN MIP solver
using the Christofides algorithm on polygon centroids, then refining with SOCP.
"""

import networkx as nx
import numpy as np
from shapely import wkt
from shapely.geometry import LineString
from shapely.geometry import Point as ShapelyPoint
from shapely.geometry import Polygon as ShapelyPolygon

from tspn_bnb2.core import Instance, TourElement, compute_tour_from_sequence
from tspn_bnb2.schemas import AnnotatedSolution


def compute_polygon_centroids(instance: Instance) -> list[tuple[float, float]]:
    """Compute centroid of each polygon using shapely.

    Args:
        instance: TSPN instance with polygon neighborhoods.

    Returns:
        List of (x, y) centroids, one per polygon.

    """
    centroids = []
    for i in range(len(instance)):
        geometry = instance[i]
        polygon = geometry.definition()
        shapely_poly = wkt.loads(str(polygon))
        centroid = shapely_poly.centroid
        centroids.append((centroid.x, centroid.y))
    return centroids


def build_centroid_graph(centroids: list[tuple[float, float]]) -> nx.Graph:
    """Build complete weighted graph on centroids.

    Args:
        centroids: List of (x, y) coordinates.

    Returns:
        Complete graph with Euclidean edge weights.

    """
    G = nx.Graph()
    n = len(centroids)
    for i in range(n):
        G.add_node(i, pos=centroids[i])
    for i in range(n):
        for j in range(i + 1, n):
            dist = np.hypot(centroids[j][0] - centroids[i][0], centroids[j][1] - centroids[i][1])
            G.add_edge(i, j, weight=dist)
    return G


def christofides_ordering(instance: Instance) -> list[int]:
    """Get tour ordering using Christofides algorithm on polygon centroids.

    Args:
        instance: TSPN instance with polygon neighborhoods.

    Returns:
        List of polygon indices in tour order.

    """
    centroids = compute_polygon_centroids(instance)
    G = build_centroid_graph(centroids)
    tour = nx.approximation.christofides(G, weight="weight")
    # tour is a list of nodes forming a cycle, remove duplicate last node
    return tour[:-1] if tour[0] == tour[-1] else tour


def compute_tour_from_ordering(
    instance: Instance,
    ordering: list[int],
) -> tuple[list[tuple[float, float]], float]:
    """Compute optimal tour points for ordering using SOCP.

    For non-convex polygons, uses convex hull (lazy TourElement).

    Args:
        instance: TSPN instance with polygon neighborhoods.
        ordering: Polygon indices in tour order.

    Returns:
        Tuple of (tour_points, tour_length) where tour_points are (x, y)
        coordinates in tour order (one per polygon, excluding closing point).

    """
    elements = [TourElement(instance, idx) for idx in ordering]
    trajectory = compute_tour_from_sequence(elements, False, False)
    # Trajectory includes closing point; exclude it to get one point per polygon
    all_points = [(p.x, p.y) for p in trajectory]
    points = all_points[:-1] if len(all_points) > len(ordering) else all_points
    return points, trajectory.length()


def _get_shapely_polygon(instance: Instance, poly_idx: int) -> ShapelyPolygon:
    """Get a Shapely polygon from a C++ instance geometry."""
    geometry = instance[poly_idx]
    polygon = geometry.definition()
    return wkt.loads(str(polygon))


def _project_point_to_polygon(
    point: tuple[float, float], polygon: ShapelyPolygon
) -> tuple[float, float]:
    """Project a point onto a polygon if it's outside.

    Args:
        point: (x, y) coordinates of the point.
        polygon: Shapely polygon to project onto.

    Returns:
        The original point if inside, or the nearest point on the polygon boundary.

    """
    polygon = polygon.buffer(-0.001)
    shapely_point = ShapelyPoint(point)
    if polygon.contains(shapely_point):
        return point

    # Project to nearest point on polygon boundary
    nearest = polygon.exterior.interpolate(polygon.exterior.project(shapely_point))
    return (nearest.x, nearest.y)


def _project_tour_points(
    instance: Instance,
    ordering: list[int],
    tour_points: list[tuple[float, float]],
) -> list[tuple[float, float]]:
    """Project tour points to ensure they are inside their respective polygons.

    For non-convex polygons, SOCP may produce points in the convex hull but
    outside the actual polygon. This function projects such points to the
    nearest point on the polygon boundary.

    Args:
        instance: TSPN instance.
        ordering: Polygon indices in tour order.
        tour_points: Points in tour order (may be outside polygons).

    Returns:
        Projected points guaranteed to be inside their polygons.

    """
    projected_points = []
    for k, point in enumerate(tour_points):
        poly_idx = ordering[k]
        polygon = _get_shapely_polygon(instance, poly_idx)
        projected_point = _project_point_to_polygon(point, polygon)
        projected_points.append(projected_point)
    return projected_points


def _compute_tour_length(tour_points: list[tuple[float, float]]) -> float:
    """Compute the total length of a closed tour."""
    total = 0.0
    n = len(tour_points)
    for i in range(n):
        p1 = tour_points[i]
        p2 = tour_points[(i + 1) % n]
        total += np.hypot(p2[0] - p1[0], p2[1] - p1[1])
    return total


def _segments_intersect(
    p1: tuple[float, float],
    p2: tuple[float, float],
    p3: tuple[float, float],
    p4: tuple[float, float],
) -> bool:
    """Check if line segment (p1, p2) intersects with segment (p3, p4).

    Uses cross product to determine if segments cross (not just touch).
    """

    def ccw(a: tuple[float, float], b: tuple[float, float], c: tuple[float, float]) -> float:
        """Return positive if counter-clockwise, negative if clockwise, 0 if collinear."""
        return (c[1] - a[1]) * (b[0] - a[0]) - (b[1] - a[1]) * (c[0] - a[0])

    d1 = ccw(p3, p4, p1)
    d2 = ccw(p3, p4, p2)
    d3 = ccw(p1, p2, p3)
    d4 = ccw(p1, p2, p4)

    return ((d1 > 0 and d2 < 0) or (d1 < 0 and d2 > 0)) and (
        (d3 > 0 and d4 < 0) or (d3 < 0 and d4 > 0)
    )


def _find_intersection(
    tour_points: list[tuple[float, float]],
) -> tuple[int, int] | None:
    """Find a pair of intersecting edges in the tour.

    Returns (i, j) where edge (i, i+1) intersects edge (j, j+1), or None.
    """
    n = len(tour_points)
    for i in range(n):
        for j in range(i + 2, n):
            # Skip adjacent edges (they share a vertex)
            if i == 0 and j == n - 1:
                continue

            p1 = tour_points[i]
            p2 = tour_points[(i + 1) % n]
            p3 = tour_points[j]
            p4 = tour_points[(j + 1) % n]

            if _segments_intersect(p1, p2, p3, p4):
                return (i, j)

    return None


def _two_opt_swap(ordering: list[int], i: int, j: int) -> list[int]:
    """Perform a 2-opt swap by reversing the segment from i+1 to j."""
    # Reverse the segment between positions i+1 and j (inclusive)
    return ordering[: i + 1] + ordering[i + 1 : j + 1][::-1] + ordering[j + 1 :]


def _remove_intersections(
    instance: Instance,
    ordering: list[int],
    tour_points: list[tuple[float, float]],
    max_iterations: int = 100,
) -> tuple[list[int], list[tuple[float, float]]]:
    """Remove self-intersections from tour using 2-opt swaps.

    When edges cross, a 2-opt swap uncrosses them by reversing the
    segment between the crossing edges.

    Args:
        instance: TSPN instance.
        ordering: Current tour ordering.
        tour_points: Current tour points.
        max_iterations: Maximum number of uncrossing iterations.

    Returns:
        Tuple of (ordering, tour_points) with no self-intersections.

    """
    for _ in range(max_iterations):
        intersection = _find_intersection(tour_points)
        if intersection is None:
            break

        i, j = intersection
        # 2-opt swap: reverse segment from i+1 to j
        ordering = _two_opt_swap(ordering, i, j)

        # Recompute tour points for new ordering
        tour_points, _ = compute_tour_from_ordering(instance, ordering)
        tour_points = _project_tour_points(instance, ordering, tour_points)

    return ordering, tour_points


def generate_christofides_warm_start(
    instance: Instance,
    remove_intersections: bool = True,
) -> AnnotatedSolution:
    """Generate warm start solution using Christofides + SOCP.

    Uses Christofides algorithm on polygon centroids to determine tour ordering,
    then SOCP to compute optimal visiting points. Points are projected to ensure
    feasibility for non-convex polygons. Optionally removes self-intersections
    using 2-opt swaps.

    Args:
        instance: TSPN instance with polygon neighborhoods.
        remove_intersections: Whether to remove self-intersections with 2-opt
            (default True). This ensures the tour has no crossing edges.

    Returns:
        AnnotatedSolution with trajectory and tour length as upper bound.

    """
    ordering = christofides_ordering(instance)

    # Compute initial tour points
    tour_points, _ = compute_tour_from_ordering(instance, ordering)
    tour_points = _project_tour_points(instance, ordering, tour_points)

    # Remove any self-intersections using 2-opt
    if remove_intersections:
        ordering, tour_points = _remove_intersections(instance, ordering, tour_points)

    tour_length = _compute_tour_length(tour_points)

    # Create closed tour trajectory
    closed_points = list(tour_points) + [tour_points[0]]
    trajectory = LineString(closed_points)

    return AnnotatedSolution(
        trajectory=trajectory,
        lower_bound=0.0,
        upper_bound=tour_length,
        is_optimal=False,
        gap=tour_length,
        statistics={"method": "christofides", "ordering": ordering},
        is_tour=True,
    )
