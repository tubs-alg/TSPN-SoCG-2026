r"""TSPN MIP Solver for Non-Convex Polygons using Gurobi.

Extends the convex MIP formulation to handle non-convex polygons via convex
decomposition with indicator variables. For each non-convex polygon i, we:
1. Decompose it into convex pieces P_i = {P_i1, P_i2, ..., P_ik}
2. Add binary indicator variables z_ij for each piece j
3. Ensure exactly one piece is active: sum_j z_ij = 1
4. Use Big-M constraints: A_ij q_i ≤ b_ij + M(1 - z_ij)

The formulation becomes:

    minimize:   sum_{i<j} ξ_ij ||q_j - q_i||_2                              (8)

    subject to: sum_{j<i} ξ_ji + sum_{j>i} ξ_ij = 2      ∀i ∈ V             (9)
                sum_{i∈S} (sum_{j∈V\S,j<i} ξ_ji + sum_{j∈V\S,j>i} ξ_ij) ≥ 2
                                                ∀S ⊂ V\{1}, |S| ≥ 3        (10)
                A_ijk q_i ≤ b_ijk + M(1 - z_ij)     ∀i ∈ V, j ∈ P_i, k    (11')
                sum_j z_ij = 1                       ∀i ∈ V                 (14)
                ξ_ij ∈ {0,1}                        ∀i,j ∈ V, j > i        (12)
                z_ij ∈ {0,1}                        ∀i ∈ V, j ∈ P_i        (15)
                q_i ∈ ℝ^m                           ∀i ∈ V                 (13)

where P_i is the set of convex pieces for polygon i.
"""

import gurobipy as gp
import numpy as np
from gurobipy import GRB

from tspn_bnb2.core import Instance
from tspn_bnb2.schemas import AnnotatedInstance, AnnotatedSolution

from .solver import TSPNMIPSolverBase
from .utils import extract_vertices_from_ring, polygon_to_halfspaces


