"""Minimal example: solve a 5x5 grid of unit squares.

Creates 25 axis-aligned unit squares on a grid, then finds the shortest
tour visiting each square. This is the simplest way to use the solver.
"""

import matplotlib.pyplot as plt
from shapely.geometry import box
from tspn_bnb2 import AnnotatedInstance, solve_annotated_instance
from tspn_bnb2.misc.plotting import plot_annotated_solution

# Create a 5x5 grid of unit squares
polygons = [box(x, y, x + 1, y + 1) for x in range(5) for y in range(5)]
instance = AnnotatedInstance(polygons=polygons)
print(f"Instance: {instance.num_polygons()} unit squares on a 5x5 grid")

# Solve
solution = solve_annotated_instance(instance, timelimit=60)

# Results
print(f"Tour length: {solution.upper_bound:.4f}")
print(f"Lower bound: {solution.lower_bound:.4f}")
print(f"Gap: {solution.relative_gap * 100:.2f}%")
print(f"Optimal: {solution.is_optimal}")

# Plot
fig, ax = plt.subplots(figsize=(6, 6))
plot_annotated_solution(ax, instance, solution)
ax.set_title(f"Tour length = {solution.upper_bound:.2f}")
plt.tight_layout()
plt.savefig("00_simple.png", dpi=150)
print("Saved plot to 00_simple.png")
plt.show()
