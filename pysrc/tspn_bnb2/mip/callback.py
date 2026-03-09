"""Lazy subtour elimination callback for the Gurobi MIP solver."""

import gurobipy as gp
from gurobipy import GRB


class SubtourCallback:
    """Callback for adding lazy subtour elimination constraints."""

    def __init__(self, n: int, xi: dict[tuple[int, int], gp.Var]) -> None:
        """Initialize subtour callback with edge variables."""
        self.n = n
        self.xi = xi
        self.num_cuts = 0

    def __call__(self, model: gp.Model, where: int) -> None:
        """Add lazy subtour elimination constraints when an integer solution is found."""
        if where != GRB.Callback.MIPSOL:
            return

        xi_vals = model.cbGetSolution(self.xi)

        # Build adjacency from selected edges
        adj: dict[int, list[int]] = {i: [] for i in range(self.n)}
        for (i, j), val in xi_vals.items():
            if val > 0.5:
                adj[i].append(j)
                adj[j].append(i)

        # Find connected components using DFS
        visited = [False] * self.n
        components: list[list[int]] = []

        for start in range(self.n):
            if visited[start]:
                continue
            component = []
            stack = [start]
            while stack:
                node = stack.pop()
                if visited[node]:
                    continue
                visited[node] = True
                component.append(node)
                stack.extend(adj[node])
            if component:
                components.append(component)

        if len(components) <= 1:
            return

        # Add subtour elimination for components not containing vertex 0
        for component in components:
            if 0 in component or len(component) < 3:
                continue

            S = set(component)
            V_minus_S = set(range(self.n)) - S

            cut_edges = []
            for i in S:
                for j in V_minus_S:
                    key = (min(i, j), max(i, j))
                    cut_edges.append(self.xi[key])

            model.cbLazy(gp.quicksum(cut_edges) >= 2)
            self.num_cuts += 1
