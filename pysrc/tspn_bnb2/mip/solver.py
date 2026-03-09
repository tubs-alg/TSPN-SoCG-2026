r"""TSPN MIP Solver using Gurobi.

Implements the following MISOCP formulation:

    minimize:   sum_{i<j} ξ_ij ||q_j - q_i||_2                              (8)

    subject to: sum_{j<i} ξ_ji + sum_{j>i} ξ_ij = 2      ∀i ∈ V             (9)
                sum_{i∈S} (sum_{j∈V\S,j<i} ξ_ji + sum_{j∈V\S,j>i} ξ_ij) ≥ 2
                                                ∀S ⊂ V\{1}, |S| ≥ 3        (10)
                A_i q_i ≤ b_i                            ∀i ∈ V            (11)
                ξ_ij ∈ {0,1}                             ∀i,j ∈ V, j > i   (12)
                q_i ∈ ℝ^m                                ∀i ∈ V            (13)

where:
    - V = {1, ..., n} is the set of neighborhoods (vertices)
    - ξ_ij are binary edge selection variables
    - q_i are the visiting points within each neighborhood
    - A_i, b_i define the polygon neighborhood as a set of linear inequalities
"""

from abc import ABC, abstractmethod

import gurobipy as gp
import numpy as np
from gurobipy import GRB
from shapely import LineString
from shapely.geometry import Polygon as ShapelyPolygon

from tspn_bnb2.core import Instance
from tspn_bnb2.mip.callback import SubtourCallback
from tspn_bnb2.mip.utils import extract_vertices_from_ring, polygon_to_halfspaces
from tspn_bnb2.schemas import AnnotatedInstance, AnnotatedSolution


