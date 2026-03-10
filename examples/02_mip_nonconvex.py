"""Example: Solve TSPN with non-convex polygons using the MIP formulation."""

from _utils import generate_random_instance
from matplotlib import pyplot as plt
from matplotlib.patches import Polygon as MplPolygon
from shapely.geometry import Point, Polygon
from tspn_bnb2 import AnnotatedInstance, Instance
from tspn_bnb2.mip import generate_christofides_warm_start, solve_tspn_nonconvex_mip
from tspn_bnb2.operations import convert_to_tspn_instance


def create_nonconvex_instance() -> Instance:
    """Create a TSPN instance with non-convex polygons.

    Returns:
        Instance with L-shaped and star-shaped polygons.

    """
    # L-shaped polygon (non-convex)
    l_shape = Polygon(
        [
            Point(0, 0),
            Point(3, 0),
            Point(3, 1),
            Point(1, 1),
            Point(1, 3),
            Point(0, 3),
        ]
    )

    # Star-shaped polygon (non-convex)
    star = Polygon(
        [
            Point(7, 1),
            Point(7.4, 2),
            Point(8.5, 2),
            Point(7.6, 2.6),
            Point(8, 3.5),
            Point(7, 2.9),
            Point(6, 3.5),
            Point(6.4, 2.6),
            Point(5.5, 2),
            Point(6.6, 2),
        ]
    )

    # T-shaped polygon (non-convex)
    t_shape = Polygon(
        [
            Point(2, 6),
            Point(6, 6),
            Point(6, 7),
            Point(4.5, 7),
            Point(4.5, 9),
            Point(3.5, 9),
            Point(3.5, 7),
            Point(2, 7),
        ]
    )

    rand_instance = generate_random_instance(num_polygons=6, seed=42, grid_size=12)

    polygons = rand_instance.polygons
    polygons.append(l_shape)
    polygons.append(star)
    polygons.append(t_shape)
    inst = AnnotatedInstance(polygons=polygons, meta=rand_instance.meta)

    return convert_to_tspn_instance(inst)


if __name__ == "__main__":
    instance = create_nonconvex_instance()

    print("\nPolygon convexity:")
    for i in range(len(instance)):
        geo = instance[i]
        decomp = geo.decomposition()
        num_pieces = len(decomp) if decomp else 1
        print(f"  Polygon {i}: convex={geo.is_convex()}, decomposition pieces={num_pieces}")

    # Solve with non-convex MIP
    initial_solution = generate_christofides_warm_start(instance)
    result = solve_tspn_nonconvex_mip(
        instance, time_limit=60, verbose=True, mip_gap=0.01, initial_solution=initial_solution
    )

    print(f"\nTour length: {result.upper_bound:.4f}")
    print(f"Lower bound: {result.lower_bound:.4f}")
    print(f"Gap: {result.gap:.6f}")
    print(f"Optimal: {result.is_optimal}")
    print(f"Total decomposition pieces: {result.statistics['num_decomposition_pieces']}")
    print(f"Selected pieces: {result.statistics['selected_pieces']}")

    # Plot the result
    fig, ax = plt.subplots(figsize=(4, 4))

    # Plot polygons
    for i in range(len(instance)):
        geo = instance[i]
        polygon = geo.definition()
        outer = polygon.outer()
        vertices = [(p.x, p.y) for p in outer]

        # Plot polygon
        patch = MplPolygon(vertices, facecolor="lightblue", edgecolor="blue", alpha=0.5)
        ax.add_patch(patch)

        # Plot decomposition pieces with different colors
        decomp = geo.decomposition()
        if decomp:
            colors = ["#ff9999", "#99ff99", "#9999ff", "#ffff99", "#ff99ff"]
            for j, piece in enumerate(decomp):
                piece_vertices = [(p.x, p.y) for p in piece]
                color = colors[j % len(colors)]
                piece_patch = MplPolygon(
                    piece_vertices, facecolor="none", edgecolor=color, linewidth=1, linestyle="--"
                )
                ax.add_patch(piece_patch)

    # Plot the tour
    coords = list(result.trajectory.coords)
    xs, ys = zip(*coords)
    ax.plot(xs, ys, color="black", linewidth=2, zorder=4)

    # Plot visiting points
    xs, ys = zip(*coords[:-1])  # Exclude closing point
    ax.scatter(xs, ys, color="red", s=80, zorder=5)

    # Add labels
    for i, (x, y) in enumerate(coords[:-1]):
        ax.annotate(str(i), (x, y), textcoords="offset points", xytext=(5, 5), fontsize=10)

    # Plot the tour
    coords = list(initial_solution.trajectory.coords)
    xs, ys = zip(*coords)
    ax.plot(xs, ys, color="gray", linewidth=2, linestyle="--", zorder=4)

    ax.set_aspect("equal")
    ax.set_title(f"TSPN Non-Convex MIP Solution (length={result.upper_bound:.2f})")
    ax.set_xlabel("x")
    ax.set_ylabel("y")

    plt.tight_layout()
    plt.show()
