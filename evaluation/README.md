# Evaluation

This directory contains the experimental evaluation framework for the CE-TSPN Branch-and-Bound solver.
The experiments are organized around five research questions (RQs) that evaluate different aspects of the algorithm.

## Instance Sets

All instance files are stored in `instances/` as compressed zip files:

| File | Content                                         | Description                                                                                                  |
|------|-------------------------------------------------|--------------------------------------------------------------------------------------------------------------|
| `instances_socg.zip` | 558 instances                                   | Full benchmark set with mixed polygon counts (30-60+)                                                        |
| `instances_socg_30.zip` | 30-polygon (subset of `instances_socg.zip`)     | Smaller instances for quick testing                                                                          |
| `instances_socg_60.zip` | 60-polygon (subset of `instances_socg.zip`)     | Larger instances for scalability testing                                                                     |
| `instances_socg_simplified.zip` | 558 instances (derived from `instances_socg.zip`) | Simplified version (holes removed, convex hull fill). This resembles the instance set `full` from the paper. |
| `instances_socg_60_simplified.zip` | 60-polygon simplified (derived from `instances_socg_60.zip`)                           | Simplified 60-polygon instances. This resembles the instance set `small` from the paper.                     |

**Instance types:**
- **OSM**: Real-world instances from OpenStreetMap building footprints
- **Tessellation**: Generated from geometric tessellation patterns
- **Random**: Randomly generated polygon neighborhoods

## Experiments

### 00_simplification - Instance Preprocessing

Applies polygon simplification techniques to reduce instance complexity:
- Removes holes from polygons
- Applies convex hull fill
- Removes supersites (non-spanning polygons)
- Adds order annotations
---

### rq1_root_node_selection - Root Node Strategy Comparison

Evaluates four strategies for selecting the initial root node configuration:

| Strategy | Code Name | Description |
|----------|-----------|-------------|
| LEFP | `LongestEdgePlusFurthestSite` | Farthest site from longest edge |
| Random | `RandomRoot` | Random initialization |
| LE | `LongestPair` | Longest pair strategy |
| CHR | `OrderRoot` | Christofides-based ordering |

---

### rq2_search_strategy - Search Strategy Comparison

Compares three tree traversal strategies:

| Strategy | Code Name | Description |
|----------|-----------|-------------|
| DFS | `CheapestChildDepthFirst` | Depth-first with cheapest child |
| BFS | `CheapestBreadthFirst` | Breadth-first with cheapest node |
| DFS+BFS | `DfsBfs` | Hybrid approach |

---

### rq3_decomposition_branching - Decomposition Strategy

Evaluates the impact of decomposition-based branching:
- With decomposition: `decomposition_branch=True`
- Without decomposition: `decomposition_branch=False`

---

### rq4_scalability - Scalability Analysis

Tests solver performance across instance sizes and optimality tolerances.

**Short runs (300s):**
- Full instance set (558 instances)
- 2 root strategies x 5 eps values = 10 configurations
- Results in `results_optimality_gaps_300/`

**Long runs (900s):**
- 16 hard instances selected from unsuccessful short runs
- Results in `long_runs/results_optimality_gaps_900/`

---

### rq5_state_of_the_art - MIP Solver Comparison

Compares the branch-and-bound solver against Gurobi's MIP solver:
- Uses nonconvex MIP formulation
- Warm-started with Christofides heuristic
- Tests 5 MIP gap tolerances: 0.001, 0.01, 0.05, 0.1, 0.15

Note that the results from this benchmark are not part of the main paper.

## Analysis Notebooks

Each experiment includes a `02_look_at_solutions.ipynb` notebook that:
1. Loads results using algbench
2. Validates solution feasibility
3. Generates performance profiles and plots
4. Computes summary statistics

Generated plots are saved to `plots/`.

## Running Experiments

Experiments use Slurm for distributed computation:

```bash
# Run from the experiment directory
python 01_run_solver.py
```
