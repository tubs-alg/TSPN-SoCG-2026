"""Geometry utilities for converting polygons to halfspace representations."""

from collections.abc import Iterable

import numpy as np

from tspn_bnb2.core import Point


def _is_ccw(vertices: list[tuple[float, float]]) -> bool:
    """Check if polygon vertices are in counter-clockwise order.

    Uses the shoelace formula to compute signed area.
    Positive area = CCW, negative = CW.
    """
    n = len(vertices)
    signed_area = 0.0
    for i in range(n):
        x1, y1 = vertices[i]
        x2, y2 = vertices[(i + 1) % n]
        signed_area += (x2 - x1) * (y2 + y1)
    return signed_area < 0  # Negative for CCW (standard convention)


def _remove_duplicate_vertices(
    vertices: list[tuple[float, float]], tol: float = 1e-9
) -> list[tuple[float, float]]:
    """Remove consecutive duplicate vertices within tolerance."""
    if len(vertices) <= 1:
        return vertices
    result = [vertices[0]]
    for v in vertices[1:]:
        if abs(v[0] - result[-1][0]) > tol or abs(v[1] - result[-1][1]) > tol:
            result.append(v)
    # Also check if last vertex duplicates first
    if (
        len(result) > 1
        and abs(result[-1][0] - result[0][0]) <= tol
        and abs(result[-1][1] - result[0][1]) <= tol
    ):
        result = result[:-1]
    return result


def polygon_to_halfspaces(
    vertices: list[tuple[float, float]],
) -> tuple[np.ndarray, np.ndarray] | None:
    """Convert polygon vertices to H-representation (A, b) where A @ q <= b.

    Handles both CW and CCW vertex orderings by detecting orientation and
    adjusting constraint signs accordingly. Also removes duplicate vertices
    that would create degenerate edges.

    Args:
        vertices: Polygon vertices (either CW or CCW order).

    Returns:
        Tuple of (A, b) where A is (n, 2) and b is (n,), or None if the
        polygon is degenerate (fewer than 3 unique vertices).

    """
    # Remove duplicate vertices that create zero-length edges
    vertices = _remove_duplicate_vertices(vertices)

    n = len(vertices)
    if n < 3:
        # Degenerate polygon (line or point) - cannot create valid halfspaces
        return None

    A = np.zeros((n, 2))
    b = np.zeros(n)

    # Check orientation - formula below assumes CW vertices
    # If CCW, we need to negate the constraints
    is_ccw = _is_ccw(vertices)
    sign = -1.0 if is_ccw else 1.0

    for i in range(n):
        x1, y1 = vertices[i]
        x2, y2 = vertices[(i + 1) % n]

        # Edge direction and normal
        # For CW vertices: inward normal points left of edge direction
        nx = y2 - y1
        ny = x1 - x2

        # Normalize for numerical stability
        norm = np.sqrt(nx * nx + ny * ny)
        if norm > 1e-10:
            nx /= norm
            ny /= norm

        # Constraint: -nx * qx - ny * qy <= -(nx * x1 + ny * y1)
        # Multiply by sign to handle CCW vertices
        A[i, 0] = sign * (-nx)
        A[i, 1] = sign * (-ny)
        b[i] = sign * (-(nx * x1 + ny * y1))

    return A, b


def extract_vertices_from_ring(ring: Iterable[Point]) -> list[tuple[float, float]]:
    """Extract vertices from a Ring or polygon outer ring.

    Args:
        ring: A Ring object or iterable of points with .x and .y attributes.

    Returns:
        List of (x, y) tuples, without the closing vertex if present.

    """
    vertices = [(p.x, p.y) for p in ring]
    if len(vertices) > 1 and vertices[0] == vertices[-1]:
        vertices = vertices[:-1]
    return vertices