class TSPNNonConvexMIPSolver(TSPNMIPSolverBase):
    """MIP solver for TSPN with non-convex polygon neighborhoods.

    Uses convex decomposition with indicator variables to handle non-convex
    polygons. For each non-convex polygon, binary variables select which
    convex piece the visiting point must lie in.
    """

    def __init__(
        self,
        instance: Instance,
        *,
        time_limit: float | None = None,
        mip_gap: float = 1e-4,
        threads: int = 0,
        verbose: bool = True,
        big_m: float | None = None,
    ) -> None:
        """Initialize the TSPN Non-Convex MIP solver.

        Args:
            instance: TSPN Instance with polygon neighborhoods (can be non-convex).
            time_limit: Maximum solve time in seconds (None for unlimited).
            mip_gap: Target MIP gap (default 0.01%).
            threads: Number of threads (0 = automatic).
            verbose: Whether to print Gurobi output.
            big_m: Big-M constant for linearization (auto-computed if None).

        """
        if isinstance(instance, AnnotatedInstance):
            raise TypeError(
                "TSPNNonConvexMIPSolver requires a C++ Instance to access "
                "convex decomposition. Use TSPNMIPSolver for AnnotatedInstance."
            )

        super().__init__(
            instance,
            time_limit=time_limit,
            mip_gap=mip_gap,
            threads=threads,
            verbose=verbose,
            big_m=big_m,
        )

        self._n = len(instance)

        # pieces[i] = list of (A, b) halfspace representations for convex pieces
        self.pieces: list[list[tuple[np.ndarray, np.ndarray]]] = []
        self._extract_decompositions()

        # Piece indicator variables z[i, j] = 1 if piece j is active for polygon i
        self._z: dict[tuple[int, int], gp.Var] | None = None

    @property
    def n(self) -> int:
        """Number of neighborhoods."""
        return self._n

    def _extract_decompositions(self) -> None:
        """Extract convex decomposition for each polygon."""
        for i in range(self._n):
            geometry = self.instance[i]
            decomposition = geometry.decomposition()

            if len(decomposition) == 0:
                # Polygon is already convex, use the original polygon
                polygon = geometry.definition()
                outer_ring = polygon.outer()
                vertices = extract_vertices_from_ring(outer_ring)
                halfspaces = polygon_to_halfspaces(vertices)
                if halfspaces is None:
                    raise ValueError(f"Polygon {i} is degenerate (fewer than 3 unique vertices)")
                self.pieces.append([halfspaces])
            else:
                # Non-convex: use the decomposition pieces
                piece_halfspaces = []
                for piece in decomposition:
                    # Each piece is a Ring (convex polygon) or Polygon
                    try:
                        # Try as Polygon first (has .outer())
                        outer = piece.outer()
                        vertices = extract_vertices_from_ring(outer)
                    except AttributeError:
                        # It's a Ring - iterate directly
                        vertices = extract_vertices_from_ring(piece)
                    halfspaces = polygon_to_halfspaces(vertices)
                    # Skip degenerate pieces (e.g., from duplicate vertices)
                    if halfspaces is not None:
                        piece_halfspaces.append(halfspaces)
                if not piece_halfspaces:
                    raise ValueError(f"Polygon {i} decomposition has no valid convex pieces")
                self.pieces.append(piece_halfspaces)

    def _compute_big_m(self) -> float:
        """Compute a valid big-M based on the problem geometry."""
        if self.big_m is not None:
            return self.big_m

        all_bounds = []
        for piece_list in self.pieces:
            for _A, b in piece_list:
                max_coord = np.max(np.abs(b)) * 2
                all_bounds.append(max_coord)

        if not all_bounds:
            raise ValueError("No piece bounds found. Cannot compute big-M.")

        max_bound = max(all_bounds)
        return max_bound * 2 * np.sqrt(2)

    def _add_neighborhood_constraints(
        self, model: gp.Model, q: dict[tuple[int, int], gp.Var], M: float
    ) -> None:
        """Add neighborhood constraints with Big-M for piece selection.

        For each polygon i with pieces P_i:
        - z_ij ∈ {0,1} for each piece j
        - sum_j z_ij = 1 (exactly one piece active)
        - A_ijk q_i ≤ b_ijk + M(1 - z_ij) for each halfspace k of piece j
        """
        # Add piece indicator variables
        z = {}
        for i in range(self.n):
            num_pieces = len(self.pieces[i])
            for j in range(num_pieces):
                z[i, j] = model.addVar(vtype=GRB.BINARY, name=f"z_{i}_{j}")

        model.update()

        # Constraint: exactly one piece active per polygon
        for i in range(self.n):
            num_pieces = len(self.pieces[i])
            model.addConstr(
                gp.quicksum(z[i, j] for j in range(num_pieces)) == 1,
                name=f"one_piece_{i}",
            )

        # Constraint: A_ijk q_i ≤ b_ijk + M(1 - z_ij) for each piece j, halfspace k
        for i in range(self.n):
            for j, (A, b) in enumerate(self.pieces[i]):
                for k in range(len(b)):
                    model.addConstr(
                        A[k, 0] * q[i, 0] + A[k, 1] * q[i, 1] <= b[k] + M * (1 - z[i, j]),
                        name=f"poly_{i}_piece_{j}_hs_{k}",
                    )

        self._z = z

    def _get_solver_name(self) -> str:
        return "mip_nonconvex"

    def _point_in_piece(self, point: tuple[float, float], A: np.ndarray, b: np.ndarray) -> bool:
        """Check if a point satisfies all halfspace constraints of a piece."""
        x, y = point
        return all(A[k, 0] * x + A[k, 1] * y <= b[k] + 1e-6 for k in range(len(b)))

    def _find_containing_piece(self, poly_idx: int, point: tuple[float, float]) -> int | None:
        """Find which piece of a polygon contains the given point.

        Returns the piece index, or None if point is not in any piece.
        """
        for j, (A, b) in enumerate(self.pieces[poly_idx]):
            if self._point_in_piece(point, A, b):
                return j
        return None

    def _project_to_nearest_piece(
        self, poly_idx: int, point: tuple[float, float]
    ) -> tuple[tuple[float, float], int]:
        """Project a point to the nearest piece of a polygon.

        Returns (projected_point, piece_index).
        """
        from shapely.geometry import Point as ShapelyPoint
        from shapely.geometry import Polygon as ShapelyPolygon

        original_point = ShapelyPoint(point)
        best_dist = float("inf")
        best_point = point
        best_piece = 0

        # Get piece geometries from the instance decomposition
        geometry = self.instance[poly_idx]
        decomposition = geometry.decomposition()

        if len(decomposition) == 0:
            # Convex polygon - project to polygon boundary
            polygon = geometry.definition()
            outer = polygon.outer()
            vertices = [(p.x, p.y) for p in outer]
            shapely_poly = ShapelyPolygon(vertices)
            if shapely_poly.contains(original_point):
                return point, 0
            nearest = shapely_poly.exterior.interpolate(
                shapely_poly.exterior.project(original_point)
            )
            return (nearest.x, nearest.y), 0

        # Non-convex: find nearest piece
        for j, piece in enumerate(decomposition):
            try:
                vertices = [(p.x, p.y) for p in piece]
            except TypeError:
                # piece might be a Ring iterable
                vertices = [(p.x, p.y) for p in piece]

            shapely_piece = ShapelyPolygon(vertices)
            if shapely_piece.contains(original_point):
                return point, j

            # Find nearest point on this piece
            nearest = shapely_piece.exterior.interpolate(
                shapely_piece.exterior.project(original_point)
            )
            dist = original_point.distance(nearest)
            if dist < best_dist:
                best_dist = dist
                best_point = (nearest.x, nearest.y)
                best_piece = j

        return best_point, best_piece

    def set_warm_start(self, solution: AnnotatedSolution) -> None:
        """Set warm start values from an AnnotatedSolution including piece selection.

        For non-convex polygons, if a point is not inside any piece (e.g., in
        the concave region), it is projected to the nearest piece boundary.

        Args:
            solution: An AnnotatedSolution with trajectory and ordering in statistics.

        """
        if self._model is None:
            self._build_model()

        assert self._q is not None
        assert self._xi is not None
        assert self._z is not None

        # Extract tour points from trajectory (exclude closing point)
        coords = list(solution.trajectory.coords)
        if len(coords) > 1 and coords[0] == coords[-1]:
            coords = coords[:-1]
        tour_points = [(x, y) for x, y in coords]

        # Get ordering from statistics, or infer from number of points
        if "ordering" in solution.statistics:
            tour_order = solution.statistics["ordering"]
        else:
            # Assume points are in polygon index order
            tour_order = list(range(len(tour_points)))

        # Set position and piece variables
        for k, (x, y) in enumerate(tour_points):
            poly_idx = tour_order[k]

            # Find which piece contains this point, or project if needed
            piece_idx = self._find_containing_piece(poly_idx, (x, y))
            px, py = x, y
            if piece_idx is None:
                # Point is in concave region - project to nearest piece
                (px, py), piece_idx = self._project_to_nearest_piece(poly_idx, (x, y))

            # Set position variables
            self._q[poly_idx, 0].Start = px
            self._q[poly_idx, 1].Start = py

            # Set piece indicator variables
            for j in range(len(self.pieces[poly_idx])):
                self._z[poly_idx, j].Start = 1 if j == piece_idx else 0

        # Set edge variables based on tour order
        n = len(tour_order)
        tour_edges = set()
        for k in range(n):
            i, j = tour_order[k], tour_order[(k + 1) % n]
            if i > j:
                i, j = j, i
            tour_edges.add((i, j))

        for (i, j), var in self._xi.items():
            var.Start = 1 if (i, j) in tour_edges else 0

    def _get_extra_statistics(self) -> dict:
        """Return statistics about the decomposition."""
        stats = {
            "num_decomposition_pieces": sum(len(p) for p in self.pieces),
        }

        # Extract which pieces were selected
        if self._z is not None:
            selected_pieces = {}
            for i in range(self.n):
                for j in range(len(self.pieces[i])):
                    if self._z[i, j].X > 0.5:
                        selected_pieces[i] = j
                        break
            stats["selected_pieces"] = selected_pieces

        return stats


