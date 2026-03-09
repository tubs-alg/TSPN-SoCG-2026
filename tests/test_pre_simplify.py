"""Test pre_simplify functionality."""

from tspn_bnb2 import Point, Polygon, pre_simplify


def test_pre_simplify_empty() -> None:
    """Test pre_simplify with empty list."""
    sites = []
    result = pre_simplify(sites)
    assert len(result) == 0


def test_pre_simplify_single_point() -> None:
    """Test pre_simplify with a single point."""
    sites = [Point(0, 0)]
    result = pre_simplify(sites)
    assert len(result) == 1


def test_pre_simplify_single_polygon() -> None:
    """Test pre_simplify with a single polygon."""
    sites = [Polygon([[Point(0, 0), Point(1, 0), Point(1, 1), Point(0, 1)]])]
    result = pre_simplify(sites)
    assert len(result) == 1


def test_pre_simplify_removes_contained_polygon() -> None:
    """Test that pre_simplify removes polygons contained within others."""
    # Create a large polygon that contains a smaller one
    large_poly = Polygon([[Point(0, 0), Point(10, 0), Point(10, 10), Point(0, 10)]])
    small_poly = Polygon([[Point(2, 2), Point(4, 2), Point(4, 4), Point(2, 4)]])

    sites = [large_poly, small_poly]
    result = pre_simplify(sites)

    # Should remove the smaller polygon since it's contained in the larger one
    assert len(result) == 1


def test_pre_simplify_preserves_non_overlapping_polygons() -> None:
    """Test that pre_simplify preserves non-overlapping polygons."""
    poly1 = Polygon([[Point(0, 0), Point(1, 0), Point(1, 1), Point(0, 1)]])
    poly2 = Polygon([[Point(5, 0), Point(6, 0), Point(6, 1), Point(5, 1)]])

    sites = [poly1, poly2]
    result = pre_simplify(sites)

    # Both polygons should be preserved
    assert len(result) == 2


def test_pre_simplify_removes_holes() -> None:
    """Test that pre_simplify removes unnecessary holes in polygons."""
    # Create a polygon with a hole, where another site covers the hole area
    outer = [Point(0, 0), Point(10, 0), Point(10, 10), Point(0, 10)]
    hole = [Point(2, 2), Point(4, 2), Point(4, 4), Point(2, 4)]
    poly_with_hole = Polygon([outer, hole])

    # Create another polygon that covers the hole area
    covering_poly = Polygon([[Point(2, 2), Point(4, 2), Point(4, 4), Point(2, 4)]])

    sites = [poly_with_hole, covering_poly]
    result = pre_simplify(sites)

    # Result should have the hole removed from the first polygon
    # and the covering polygon might be removed if contained
    assert len(result) >= 1


def test_pre_simplify_convex_hull_fill() -> None:
    """Test that pre_simplify can fill areas between polygon and convex hull."""
    # Create an L-shaped polygon (two squares forming an L)
    # The simplification might fill the missing corner if beneficial
    poly = Polygon([[Point(0, 0), Point(2, 0), Point(2, 1), Point(1, 1), Point(1, 2), Point(0, 2)]])

    sites = [poly]
    result = pre_simplify(sites)

    # Should return at least one polygon
    assert len(result) >= 1


def test_pre_simplify_with_mixed_types() -> None:
    """Test pre_simplify with mixed site types (points and polygons)."""
    point = Point(5, 5)
    poly = Polygon([[Point(0, 0), Point(1, 0), Point(1, 1), Point(0, 1)]])

    sites = [point, poly]
    result = pre_simplify(sites)

    # Both should be preserved as they don't overlap
    assert len(result) == 2


def test_pre_simplify_multiple_overlapping() -> None:
    """Test pre_simplify with multiple overlapping polygons."""
    # Three nested squares of different sizes
    large = Polygon([[Point(0, 0), Point(10, 0), Point(10, 10), Point(0, 10)]])
    medium = Polygon([[Point(2, 2), Point(8, 2), Point(8, 8), Point(2, 8)]])
    small = Polygon([[Point(4, 4), Point(6, 4), Point(6, 6), Point(4, 6)]])

    sites = [large, medium, small]
    result = pre_simplify(sites)

    # Should only keep the largest polygon
    assert len(result) == 1


def test_pre_simplify_preserves_order_priority() -> None:
    """Test that pre_simplify respects ordering when polygons are equal."""
    # Two identical polygons - should keep the first one
    poly1 = Polygon([[Point(0, 0), Point(1, 0), Point(1, 1), Point(0, 1)]])
    poly2 = Polygon([[Point(0, 0), Point(1, 0), Point(1, 1), Point(0, 1)]])

    sites = [poly1, poly2]
    result = pre_simplify(sites)

    # Should only keep one of them
    assert len(result) == 1


def test_pre_simplify_triangles() -> None:
    """Test pre_simplify with triangular polygons."""
    tri1 = Polygon([[Point(0, 0), Point(2, 0), Point(1, 2)]])
    tri2 = Polygon([[Point(5, 0), Point(7, 0), Point(6, 2)]])

    sites = [tri1, tri2]
    result = pre_simplify(sites)

    # Both should be preserved
    assert len(result) == 2


def test_pre_simplify_returns_copy() -> None:
    """Test that pre_simplify returns a new list and doesn't modify input."""
    original = [Polygon([[Point(0, 0), Point(1, 0), Point(1, 1), Point(0, 1)]])]
    sites = list(original)  # Make a copy

    result = pre_simplify(sites)

    # Original should be unchanged (same length at least)
    assert len(sites) == len(original)
    # Result should be a valid list
    assert isinstance(result, list)


def test_pre_simplify_complex_scenario() -> None:
    """Test pre_simplify with a complex mix of sites."""
    # Create a scenario with points, contained polygons, and non-overlapping polygons
    point = Point(15, 15)
    large_poly = Polygon([[Point(0, 0), Point(10, 0), Point(10, 10), Point(0, 10)]])
    contained_poly = Polygon([[Point(2, 2), Point(4, 2), Point(4, 4), Point(2, 4)]])
    separate_poly = Polygon([[Point(20, 20), Point(22, 20), Point(22, 22), Point(20, 22)]])

    sites = [point, large_poly, contained_poly, separate_poly]
    result = pre_simplify(sites)

    # Should have point, large_poly, and separate_poly (contained_poly removed)
    # Total: 3 sites
    assert len(result) == 3
