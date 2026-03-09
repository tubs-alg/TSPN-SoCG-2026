# %%
from algbench import read_as_pandas
from tspn_bnb2.misc import init_params
from tspn_bnb2.schemas import AnnotatedInstance, AnnotatedSolution

init_params()
# %%
search_strategy_labels = {
    "CheapestChildDepthFirst": "DFS",
    "CheapestBreadthFirst": "BFS",
    "DfsBfs": "DFS+BFS",
    "Random": "Random",
}


# %%
def parse_simplification(row):
    if not row["parameters"]["args"]["alg_params"]["node_simplification"]:
        return "no simplification"
    alg = row["parameters"]["args"]["alg_params"]
    return (
        rf"simplification ($g={alg['max_global_toggle']},"
        rf" \ell={alg['max_local_toggle']}$)"
    )


def read_row(row):
    instance = AnnotatedInstance.model_validate_json(
        row["parameters"]["args"]["kwargs"]["instance_json"]
    )
    solution = (
        AnnotatedSolution.model_validate_json(row["result"]["solution"])
        if row["result"]["solution"]
        else None
    )

    if solution is None:
        print(
            row["result"]["error"],
            row["parameters"]["args"]["kwargs"]["instance_name"],
            row["parameters"]["args"]["alg_params"],
        )
        return None

    return {
        "solution": solution,
        "upper_bound": solution.upper_bound,
        "lower_bound": solution.lower_bound,
        "relative_gap": solution.relative_gap,
        "is_optimal": solution.is_optimal,
        "instance_name": row["parameters"]["args"]["kwargs"]["instance_name"],
        "instance": instance,
        "solve_time": row["result"].get("solve_time"),
        "n": instance.num_polygons(),
        "node_simplification": row["parameters"]["args"]["alg_params"]["node_simplification"],
        "max_local_toggle": row["parameters"]["args"]["alg_params"]["max_local_toggle"],
        "max_global_toggle": row["parameters"]["args"]["alg_params"]["max_global_toggle"],
        "simplification": parse_simplification(row),
        "num_explored": solution.statistics.get("num_explored", 0),
        "te_enable_count": solution.statistics.get("te_enable_count", 0),
        "te_disable_count": solution.statistics.get("te_disable_count", 0),
        "instance_type": instance.derive_instance_type(),
    }


benchmark = read_as_pandas("results_node_simplification", read_row)
benchmark = benchmark.sort_values(by=["simplification"])
print("loaded", len(benchmark), "runs")
print(benchmark.groupby("simplification")["is_optimal"].value_counts())
print(benchmark.groupby("simplification")["num_explored"].describe())


print("loaded", len(benchmark), "runs")
# %%
print(benchmark)
# %%
# Describe the data we have in the table
df = benchmark[
    [
        "instance_name",
        "instance_type",
        "upper_bound",
        "lower_bound",
        "solve_time",
        "n",
        "node_simplification",
        "max_local_toggle",
        "max_global_toggle",
        "simplification",
    ]
]
# %%
df.to_csv(
    "/home/krupke/Repositories/tspn/dominiks_data_analysis/simplification/simplification_data.csv"
)

# %%