def solve_tspn_nonconvex_mip(
    instance: Instance,
    *,
    time_limit: float | None = None,
    mip_gap: float = 1e-4,
    threads: int = 0,
    verbose: bool = True,
    big_m: float | None = None,
    initial_solution: AnnotatedSolution | None = None,
) -> AnnotatedSolution:
    """Solve a TSPN instance with non-convex polygons using the MIP formulation.

    This solver extends the standard MIP formulation to handle non-convex
    polygons by decomposing them into convex pieces and using binary indicator
    variables to select which piece the visiting point lies in.

    Args:
        instance: TSPN Instance with polygon neighborhoods (can be non-convex).
        time_limit: Maximum solve time in seconds (None for unlimited).
        mip_gap: Target MIP gap (default 0.01%).
        threads: Number of threads (0 = automatic).
        verbose: Whether to print Gurobi output.
        big_m: Big-M constant for linearization (auto-computed if None).
        initial_solution: Optional warm start solution (e.g., from
            generate_christofides_warm_start).

    Returns:
        AnnotatedSolution containing the optimal tour and statistics.

    """
    solver = TSPNNonConvexMIPSolver(
        instance,
        time_limit=time_limit,
        mip_gap=mip_gap,
        threads=threads,
        verbose=verbose,
        big_m=big_m,
    )
    if initial_solution is not None:
        solver.set_warm_start(initial_solution)
    return solver.solve()
