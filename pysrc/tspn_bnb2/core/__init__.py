"""Package especially provides the BnB-algorithm including callbacks."""

from typing import Any

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
    "annotate_geometry",
    "annotate_instance",
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


def annotate_instance(instance: Instance, annotate_values: list[dict[str, Any]]) -> None:
    """Add geometry annotations to instance geometries.

    Args:
        instance (Instance): the instance
        annotate_values (list[dict[str, Any]]): list of annotation keys and values for each geometry

    """
    for geo_idx, params in zip(range(len(instance)), annotate_values, strict=True):
        annotate_geometry(instance, geo_idx, params)


def annotate_geometry(instance: Instance, geo_index: int, annotate_params: dict[str, Any]) -> None:
    """Add geometry annotations to one instance geometry.

    Args:
        instance (Instance): instance
        geo_index (int): geometry index to be annotated
        annotate_params (dict[str, Any]): keys and values to be annotated

    """
    annotations = GeometryAnnotations()
    for k, v in annotate_params.items():
        setattr(annotations, k, v)
    instance.add_annotation(geo_index, annotations)
