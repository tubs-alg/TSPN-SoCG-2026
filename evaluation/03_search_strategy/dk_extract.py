# %%
from algbench import read_as_pandas
from tspn_bnb2.plotting import init_params
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
        "search_strategy": search_strategy_labels[
            row["parameters"]["args"]["alg_params"]["search"]
        ],
        "eps": row["parameters"]["args"]["alg_params"]["eps"],
        "instance_type": instance.derive_instance_type(),
    }


benchmark = read_as_pandas("results_search_strategy", read_row)
print(benchmark.columns)
benchmark = benchmark.sort_values(by=["search_strategy"], ascending=False)

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
        "search_strategy",
        "eps",
    ]
]
df = df[df["eps"] == 0.01]
# %%
df.to_csv("/home/krupke/Repositories/tspn/dominiks_data_analysis/search/search_strategy_data.csv")

# %%