class TSPNMIPSolverBase(ABC):
    """Base class for TSPN MIP solvers with shared functionality."""

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
        """Initialize the TSPN MIP solver base.

        Args:
            instance: TSPN Instance or AnnotatedInstance with polygon neighborhoods.
            time_limit: Maximum solve time in seconds (None for unlimited).
            mip_gap: Target MIP gap (default 0.01%).
            threads: Number of threads (0 = automatic).
            verbose: Whether to print Gurobi output.
            big_m: Big-M constant for linearization (auto-computed if None).

        """
        self.instance = instance
        self.time_limit = time_limit
        self.mip_gap = mip_gap
        self.threads = threads
        self.verbose = verbose
        self.big_m = big_m

        self._model: gp.Model | None = None
        self._q: dict[tuple[int, int], gp.Var] | None = None
        self._xi: dict[tuple[int, int], gp.Var] | None = None
        self._d: dict[tuple[int, int], gp.Var] | None = None
        self._callback: SubtourCallback | None = None

    @property
    @abstractmethod
    def n(self) -> int:
        """Number of neighborhoods."""

    @abstractmethod
    def _compute_big_m(self) -> float:
        """Compute a valid big-M based on the problem geometry."""

    @abstractmethod
    def _add_neighborhood_constraints(
        self, model: gp.Model, q: dict[tuple[int, int], gp.Var], M: float
    ) -> None:
        """Add neighborhood containment constraints to the model."""

    @abstractmethod
    def _get_solver_name(self) -> str:
        """Return the solver name for statistics."""

    def _get_extra_statistics(self) -> dict:
        """Return any additional solver-specific statistics."""
        return {}

    def _setup_model(self) -> gp.Model:
        """Create and configure the Gurobi model."""
        model = gp.Model("TSPN")
        model.Params.OutputFlag = 1 if self.verbose else 0
        model.Params.LazyConstraints = 1

        if self.time_limit is not None:
            model.Params.TimeLimit = self.time_limit
        model.Params.MIPGap = self.mip_gap
        if self.threads > 0:
            model.Params.Threads = self.threads

        return model

    def _add_position_variables(self, model: gp.Model) -> dict[tuple[int, int], gp.Var]:
        """Add position variables q_i ∈ ℝ^2."""
        q = {}
        for i in range(self.n):
            q[i, 0] = model.addVar(lb=-GRB.INFINITY, ub=GRB.INFINITY, name=f"q_{i}_x")
            q[i, 1] = model.addVar(lb=-GRB.INFINITY, ub=GRB.INFINITY, name=f"q_{i}_y")
        return q

    def _add_edge_variables(
        self, model: gp.Model
    ) -> tuple[dict[tuple[int, int], gp.Var], dict[tuple[int, int], gp.Var]]:
        """Add edge and distance variables."""
        xi = {}  # Binary edge variables ξ_ij for i < j
        d = {}  # Auxiliary distance variables d_ij

        for i in range(self.n):
            for j in range(i + 1, self.n):
                xi[i, j] = model.addVar(vtype=GRB.BINARY, name=f"xi_{i}_{j}")
                d[i, j] = model.addVar(lb=0, name=f"d_{i}_{j}")

        return xi, d

    def _add_degree_constraints(self, model: gp.Model, xi: dict[tuple[int, int], gp.Var]) -> None:
        """Add degree constraints: each vertex has exactly 2 incident edges."""
        for i in range(self.n):
            edges = [xi[j, i] for j in range(i)]
            edges.extend(xi[i, j] for j in range(i + 1, self.n))
            model.addConstr(gp.quicksum(edges) == 2, name=f"degree_{i}")

    def _add_distance_linearization(
        self,
        model: gp.Model,
        q: dict[tuple[int, int], gp.Var],
        xi: dict[tuple[int, int], gp.Var],
        d: dict[tuple[int, int], gp.Var],
        M: float,
    ) -> None:
        """Add linearization constraints for ξ_ij * ||q_j - q_i||_2."""
        for i in range(self.n):
            for j in range(i + 1, self.n):
                diff_x = model.addVar(lb=-GRB.INFINITY, name=f"diff_{i}_{j}_x")
                diff_y = model.addVar(lb=-GRB.INFINITY, name=f"diff_{i}_{j}_y")

                model.addConstr(diff_x == q[j, 0] - q[i, 0], name=f"diff_x_{i}_{j}")
                model.addConstr(diff_y == q[j, 1] - q[i, 1], name=f"diff_y_{i}_{j}")

                norm_ij = model.addVar(lb=0, name=f"norm_{i}_{j}")
                model.addGenConstrNorm(norm_ij, [diff_x, diff_y], 2.0, name=f"soc_{i}_{j}")

                # d_ij <= M * ξ_ij
                model.addConstr(d[i, j] <= M * xi[i, j], name=f"d_upper_{i}_{j}")

                # d_ij >= norm_ij - M*(1 - ξ_ij)
                model.addConstr(d[i, j] >= norm_ij - M * (1 - xi[i, j]), name=f"d_lower_{i}_{j}")

    def _build_model(self) -> None:
        """Build the Gurobi model."""
        M = self._compute_big_m()

        model = self._setup_model()
        q = self._add_position_variables(model)
        xi, d = self._add_edge_variables(model)

        model.update()

        # Objective: minimize sum of d_ij
        model.setObjective(gp.quicksum(d.values()), GRB.MINIMIZE)

        self._add_degree_constraints(model, xi)
        self._add_neighborhood_constraints(model, q, M)
        self._add_distance_linearization(model, q, xi, d, M)

        self._model = model
        self._q = q
        self._xi = xi
        self._d = d
        self._callback = SubtourCallback(self.n, xi)

    def set_warm_start(self, solution: AnnotatedSolution) -> None:
        """Set warm start values from an AnnotatedSolution.

        Extracts tour points and ordering from the solution's trajectory
        and statistics, then sets MIP variable start values.

        Args:
            solution: An AnnotatedSolution with trajectory and ordering in statistics.

        """
        if self._model is None:
            self._build_model()

        assert self._q is not None
        assert self._xi is not None

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

        # Set position variables (tour_points are in tour order)
        for k, (x, y) in enumerate(tour_points):
            poly_idx = tour_order[k]
            self._q[poly_idx, 0].Start = x
            self._q[poly_idx, 1].Start = y

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

    def _extract_tour_order(self, edges: list[tuple[int, int]]) -> list[int]:
        """Extract ordered tour from edge list."""
        if not edges:
            return []

        adj: dict[int, list[int]] = {i: [] for i in range(self.n)}
        for i, j in edges:
            adj[i].append(j)
            adj[j].append(i)

        tour = [0]
        visited = {0}
        current = 0

        while len(tour) < self.n:
            for neighbor in adj[current]:
                if neighbor not in visited:
                    tour.append(neighbor)
                    visited.add(neighbor)
                    current = neighbor
                    break
            else:
                break

        return tour

    def solve(self) -> AnnotatedSolution:
        """Solve the TSPN MIP.

        Returns:
            AnnotatedSolution containing the optimal tour and statistics.

        Raises:
            RuntimeError: If no feasible solution is found.

        """
        if self._model is None:
            self._build_model()

        assert self._model is not None
        assert self._q is not None
        assert self._xi is not None
        assert self._callback is not None

        self._model.optimize(self._callback)

        status = self._model.Status
        if status not in (GRB.OPTIMAL, GRB.TIME_LIMIT, GRB.SOLUTION_LIMIT):
            raise RuntimeError(f"Optimization failed with status {status}")

        if self._model.SolCount == 0:
            raise RuntimeError("No feasible solution found")

        # Extract solution
        q_vals = np.zeros((self.n, 2))
        for i in range(self.n):
            q_vals[i, 0] = self._q[i, 0].X
            q_vals[i, 1] = self._q[i, 1].X

        tour_edges = []
        for (i, j), var in self._xi.items():
            if var.X > 0.5:
                tour_edges.append((i, j))

        tour_order = self._extract_tour_order(tour_edges)

        # Create trajectory with correct order (closed tour)
        ordered_points = q_vals[tour_order]
        coords = [(p[0], p[1]) for p in ordered_points]
        coords.append(coords[0])  # Close the tour
        trajectory = LineString(coords)

        upper_bound = self._model.ObjVal
        lower_bound = self._model.ObjBound
        gap = upper_bound - lower_bound
        is_optimal = status == GRB.OPTIMAL

        statistics = {
            "runtime": self._model.Runtime,
            "num_subtour_cuts": self._callback.num_cuts,
            "gurobi_status": status,
            "mip_gap": self._model.MIPGap,
            "solver": self._get_solver_name(),
        }
        statistics.update(self._get_extra_statistics())

        return AnnotatedSolution(
            trajectory=trajectory,
            lower_bound=lower_bound,
            upper_bound=upper_bound,
            is_optimal=is_optimal,
            gap=gap,
            statistics=statistics,
            is_tour=True,
        )


