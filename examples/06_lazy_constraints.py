"""Example: using lazy constraints to add neighborhoods incrementally.

Demonstrates the lazy_callback mechanism. We start with only the 4 corner
polygons and lazily add the remaining polygons whenever the solver finds a
tour that does not yet cover them. This can speed up solving because the
solver works with smaller sequences in early iterations.

The lazy_callback is called whenever the solver finds a feasible solution
for the current set of constraints. If the tour does not yet cover all
required polygons, we add a missing one via context.add_lazy_site(), which
makes the solution infeasible again and forces the solver to continue.
"""

import matplotlib.pyplot as plt
from shapely.geometry import box
from shapely.plotting import plot_polygon
from tspn_bnb2 import AnnotatedInstance, solve_annotated_instance, to_native_polygon
from tspn_bnb2.misc.plotting import plot_annotated_solution

# --- Define all polygons as Shapely ---
# 4 corner polygons form the initial instance.
corner_polys = [box(0, 0, 1, 1), box(9, 0, 10, 1), box(9, 9, 10, 10), box(0, 9, 1, 10)]

# Additional polygons to add lazily — placed so the initial tour
# doesn't necessarily pass through them.
lazy_polys = [box(4, 4, 5, 5), box(2, 7, 3, 8), box(7, 2, 8, 3)]

# Convert lazy polygons to native types for add_lazy_site().
lazy_native = [to_native_polygon(p) for p in lazy_polys]

instance = AnnotatedInstance(polygons=corner_polys)


class LazyNeighborhoodAdder:
    """Adds uncovered neighborhoods one at a time when the solver finds a tour."""

    def __init__(self, native_polys):
        self.native_polys = native_polys
        self.added = [False] * len(native_polys)

    def __call__(self, context):
        traj = context.get_relaxed_solution().get_trajectory()
        for i, poly in enumerate(self.native_polys):
            if not self.added[i] and traj.distance_site(poly) > 0.001:
                context.add_lazy_site(poly)
                self.added[i] = True
                print(f"  Lazy: added polygon {i} (tour length so far: {traj.length():.2f})")
                return  # add one at a time to keep constraints tight


# --- Solve ---
lazy_adder = LazyNeighborhoodAdder(lazy_native)
print(
    f"Starting with {instance.num_polygons()} corner polygons, "
    f"{len(lazy_polys)} to be added lazily\n"
)

solution = solve_annotated_instance(
    instance,
    timelimit=60,
    lazy_callback=lazy_adder,
    eps=0.01,
)

num_added = sum(lazy_adder.added)
print("\nResults:")
print(f"  Tour length: {solution.upper_bound:.4f}")
print(f"  Lower bound: {solution.lower_bound:.4f}")
print(f"  Gap: {solution.relative_gap * 100:.2f}%")
print(f"  Optimal: {solution.is_optimal}")
print(f"  Lazy additions: {num_added}/{len(lazy_polys)}")

# --- Plot ---
fig, ax = plt.subplots(figsize=(6, 6))
plot_annotated_solution(ax, instance, solution)

# Overlay lazy polygons: orange = added by solver, gray = already covered
for i, poly in enumerate(lazy_polys):
    if lazy_adder.added[i]:
        plot_polygon(poly, ax=ax, add_points=False, color="orange", alpha=0.5)
        plot_polygon(poly, ax=ax, add_points=False, facecolor="none", edgecolor="darkorange", lw=2)
    else:
        plot_polygon(poly, ax=ax, add_points=False, color="lightgray", alpha=0.4)
        plot_polygon(poly, ax=ax, add_points=False, facecolor="none", edgecolor="gray", ls="--")

ax.set_title(f"Tour = {solution.upper_bound:.2f}  (orange: lazily added, gray: already covered)")
plt.tight_layout()
plt.savefig("06_lazy_constraints.png", dpi=150)
print("Saved plot to 06_lazy_constraints.png")
plt.show()
