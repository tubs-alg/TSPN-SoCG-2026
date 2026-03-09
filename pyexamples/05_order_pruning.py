"""Example: Full order-pruning pipeline for Branch and Bound.

Demonstrates the complete flow:
1. Generate a random polygon instance
2. Simplify geometries (remove holes, fill convex hulls, remove supersites)
3. Compute order annotations (convex hull ordering + overlap detection)
4. Solve with Branch and Bound using OrderFiltering and OrderRoot
5. Plot the instance and solution side by side
"""

import matplotlib.pyplot as plt
from matplotlib.patches import Polygon as MplPolygon
from shapely.plotting import plot_line

from _utils import generate_random_instance
from tspn_bnb2.operations import simplify_annotated_instance, solve_annotated_instance
from tspn_bnb2.order_annotation import add_order_annotations


def plot_instance(ax: plt.Axes, instance, title: str = "Instance") -> None:
    """Plot polygons colored by hull order annotation."""
    n_annotated = len(instance.annotations)
    cmap = plt.cm.hsv

    for idx, poly in enumerate(instance.polygons):
        ann = instance.get_annotation(idx)
        if ann is not None and ann.hull_index >= 0:
            color = cmap(ann.hull_index / max(n_annotated, 1))
            alpha = 0.6
        else:
            color = "lightgray"
            alpha = 0.4

        patch = MplPolygon(
            list(poly.exterior.coords),
            facecolor=color,
            edgecolor="black",
            alpha=alpha,
            linewidth=0.8,
        )
        ax.add_patch(patch)

        # Label with hull order
        cx, cy = poly.centroid.x, poly.centroid.y
        label = str(ann.hull_index) if ann is not None and ann.hull_index >= 0 else ""
        ax.text(cx, cy, label, ha="center", va="center", fontsize=6, fontweight="bold")

    ax.set_aspect("equal")
    ax.autoscale_view()
    ax.set_title(title, fontsize=10)


def plot_solution(ax: plt.Axes, instance, solution, title: str = "Solution") -> None:
    """Plot the instance with the solution trajectory overlaid."""
    for poly in instance.polygons:
        patch = MplPolygon(
            list(poly.exterior.coords),
            facecolor="lightblue",
            edgecolor="blue",
            alpha=0.4,
            linewidth=0.8,
        )
        ax.add_patch(patch)

    # Plot trajectory
    plot_line(solution.trajectory, ax=ax, color="black", linewidth=1.5, zorder=4)

    # Plot visiting points
    coords = list(solution.trajectory.coords)
    tour_points = coords[:-1] if len(coords) > 1 and coords[0] == coords[-1] else coords
    ax.scatter(
        [p[0] for p in tour_points],
        [p[1] for p in tour_points],
        color="red",
        s=30,
        zorder=5,
    )

    ax.set_aspect("equal")
    ax.autoscale_view()
    ax.set_title(title, fontsize=10)


def main():
    # --- Step 1: Generate a random instance ---
    print("Step 1: Generating random instance...")
    instance = generate_random_instance(num_polygons=40, seed=42, grid_size=6, avg_radius=0.6)
    print(f"  {instance.num_polygons()} polygons")

    # --- Step 2: Simplify ---
    print("Step 2: Simplifying geometries...")
    simplified = simplify_annotated_instance(
        instance, remove_holes=True, convex_hull_fill=True, remove_supersites=True
    )
    print(f"  {simplified.num_polygons()} polygons after simplification")

    # --- Step 3: Add order annotations ---
    print("Step 3: Computing order annotations...")
    annotated = add_order_annotations(simplified)
    hull_polys = annotated.hull_polygons()
    print(f"  {len(hull_polys)} polygons on convex hull")
    for idx in hull_polys:
        ann = annotated.get_annotation(idx)
        overlaps = ann.intersections if ann else []
        print(f"    polygon {idx}: hull_index={ann.hull_index}, overlaps={overlaps}")

    # --- Step 4: Solve with order pruning ---
    print("Step 4: Solving with OrderRoot + OrderFiltering...")
    solution = solve_annotated_instance(
        annotated,
        timelimit=60,
        root="OrderRoot",
        rules=["OrderFiltering"],
        search="DfsBfs",
        node_simplification=True,
        decomposition_branch=True,
        eps=0.01,
    )
    print(f"  Tour length: {solution.upper_bound:.4f}")
    print(f"  Lower bound: {solution.lower_bound:.4f}")
    print(f"  Gap: {solution.relative_gap * 100:.2f}%")
    print(f"  Optimal: {solution.is_optimal}")
    print(f"  Statistics: {solution.statistics}")

    # --- Step 5: Solve without order pruning for comparison ---
    print("\nStep 5: Solving WITHOUT order pruning for comparison...")
    solution_no_order = solve_annotated_instance(
        annotated,
        timelimit=60,
        root="LongestEdgePlusFurthestSite",
        rules=[],
        search="DfsBfs",
        node_simplification=True,
        decomposition_branch=True,
        eps=0.01,
    )
    print(f"  Tour length: {solution_no_order.upper_bound:.4f}")
    print(f"  Statistics: {solution_no_order.statistics}")

    # --- Step 6: Plot ---
    print("\nStep 6: Plotting...")
    fig, axes = plt.subplots(1, 3, figsize=(15, 5))

    plot_instance(axes[0], annotated, title="Order Annotations")

    plot_solution(
        axes[1],
        annotated,
        solution,
        title=f"With OrderPruning (L={solution.upper_bound:.2f})",
    )

    plot_solution(
        axes[2],
        annotated,
        solution_no_order,
        title=f"Without OrderPruning (L={solution_no_order.upper_bound:.2f})",
    )

    plt.tight_layout()
    plt.savefig("05_order_pruning.png", dpi=150)
    print("  Saved to 05_order_pruning.png")
    plt.show()


if __name__ == "__main__":
    main()
