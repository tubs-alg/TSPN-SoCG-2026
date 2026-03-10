"""MIP-based solver for the Traveling Salesman Problem with Neighborhoods (TSPN).

This module implements the TSPN as a Mixed Integer Second-Order Cone Program (MISOCP)
using Gurobi. The formulation uses binary edge variables and continuous position
variables, with subtour elimination constraints added lazily via callbacks.

Two solvers are provided:
- TSPNMIPSolver: For convex polygon neighborhoods only
- TSPNNonConvexMIPSolver: For general (including non-convex) polygon neighborhoods
"""

from .nonconvex_solver import TSPNNonConvexMIPSolver, solve_tspn_nonconvex_mip
from .solver import TSPNMIPSolver, solve_tspn_mip
from .warm_start import generate_christofides_warm_start

__all__ = [
    "TSPNMIPSolver",
    "TSPNNonConvexMIPSolver",
    "generate_christofides_warm_start",
    "solve_tspn_mip",
    "solve_tspn_nonconvex_mip",
]
