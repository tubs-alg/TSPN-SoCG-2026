import numpy as np
from scipy.spatial import ConvexHull
from shapely.geometry import Polygon as ShapelyPolygon
from tspn_bnb2.schemas import AnnotatedInstance


def generate_random_convex_polygon(
    center: tuple[float, float],
    avg_radius: float = 0.3,
    num_points: int = 6,
    rng: np.random.Generator | None = None,
) -> ShapelyPolygon:
    """Generate a random convex polygon around a center point.

    Args:
        center: (x, y) coordinates of the polygon center.
        avg_radius: Average distance from center to vertices.
        num_points: Number of random points to generate (hull may have fewer vertices).
        rng: Random number generator for reproducibility.

    Returns:
        A Shapely Polygon that is guaranteed to be convex.

    """
    if rng is None:
        rng = np.random.default_rng()

    # Generate random points around the center
    angles = rng.uniform(0, 2 * np.pi, num_points)
    radii = rng.uniform(0.5 * avg_radius, 1.5 * avg_radius, num_points)

    points = np.column_stack(
        [
            center[0] + radii * np.cos(angles),
            center[1] + radii * np.sin(angles),
        ]
    )

    # Compute convex hull to ensure convexity
    hull = ConvexHull(points)
    hull_points = points[hull.vertices]

    return ShapelyPolygon(hull_points)


def generate_random_instance(
    num_polygons: int = 6,
    grid_size: float = 10.0,
    avg_radius: float = 0.5,
    seed: int | None = None,
) -> AnnotatedInstance:
    """Generate a random TSPN instance with convex polygons.

    Args:
        num_polygons: Number of polygons to generate.
        grid_size: Size of the area to place polygon centers.
        avg_radius: Average radius of the polygons.
        seed: Random seed for reproducibility.

    Returns:
        An AnnotatedInstance with randomly generated convex polygons.

    """
    rng = np.random.default_rng(seed)

    # Generate random centers
    centers = rng.uniform(0, grid_size, (num_polygons, 2))

    # Generate convex polygons around each center
    polygons = [
        generate_random_convex_polygon(tuple(center), avg_radius, rng=rng) for center in centers
    ]

    return AnnotatedInstance(
        polygons=polygons,
        meta={"source": "random", "num_polygons": num_polygons, "seed": seed},
    )
