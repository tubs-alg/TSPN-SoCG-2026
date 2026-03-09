"""Collection of plotting functions."""

from collections.abc import Callable
from typing import Any

import matplotlib.colors as mpc
import matplotlib.pyplot as plt
from matplotlib.patches import Circle as CirclePatch
from matplotlib.patches import PathPatch
from matplotlib.path import Path
from shapely.geometry import Polygon as ShapelyPolygon
from shapely.plotting import plot_polygon
from shapely.wkt import loads as load_wkt

from tspn_bnb2.core._tspn_bindings import (
    Circle,
    Instance,
    Point,
    Polygon,
    Ring,
    Solution,
    Trajectory,
)


def patchify(polys: list[Polygon], **kwargs: Any) -> PathPatch:
    """Transform polys into a PathPatch of all of them.

    from: https://stackoverflow.com/questions/8919719/how-to-plot-a-complex-polygon
    """

    def ring_coding(n: int) -> list:
        """Encode ring as a list of plotting commands.

        Returns a list of len(n) of this format:
        [MOVETO, LINETO, LINETO, ..., LINETO, LINETO, CLOSEPOLY]
        """
        codes = [Path.LINETO] * n
        codes[0] = Path.MOVETO
        codes[-1] = Path.CLOSEPOLY
        return codes

    codes = [movecode for p in polys for movecode in ring_coding(len(p))]
    vertices = [(point.x, point.y) for p in polys for point in p]
    return PathPatch(Path(vertices, codes), **kwargs)


def agg(pointa: Point, pointb: Point, op: Callable[[float, float], float]) -> Point:
    """Aggregate two Points into one by piecewise applying op."""
    return Point(
        op(pointa.x, pointb.x),
        op(pointa.y, pointb.y),
    )


def set_bbox(ax: plt.Axes, sites: list) -> None:
    """Set the axes bbox according to the polys."""
    minpoint = Point(+(2**30), +(2**30))
    maxpoint = Point(-(2**30), -(2**30))
    for site in sites:
        minpoint = agg(minpoint, site.bbox().min_corner(), min)
        maxpoint = agg(maxpoint, site.bbox().max_corner(), max)

    xmrg = (maxpoint.x - minpoint.x) * 0.1
    ymrg = (maxpoint.y - minpoint.y) * 0.1
    ax.set_xlim(minpoint.x - xmrg, maxpoint.x + xmrg)
    ax.set_ylim(minpoint.y - ymrg, maxpoint.y + ymrg)


def autocolor(i: int, polys: list) -> tuple[float, float, float]:
    """Get the i'th color of a sequence of colors."""
    return mpc.hsv_to_rgb((i / len(polys), 1, 0.8))


def plot_site(ax: plt.Axes, site: Point | Circle | Ring | Polygon, **kwargs: Any) -> None:
    """Plot a polygon to the ax."""
    if isinstance(site, Point):
        kwa = kwargs
        kwa.pop("zorder", None)
        ax.plot(site.x, site.y, "x", color=kwargs["ec"], markersize=10)
        patch = CirclePatch((site.x, site.y), 0, **kwargs)
        ax.add_patch(patch)
    elif isinstance(site, Circle):
        patch = CirclePatch((site.center().x, site.center().y), site.radius(), **kwargs)
        ax.add_patch(patch)
    elif isinstance(site, Ring):
        patch = patchify([site], **kwargs)
        ax.add_patch(patch)
    elif isinstance(site, Polygon):
        poly = load_wkt(str(site))  # type: ShapelyPolygon
        assert isinstance(poly, ShapelyPolygon)

        plot_polygon(poly, ax=ax, **kwargs)
    else:
        msg = f"not implemented site type {type(site)}: {site}"
        raise NotImplementedError(msg)


def plot_sites(
    ax: plt.Axes,
    sites: list,
    colors: list[tuple[float, float, float] | str] | None = None,
    text: list[str | None] | dict[int, str] | None = None,
    textcolor: tuple[float, float, float] | str | None = None,
) -> None:
    """Plot a list of sites and set the bbox accordingly."""
    set_bbox(ax, sites)
    for i, p in enumerate(sites):
        color = colors[i] if colors else autocolor(i, sites)
        plot_site(
            ax,
            p,
            lw=1,
            ec=mpc.to_rgba(color, 0.8),
            fc=mpc.to_rgba(color, 0.3),
            zorder=0,
        )
        if text is not None and text[i] is not None:
            bbl = p.bbox().min_corner()
            bbh = p.bbox().max_corner()
            col = textcolor if textcolor else color
            ax.text(
                x=(bbl.x + 0.5 * (bbh.x - bbl.x)),
                y=(bbl.y + 0.5 * (bbh.y - bbl.y)),
                s=text[i],
                c=col,
                ha="center",
                va="center",
            )


def plot_instance(ax: plt.Axes, instance: Instance) -> None:
    """Plot an Instance."""
    return plot_solution(ax, instance, None)


def plot_solution(ax: plt.Axes, instance: Instance, solution: Solution) -> None:
    """Plot a Solution."""
    # todo: rework
    trajectory = solution.get_trajectory() if solution else None
    return plot_trajectory(ax, instance, trajectory)


def plot_trajectory(ax: plt.Axes, instance: Instance, trajectory: Trajectory | None) -> None:
    """Plot a trajectory, set the bbox etc."""
    set_bbox(ax, instance.sites())
    if instance.is_path():
        src = trajectory.begin()
        ax.plot(src.x, src.y, "+", color="black", markersize=10)
        dest = trajectory.end()
        ax.plot(dest.x, dest.y, "x", color="black", markersize=10)

    for i, site in enumerate(instance.sites()):
        color = autocolor(i, instance.sites())
        distance_tolerance = 0.0001
        if trajectory and trajectory.distance_site(site) >= distance_tolerance:
            color = "black"

        plot_site(
            ax,
            site,
            lw=1,
            ec=mpc.to_rgba(color, 0.8),
            fc=mpc.to_rgba(color, 0.3),
            zorder=0,
        )
    if trajectory:
        ax.plot(
            [p.x for p in trajectory],
            [p.y for p in trajectory],
            ".-",
            color="black",
            lw=1,
        )
    ax.set_aspect("equal", "datalim")
