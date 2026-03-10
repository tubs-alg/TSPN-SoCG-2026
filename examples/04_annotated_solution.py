"""Example demonstrating the use of AnnotatedInstance and AnnotatedSolution."""

from shapely.geometry import Polygon as ShapelyPolygon
from tspn_bnb2 import AnnotatedInstance, solve_annotated_instance

# Create some polygons
poly1 = ShapelyPolygon([(0, 0), (1, 0), (1, 1), (0, 1)])
poly2 = ShapelyPolygon([(2, 0), (3, 0), (3, 1), (2, 1)])
poly3 = ShapelyPolygon([(1, 2), (2, 2), (2, 3), (1, 3)])

# Create an annotated instance
instance = AnnotatedInstance(
    polygons=[poly1, poly2, poly3],
    meta={"name": "example_instance", "difficulty": "easy", "source": "manual_creation"},
)

print("Instance created with", instance.num_polygons(), "polygons")

# Solve the instance
solution = solve_annotated_instance(
    instance=instance,
    timelimit=30,
    branching="FarthestPoly",
    search="DfsBfs",
    node_simplification=True,
    # Add custom metadata
    experiment_id="demo_001",
    solver_version="1.0",
)

# Print solution information
print("\n=== Solution Results ===")
print(f"Lower bound: {solution.lower_bound:.4f}")
print(f"Upper bound: {solution.upper_bound:.4f}")
print(f"Gap: {solution.gap:.4f}")
print(f"Relative gap: {solution.relative_gap * 100:.2f}%")
print(f"Is optimal: {solution.is_optimal}")
print(f"Is tour: {solution.is_tour}")
print(f"Trajectory length: {solution.length:.4f}")
print(f"\nStatistics: {solution.statistics}")
print(f"\nMetadata: {solution.meta}")

# Serialize to JSON
json_str = solution.model_dump_json(indent=2)
print("\n=== Serialized Solution (first 500 chars) ===")
print(json_str[:500])

# The solution can be saved and loaded
with open("/tmp/solution.json", "w") as f:
    f.write(json_str)

print("\nSolution saved to /tmp/solution.json")

# Load it back
from tspn_bnb2 import AnnotatedSolution

with open("/tmp/solution.json") as f:
    loaded_solution = AnnotatedSolution.model_validate_json(f.read())

print(f"Loaded solution has trajectory length: {loaded_solution.length:.4f}")