class TSPNMIPSolver(TSPNMIPSolverBase):
    """MIP solver for TSPN with convex polygon neighborhoods."""

    def __init__(
        self,
        instance: Instance | AnnotatedInstance,
        *,
        time_limit: float | None = None,
        mip_gap: float = 1e-4,
        threads: int = 0,
        verbose: bool = True,
        big_m: float | None = None,
    ) -> None:
        """Initialize the TSPN MIP solver.

        Args:
            instance: TSPN Instance or AnnotatedInstance with convex polygon neighborhoods.
            time_limit: Maximum solve time in seconds (None for unlimited).
            mip_gap: Target MIP gap (default 0.01%).
            threads: Number of threads (0 = automatic).
            verbose: Whether to print Gurobi output.
            big_m: Big-M constant for linearization (auto-computed if None).

        """
        super().__init__(
            instance,
            time_limit=time_limit,
            mip_gap=mip_gap,
            threads=threads,
            verbose=verbose,
            big_m=big_m,
        )

        self._n = len(instance)
        self.halfspaces = self._extract_halfspaces_from_instance(instance)

    @property
    def n(self) -> int:
        """Number of neighborhoods."""
        return self._n

    def _extract_halfspaces_from_instance(
        self, instance: Instance
    ) -> list[tuple[np.ndarray, np.ndarray]]:
        """Extract H-representation for each polygon in a C++ Instance."""
        halfspaces = []
        for i in range(len(instance)):
            geometry = instance[i]
            polygon = geometry.definition()
            outer_ring = polygon.outer()
            vertices = extract_vertices_from_ring(outer_ring)
            halfspaces.append(polygon_to_halfspaces(vertices))
        return halfspaces

    def _extract_halfspaces_from_shapely(
        self, polygons: list[ShapelyPolygon]
    ) -> list[tuple[np.ndarray, np.ndarray]]:
        """Extract H-representation for each Shapely polygon."""
        halfspaces = []
        for polygon in polygons:
            vertices = list(polygon.exterior.coords)
            if len(vertices) > 1 and vertices[0] == vertices[-1]:
                vertices = vertices[:-1]
            halfspaces.append(polygon_to_halfspaces(vertices))
        return halfspaces

    def _compute_big_m(self) -> float:
        """Compute a valid big-M based on the problem geometry."""
        if self.big_m is not None:
            return self.big_m

        all_bounds = []
        for _A, b in self.halfspaces:
            max_coord = np.max(np.abs(b)) * 2
            all_bounds.append(max_coord)

        max_bound = max(all_bounds) if all_bounds else 1000.0
        return max_bound * 2 * np.sqrt(2)

    def _add_neighborhood_constraints(
        self,
        model: gp.Model,
        q: dict[tuple[int, int], gp.Var],
        M: float,  # noqa: ARG002
    ) -> None:
        """Add neighborhood constraints A_i @ q_i <= b_i."""
        for i, (A, b) in enumerate(self.halfspaces):
            for k in range(len(b)):
                model.addConstr(
                    A[k, 0] * q[i, 0] + A[k, 1] * q[i, 1] <= b[k],
                    name=f"poly_{i}_{k}",
                )

    def _get_solver_name(self) -> str:
        return "mip"


def solve_tspn_mip(
    instance: Instance,
    *,
    time_limit: float | None = None,
    mip_gap: float = 1e-4,
    threads: int = 0,
    verbose: bool = True,
    big_m: float | None = None,
    initial_solution: AnnotatedSolution | None = None,
) -> AnnotatedSolution:
    """Solve a TSPN instance using the MIP formulation.

    Args:
        instance: TSPN Instance with polygon neighborhoods.
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
    solver = TSPNMIPSolver(
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
