"""Visualize Christofides warm start solutions for instances from the benchmark set.

This script:
1. Loads instances from instances_socg_simplified.zip
2. Generates Christofides-based warm start solutions (without solving the MIP)
3. Plots 9 random instances with their warm start tours
"""

import random
import zipfile
from pathlib import Path

import matplotlib.pyplot as plt
from matplotlib.patches import Polygon as MplPolygon
from tspn_bnb2.mip.warm_start import generate_christofides_warm_start
from tspn_bnb2.operations import convert_to_tspn_instance
from tspn_bnb2.schemas import AnnotatedInstance


def load_instances_from_zip(
    zip_path: Path, max_instances: int | None = None
) -> list[tuple[str, AnnotatedInstance]]:
    """Load all instances from a zip file.

    Args:
        zip_path: Path to the zip file containing JSON instance files.
        max_instances: Maximum number of instances to load (None for all).

    Returns:
        List of (instance_name, AnnotatedInstance) tuples.

    """
    instances = []
    with zipfile.ZipFile(zip_path, "r") as zf:
        instance_files = [name for name in zf.namelist() if name.endswith(".json")]

        for instance_path in instance_files:
            if max_instances is not None and len(instances) >= max_instances:
                break

            with zf.open(instance_path) as f:
                instance_json = f.read().decode("utf-8")
                instance = AnnotatedInstance.model_validate_json(instance_json)
                instance_name = Path(instance_path).stem
                instances.append((instance_name, instance))

    return instances


def plot_warm_start_solution(
    ax: plt.Axes,
    instance: AnnotatedInstance,
    instance_name: str,
) -> float:
    """Plot an instance with its Christofides warm start solution.

    Args:
        ax: Matplotlib axes to plot on.
        instance: The TSPN instance.
        instance_name: Name of the instance for the title.

    Returns:
        Tour length of the warm start solution.

    """
    # Convert to C++ instance and generate warm start
    cpp_instance = convert_to_tspn_instance(instance)
    solution = generate_christofides_warm_start(cpp_instance)

    # Plot polygons
    for poly in instance.polygons:
        vertices = list(poly.exterior.coords)

        patch = MplPolygon(
            vertices, facecolor="lightblue", edgecolor="blue", alpha=0.5, linewidth=1
        )
        ax.add_patch(patch)

    # Plot tour from trajectory
    coords = list(solution.trajectory.coords)
    xs = [p[0] for p in coords]
    ys = [p[1] for p in coords]
    ax.plot(xs, ys, color="black", linewidth=1.5, zorder=4)

    # Plot visiting points (exclude closing point)
    tour_points = coords[:-1] if coords[0] == coords[-1] else coords
    ax.scatter(
        [p[0] for p in tour_points],
        [p[1] for p in tour_points],
        color="red",
        s=30,
        zorder=5,
    )

    # Set title with tour length
    ax.set_title(f"{instance_name}\nL={solution.upper_bound:.2f}", fontsize=8)
    ax.set_aspect("equal")
    ax.tick_params(axis="both", which="major", labelsize=6)

    return solution.upper_bound


def main():
    # Path to instances
    zip_path = (
        Path(__file__).parent.parent / "evaluation" / "instances" / "instances_socg_simplified.zip"
    )

    if not zip_path.exists():
        print(f"Error: Instance file not found at {zip_path}")
        return

    print(f"Loading instances from {zip_path}...")
    all_instances = load_instances_from_zip(zip_path)
    print(f"Loaded {len(all_instances)} instances")

    # Select 9 random instances
    num_plots = 9
    seed = 42
    random.seed(seed)
    selected_instances = random.sample(all_instances, min(num_plots, len(all_instances)))

    print(f"\nGenerating warm start solutions for {len(selected_instances)} instances...")

    # Create 3x3 subplot grid
    _fig, axes = plt.subplots(3, 3, figsize=(5, 5))
    axes = axes.flatten()

    total_length = 0.0
    for idx, (instance_name, instance) in enumerate(selected_instances):
        print(f"  [{idx + 1}/{len(selected_instances)}] {instance_name}...")
        tour_length = plot_warm_start_solution(axes[idx], instance, instance_name)
        total_length += tour_length

    avg_length = total_length / len(selected_instances)
    print(f"\nAverage tour length: {avg_length:.2f}")

    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    main()
