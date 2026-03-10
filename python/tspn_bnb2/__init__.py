"""TSPN BnB2: exact solver for the Traveling Salesman Problem with Neighborhoods."""

__version__ = "0.2.1"

from .core import to_boost
from .core._tspn_bindings import (
    Instance,
    Point,
    Polygon,
    branch_and_bound,
    convex_hull_fill,
    get_float_parameter,
    get_int_parameter,
    pre_simplify,
    remove_holes,
    remove_supersites,
    reset_parameters,
    set_float_parameter,
    set_int_parameter,
)
from .mip import TSPNMIPSolver, solve_tspn_mip
from .misc import (
    InstanceDb,
    autocolor,
    plot_sites,
    plot_solution,
    plot_trajectory,
)
from .operations import (
    simplify_annotated_instance,
    solve_annotated_instance,
    to_native_polygon,
    to_shapely_linestring,
    to_shapely_polygon,
)
from .schemas import (
    AnnotatedInstance,
    AnnotatedSolution,
    PolygonAnnotation,
)

__all__ = [
    "AnnotatedInstance",
    "AnnotatedSolution",
    "Instance",
    "InstanceDb",
    "Point",
    "Polygon",
    "PolygonAnnotation",
    "TSPNMIPSolver",
    "autocolor",
    "branch_and_bound",
    "convex_hull_fill",
    "get_float_parameter",
    "get_int_parameter",
    "plot_sites",
    "plot_solution",
    "plot_trajectory",
    "pre_simplify",
    "remove_holes",
    "remove_supersites",
    "reset_parameters",
    "set_float_parameter",
    "set_int_parameter",
    "simplify_annotated_instance",
    "solve_annotated_instance",
    "solve_tspn_mip",
    "to_boost",
    "to_native_polygon",
    "to_shapely_linestring",
    "to_shapely_polygon",
]


def check_gurobi_license() -> None:
    """Check if a Gurobi license is installed on the system."""
    import os
    from pathlib import Path

    gurobi_lic = os.environ.get("GRB_LICENSE_FILE", str(Path.home() / "gurobi.lic"))
    if not Path(gurobi_lic).exists():
        raise RuntimeError(
            f"No Gurobi license found! Please install a license first. Looked in '{gurobi_lic}'."
        )


check_gurobi_license()
