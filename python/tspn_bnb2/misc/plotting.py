"""Collection of plotting functions."""

from __future__ import annotations

from collections.abc import Callable
from typing import TYPE_CHECKING, Any

import matplotlib.colors as mpc
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from matplotlib.axes import Axes
from matplotlib.patches import Circle as CirclePatch
from matplotlib.patches import PathPatch
from matplotlib.path import Path
from shapely.geometry import Polygon as ShapelyPolygon
from shapely.plotting import plot_line, plot_polygon
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

if TYPE_CHECKING:
    from tspn_bnb2.schemas import AnnotatedInstance, AnnotatedSolution


def cactus_plot(
    data: pd.DataFrame,
    runtime_column: str,
    strategy_column: str,
    instance_column: str,
    ax: Axes | None = None,
    flat_line_to: float | None = None,
    linestyle: str = "-",
    colors: dict | None = None,
) -> Axes:
    """Create a cactus plot (#solved instances vs runtime).

    Args:
        data: DataFrame containing the experiment results.
        runtime_column: column containing runtime values.
        strategy_column: column identifying the strategy/solver.
        instance_column: column identifying the problem instance.
        flat_line_to: optional value to extend the curve horizontally to.
        linestyle: optional linestyle for the plot.
        colors: optional dictionary mapping strategy to color.
        ax: optional matplotlib Axes to plot into.

    Returns:
        The matplotlib Axes with the cactus plot.

    """
    if ax is None:
        ax = plt.gca()

    for strategy, group in data.groupby(strategy_column):
        # keep best runtime per instance
        runtimes = (
            group.groupby(instance_column)[runtime_column].min().dropna().sort_values().to_numpy()
        )

        if len(runtimes) == 0:
            continue

        solved = np.arange(1, len(runtimes) + 1)

        x = runtimes
        y = solved

        # extend curve horizontally if requested
        if flat_line_to is not None and runtimes[-1] < flat_line_to:
            x = np.append(x, flat_line_to)
            y = np.append(y, solved[-1])

        color = None
        if colors is not None:
            color = colors.get(strategy)

        ax.step(
            x,
            y,
            where="post",
            label=strategy,
            linestyle=linestyle,
            color=color,
        )

    ax.set_xlabel("runtime [s]")
    ax.set_ylabel("\\# solved instances")

    return ax


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
        poly: ShapelyPolygon = load_wkt(str(site))
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
            col = textcolor or color
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


def plot_annotated_solution(
    ax: plt.Axes,
    instance: AnnotatedInstance,
    solution: AnnotatedSolution | None = None,
    *,
    polygon_color: str = "lightblue",
    edge_color: str = "blue",
    trajectory_color: str = "black",
    point_color: str = "red",
) -> None:
    """Plot an AnnotatedInstance with an optional AnnotatedSolution overlay.

    Args:
        ax: Matplotlib axes to plot on.
        instance: The annotated instance (Shapely polygons).
        solution: Optional annotated solution (Shapely LineString trajectory).
        polygon_color: Fill color for polygons.
        edge_color: Edge color for polygons.
        trajectory_color: Color of the tour trajectory.
        point_color: Color of the visiting points on the trajectory.

    """
    for poly in instance.polygons:
        plot_polygon(poly, ax=ax, add_points=False, color=polygon_color, alpha=0.4)
        plot_polygon(poly, ax=ax, add_points=False, facecolor="none", edgecolor=edge_color)

    if solution is not None:
        plot_line(solution.trajectory, ax=ax, color=trajectory_color, linewidth=1.5)
        coords = list(solution.trajectory.coords)
        # Exclude the closing point for tours
        if len(coords) > 1 and coords[0] == coords[-1]:
            coords = coords[:-1]
        ax.scatter(
            [c[0] for c in coords],
            [c[1] for c in coords],
            color=point_color,
            s=30,
            zorder=5,
        )

    ax.set_aspect("equal")
    ax.autoscale_view()
