"""Order annotation for TSPN instances based on convex hull position.

Computes a circular ordering of polygons along the convex hull boundary
and detects mutual polygon intersections. Uses pure Shapely geometry
(no CGAL dependency).
"""

from collections.abc import Sequence

from shapely import wkt
from shapely.geometry import LineString, MultiPoint
from shapely.geometry import Polygon as ShapelyPolygon
from shapely.ops import nearest_points

from .schemas import AnnotatedInstance, PolygonAnnotation


def _compute_polygon_order_and_intersections(
    polygons: Sequence[ShapelyPolygon],
    ring_points: Sequence[tuple[float, float]],
    epsilon: float = 1e-6,
    intersection_epsilon: float | None = None,
) -> list[tuple[int, list[int]]]:
    """Compute polygon ordering along a ring and their mutual intersections.

    Given a list of polygons and a convex ring (boundary), this function:
    1. Determines which polygons intersect the ring (within epsilon tolerance)
    2. Orders them by their first intersection point along the ring
    3. For each polygon, finds all other polygons it intersects with

    Polygons are shrunk by ``intersection_epsilon`` before testing to reduce
    false positives from near-touching boundaries.

    Args:
        polygons: Polygons to analyze.
        ring_points: Vertices forming a convex ring (automatically closed).
        epsilon: Distance tolerance for ring intersection.
        intersection_epsilon: Shrink distance for intersection tests
            (default: 0.4 * epsilon).

    Returns:
        List of ``(polygon_index, [intersecting_indices])`` ordered by
        first intersection with the ring. Only polygons within epsilon
        of the ring are included.

    """
    if intersection_epsilon is None:
        intersection_epsilon = 0.4 * epsilon

    # Shrink polygons for intersection tests
    shrunken: list[ShapelyPolygon | None] = []
    for poly in polygons:
        s = poly.buffer(-intersection_epsilon)
        if s.is_empty or not isinstance(s, ShapelyPolygon):
            shrunken.append(None)
        else:
            shrunken.append(s)

    # Build ring segments (close the ring)
    ring_segments = []
    for i in range(len(ring_points)):
        p1 = ring_points[i]
        p2 = ring_points[(i + 1) % len(ring_points)]
        ring_segments.append(LineString([p1, p2]))

    # Find first intersection position for each polygon along the ring
    polygon_positions: list[tuple[int, float]] = []

    for seg_idx, seg in enumerate(ring_segments):
        seg_bounds = seg.bounds  # (minx, miny, maxx, maxy)
        ext_minx = seg_bounds[0] - epsilon
        ext_miny = seg_bounds[1] - epsilon
        ext_maxx = seg_bounds[2] + epsilon
        ext_maxy = seg_bounds[3] + epsilon

        for poly_idx in range(len(polygons)):
            # Skip if already positioned
            if any(idx == poly_idx for idx, _ in polygon_positions):
                continue

            sp = shrunken[poly_idx]
            if sp is None:
                continue

            # Coarse bbox check
            pb = sp.bounds
            if pb[2] < ext_minx or pb[0] > ext_maxx or pb[3] < ext_miny or pb[1] > ext_maxy:
                continue

            # Closest points between shrunken polygon and ring segment
            cp_on_poly, cp_on_seg = nearest_points(sp, seg)

            dist = cp_on_poly.distance(cp_on_seg)
            if dist <= epsilon:
                # Position along ring = segment index + fractional position within segment
                seg_start = seg.coords[0]
                dist_to_start = (
                    (cp_on_seg.x - seg_start[0]) ** 2 + (cp_on_seg.y - seg_start[1]) ** 2
                ) ** 0.5
                seg_length = seg.length
                position_in_segment = dist_to_start / seg_length if seg_length > 1e-12 else 0
                position = seg_idx + position_in_segment
                polygon_positions.append((poly_idx, position))

    polygon_positions.sort(key=lambda x: x[1])
    ordered_indices = [idx for idx, _ in polygon_positions]

    return _compute_intersections(ordered_indices, shrunken)


def _compute_intersections(
    ordered_indices: list[int],
    shrunken: list[ShapelyPolygon | None],
) -> list[tuple[int, list[int]]]:
    """Find mutual intersections between shrunken polygons."""
    result: list[tuple[int, list[int]]] = []
    for poly_idx in ordered_indices:
        intersecting = []
        sp = shrunken[poly_idx]
        if sp is not None:
            sp_bounds = sp.bounds
            for other_idx in ordered_indices:
                if other_idx == poly_idx:
                    continue
                so = shrunken[other_idx]
                if so is None:
                    continue
                ob = so.bounds
                if (
                    sp_bounds[2] < ob[0]
                    or sp_bounds[0] > ob[2]
                    or sp_bounds[3] < ob[1]
                    or sp_bounds[1] > ob[3]
                ):
                    continue
                if sp.intersects(so):
                    intersecting.append(other_idx)
        result.append((poly_idx, intersecting))
    return result


def add_order_annotations(instance: AnnotatedInstance) -> AnnotatedInstance:
    """Annotate polygons with order information based on their position on the convex hull.

    Computes a circular ordering of polygons along the global convex hull
    and detects which polygons overlap. Results are stored as
    :class:`PolygonAnnotation` entries with ``hull_index`` and ``intersections``.
    """
    # Parse polygons to Shapely
    shapely_polys = [wkt.loads(p) if isinstance(p, str) else p for p in instance.polygons]

    # Convex hull of all boundary points
    all_coords: list[tuple[float, float]] = []
    for poly in shapely_polys:
        all_coords.extend(poly.exterior.coords[:-1])
    hull = MultiPoint(all_coords).convex_hull

    # Convert Shapely CCW hull to CW ordering (matching CGAL convention):
    # keep the start vertex, reverse the rest.
    ccw = list(hull.exterior.coords[:-1])
    ring_points = [ccw[0]] + ccw[1:][::-1]

    ordering = _compute_polygon_order_and_intersections(shapely_polys, ring_points, epsilon=1e-9)

    annotations: dict[int, PolygonAnnotation] = dict(instance.annotations)
    for order, (idx, inter) in enumerate(ordering):
        annotations[idx] = PolygonAnnotation(hull_index=order, intersections=inter)

    return AnnotatedInstance(
        polygons=instance.polygons,
        annotations=annotations,
        meta=instance.meta,
    )
