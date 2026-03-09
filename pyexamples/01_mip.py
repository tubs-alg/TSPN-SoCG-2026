"""Example: Solve TSPN using the MIP formulation with random convex polygons."""

from _utils import generate_random_instance
from matplotlib import pyplot as plt
from shapely.plotting import plot_line, plot_polygon
from tspn_bnb2.mip import solve_tspn_mip
from tspn_bnb2.operations import convert_to_tspn_instance

if __name__ == "__main__":
    # Generate a random instance
    instance = generate_random_instance(num_polygons=10, seed=1337, grid_size=10)
    tspn_instance = convert_to_tspn_instance(instance)
    print(f"Solving TSPN with {len(tspn_instance)} random convex polygons...")

    # Solve with MIP
    result = solve_tspn_mip(tspn_instance, time_limit=60, verbose=True, mip_gap=0.01)

    print(f"\nTour length: {result.upper_bound:.4f}")
    print(f"Lower bound: {result.lower_bound:.4f}")
    print(f"Gap: {result.gap:.6f}")
    print(f"Optimal: {result.is_optimal}")

    # Plot the result
    fig, ax = plt.subplots(figsize=(8, 8))

    # Plot polygons
    for polygon in instance.polygons:
        plot_polygon(polygon, ax=ax, add_points=False, color="lightblue", alpha=0.5)
        plot_polygon(polygon, ax=ax, add_points=False, facecolor="none", edgecolor="blue")

    # Plot the tour
    plot_line(result.trajectory, ax=ax, color="black", linewidth=2)

    # Plot visiting points
    coords = list(result.trajectory.coords)[:-1]  # Exclude closing point
    xs, ys = zip(*coords)
    ax.scatter(xs, ys, color="red", s=50, zorder=5)

    ax.set_aspect("equal")
    ax.set_title(f"TSPN MIP Solution (length={result.upper_bound:.2f})")

    plt.tight_layout()
    plt.show()
