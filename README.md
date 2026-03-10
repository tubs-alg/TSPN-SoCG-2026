# TSPN BnB2: An exact and modular Traveling Salesman Problem with Neighborhoods Solver

**Version 0.2.1** | **Status: Beta/Early Stable**

*Developed by [Sandor P. Fekete](https://www.ibr.cs.tu-bs.de/users/fekete/), [Rouven Kniep](https://www.ibr.cs.tu-bs.de/users/kniep/), [Dominik Krupke](https://www.ibr.cs.tu-bs.de/users/krupke/), and [Michael Perk](https://www.ibr.cs.tu-bs.de/users/perk/) at TU Braunschweig. Initial development with Barak Ugav at Tel Aviv University.*

The *Traveling Salesman Problem with Neighborhoods* (TSPN) asks for the shortest tour that visits a given set of polygons (neighborhoods).
It is a generalization of the classical [Traveling Salesman Problem](https://en.wikipedia.org/wiki/Travelling_salesman_problem)
and the *Close-Enough Traveling Salesman Problem* (CE-TSP), which considers only circular neighborhoods.
The problem is significantly more challenging as the distances between two successive neighborhoods are not constant,
and neighborhoods can be non-convex polygons, possibly with holes.
The problem is NP-hard, and thus, very challenging to solve to optimality.
We provide an algorithm implementation that is capable of solving reasonably sized instances to provable optimality
within seconds to minutes.
Depending on the character of the instance, the solvable instance size is between 20 to multiple hundred neighborhoods.

The implementation is a [Branch and Bound algorithm](https://youtu.be/KMlyhggSqYw) making use of a Second Order Cone Program (SOCP), building on
the work of [Coutinho et al.](https://optimization-online.org/2014/02/4248/).

**Key Features:**
- A highly modular implementation with configurable search and branching strategies
- Warm starts with initial solutions provided by heuristics
- Callbacks allowing lazy constraints, custom heuristics, and advanced search control
- Parallelization of child node evaluation
- New pruning rules based on geometric insights (convex hull constraints)
- Branching degree reduction giving exponential speed-ups in some cases
- Geometric preprocessing to simplify instances before solving

**Academic Publications:**
- Paper: "A Branch-and-Bound Algorithm for the Traveling Salesman Problem with Difficult Neighborhoods" (SoCG2026, EuroCG2025)
- Full version in `paper/` directory

## Installation

### Prerequisites

**Required:**
- **Gurobi license** (version >= 12.0.0): Required for the SOCP solver. You can easily get a free license for academic purposes at [gurobi.com](https://www.gurobi.com/). The free non-academic license is probably not sufficient and will lead to errors.
- **Python 3.12+**: For the Python bindings and utilities
- **CMake 3.23+**: For building the C++ components
- **C++23 compiler**: GCC 13+, Clang 16+, or MSVC 19.35+

**C++ Dependencies (managed by Conan):**
- CGAL >= 6.0.1 (free for academic use, requires license for commercial use)
- Boost >= 1.83.0
- pybind11 >= 2.13.6
- fmt >= 11.2.0
- nlohmann_json >= 3.11
- mpfr >= 4.2.1
- doctest >= 2.4.11 (for C++ tests)

**Python Dependencies (installed automatically):**
- Core: chardet, networkx, requests, shapely ~= 2.1.2, pydantic ~= 2.12.5
- Development: pytest, matplotlib, rich, slurminade, aemeasure

### Python Installation

1. **Install the package:**
   ```bash
   cd tspn
   pip install .
   ```

2. **Install in development mode (editable):**
   ```bash
   cd tspn
   pip install -e .
   # or for setup.py develop workflow:
   python3 setup.py develop
   ```

3. **Test the installation:**
   ```bash
   cd tspn
   pytest -s tests
   ```

### C++ Development Setup

For C++ development, the recommended approach uses the automated build script:

```bash
cd tspn
./build_and_run_tests.sh
```

This script:
- Ensures Conan dependencies are installed via pip editable install
- Configures CMake with the Conan toolchain
- Uses Ninja generator if available (for faster builds)
- Builds the C++ tests
- Runs the doctest executable

**Manual C++ build:**
```bash
cd tspn
# Install conan dependencies first
conan install . --build=missing
# Configure CMake
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=.conan/release/conan_toolchain.cmake
# Build
cmake --build build
# Run C++ tests
cd build && ctest
```

**Integration into existing C++ projects:**
- Copy the `tspn/` subdirectory into your project
- Install Conan dependencies using the provided `conanfile.txt`
- Add via `add_subdirectory(tspn)` in your `CMakeLists.txt`
- Link against the `tspn` target

## Quick Start

The following example shows the full recommended workflow: create an instance from
Shapely polygons, simplify geometries, add order annotations for pruning, and
solve with Branch and Bound. This mirrors `examples/05_order_pruning.py`.

```python
from shapely.geometry import Point as ShapelyPoint

from tspn_bnb2 import AnnotatedInstance, solve_annotated_instance
from tspn_bnb2.operations import simplify_annotated_instance
from tspn_bnb2.order_annotation import add_order_annotations

# --- Step 1: Create an instance from Shapely polygons ---
polygons = [ShapelyPoint(x, y).buffer(0.5) for x in range(6) for y in range(6)]
instance = AnnotatedInstance(polygons=polygons)
print(f"{instance.num_polygons()} polygons")

# --- Step 2: Simplify geometries ---
# Removes unnecessary holes, fills gaps between polygon and convex hull,
# and removes polygons fully contained in others.
simplified = simplify_annotated_instance(
    instance, remove_holes=True, convex_hull_fill=True, remove_supersites=True
)
print(f"{simplified.num_polygons()} polygons after simplification")

# --- Step 3: Add order annotations ---
# Computes convex hull ordering and overlap detection, enabling
# the OrderFiltering pruning rule.
annotated = add_order_annotations(simplified)
hull_polys = annotated.hull_polygons()
print(f"{len(hull_polys)} polygons on convex hull")

# --- Step 4: Solve with Branch and Bound ---
solution = solve_annotated_instance(
    annotated,
    timelimit=60,
    root="OrderRoot",            # start from convex hull order
    rules=["OrderFiltering"],    # prune branches violating hull order
    search="DfsBfs",             # depth-first, then best-first
    node_simplification=True,    # remove non-spanning tour elements
    decomposition_branch=True,   # branch on convex decomposition pieces
    eps=0.01,                    # optimality tolerance
)

# --- Step 5: Inspect the solution ---
print(f"Tour length: {solution.upper_bound:.4f}")
print(f"Lower bound: {solution.lower_bound:.4f}")
print(f"Gap: {solution.relative_gap * 100:.2f}%")
print(f"Optimal: {solution.is_optimal}")
print(f"Statistics: {solution.statistics}")
```

See `examples/05_order_pruning.py` for the full version with side-by-side
visualization comparing with and without order pruning.

### More Examples

The `examples/` directory contains additional examples:

- **`01_mip.py`**: Solve TSPN using the MIP formulation with random convex polygons
- **`02_mip_nonconvex.py`**: Solve TSPN with non-convex polygons using the MIP formulation
- **`03_warm_start_visualization.py`**: Visualize Christofides warm start solutions
- **`04_annotated_solution.py`**: Solving with `AnnotatedInstance` and `AnnotatedSolution`
- **`05_order_pruning.py`**: Full order-pruning pipeline with simplification and visualization

## Algorithm Architecture

The Branch and Bound algorithm is highly modular and can be customized by replacing strategies. The main entry point is `tspn_core/bnb.h`.

### Three Core Strategies

The algorithm's behavior is controlled by three primary strategies that can be mixed and matched.
From Python, these are selected by name via `solve_annotated_instance()` or `branch_and_bound()`.

#### 1. Root Node Strategy

**Location:** `tspn_core/strategies/root_node_strategy.h`

Determines the initial relaxed solution that serves as the starting point for the branch and bound tree.

**Available Implementations:**
- **`LongestEdgePlusFurthestSite`** (default): Classical approach using the longest edge plus the farthest neighborhood, creating an initial triangular tour.
- **`OrderRoot`**: Starts with the convex hull order sequence. Required for order-based pruning rules.
- **`LongestTriple`**, **`LongestPair`**, **`RandomPair`**, **`RandomRoot`**: Alternative root strategies for experimentation.

#### 2. Branching Strategy

**Location:** `tspn_core/strategies/branching_strategy.h`

Decides how to split the solution space at each node. Unlike classical MIP solvers that branch on fractional variables, this solver branches by selecting which neighborhood to add and where to insert it in the sequence.

**Key Aspects:**
- **Neighborhood Selection**: Typically selects the farthest uncovered neighborhood to maximize lower bound improvement
- **Insertion Positions**: Considers all valid positions in the sequence
- **Pruning Rules**: Custom rules (`tspn_core/strategies/rule.h`) can eliminate suboptimal branches without computing trajectories
- **Simplification**: Removes non-spanning neighborhoods from sequences to reduce branching factor
- **Parallelization**: Child nodes are computed in parallel for better CPU utilization

**Available Implementations:**
- **`FarthestPoly`** (default): Branches on the farthest uncovered neighborhood. Supports custom pruning rules.
- **`RandomPoly`**: Branches on a random uncovered neighborhood (mainly for testing).

**Pruning Rules:**
- **`OrderFiltering`**: Checks convex hull order constraints to prune invalid sequences. Enable via `rules=["OrderFiltering"]`.

#### 3. Search Strategy

**Location:** `tspn_core/strategies/search_strategy.h`

Controls the order in which nodes in the branch and bound tree are explored.

**Available Implementations:**
- **`DfsBfs`** (default): Uses depth-first search until a node is pruned or becomes feasible, then switches to breadth-first (cheapest leaf). This is a popular technique in modern MILP solvers and provides a good balance.
- **`CheapestChildDepthFirst`**: Always explores the most promising child of the current node.
- **`CheapestBreadthFirst`**: Always explores the node with the lowest lower bound.
- **`Random`**: Random node selection (mainly for testing).

### Callbacks

**Location:** `tspn_core/callbacks.h`

Callbacks provide extensibility points for customizing the search process at runtime.

**Use Cases:**
- Add lazy constraints during the search
- Inject custom heuristics for better upper bounds
- Log progress and statistics

**Example: Monitoring the Search**

```python
from shapely.geometry import Point as ShapelyPoint

from tspn_bnb2 import AnnotatedInstance, solve_annotated_instance

polygons = [ShapelyPoint(x, y).buffer(0.5) for x in range(5) for y in range(5)]
instance = AnnotatedInstance(polygons=polygons)

def callback(context):
    node = context.current_node
    if context.is_feasible():
        print(f"Feasible at depth {node.depth()}, "
              f"LB={node.get_lower_bound():.2f}, UB={context.get_upper_bound():.2f}")

solution = solve_annotated_instance(instance, callback=callback, timelimit=60)
```

## Related Work

This Branch and Bound algorithm builds upon and significantly extends [the work of Coutinho et al.](https://optimization-online.org/2014/02/4248/) for CE-TSP with circles, adapting it to handle general polygonal neighborhoods.

This [master thesis](https://dspace.cvut.cz/bitstream/handle/10467/96747/F3-DP-2021-Fanta-Lukas-Fanta_Lukas_The_Close_Enough%20_Travelling%20_Salesman_Problem_in_polygonal_domain.pdf?sequence=-1&isAllowed=y)
provides a good overview of the previous work and provides an LNS-algorithm
(a highly effective and modern heuristic for hard combinatorial optimization problems).

This [paper](https://www.researchgate.net/profile/Carmine-Cerrone/publication/308037271_A_Novel_Discretization_Scheme_for_the_Close_Enough_Traveling_Salesman_Problem/links/5af8721da6fdcc0c03289b38/A-Novel-Discretization-Scheme-for-the-Close-Enough-Traveling-Salesman-Problem.pdf)
uses a GTSP-MIP for solving the CE-TSP.
The reduction techniques and geometric observations may be interesting for this problem.

## License and Citation

This project is available for academic use. CGAL is free for academic use but requires a license for commercial usage.

**Citation:**
```
@software{tspn_bnb2,
  title = {TSPN BnB2: An exact and modular Traveling Salesman Problem with Neighborhoods Solver},
  author = {Fekete, S\'{a}ndor P. and Kniep, Rouven and Krupke, Dominik and Perk, Michael},
  year = {2026},
  version = {0.2.1}
}
```

**Paper Citation:**
```
@inproceedings{fekete2026tspn,
  title = {A Branch-and-Bound Algorithm for the Traveling Salesman Problem with Difficult Neighborhoods},
  author = {Fekete, S\'{a}ndor P. and Kniep, Rouven and Krupke, Dominik and Perk, Michael},
  booktitle = {Proceedings of the 42nd International Symposium on Computational Geometry (SoCG 2026)},
  year = {2026}
}
```
