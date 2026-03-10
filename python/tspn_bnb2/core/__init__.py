"""Package especially provides the BnB-algorithm including callbacks."""

from ._tspn_bindings import (
    Circle,
    GeometryAnnotations,
    Instance,
    Point,
    Polygon,
    Ring,
    SocSolver,
    TourElement,
    branch_and_bound,
    compute_tour_from_sequence,
    parse_site_wkt,
)

__all__ = [
    "Circle",
    "GeometryAnnotations",
    "Instance",
    "Point",
    "Polygon",
    "Ring",
    "SocSolver",
    "TourElement",
    "branch_and_bound",
    "compute_tour_from_sequence",
    "parse_site_wkt",
    "to_boost",
    "to_instance",
]


def to_boost(pointlist: list[list[tuple[int | float, int | float]]]) -> str:
    """Build a polygon from a list of pointlists (rings) and return its WKT string."""
    nested_vector = []
    for ring in pointlist:
        ring_vector = [Point(point[0], point[1]) for point in ring]
        nested_vector.append(ring_vector)
    return Polygon(nested_vector).wkt


def to_instance(pointlistlist: list[list[list[tuple[int | float, int | float]]]]) -> Instance:
    """Build an Instance from a list of pointlists."""
    polys = [to_boost(pointlist) for pointlist in pointlistlist]
    return Instance(polys)
