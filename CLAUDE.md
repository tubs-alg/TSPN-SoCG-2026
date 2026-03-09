# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

TSPN BnB2 is an exact solver for the Traveling Salesman Problem with Neighborhoods (TSPN), which finds the shortest tour visiting a set of polygon neighborhoods. The implementation is a modular Branch and Bound algorithm using Second Order Cone Programming (SOCP) with Gurobi, built on C++ with Python bindings.

**Key Dependencies:**
- Requires a Gurobi license (free for academic use)
- Uses CGAL for computational geometry
- C++23 standard
- Python 3.12+

## Build and Development Commands

### Python Development

**Install the package:**
```bash
cd tspn
pip install .
```

**Install in development mode (editable):**
```bash
cd tspn
python3 setup.py develop
```

**Run all tests:**
```bash
cd tspn
pytest -s tests
```

**Run a single test file:**
```bash
cd tspn
pytest -s tests/test_tours.py
```

**Run a specific test:**
```bash
cd tspn
pytest -s tests/test_tours.py::test_name
```

### C++ Development

The C++ build system uses CMake with Conan for dependency management:

```bash
cd tspn
# Install conan dependencies first
conan install . --build=missing
# Then build with CMake
cmake -B build -S .
cmake --build build
```

**Run C++ tests:**
```bash
cd tspn/build
ctest
```

### Code Quality

**Linting with ruff:**
```bash
cd tspn
ruff check --fix .
ruff format .
```

**C++ formatting:**
```bash
clang-format -i src/**/*.cpp include/**/*.h
```

## Architecture Overview

### Core Algorithm Structure

The Branch and Bound algorithm is highly modular with three main customizable strategies:

1. **Root Node Strategy** (`include/tspn/strategies/root_node_strategy.h`)
   - Determines initial relaxed solution
   - Implementations include `LongestEdgePlusFurthestSite`, `LongestTriple`, `LongestPair`, etc.

2. **Branching Strategy** (`include/tspn/strategies/branching_strategy.h`)
   - Decides how to split solution space at each node
   - Implementations: `FarthestPoly` (branch on farthest uncovered polygon), `RandomPoly`
   - Supports custom pruning rules (`include/tspn/strategies/rule.h`)
   - Handles parallelization of child node computation

3. **Search Strategy** (`include/tspn/strategies/search_strategy.h`)
   - Controls tree traversal order
   - Types: `DfsBfs`, `CheapestChildDepthFirst`, `CheapestBreadthFirst`, `Random`

### Key Components

- **Branch and Bound Core** (`include/tspn/bnb.h`) - Main algorithm orchestration
- **Node** (`include/tspn/node.h`, `src/node.cpp`) - Represents nodes in the search tree
- **Relaxed Solution** (`include/tspn/relaxed_solution.h`, `src/relaxed_solution.cpp`) - SOCP-based relaxations
- **SOCP Solver** (`src/soc.cpp`) - Gurobi-based Second Order Cone Program solver
- **Callbacks** (`include/tspn/callbacks.h`) - User-defined hooks for custom constraints and heuristics

### Python Bindings

- Python interface defined in `pysrc/tspn_bnb2/core/_tspn_bindings.cpp` using pybind11
- Python modules in `pysrc/tspn_bnb2/` with utilities for IO (`misc/io.py`) and plotting (`misc/plotting.py`)
- Examples in `pyexamples/` demonstrate Voronoi sampling and dynamic programming approaches

### Project Layout

```
tspn/
├── include/tspn/          # Public C++ headers
├── src/                   # C++ implementation
│   ├── branching_strategies/
│   ├── root_node_strategies/
│   ├── details/           # Internal implementation details
│   └── utils/
├── pysrc/tspn_bnb2/       # Python package source
│   ├── core/              # Python bindings
│   └── misc/              # Python utilities
├── tests/                 # Both C++ and Python tests
├── pyexamples/            # Python usage examples
├── instances/             # Test instances (OSM-based)
├── apps/                  # C++ standalone applications
├── cmake/                 # CMake utilities and Conan recipes
└── notebooks/             # Jupyter notebooks for experimentation
```

## Important Implementation Notes

### Building with scikit-build

The package uses `scikit-build` with `skbuild-conan` to seamlessly integrate C++ builds into Python packaging. The `setup.py` configures:
- Conan dependencies (Gurobi, CGAL, Boost, fmt, pybind11, etc.)
- CMake minimum version and build targets
- Python package discovery from `pysrc/` subdirectory

### Geometry and Computational Complexity

- Uses CGAL for robust geometric computations (convex hulls, partitions)
- The problem is NP-hard; solvable instance size ranges from 20 to several hundred polygons depending on structure
- Key optimizations: convex hull pruning, simplification by removing non-spanning polygons, parallel child evaluation

### Callbacks and Extensibility

Python callbacks allow runtime customization:
- Add lazy constraints during search
- Inject custom heuristics
- Modify lower bounds based on problem-specific insights
- Example usage in `README.md` shows how to add distance-based lower bounds

### Testing

- C++ tests use doctest framework (`tests/doctest.cpp`)
- Python tests use pytest (`tests/test_*.py`)
- Tests cover tours, paths, and dynamic programming approaches
